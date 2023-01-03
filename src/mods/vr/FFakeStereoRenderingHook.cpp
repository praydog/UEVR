#include <windows.h>
#include <winternl.h>

#include <asmjit/asmjit.h>

#include <spdlog/spdlog.h>
#include <utility/Memory.hpp>
#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/String.hpp>
#include <utility/Thread.hpp>
#include <utility/Emulation.hpp>

#include <sdk/EngineModule.hpp>
#include <sdk/UEngine.hpp>
#include <sdk/UGameEngine.hpp>
#include <sdk/CVar.hpp>
#include <sdk/Slate.hpp>
#include <sdk/DynamicRHI.hpp>
#include <sdk/FViewportInfo.hpp>

#include "Framework.hpp"
#include "Mods.hpp"

#include <bdshemu.h>
#include <bddisasm.h>
#include <disasmtypes.h>

#include "../VR.hpp"

#include "FFakeStereoRenderingHook.hpp"

//#define FFAKE_STEREO_RENDERING_LOG_ALL_CALLS

FFakeStereoRenderingHook* g_hook = nullptr;

// Scan through function instructions to detect usage of double
// floating point precision instructions.
bool is_using_double_precision(uintptr_t addr) {
    spdlog::info("Scanning function at {:x} for double precision usage", addr);

    bool result = false;

    utility::exhaustive_decode((uint8_t*)addr, 30, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
        if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
            return utility::ExhaustionResult::STEP_OVER;
        }

        if (ix.Instruction == ND_INS_MOVSD && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG) {
            spdlog::info("[UE5 Detected] Detected Double precision MOVSD at {:x}", (uintptr_t)ip);
            result = true;
            return utility::ExhaustionResult::BREAK;
        }

        if (ix.Instruction == ND_INS_ADDSD) {
            spdlog::info("[UE5 Detected] Detected Double precision ADDSD at {:x}", (uintptr_t)ip);
            result = true;
            return utility::ExhaustionResult::BREAK;
        }

        return utility::ExhaustionResult::CONTINUE;
    });

    return result;
}

FFakeStereoRenderingHook::FFakeStereoRenderingHook() {
    g_hook = this;
}

void FFakeStereoRenderingHook::attempt_hooking() {
    if (m_finished_hooking || m_tried_hooking) {
        return;
    }

    if (!m_injected_stereo_at_runtime) {
        attempt_runtime_inject_stereo();
        m_injected_stereo_at_runtime = true;
    }
    
    m_hooked = hook();
}

void FFakeStereoRenderingHook::attempt_hook_game_engine_tick() {
    if (m_hooked_game_engine_tick || m_attempted_hook_game_engine_tick) {
        return;
    }
    
    spdlog::info("Attempting to hook UGameEngine::Tick!");

    m_attempted_hook_game_engine_tick = true;

    const auto func = sdk::UGameEngine::get_tick_address();

    if (!func) {
        spdlog::error("Cannot hook UGameEngine::Tick");
        return;
    }

    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    m_tick_hook = builder.create_inline((void*)*func, +[](sdk::UGameEngine* engine, float delta, bool idle) -> void* {
        static bool once = true;

        if (once) {
            spdlog::info("First time calling UGameEngine::Tick!");
            once = false;
        }

        if (!g_framework->is_game_data_intialized()) {
            return g_hook->m_tick_hook->call<void*>(engine, delta, idle);
        }

        g_hook->attempt_hooking();

        const auto& mods = g_framework->get_mods()->get_mods();
        for (auto& mod : mods) {
            mod->on_pre_engine_tick(engine, delta);
        }

        const auto result = g_hook->m_tick_hook->call<void*>(engine, delta, idle);

        for (auto& mod : mods) {
            mod->on_post_engine_tick(engine, delta);
        }

        return result;
    });

    m_hooked_game_engine_tick = true;

    spdlog::info("Hooked UGameEngine::Tick!");
}

void FFakeStereoRenderingHook::attempt_hook_slate_thread(uintptr_t return_address) {
    if (m_hooked_slate_thread) {
        return;
    }

    if (return_address == 0 && m_attempted_hook_slate_thread) {
        return;
    }

    spdlog::info("Attempting to hook FSlateRHIRenderer::DrawWindow_RenderThread!");
    m_attempted_hook_slate_thread = true;

    auto func = sdk::slate::locate_draw_window_renderthread_fn();

    if (!func && return_address == 0) {
        spdlog::error("Cannot hook FSlateRHIRenderer::DrawWindow_RenderThread");
        return;
    }

    if (return_address != 0) {
        func = utility::find_function_start_with_call(return_address);

        if (!func) {
            spdlog::error("Cannot hook FSlateRHIRenderer::DrawWindow_RenderThread with alternative return address method");
            m_hooked_slate_thread = true; // not actually true but just to stop spamming the scans
            return;
        }

        spdlog::info("Found FSlateRHIRenderer::DrawWindow_RenderThread with alternative return address method: {:x}", *func);
    }

    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    m_slate_thread_hook = builder.create_inline((void*)*func, &FFakeStereoRenderingHook::slate_draw_window_render_thread);
    m_hooked_slate_thread = true;

    spdlog::info("Hooked FSlateRHIRenderer::DrawWindow_RenderThread!");
}

bool FFakeStereoRenderingHook::hook() {
    spdlog::info("Entering FFakeStereoRenderingHook::hook");

    m_tried_hooking = true;

    //sdk::FDynamicRHI::get();

    //experimental_patch_fix();

    const auto vtable = locate_fake_stereo_rendering_vtable();

    if (!vtable) {
        spdlog::error("Failed to locate Fake Stereo Rendering VTable, attempting to perform nonstandard hook");
        return nonstandard_create_stereo_device_hook();
    }

    return standard_fake_stereo_hook(*vtable);
}

bool FFakeStereoRenderingHook::standard_fake_stereo_hook(uintptr_t vtable) {
    spdlog::info("Performing standard fake stereo hook");

    const auto game = sdk::get_ue_module(L"Engine");
    std::array<uint8_t, 0x1000> og_vtable{};
    memcpy(og_vtable.data(), (void*)vtable, og_vtable.size()); // to perform tests on.

    const auto module_vtable_within = utility::get_module_within(vtable);

    auto is_vfunc_pattern = [](uintptr_t addr, std::string_view pattern) {
        if (utility::scan(addr, 100, pattern.data()).value_or(0) == addr) {
            return true;
        }

        // In some very rare cases, there can be obfuscation that makes the above scan fail.
        // One such form of obfuscation is multiple jmps and/or calls to reach the real function.
        // We can use emulation to find the real function.
        const auto module_within = utility::get_module_within(addr);

        if (!module_within) {
            spdlog::error("Cannot perform emulation, module not found to create pseudo shellcode");
            return false;
        }

        spdlog::info("Performing emulation to find real function...");

        utility::ShemuContext ctx{ *module_within };

        ctx.ctx->Registers.RegRip = addr;
        bool prev_was_branch = true;

        do {
            prev_was_branch = ctx.ctx->InstructionsCount == 0 || ctx.ctx->Instruction.BranchInfo.IsBranch;

            if (prev_was_branch) {
                spdlog::info("Branch!");
            }

            spdlog::info("Emulating at {:x}", ctx.ctx->Registers.RegRip);

            if (prev_was_branch && !IsBadReadPtr((void*)ctx.ctx->Registers.RegRip, 4)) {
                if (utility::scan(ctx.ctx->Registers.RegRip, 100, pattern.data()).value_or(0) == ctx.ctx->Registers.RegRip) {
                    spdlog::info("Encountered true vfunc at {:x}", ctx.ctx->Registers.RegRip);
                    return true;
                }
            }
        } while(ctx.emulate() == SHEMU_SUCCESS && ctx.ctx->InstructionsCount < 50);

        spdlog::error("Failed to find true vfunc at {:x}, reason {}", addr, ctx.status);
        return false;
    };

    // In 4.18 the destructor virtual doesn't exist or is at the very end of the vtable.
    const auto is_stereo_enabled_index = is_vfunc_pattern(*(uintptr_t*)vtable, "B0 01") ? 0 : 1;
    const auto is_stereo_enabled_func_ptr = &((uintptr_t*)vtable)[is_stereo_enabled_index];

    spdlog::info("IsStereoEnabled Index: {}", is_stereo_enabled_index);

    const auto stereo_view_offset_index = get_stereo_view_offset_index(vtable);

    if (!stereo_view_offset_index) {
        spdlog::error("Failed to locate Stereo View Offset Index");
        return false;
    }

    const auto stereo_projection_matrix_index = *stereo_view_offset_index + 1;
    const auto is_4_18_or_lower = stereo_view_offset_index <= 6;

    const auto stereo_view_offset_func = ((uintptr_t*)vtable)[*stereo_view_offset_index];

    auto render_texture_render_thread_func = utility::find_virtual_function_from_string_ref(game, L"RenderTexture_RenderThread");

    // Seems more robust than simply just checking the vtable index.
    m_uses_old_rendertarget_manager = stereo_view_offset_index <= 11 && !render_texture_render_thread_func;

    if (!render_texture_render_thread_func) {
        // Fallback scan to checking for the first non-default virtual function (<= 4.18)
        spdlog::info("Failed to find RenderTexture_RenderThread, falling back to first non-default virtual function");

        for (auto i = 2; i < 10; ++i) {
            const auto func = ((uintptr_t*)vtable)[stereo_projection_matrix_index + i];

            if (!utility::is_stub_code((uint8_t*)func)) {
                render_texture_render_thread_func = func;
                break;
            }
        }

        if (!render_texture_render_thread_func) {
            spdlog::error("Failed to find RenderTexture_RenderThread");
            return false;
        }
    }

    spdlog::info("RenderTexture_RenderThread: {:x}", (uintptr_t)*render_texture_render_thread_func);

    // Scan for the function pointer, it should be in the middle of the vtable.
    auto rendertexture_fn_vtable_middle = utility::scan_ptr(vtable + ((stereo_projection_matrix_index + 2) * sizeof(void*)), 50 * sizeof(void*), *render_texture_render_thread_func);

    if (!rendertexture_fn_vtable_middle) {
        spdlog::error("Failed to find RenderTexture_RenderThread VTable Middle");
        return false;
    }

    auto rendertexture_fn_vtable_index = (*rendertexture_fn_vtable_middle - vtable) / sizeof(uintptr_t);
    spdlog::info("RenderTexture_RenderThread VTable Middle: {} {:x}", rendertexture_fn_vtable_index, (uintptr_t)*rendertexture_fn_vtable_middle);

    auto render_target_manager_vtable_index = rendertexture_fn_vtable_index + 1 + (2 * (size_t)is_4_18_or_lower);

    // verify first that the render target manager index is returning a null pointer
    // and if not, scan forward until we run into a vfunc that returns a null pointer
    auto get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

    bool is_4_11 = false;

    if (!is_vfunc_pattern(*(uintptr_t*)get_render_target_manager_func_ptr, "33 C0")) {
        spdlog::info("Expected GetRenderTargetManager function at index {} does not return null, scanning forward for return nullptr.", render_target_manager_vtable_index);

        for (;;++render_target_manager_vtable_index) {
            get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

            if (IsBadReadPtr(*(void**)get_render_target_manager_func_ptr, 1)) {
                spdlog::error("Failed to find GetRenderTargetManager vtable index, a crash is imminent");
                return false;
            }

            if (is_vfunc_pattern(*(uintptr_t*)get_render_target_manager_func_ptr, "33 C0")) {
                const auto distance_from_rendertexture_fn = render_target_manager_vtable_index - rendertexture_fn_vtable_index;

                // means it's 4.17 I think. 12 means 4.11.
                if (distance_from_rendertexture_fn == 10 || distance_from_rendertexture_fn == 11 || distance_from_rendertexture_fn == 12) {
                    is_4_11 = distance_from_rendertexture_fn == 12;
                    m_rendertarget_manager_embedded_in_stereo_device = true;
                    spdlog::info("Render target manager appears to be directly embedded in the stereo device vtable");
                } else {
                    // Now this may potentially be the correct index, but we're not quite done yet.
                    // On 4.19 (and possibly others), the index is 1 higher than it should be.
                    // We can tell by checking how many functions in front of this index return null.
                    // if there are two functions in front of this index that return null, we need to add 1 to the index.
                    spdlog::info("Found potential GetRenderTargetManager function at index {}", render_target_manager_vtable_index);
                    spdlog::info("Double checking GetRenderTargetManager index...");

                    int32_t count = 0;
                    for (auto i = render_target_manager_vtable_index + 1; i < render_target_manager_vtable_index + 5; ++i) {
                        const auto addr_of_func = (uintptr_t)&((uintptr_t*)vtable)[i];
                        const auto func = ((uintptr_t*)vtable)[i];

                        if (func == 0 || IsBadReadPtr((void*)func, 1)) {
                            break;
                        }

                        // Make sure we didn't cross over into another vtable's boundaries.
                        const auto module_within = utility::get_module_within(addr_of_func);

                        if (module_within && utility::scan_displacement_reference(*module_within, addr_of_func)) {
                            spdlog::info("Crossed over into another vtable's boundaries, aborting double check");
                            spdlog::info("Reached end of double check at index {}, {} appears to be the correct index.", i, render_target_manager_vtable_index);
                            break;
                        }

                        if (!is_vfunc_pattern(func, "33 C0")) {
                            spdlog::info("Reached end of double check at index {}, {} appears to be the correct index.", i, render_target_manager_vtable_index);
                            break;
                        }

                        if (++count >= 2) {
                            ++render_target_manager_vtable_index;
                            get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

                            spdlog::info("Adjusted GetRenderTargetManager index to {}", render_target_manager_vtable_index);
                            break;
                        }
                    }

                    spdlog::info("Distance: {}", distance_from_rendertexture_fn);
                }

                break;
            }
        }
    } else {
        spdlog::info("GetRenderTargetManager function at index {} appears to be valid.", render_target_manager_vtable_index);
    }
    
    const auto get_stereo_layers_func_ptr = (uintptr_t)(get_render_target_manager_func_ptr + sizeof(void*));

    if (get_render_target_manager_func_ptr == 0) {
        spdlog::error("Failed to find GetRenderTargetManager");
        return false;
    }

    if (get_stereo_layers_func_ptr == 0) {
        spdlog::error("Failed to find GetStereoLayers");
        return false;
    }

    spdlog::info("GetRenderTargetManagerptr: {:x}", (uintptr_t)get_render_target_manager_func_ptr);
    spdlog::info("GetStereoLayersptr: {:x}", (uintptr_t)get_stereo_layers_func_ptr);

    const auto adjust_view_rect_distance = is_4_18_or_lower ? 2 : 3;
    const auto adjust_view_rect_index = *stereo_view_offset_index - adjust_view_rect_distance;

    spdlog::info("AdjustViewRect Index: {}", adjust_view_rect_index);
    
    auto calculate_stereo_projection_matrix_index = *stereo_view_offset_index + 1;

    // While generally most of the time the stereo projection matrix func is the next one after the stereo view offset func,
    // it's not always the case. We can scan for a call to the tanf function in one of the virtual functions to find it.
    for (auto i = 0; i < 10; ++i) {
        const auto potential_func = ((uintptr_t*)vtable)[calculate_stereo_projection_matrix_index + i];
        if (potential_func == 0 || IsBadReadPtr((void*)potential_func, 1) || utility::is_stub_code((uint8_t*)potential_func)) {
            continue;
        }

        auto ip = (uint8_t*)potential_func;
        bool found = false;

        spdlog::info("Scanning {:x}...", (uintptr_t)ip);

        for (auto j = 0; j < 50; ++j) {
            INSTRUX ix{};

            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                spdlog::info("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            if (ix.Category == ND_CAT_RET || ix.InstructionBytes[0] == 0xE9) {
                spdlog::info("Encountered RET or JMP at {:x}, aborting scan", (uintptr_t)ip);
                break;
            }

            if (ix.InstructionBytes[0] == 0xE8) {
                auto called_func = (uintptr_t)(ip + ix.Length + (int32_t)ix.RelativeOffset);
                auto inner_ins = utility::decode_one((uint8_t*)called_func);

                spdlog::info("called {:x}", (uintptr_t)called_func);
                uintptr_t final_func = 0;

                // Fully resolve the pointer jmps until we reach another module.
                while (inner_ins && inner_ins->InstructionBytes[0] == 0xFF && inner_ins->InstructionBytes[1] == 0x25) {
                    const auto called_func_ptr = (uintptr_t*)(called_func + inner_ins->Length + (int32_t)inner_ins->Displacement);
                    const auto called_func_ptr_val = *called_func_ptr;

                    spdlog::info("called ptr {:x}", (uintptr_t)called_func_ptr_val);

                    inner_ins = utility::decode_one((uint8_t*)called_func_ptr_val);
                    final_func = called_func_ptr_val;
                    called_func = called_func_ptr_val;
                }

                // Check if this function is jmping into the "tanf" export in ucrtbase.dll
                if (final_func != 0) {
                    const auto module_within = utility::get_module_within(final_func);

                    if (module_within &&
                        (final_func == (uintptr_t)GetProcAddress(*module_within, "tanf") ||
                        final_func == (uintptr_t)GetProcAddress(*module_within, "tan"))) 
                    {
                        spdlog::info("Found CalculateStereoProjectionMatrix: {} {:x}", calculate_stereo_projection_matrix_index + i, potential_func);
                        calculate_stereo_projection_matrix_index += i;
                        found = true;
                        break;
                    } else {
                        spdlog::info("Function did not call tanf, skipping");
                    }
                } else {
                    spdlog::info("Failed to resolve inner pointer");
                }
            }

            ip += ix.Length;
        }

        if (found) {
            break;
        }
    }

    const auto init_canvas_index = calculate_stereo_projection_matrix_index + 1;

    const auto adjust_view_rect_func = ((uintptr_t*)vtable)[adjust_view_rect_index];
    const auto calculate_stereo_projection_matrix_func = ((uintptr_t*)vtable)[calculate_stereo_projection_matrix_index];
    const auto init_canvas_func_ptr = &((uintptr_t*)vtable)[init_canvas_index];
    // const auto render_texture_render_thread_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index + 3];

    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    spdlog::info("AdjustViewRect: {:x}", (uintptr_t)adjust_view_rect_func);
    spdlog::info("CalculateStereoProjectionMatrix: {:x}", (uintptr_t)calculate_stereo_projection_matrix_func);
    spdlog::info("IsStereoEnabled: {:x}", (uintptr_t)*is_stereo_enabled_func_ptr);

    m_has_double_precision = is_using_double_precision(stereo_view_offset_func) || is_using_double_precision(calculate_stereo_projection_matrix_func);

    m_adjust_view_rect_hook = builder.create_inline((void*)adjust_view_rect_func, adjust_view_rect);
    m_calculate_stereo_view_offset_hook = builder.create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
    m_calculate_stereo_projection_matrix_hook = builder.create_inline((void*)calculate_stereo_projection_matrix_func, calculate_stereo_projection_matrix);

    // This requires a pointer hook because the virtual just returns false
    // compiler optimization makes that function get re-used in a lot of places
    // so it's not feasible to just detour it, we need to replace the pointer in the vtable.
    if (!m_rendertarget_manager_embedded_in_stereo_device) {
        m_render_texture_render_thread_hook = builder.create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);

        // Seems to exist in 4.18+
        m_get_render_target_manager_hook = std::make_unique<PointerHook>((void**)get_render_target_manager_func_ptr, (void*)&get_render_target_manager_hook);
    } else {
        // When the render target manager is embedded in the stereo device, it just means
        // that all of the virtuals are now part of FFakeStereoRendering
        // instead of being a part of IStereoRenderTargetManager and being returned via GetRenderTargetManager.
        // Only seen in 4.17 and below.
        spdlog::info("Performing hooks on embedded RenderTargetManager");

        // Scan forward from the alleged RenderTexture_RenderThread function to find the
        // real RenderTexture_RenderThread function, because it is different when the
        // render target manager is embedded in the stereo device.
        // When it's embedded, it seems like it's the first function right after
        // a set of functions that return false sequentially.
        bool prev_function_returned_false = false;

        for (auto i = rendertexture_fn_vtable_index + 1; i < 100; ++i) {
            const auto func = ((uintptr_t*)og_vtable.data())[i];

            if (func == 0 || IsBadReadPtr((void*)func, 3)) {
                spdlog::error("Failed to find real RenderTexture_RenderThread");
                return false;
            }
            
            if (is_vfunc_pattern(func, "32 C0")) {
                prev_function_returned_false = true;
            } else {
                if (prev_function_returned_false) {
                    render_texture_render_thread_func = func;
                    rendertexture_fn_vtable_index = i;
                    m_render_texture_render_thread_hook = builder.create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);
                    spdlog::info("Real RenderTexture_RenderThread: {} {:x}", rendertexture_fn_vtable_index, (uintptr_t)*render_texture_render_thread_func);
                    break;
                }

                prev_function_returned_false = false;
            }
        }

        // Scan backwards from RenderTexture_RenderThread for the first virtual that just returns
        int32_t calculate_render_target_size_index = 0;

        for (auto i = rendertexture_fn_vtable_index - 1; i > 0; --i) {
            const auto func = ((uintptr_t*)og_vtable.data())[i];

            if (func == 0 || IsBadReadPtr((void*)func, 3)) {
                spdlog::error("Failed to find calculate render target size index, falling back to hardcoded index");
                calculate_render_target_size_index = rendertexture_fn_vtable_index - 3;
                break;
            }

            if (is_vfunc_pattern(func, "C3") || is_vfunc_pattern(func, "C2 00 00")) {
                spdlog::info("Dynamically found CalculateRenderTargetSize index: {}", i);
                calculate_render_target_size_index = i;
                break;
            }
        }

        const auto calculate_render_target_size_func_ptr = &((uintptr_t*)vtable)[calculate_render_target_size_index];
        spdlog::info("CalculateRenderTargetSize index: {}", calculate_render_target_size_index);

        // To be seen if this one needs automated analysis
        const auto should_use_separate_render_target_index = calculate_render_target_size_index + 2;
        const auto should_use_separate_render_target_func_ptr = &((uintptr_t*)vtable)[should_use_separate_render_target_index];

        spdlog::info("ShouldUseSeparateRenderTarget index: {}", should_use_separate_render_target_index);

        // Scan forward from RenderTexture_RenderThread for the first virtual that returns false
        int32_t allocate_render_target_index = 0;

        for (auto i = rendertexture_fn_vtable_index + 1; i < 100; ++i) {
            const auto func = ((uintptr_t*)og_vtable.data())[i];

            if (func == 0 || IsBadReadPtr((void*)func, 3)) {
                spdlog::error("Failed to find allocate render target index, falling back to hardcoded index");
                allocate_render_target_index = render_target_manager_vtable_index + 3;
                break;
            }

            if (is_vfunc_pattern(func, "32 C0")) {
                spdlog::info("Dynamically found AllocateRenderTarget index: {}", i);
                allocate_render_target_index = i;
                break;
            }
        }

        const auto allocate_render_target_func_ptr = &((uintptr_t*)vtable)[allocate_render_target_index];
        spdlog::info("AllocateRenderTarget index: {}", allocate_render_target_index);

        m_embedded_rtm.calculate_render_target_size_hook = 
            std::make_unique<PointerHook>((void**)calculate_render_target_size_func_ptr, +[](void* self, const FViewport& viewport, uint32_t& x, uint32_t& y) {
            #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                spdlog::info("CalculateRenderTargetSize (embedded)");
            #endif

                return g_hook->get_render_target_manager()->calculate_render_target_size(viewport, x, y);
            }
        );

        m_embedded_rtm.allocate_render_target_texture_hook = 
            std::make_unique<PointerHook>((void**)allocate_render_target_func_ptr, +[](void* self, 
                uint32_t index, uint32_t w, uint32_t h, uint8_t format, uint32_t num_mips,
                ETextureCreateFlags lags, ETextureCreateFlags targetable_texture_flags, FTexture2DRHIRef& out_texture,
                FTexture2DRHIRef& out_shader_resource, uint32_t num_samples) -> bool {
            #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                spdlog::info("AllocateRenderTargetTexture (embedded)");
            #endif

                return g_hook->get_render_target_manager()->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &out_texture, &out_shader_resource);
            }
        );

        m_embedded_rtm.should_use_separate_render_target_hook = 
            std::make_unique<PointerHook>((void**)should_use_separate_render_target_func_ptr, +[](void* self) -> bool {
            #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                spdlog::info("ShouldUseSeparateRenderTarget (embedded)");
            #endif

                return VR::get()->is_hmd_active();
            }
        );
    }
    
    //m_get_stereo_layers_hook = std::make_unique<PointerHook>((void**)get_stereo_layers_func_ptr, (void*)&get_stereo_layers_hook);
    m_is_stereo_enabled_hook = std::make_unique<PointerHook>((void**)is_stereo_enabled_func_ptr, (void*)&is_stereo_enabled);
    //m_init_canvas_hook = std::make_unique<PointerHook>((void**)init_canvas_func_ptr, (void*)&init_canvas);

    spdlog::info("Leaving FFakeStereoRenderingHook::hook");

    const auto renderer_module = sdk::get_ue_module(L"Renderer");
    const auto backbuffer_format_cvar = sdk::find_cvar_by_description(L"Defines the default back buffer pixel format.", L"r.DefaultBackBufferPixelFormat", 4, renderer_module);
    m_pixel_format_cvar_found = backbuffer_format_cvar.has_value();

    // In 4.18 this doesn't exist. Not much we can do about that.
    if (backbuffer_format_cvar) {
        spdlog::info("Backbuffer Format CVar: {:x}", (uintptr_t)*backbuffer_format_cvar);
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0) = 0;   // 8bit RGBA, which is what VR headsets support
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0x4) = 0; // 8bit RGBA, which is what VR headsets support
    } else {
        spdlog::error("Failed to find backbuffer format cvar, continuing anyways...");
    }

    m_finished_hooking = true;

    // make a shadow copy of FFakeStereoRendering's vtable to get past weird compiler optimizations
    // that cause the hook to not work, reason being that the compiler will optimize
    // if the vtable pointer is equal to the original vtable pointer, and it will
    // not call the hook function, so we make a shadow copy of the vtable
    auto active_stereo_device = locate_active_stereo_rendering_device();

    if (active_stereo_device) {
        spdlog::info("Found active stereo device: {:x}", (uintptr_t)*active_stereo_device);
        spdlog::info("Overwriting vtable...");

        static std::vector<uintptr_t> shadow_vtable{};
        auto& vtable = *(uintptr_t**)*active_stereo_device;

        for (auto i = 0; i < 100; i++) {
            shadow_vtable.push_back(vtable[i]);
        }

        vtable = shadow_vtable.data();
    } else {
        spdlog::info("Current stereo device is null, cannot overwrite vtable");
        patch_vtable_checks(); // fallback to patching vtable checks
    }

    spdlog::info("Finished hooking FFakeStereoRendering!");

    return true;
}

bool FFakeStereoRenderingHook::nonstandard_create_stereo_device_hook() {
    // This may only work on one game for now, but it should be a good placeholder
    // for creating a stereo device for games that don't have one.
    // We can figure out how to make it work for other games when we run into one
    // that needs this same functionality.

    // The reason why this function is needed is because in the one game that
    // the FFakeStereoRenderingHook doesn't work through the standard method,
    // is because the VR pipeline seems to have been heavily modified,
    // and so the -emulatestereo command line argument doesn't work, and
    // the FFakeStereoRendering vtable does not seem to exist
    // However the StereoRenderingDevice within GEngine seems to still exist
    // so we can take advantage of that and create our own stereo device
    // the downside is it will be much more difficult to figure out the 
    // proper vtable indices for the functions we need to hook
    // and we will need to actually implement some of the functions
    spdlog::info("Attempting to create a stereo device for the game using nonstandard method");
    m_fallback_vtable.resize(30);

    // Give all of the functions placeholders.
    for (auto i = 0; i < m_fallback_vtable.size(); ++i) {
        m_fallback_vtable[i] = +[](FFakeStereoRendering* stereo) -> void* {
            return nullptr;
        };
    }

    // Actually implement the ones we care about now.
    auto idx = 0;
    //m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> void { spdlog::info("Destructor called?");  }; // destructor.
    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> bool { 
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        spdlog::info("IsStereoEnabled called: {:x}", (uintptr_t)_ReturnAddress());
#endif

        return g_hook->is_stereo_enabled(stereo); 
    }; // IsStereoEnabled
    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // IsStereoEnabledOnNextFrame
    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // EnableStereo

    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) { 
        return g_hook->adjust_view_rect(stereo, index, x, y, w, h);
    }; // AdjustViewRect


    ++idx; // idk waht this is.

    // in this version the index is passed...?
    /*m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, uint32_t index, Vector2f* bounds) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        spdlog::info("GetTextSafeRegionBounds called");
#endif

        bounds->x = 0.75f;
        bounds->y = 0.75f;

        return bounds;
    };*/ // GetTextSafeRegionBounds

    m_fallback_vtable[idx++] = 
    +[](FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, const float world_to_meters, Vector3f* view_location) {
        return g_hook->calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location);
    }; // CalculateStereoViewOffset

    
    idx++;

    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        spdlog::info("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
#endif

        if (!g_hook->m_has_double_precision) {
            (*out)[3][2] = 0.1f; // Need to pre-set the Z value to something, otherwise it will be 0.0f & probably break something.
        } else {
            auto dmat = (Matrix4x4d*)out;
            (*dmat)[3][2] = 0.1;
        }

        return g_hook->calculate_stereo_projection_matrix(stereo, out, view_index);
    }; // CalculateStereoProjectionMatrix

    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, void* a2) {
        // do nothing
    }; // not sure what this one is. think it sets the FOV. Not present in newer UE4 versions.

    idx++; // just leave this one as a placeholder for now. Returns false.

    m_fallback_vtable[idx++] = 
    +[](FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
        return g_hook->render_texture_render_thread(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    };

    idx++; // just leave this one as a placeholder for now. Probably SetClippingPlanes.

    m_fallback_vtable[13] = +[](FFakeStereoRendering* stereo) { return g_hook->get_render_target_manager_hook(stereo); }; // GetRenderTargetManager
    //m_fallback_vtable[13] = +[](FFakeStereoRendering* stereo) { return nullptr; }; // GetRenderTargetManager

    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        spdlog::error("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }

    //m_418_detected = true;
    m_special_detected = true;
    m_fallback_device.vtable = m_fallback_vtable.data();
    *(uintptr_t*)((uintptr_t)engine + 0xAC8) = (uintptr_t)&m_fallback_device; // TODO: Automatically find this offset.

    spdlog::info("Finished creating stereo device for the game using nonstandard method");

    m_finished_hooking = true;

    return true;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_fake_stereo_rendering_constructor() {
    static std::optional<uintptr_t> cached_result{};

    if (cached_result) {
        return cached_result;
    }

    const auto engine_dll = sdk::get_ue_module(L"Engine");

    auto fake_stereo_rendering_constructor = utility::find_function_from_string_ref(engine_dll, L"r.StereoEmulationHeight");

    if (!fake_stereo_rendering_constructor) {
        fake_stereo_rendering_constructor = utility::find_function_from_string_ref(engine_dll, L"r.StereoEmulationFOV");

        if (!fake_stereo_rendering_constructor) {
            spdlog::error("Failed to find FFakeStereoRendering constructor");
            return std::nullopt;
        }
    }

    if (!fake_stereo_rendering_constructor) {
        spdlog::error("Failed to find FFakeStereoRendering constructor");
        return std::nullopt;
    }

    spdlog::info("FFakeStereoRendering constructor: {:x}", (uintptr_t)*fake_stereo_rendering_constructor);
    cached_result = *fake_stereo_rendering_constructor;

    return *fake_stereo_rendering_constructor;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_fake_stereo_rendering_vtable() {
    static std::optional<uintptr_t> cached_result{};

    if (cached_result) {
        return cached_result;
    }

    const auto fake_stereo_rendering_constructor = locate_fake_stereo_rendering_constructor();

    if (!fake_stereo_rendering_constructor) {
        // If this happened, then that's bad news, the UE version is probably extremely old
        // so we have to use this fallback method.
        spdlog::info("Failed to locate FFakeStereoRendering constructor, using fallback method");
        const auto initialize_hmd_device = sdk::UEngine::get_initialize_hmd_device_address();

        if (!initialize_hmd_device) {
            spdlog::error("Failed to find FFakeStereoRendering VTable via fallback method");
            return std::nullopt;
        }

        // To be seen if this needs to be adjusted. At first glance it doesn't look very reliable.
        // maybe perform emulation or something in the future?
        const auto instruction = utility::scan_disasm(*initialize_hmd_device, 100, "48 8D 05 ? ? ? ?");

        if (!instruction) {
            spdlog::error("Failed to find FFakeStereoRendering VTable via fallback method (2)");
            return std::nullopt;
        }

        const auto result = utility::calculate_absolute(*instruction + 3);

        if (!result) {
            spdlog::error("Failed to find FFakeStereoRendering VTable via fallback method (3)");
            return std::nullopt;
        }

        spdlog::info("FFakeStereoRendering VTable: {:x}", (uintptr_t)result);
        cached_result = result;

        return result;
    }

    const auto vtable_ref = utility::scan(*fake_stereo_rendering_constructor, 100, "48 8D 05 ? ? ? ?");

    if (!vtable_ref) {
        spdlog::error("Failed to find FFakeStereoRendering VTable Reference");
        return std::nullopt;
    }

    const auto vtable = utility::calculate_absolute(*vtable_ref + 3);

    if (!vtable) {
        spdlog::error("Failed to find FFakeStereoRendering VTable");
        return std::nullopt;
    }

    spdlog::info("FFakeStereoRendering VTable: {:x}", (uintptr_t)vtable);
    cached_result = vtable;

    return vtable;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_active_stereo_rendering_device() {
    auto engine = (uintptr_t)sdk::UEngine::get();

    if (engine == 0) {
        spdlog::error("GEngine does not appear to be instantiated, cannot verify stereo rendering device is setup.");
        return std::nullopt;
    }

    spdlog::info("Checking engine pointers for StereoRenderingDevice...");
    auto fake_stereo_device_vtable = locate_fake_stereo_rendering_vtable();

    if (!fake_stereo_device_vtable) {
        spdlog::error("Failed to locate fake stereo rendering device vtable, cannot verify stereo rendering device is setup.");
        return std::nullopt;
    }

    if (s_stereo_rendering_device_offset != 0) {
        const auto result = *(uintptr_t*)(engine + s_stereo_rendering_device_offset);

        if (result == 0) {
            return std::nullopt;
        }

        return result;
    }

    for (auto i = 0; i < 0x2000; i += sizeof(void*)) {
        const auto addr_of_ptr = engine + i;

        if (IsBadReadPtr((void*)addr_of_ptr, sizeof(void*))) {
            spdlog::info("Reached end of engine pointers at offset {:x}", i);
            break;
        }

        const auto ptr = *(uintptr_t*)addr_of_ptr;

        if (ptr == 0 || IsBadReadPtr((void*)ptr, sizeof(void*))) {
            continue;
        }

        auto potential_vtable = *(uintptr_t*)ptr;

        if (potential_vtable == *fake_stereo_device_vtable) {
            spdlog::info("Found fake stereo rendering device at offset {:x} -> {:x}", i, ptr);
            s_stereo_rendering_device_offset = i;
            return ptr;
        }
    }

    spdlog::error("Failed to find stereo rendering device");
    return std::nullopt;
}

std::optional<uint32_t> FFakeStereoRenderingHook::get_stereo_view_offset_index(uintptr_t vtable) {
    for (auto i = 0; i < 30; ++i) {
        auto func = ((uintptr_t*)vtable)[i];

        if (func == 0 || IsBadReadPtr((void*)func, sizeof(void*))) {
            continue;
        }

        // Resolve jmps if needed.
        while (*(uint8_t*)func == 0xE9) {
            spdlog::info("VFunc at index {} contains a jmp, resolving...", i);
            func = utility::calculate_absolute(func + 1);
        }

        bool found = false;
        uint32_t xmm_register_usage_count = 0;

        // We do an exhaustive decode (disassemble all possible code paths) that correctly follows the control flow
        // because some games are obfuscated and do huge jumps across gaps of junk code.
        // so we can't just linearly scan forward as the disassembler will fail at some point.
        utility::exhaustive_decode((uint8_t*)func, 30, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (found) {
                return utility::ExhaustionResult::BREAK;
            }

            if (ix.BranchInfo.IsBranch && !ix.BranchInfo.IsConditional && std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            char txt[ND_MIN_BUF_SIZE]{};
            NdToText(&ix, 0, sizeof(txt), txt);

            if (std::string_view{txt}.find("xmm") != std::string_view::npos && ++xmm_register_usage_count >= 10) {
                found = true;
                return utility::ExhaustionResult::BREAK;
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (found) {
            spdlog::info("Found Stereo View Offset Index: {}", i);
            return i;
        }
    }

    return std::nullopt;
}

// DISCLAIMER: I've only seen this in one game so far...
// So, there's some kind of compiler optimization for inlined virtuals
// that checks whether the vtable pointer matches the base FFakeStereoRendering class.
// if it matches, it just calls an inlined version of the function.
// otherwise it actually calls the function within the vtable.
bool FFakeStereoRenderingHook::patch_vtable_checks() {
    spdlog::info("Attempting to patch inlined vtable checks...");

    const auto fake_stereo_rendering_constructor = locate_fake_stereo_rendering_constructor();
    const auto fake_stereo_rendering_vtable = locate_fake_stereo_rendering_vtable();

    if (!fake_stereo_rendering_constructor || !fake_stereo_rendering_vtable) {
        spdlog::error("Cannot patch vtables, constructor or vtable not found!");
        return false;
    }

    const auto vtable_module_within = utility::get_module_within(*fake_stereo_rendering_vtable);
    const auto module_size = utility::get_module_size(*vtable_module_within);
    const auto module_end = (uintptr_t)*vtable_module_within + *module_size;

    spdlog::info("{:x} {:x} {:x}", *fake_stereo_rendering_vtable, (uintptr_t)*vtable_module_within, *module_size);

    for (auto ref = utility::scan_displacement_reference(*vtable_module_within, *fake_stereo_rendering_vtable); 
        ref.has_value();
        ref = utility::scan_displacement_reference((uintptr_t)*ref + 4, (module_end - *ref) - sizeof(void*), *fake_stereo_rendering_vtable)) 
    {
        const auto distance_from_constructor = *ref - *fake_stereo_rendering_constructor;

        // We don't want to mess with the one within the constructor.
        if (distance_from_constructor < 0x100) {
            spdlog::info("Skipping vtable reference within constructor");
            continue;
        }

        // Change the bytes to be some random number
        // this causes the vtable check to fail and will call the function within the vtable.
        DWORD old{};
        VirtualProtect((void*)*ref, 4, PAGE_EXECUTE_READWRITE, &old);
        *(uint32_t*)*ref = 0x12345678;
        VirtualProtect((void*)*ref, 4, old, &old);
        spdlog::info("Patched vtable check at {:x}", (uintptr_t)*ref);
    }

    spdlog::info("Finished patching inlined vtable checks.");
    return true;
}

bool FFakeStereoRenderingHook::attempt_runtime_inject_stereo() {
    // This attempts to create a new StereoRenderingDevice in the GEngine
    // if it doesn't already exist via using -emulatestereo.
    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        spdlog::error("Failed to locate GEngine, cannot inject stereo rendering device at runtime.");
        return false;
    }

    static auto enable_stereo_emulation_cvar = sdk::vr::get_enable_stereo_emulation_cvar();

    if (!locate_active_stereo_rendering_device()) {
        spdlog::info("Calling InitializeHMDDevice...");

        //utility::ThreadSuspender _{};

        engine->initialize_hmd_device();

        spdlog::info("Called InitializeHMDDevice.");

        if (!locate_active_stereo_rendering_device()) {
            spdlog::info("Previous call to InitializeHMDDevice did not setup the stereo rendering device, attempting to call again...");

            // We don't call this before because the cvar will not be set up
            // until it's referenced once. after we set this we need to call the function again.
            if (enable_stereo_emulation_cvar) {
                try {
                    enable_stereo_emulation_cvar->set<int>(1);
                } catch(...) {
                    spdlog::error("Access violation occurred when writing to r.EnableStereoEmulation, the address may be incorrect!");
                }
            } else {
                spdlog::error("Failed to locate r.EnableStereoEmulation cvar, next call may fail.");
            }

            spdlog::info("Calling InitializeHMDDevice... AGAIN");

            engine->initialize_hmd_device();

            spdlog::info("Called InitializeHMDDevice again.");
        }

        if (locate_active_stereo_rendering_device()) {
            spdlog::info("Stereo rendering device setup successfully.");
        } else {
            spdlog::error("Failed to setup stereo rendering device.");
            return false;
        }
    } else {
        spdlog::info("Not necessary to call InitializeHMDDevice, stereo rendering device is already setup.");
        m_fixed_localplayer_view_count = true; // Everything was set up beforehand, we don't need to do anything, so just set it to true.
    }

    return true;
}

bool FFakeStereoRenderingHook::is_stereo_enabled(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("is stereo enabled called!");
#endif

    // wait!!!
    if (!g_framework->is_game_data_intialized()) {
        return false;
    }

    static uint32_t count = 0;

    // Forcefully return true the first few times to let stuff initialize.
    if (count < 50) {
        ++count;
        return true;
    }

    return !VR::get()->get_runtime()->got_first_sync || VR::get()->is_hmd_active();
}

void FFakeStereoRenderingHook::adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("adjust view rect called! {}", index);
    spdlog::info(" x: {}, y: {}, w: {}, h: {}", *x, *y, *w, *h);
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    static bool index_starts_from_one = true;

    if (index == 2) {
        index_starts_from_one = true;
    } else if (index == 0) {
        index_starts_from_one = false;
    }

    *w = VR::get()->get_hmd_width() * 2;
    *h = VR::get()->get_hmd_height();

    *w = *w / 2;

    const auto true_index = index_starts_from_one ? ((index + 1) % 2) : (index % 2);
    *x += *w * true_index;
}

__forceinline void FFakeStereoRenderingHook::calculate_stereo_view_offset(
    FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, 
    const float world_to_meters, Vector3f* view_location)
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate stereo view offset called! {}", view_index);
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location, g_hook->m_has_double_precision);
    }

    static bool index_starts_from_one = true;

    if (view_index == 2) {
        index_starts_from_one = true;
    } else if (view_index == 0) {
        index_starts_from_one = false;
    }

    const auto has_double_precision = g_hook->m_has_double_precision;
    const auto true_index = index_starts_from_one ? ((view_index + 1) % 2) : (view_index % 2);
    const auto rot_d = (Rotator<double>*)view_rotation;
    const auto view_d = (Vector3d*)view_location;

    auto vr = VR::get();

    /*if (view_index % 2 == 1 && VR::get()->get_runtime()->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
        std::scoped_lock _{ vr->get_runtime()->render_mtx };
        spdlog::info("SYNCING!!!");
        //vr->get_runtime()->synchronize_frame();
        vr->update_hmd_state();
    }*/

    if (true_index == 0) {
        std::scoped_lock _{ vr->m_openvr_mtx };
        vr->update_hmd_state();
    }

    const auto view_mat = !has_double_precision ? 
        glm::yawPitchRoll(
            glm::radians(view_rotation->yaw),
            glm::radians(view_rotation->pitch),
            glm::radians(view_rotation->roll)) : 
        glm::yawPitchRoll(
            glm::radians((float)rot_d->yaw),
            glm::radians((float)rot_d->pitch),
            glm::radians((float)rot_d->roll));

    const auto view_mat_inverse = !has_double_precision ? 
        glm::yawPitchRoll(
            glm::radians(-view_rotation->yaw),
            glm::radians(view_rotation->pitch),
            glm::radians(-view_rotation->roll)) : 
        glm::yawPitchRoll(
            glm::radians(-(float)rot_d->yaw),
            glm::radians((float)rot_d->pitch),
            glm::radians(-(float)rot_d->roll));

    const auto view_quat_inverse = glm::quat {
        view_mat_inverse
    };

    const auto view_quat = glm::quat {
        view_mat
    };

    const auto quat_converter = glm::quat{Matrix4x4f {
        0, 0, -1, 0,
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0, 1
    }};

    const auto camera_distance = VR::get()->get_camera_distance();
    const auto camera_forward = quat_converter * (glm::normalize(view_quat_inverse) * glm::vec3{0, 0, -camera_distance});
    const auto world_scale = world_to_meters * VR::get()->get_world_scale();

    if (has_double_precision) {
        *view_d += camera_forward;
    } else {
        *view_location += camera_forward;
    }

    const auto rotation_offset = vr->get_rotation_offset();
    const auto current_hmd_rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(0)});

    const auto new_rotation = glm::normalize(view_quat_inverse * current_hmd_rotation);
    const auto eye_offset = glm::vec3{vr->get_eye_offset((VRRuntime::Eye)(true_index))};

    const auto pos = glm::vec3{rotation_offset * ((vr->get_position(0) - vr->get_standing_origin()))};

    const auto offset1 = quat_converter * (glm::normalize(view_quat_inverse) * (pos * world_scale));
    const auto offset2 = quat_converter * (glm::normalize(new_rotation) * (eye_offset * world_scale));

    if (!has_double_precision) {
        *view_location -= offset1;
        *view_location -= offset2;
    } else {
        *view_d -= offset1;
        *view_d -= offset2;
    }

    const auto euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

    if (!has_double_precision) {
        view_rotation->pitch = euler.x;
        view_rotation->yaw = euler.y;
        view_rotation->roll = euler.z;
    } else {
        rot_d->pitch = euler.x;
        rot_d->yaw = euler.y;
        rot_d->roll = euler.z;
    }

    for (auto& mod : mods) {
        mod->on_post_calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location, g_hook->m_has_double_precision);
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("Finished calculating stereo view offset!");
#endif
}

__forceinline Matrix4x4f* FFakeStereoRenderingHook::calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate stereo projection matrix called! {} from {:x}", view_index, (uintptr_t)_ReturnAddress() - (uintptr_t)utility::get_module_within((uintptr_t)_ReturnAddress()).value_or(nullptr));
#endif

    if (!g_hook->m_fixed_localplayer_view_count) {
        if (g_hook->m_calculate_stereo_projection_matrix_post_hook == nullptr) {
            const auto return_address = (uintptr_t)_ReturnAddress();
            spdlog::info("Inserting midhook after CalculateStereoProjectionMatrix... @ {:x}", return_address);

            constexpr auto max_stack_depth = 100;
            uintptr_t stack[max_stack_depth]{};

            const auto depth = RtlCaptureStackBackTrace(0, max_stack_depth, (void**)&stack, nullptr);

            for (int i = 0; i < depth; i++) {
                g_hook->m_projection_matrix_stack.push_back(stack[i]);
                spdlog::info(" {:x}", (uintptr_t)stack[i]);
            }

            auto factory = SafetyHookFactory::init();
            auto builder = factory->acquire();

            g_hook->m_calculate_stereo_projection_matrix_post_hook = builder.create_mid((void*)return_address, &FFakeStereoRenderingHook::post_calculate_stereo_projection_matrix);
        }
    } else if (g_hook->m_calculate_stereo_projection_matrix_post_hook != nullptr) {
        spdlog::info("Removing midhook after CalculateStereoProjectionMatrix, job is done...");
        g_hook->m_calculate_stereo_projection_matrix_post_hook.reset();
        g_hook->m_get_projection_data_pre_hook.reset();
    }

    if (!g_framework->is_game_data_intialized()) {
        if (g_hook->m_calculate_stereo_projection_matrix_hook) {
            return g_hook->m_calculate_stereo_projection_matrix_hook->call<Matrix4x4f*>(stereo, out, view_index);
        }

        return out;
    }

    static bool index_starts_from_one = true;

    if (view_index == 2) {
        index_starts_from_one = true;
    } else if (view_index == 0) {
        index_starts_from_one = false;
    }

    // Can happen if we hooked this differently.
    if (g_hook->m_calculate_stereo_projection_matrix_hook) {
        g_hook->m_calculate_stereo_projection_matrix_hook->call<Matrix4x4f*>(stereo, out, view_index);
    }

    // spdlog::info("NearZ: {}", old_znear);

    if (out != nullptr) {
        const auto true_index = index_starts_from_one ? ((view_index + 1) % 2) : (view_index % 2);
        auto& double_matrix = *(Matrix4x4d*)out;

        if (!g_hook->m_has_double_precision) {
            float old_znear = (*out)[3][2];
            VR::get()->m_nearz = old_znear;            
            VR::get()->get_runtime()->update_matrices(old_znear, 10000.0f);
        } else {
            double old_znear = (double_matrix)[3][2];
            VR::get()->m_nearz = (float)old_znear;
            VR::get()->get_runtime()->update_matrices((float)old_znear, 10000.0f);
        }

        if (!g_hook->m_has_double_precision) {
            *out = VR::get()->get_projection_matrix((VRRuntime::Eye)(true_index));
        } else {
            const auto fmat = VR::get()->get_projection_matrix((VRRuntime::Eye)(true_index));
            double_matrix = fmat;
        }
    } else {
        spdlog::error("CalculateStereoProjectionMatrix returned nullptr!");
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("Finished calculating stereo projection matrix!");
#endif
    
    return out;
}

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

__forceinline void FFakeStereoRenderingHook::render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
    FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) 
{
    if (!g_framework->is_game_data_intialized()) {
        return;
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("render texture render thread called!");
#endif


    if (!g_hook->is_slate_hooked() && g_hook->has_attempted_to_hook_slate()) {
        spdlog::info("Attempting to hook SlateRHIRenderer::DrawWindow_RenderThread using RenderTexture_RenderThread return address...");
        const auto return_address = (uintptr_t)_ReturnAddress();
        spdlog::info(" Return address: {:x}", return_address);
        g_hook->attempt_hook_slate_thread(return_address);
    }

    /*const auto return_address = (uintptr_t)_ReturnAddress();
    const auto slate_cvar_usage_location = sdk::vr::get_slate_draw_to_vr_render_target_usage_location();

    if (slate_cvar_usage_location) {
        const auto distance_from_usage = (intptr_t)(return_address - *slate_cvar_usage_location);

        if (distance_from_usage <= 0x200) {
            //spdlog::info("Ret: {:x} Distance: {:x}", return_address, distance_from_usage);

            auto& d3d11_vr = VR::get()->m_d3d11;
            auto& hook = g_framework->get_d3d11_hook();
            auto device = hook->get_device();
            ComPtr<ID3D11DeviceContext> context{};

            device->GetImmediateContext(&context);
            context->CopyResource(d3d11_vr.get_test_tex().Get(), (ID3D11Resource*)src_texture->get_native_resource());
            context->Flush();
        }
    }*/

    //g_hook->m_rtm.set_render_target(src_texture);

    /*if (g_hook->m_rtm.get_scene_target() != src_texture) {
        g_hook->m_rtm.set_render_target(src_texture);
    }*/

    // spdlog::info("{:x}", (uintptr_t)src_texture->GetNativeResource());

    // maybe the window size is actually a pointer we will find out later.
    /*if (g_hook->m_render_texture_render_thread_hook) {
        g_hook->m_render_texture_render_thread_hook->call<void*>(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    }*/
}

void FFakeStereoRenderingHook::init_canvas(FFakeStereoRendering* stereo, FSceneView* view, UCanvas* canvas) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("init canvas called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    // Since the FSceneView and UCanvas structures will probably vary wildly
    // in terms of field offsets and size, we will need to dynamically scan
    // from the return address of this function to find the ViewProjectionMatrix offset.
    // in the FSceneView and also the UCanvas.
    // it happens in the else block of the conditional statement that calls this function
    static uint32_t fsceneview_viewproj_offset = 0;
    static uint32_t ucanvas_viewproj_offset = 0;

    if (fsceneview_viewproj_offset == 0 || ucanvas_viewproj_offset == 0) {
        spdlog::info("Searching for FSceneView and UCanvas offsets...");
        spdlog::info("Canvas: {:x}", (uintptr_t)canvas);

        const auto return_address = (uintptr_t)_ReturnAddress();
        const auto containing_function = utility::find_function_start(return_address);

        spdlog::info("Found containing function at {:x}", *containing_function);

        auto find_offsets = [](uintptr_t start, uintptr_t end) -> bool {
            for (auto ip = (uintptr_t)start; ip < end + 0x100;) {
                const auto ix = utility::decode_one((uint8_t*)ip);

                if (!ix) {
                    spdlog::error("Failed to decode instruction at {:x}", ip);
                    break;
                }

                // The initial instructions look something like this
                /*
                0F 28 86 C0 03 00 00                          movaps  xmm0, xmmword ptr [rsi+3C0h]
                41 0F 11 87 80 02 00 00                       movups  xmmword ptr [r15+280h], xmm0
                */
                if (std::string_view{ix->Mnemonic} == "MOVAPS" && ix->Operands[1].Type == ND_OP_MEM) {
                    const auto next = utility::decode_one((uint8_t*)(ip + ix->Length));

                    if (next) {
                        if (std::string_view{next->Mnemonic} == "MOVUPS" && next->Operands[0].Type == ND_OP_MEM) {
                            fsceneview_viewproj_offset = ix->Operands[1].Info.Memory.Disp;
                            ucanvas_viewproj_offset = next->Operands[0].Info.Memory.Disp;
                            
                            spdlog::info("Found at {:x}", ip);
                            spdlog::info("Found FSceneView ViewProjectionMatrix offset: {:x}", fsceneview_viewproj_offset);
                            spdlog::info("Found UCanvas ViewProjectionMatrix offset: {:x}", ucanvas_viewproj_offset);
                            return true;
                            break;
                        }
                    }
                }

                ip += ix->Length;
            }

            return false;
        };

        if (!find_offsets(*containing_function, return_address)) {
            // If we still didn't find it at this stage, re-scan from the previous function from the previous function call instead.
            const auto potential_func = utility::calculate_absolute(return_address - 4);
            if (!find_offsets(potential_func, potential_func + 0x100)) {
                spdlog::error("Failed to find offsets!");
                return;
            }
        }
    }

    //*(Matrix4x4f*)((uintptr_t)view + fsceneview_viewproj_offset) = VR::get()->get_projection_matrix(VRRuntime::Eye::LEFT);
    *(Matrix4x4f*)((uintptr_t)canvas + ucanvas_viewproj_offset) = *(Matrix4x4f*)((uintptr_t)view + fsceneview_viewproj_offset);
}

IStereoRenderTargetManager* FFakeStereoRenderingHook::get_render_target_manager_hook(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("get render target manager hook called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return nullptr;
    }

    if (!VR::get()->get_runtime()->got_first_poses || VR::get()->is_hmd_active()) {
        if (g_hook->m_uses_old_rendertarget_manager) {
            return (IStereoRenderTargetManager*)&g_hook->m_rtm_418;
        }

        if (g_hook->m_special_detected) {
            return (IStereoRenderTargetManager*)&g_hook->m_rtm_special;
        }

        return &g_hook->m_rtm;
    }

    return nullptr;
}

IStereoLayers* FFakeStereoRenderingHook::get_stereo_layers_hook(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("get stereo layers hook called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return nullptr;
    }

    if (!VR::get()->get_runtime()->got_first_poses || VR::get()->is_hmd_active()) {
        /*static uint8_t fake_data[0x100]{};

        if (*(uintptr_t*)&fake_data == 0) {
            *(uintptr_t*)&fake_data = (uintptr_t)utility::get_executable() + 0x3D13420; // test
        }

        //return &g_hook->m_sl;
        return (IStereoLayers*)&fake_data;*/
    }

    return nullptr;
}

void FFakeStereoRenderingHook::post_calculate_stereo_projection_matrix(safetyhook::Context& ctx) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("post calculate stereo projection matrix called!");
#endif

    if (g_hook->m_fixed_localplayer_view_count || g_hook->m_hooked_alternative_localplayer_scan) {
        return;
    }

    auto vfunc = utility::find_virtual_function_start(g_hook->m_calculate_stereo_projection_matrix_post_hook->target());

    if (!vfunc) {
        // attempt to hook GetProjectionData instead to get the localplayer
        spdlog::info("Failed to find virtual function start for CalculateStereoProjectionMatrix, attempting to hook GetProjectionData instead...");

        if (!g_hook->m_projection_matrix_stack.empty() && g_hook->m_projection_matrix_stack.size() >= 3) {
            const auto post_get_projection_data = g_hook->m_projection_matrix_stack[2];
            const auto get_projection_data = utility::find_function_start(post_get_projection_data);

            if (get_projection_data) {
                spdlog::info("Successfully found GetProjectionData at {:x}", *get_projection_data);

                g_hook->m_hooked_alternative_localplayer_scan = true;

                auto factory = SafetyHookFactory::init();
                auto builder = factory->acquire();

                g_hook->m_get_projection_data_pre_hook = builder.create_mid((void*)*get_projection_data, &FFakeStereoRenderingHook::pre_get_projection_data);
                g_hook->m_projection_matrix_stack.clear();

                if (g_hook->m_get_projection_data_pre_hook != nullptr) {
                    spdlog::info("Successfully hooked GetProjectionData");
                    return;
                } else {
                    spdlog::error("Failed to hook GetProjectionData");
                }
            }
        }
    }

    if (!vfunc) {
        spdlog::info("Could not find function via normal means, scanning for int3s...");

        const auto ref = utility::scan_reverse(g_hook->m_calculate_stereo_projection_matrix_post_hook->target(), 0x2000, "CC CC CC");

        if (ref) {
            vfunc = *ref + 3;
        }

        if (!vfunc) {
            g_hook->m_fixed_localplayer_view_count = true;
            spdlog::error("Failed to find virtual function start for post calculate_stereo_projection_matrix!");
            return;
        }
    }

    // Scan forward until we find an assignment of the RCX register into a storage register.
    std::unordered_map<uint32_t, uintptr_t*> register_to_context {
        { NDR_RBX, &ctx.rbx },
        { NDR_RCX, &ctx.rcx },
        { NDR_RDX, &ctx.rdx },
        { NDR_RSI, &ctx.rsi },
        { NDR_RDI, &ctx.rdi },
        { NDR_RBP, &ctx.rbp },
        { NDR_RSP, &ctx.rsp },
        { NDR_R8, &ctx.r8 },
        { NDR_R9, &ctx.r9 },
        { NDR_R10, &ctx.r10 },
        { NDR_R11, &ctx.r11 },
        { NDR_R12, &ctx.r12 },
        { NDR_R13, &ctx.r13 },
        { NDR_R14, &ctx.r14 },
        { NDR_R15, &ctx.r15 },
    };

    INSTRUX ix{};
    std::optional<uint32_t> found_register{};
    auto ip = (uint8_t*)vfunc.value_or(0);

    while (true) {
        const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

        if (!ND_SUCCESS(status)) {
            spdlog::info("Decoding failed with error {:x}!", (uint32_t)status);
            break;
        }

        if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_REG && ix.Operands[1].Info.Register.Reg == NDR_RCX) {
            spdlog::info("Found assignment of RCX to storage register at {:x} ({})!", (uintptr_t)ip, ix.Operands[0].Info.Register.Reg);
            found_register = ix.Operands[0].Info.Register.Reg;
            break;
        }

        ip += ix.Length;
    }

    if (!found_register) {
        g_hook->m_fixed_localplayer_view_count = true;
        spdlog::error("Failed to find assignment of RCX to storage register!");
        return;
    }

    const auto localplayer = *register_to_context[found_register.value_or(0)];
    spdlog::info("Local player: {:x}", localplayer);

    if (localplayer == 0) {
        g_hook->m_fixed_localplayer_view_count = true;
        spdlog::error("Failed to find local player, cannot call PostInitProperties!");
        return;
    }

    g_hook->post_init_properties(localplayer);
}

void FFakeStereoRenderingHook::pre_get_projection_data(safetyhook::Context& ctx) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("pre get projection data called!");
#endif

    if (g_hook->m_fixed_localplayer_view_count) {
        return;
    }

    const auto localplayer = ctx.rcx;
    spdlog::info("Local player: {:x}", localplayer);

    if (localplayer == 0) {
        g_hook->m_fixed_localplayer_view_count = true;
        spdlog::error("Failed to find local player, cannot call PostInitProperties!");
        return;
    }

    g_hook->post_init_properties(localplayer);
}

void FFakeStereoRenderingHook::post_init_properties(uintptr_t localplayer) {
    spdlog::info("Searching for PostInitProperties virtual function...");

    std::optional<uint32_t> idx{};
    const auto engine = sdk::UEngine::get_lvalue();

    INSTRUX ix{};

    for (auto i = 1; i < 25; ++i) {
        if (idx) {
            break;
        }

        const auto vfunc = (*(uintptr_t**)localplayer)[i];

        if (vfunc == 0 || IsBadReadPtr((void*)vfunc, 1)) {
            spdlog::error("Encountered invalid vfunc at index {}!", i);
            break;
        }

        spdlog::info("Scanning vfunc at index {} ({:x})...", i, vfunc);

        utility::exhaustive_decode((uint8_t*)vfunc, 25, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (idx) {
                return utility::ExhaustionResult::BREAK;
            }

            if (const auto disp = utility::resolve_displacement(ip); disp) {
                // the second expression catches UE dynamic/debug builds
                if (*disp == (uintptr_t)engine || 
                    (!IsBadReadPtr((void*)*disp, sizeof(void*)) && *(uintptr_t*)*disp == (uintptr_t)*engine)) 
                {
                    spdlog::info("Found PostInitProperties at {} {:x}!", i, (uintptr_t)vfunc);
                    idx = i;
                    return utility::ExhaustionResult::BREAK;
                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });
    }

    if (!idx) {
        spdlog::error("Failed to find PostInitProperties virtual function! A crash may occur!");
    }

    // Now call PostInitProperties.
    // The purpose of this is setting up the view for the other eye.
    // Just creating the StereoRenderingDevice does not automatically do it, so we have to do it manually.
    // Usually the game just calls this function near startup after calling InitializeHMDDevice.
    if (idx) {
        spdlog::info("Calling PostInitProperties on local player!");

        // Get PEB and set debugger present
        auto peb = (PEB*)__readgsqword(0x60);

        const auto old = peb->BeingDebugged;
        peb->BeingDebugged = true;

        // set up a handler to skip int3 assertions
        // we do this because debug builds assert when the views are already setup.
        const auto seh_handler = [](PEXCEPTION_POINTERS info) -> LONG {
            if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
                spdlog::info("Skipping int3 breakpoint at {:x}!", info->ContextRecord->Rip);
                info->ContextRecord->Rip += 1;
                return EXCEPTION_CONTINUE_EXECUTION;
            } else {
                spdlog::info("Encountered exception {:x} at {:x}!", info->ExceptionRecord->ExceptionCode, info->ContextRecord->Rip);
            }

            return EXCEPTION_CONTINUE_SEARCH;
        };

        const auto exception_handler = AddVectoredExceptionHandler(1, seh_handler);

        const void (*post_init_properties)(uintptr_t) = (*(decltype(post_init_properties)**)localplayer)[*idx];
        post_init_properties(localplayer);

        spdlog::info("PostInitProperties called!");

        // remove the handler
        RemoveVectoredExceptionHandler(exception_handler);
        peb->BeingDebugged = old;
    }

    g_hook->m_fixed_localplayer_view_count = true;
}

void* FFakeStereoRenderingHook::slate_draw_window_render_thread(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info, 
                                                                void* elements, void* params, void* unk1, void* unk2) 
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("SlateRHIRenderer::DrawWindow_RenderThread called!");
#endif

    static bool once = true;

    if (once) {
        once = false;
        spdlog::info("SlateRHIRenderer::DrawWindow_RenderThread called for the first time!");
    }

    if (!g_framework->is_game_data_intialized()) {
        return g_hook->m_slate_thread_hook->call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);
    }

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_slate_draw_window(renderer, command_list, viewport_info);
    }

    auto call_orig = [&]() {
        auto ret = g_hook->m_slate_thread_hook->call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);

        for (auto& mod : mods) {
            mod->on_post_slate_draw_window(renderer, command_list, viewport_info);
        }

        return ret;
    };


    if (!VR::get()->is_hmd_active()) {
        return call_orig();
    }

    const auto ui_target = g_hook->get_render_target_manager()->get_ui_target();

    if (ui_target == nullptr) {
        spdlog::info("No UI target, skipping!");
        return call_orig();
    }

    const auto viewport_rt_provider = viewport_info->get_rt_provider(g_hook->get_render_target_manager()->get_render_target());

    if (viewport_rt_provider == nullptr) {
        spdlog::info("No viewport RT provider, skipping!");
        return call_orig();
    }

    const auto slate_resource = viewport_rt_provider->get_viewport_render_target_texture();

    if (slate_resource == nullptr) {
        spdlog::info("No slate resource, skipping!");
        return call_orig();
    }

    auto vr = VR::get();

    // Replace the texture with one we have control over.
    // This isolates the UI to render on our own texture separate from the scene.
    const auto old_texture = slate_resource->get_mutable_resource();
    slate_resource->get_mutable_resource() = ui_target;

    // To be seen if we need to resort to a MidHook on this function if the parameters
    // are wildly different between UE versions.
    const auto ret = g_hook->m_slate_thread_hook->call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);

    // Restore the old texture.
    slate_resource->get_mutable_resource() = old_texture;

    for (auto& mod : mods) {
        mod->on_post_slate_draw_window(renderer, command_list, viewport_info);
    }
    
    // After this we copy over the texture and clear it in the present hook. doing it here just seems to crash sometimes.

    return ret;
}

// INTERNAL USE ONLY!!!!
__declspec(noinline) void VRRenderTargetManager::CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) {
    m_last_calculate_render_size_return_address = (uintptr_t)_ReturnAddress();

    VRRenderTargetManager_Base::calculate_render_target_size(Viewport, InOutSizeX, InOutSizeY);
}

__declspec(noinline) bool VRRenderTargetManager::NeedReAllocateDepthTexture(const void* DepthTarget) {
    m_last_needs_reallocate_depth_texture_return_address = (uintptr_t)_ReturnAddress();

    return false;
}

__declspec(noinline) bool VRRenderTargetManager::NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) {
    const auto return_address = (uintptr_t)_ReturnAddress();
    const auto diff = return_address - m_last_calculate_render_size_return_address;

    if (diff <= 0x50) {
        // We need to switch the FFakeStereoRenderingHook's render target manager
        // to the old one NOW or we will crash. Reason being what was actually called
        // is the GetNumberOfBufferedFrames function, not NeedReAllocateShadingRateTexture.
        spdlog::info("Switching to old render target manager! Incorrect function called!");
        //g_hook->switch_to_old_rendertarget_manager();

        // Do a switcharoo on the vtable of this object to the old one because we will crash if we don't.
        // I've decided against actually switching the entire object over in favor of just vtable
        // swapping for now even though it's kind of a hack.
        const auto fake_object = std::make_unique<VRRenderTargetManager_418>();
        *(void**)this = *(void**)fake_object.get();

        return true; // The return value should actually be 1, so just return true.
    }

    return false;
}

void VRRenderTargetManager_Base::update_viewport(bool use_separate_rt, const FViewport& vp, class SViewport* vp_widget) {
    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    //spdlog::info("Widget: {:x}", (uintptr_t)ViewportWidget);
}

void VRRenderTargetManager_Base::calculate_render_target_size(const FViewport& viewport, uint32_t& x, uint32_t& y) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate render target size called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    spdlog::info("RenderTargetSize Before: {}x{}", x, y);

    x = VR::get()->get_hmd_width() * 2;
    y = VR::get()->get_hmd_height();

    spdlog::info("RenderTargetSize After: {}x{}", x, y);
}

bool VRRenderTargetManager_Base::need_reallocate_view_target(const FViewport& Viewport) {
    if (!g_framework->is_game_data_intialized()) {
        return false;
    }

    const auto w = VR::get()->get_hmd_width();
    const auto h = VR::get()->get_hmd_height();

    if (w != this->last_width || h != this->last_height) {
        this->last_width = w;
        this->last_height = h;
        return true;
    }

    return false;
}

void VRRenderTargetManager_Base::pre_texture_hook_callback(safetyhook::Context& ctx) {
    spdlog::info("PreTextureHook called! {}", ctx.r8);

    // maybe do some work later to bruteforce the registers/offsets for these
    // a la emulation or something more rudimentary
    // since it always seems to access a global right before, which
    // refers to the current pixel format, which we can overwrite (which may not be safe)
    // so we could just follow how the global is being written to registers or the stack
    // and then just overwrite the registers/stack with our own values
    if (!g_hook->has_pixel_format_cvar()) {
        if (g_hook->get_render_target_manager()->is_pre_texture_call_e8) {
            //ctx.r8 = 2; // PF_B8G8R8A8 // decided not to actually set it here, we need to double check when it's actually called
        } else {
            *((uint8_t*)ctx.rsp + 0x28) = 2; // PF_B8G8R8A8
        }
    }

    // Now we are going to attempt to JIT a function that will call the original function
    // using the context we have. This will call it twice, but allow us to
    // have control over one of the textures it generates. We need
    // the other generated texture as a UI render target to be used in FFakeStereoRenderingHook::slate_draw_window_render_thread.
    // This will allow the original game UI to be rendered in world space without resorting to WidgetComponent.
    // One can argue that this may be an overengineered alternative to "just" calling FDynamicRHI::CreateTexture2D
    // but that function is very hard to pattern scan for, and we already have it here, so why not use it?
    using namespace asmjit;
    using namespace asmjit::x86;

    spdlog::info("Attempting to JIT a function to call the original function!");

    auto rtm = g_hook->get_render_target_manager();

    const auto ix = utility::decode_one(rtm->texture_create_insn_bytes.data(), rtm->texture_create_insn_bytes.size());

    if (!ix) {
        spdlog::error("Failed to decode instruction!");
        return;
    }
    
    // We can't do it to the normal E8 call because the code is not in the same area
    // so RIP relative calls are not possible through the emulator. will just have to
    // resolve those manually through disassembly.
    uintptr_t func_ptr = 0;

    if (!g_hook->get_render_target_manager()->is_pre_texture_call_e8) {
        // Set up the emulator. We will use it to emulate the function call.
        // All we need from it is where the function call lands, so we can call it for real.
        auto emu_ctx = utility::ShemuContext(
            (uintptr_t)rtm->texture_create_insn_bytes.data(), 
            rtm->texture_create_insn_bytes.size());

        emu_ctx.ctx->Registers.RegRcx = ctx.rcx;
        emu_ctx.ctx->Registers.RegRdx = ctx.rdx;
        emu_ctx.ctx->Registers.RegR8 = ctx.r8;
        emu_ctx.ctx->Registers.RegR9 = ctx.r9;
        emu_ctx.ctx->Registers.RegRbx = ctx.rbx;
        emu_ctx.ctx->Registers.RegRax = ctx.rax;
        emu_ctx.ctx->Registers.RegRdi = ctx.rdi;
        emu_ctx.ctx->Registers.RegRsi = ctx.rsi;
        emu_ctx.ctx->Registers.RegR10 = ctx.r10;
        emu_ctx.ctx->Registers.RegR11 = ctx.r11;
        emu_ctx.ctx->Registers.RegR12 = ctx.r12;
        emu_ctx.ctx->Registers.RegR13 = ctx.r13;
        emu_ctx.ctx->Registers.RegR14 = ctx.r14;
        emu_ctx.ctx->Registers.RegR15 = ctx.r15;
        emu_ctx.ctx->MemThreshold = 1;

        if (emu_ctx.emulate((uintptr_t)rtm->texture_create_insn_bytes.data(), 1) != SHEMU_SUCCESS) {
            spdlog::error("Failed to emulate instruction!: {} RIP: {:x}", emu_ctx.status, emu_ctx.ctx->Registers.RegRip);
            return;
        }
    
        spdlog::info("Emu landed at {:x}", emu_ctx.ctx->Registers.RegRip);
        func_ptr = emu_ctx.ctx->Registers.RegRip;
    } else {
        const auto target = g_hook->get_render_target_manager()->pre_texture_hook->target();
        func_ptr = target + 5 + *(int32_t*)&rtm->texture_create_insn_bytes.data()[1];
    }

    spdlog::info("Function pointer: {:x}", func_ptr);

    /*CodeHolder code{};
    JitRuntime runtime{};
    code.init(runtime.environment());

    Assembler a{&code};
    
    static auto cloned_stack = std::make_unique<std::array<uint8_t, 0x3000>>();
    static auto cloned_registers = std::make_unique<std::array<uint8_t, 0x1000>>();

    auto aligned_stack = ((uintptr_t)&(*cloned_stack)[0x2000]);
    aligned_stack += (-(intptr_t)aligned_stack) & (40 - 1);

    memcpy((void*)aligned_stack, (void*)(ctx.rsp), 0x1000);

    static auto stack_ptr = std::make_unique<uintptr_t>();
    static auto post_register_storage = std::make_unique<uintptr_t>();

    // Store the original stack pointer.
    a.movabs(rax, (void*)stack_ptr.get());
    a.mov(ptr(rax), rsp);

    // Push all of the original registers onto the stack.
    a.movabs(rsp, (void*)&(*cloned_registers)[0x500]);
    //a.mov(rsp, rax);

    a.push(rcx);
    a.push(rdx);
    a.push(r8);
    a.push(r9);
    a.push(r10);
    a.push(r11);
    a.push(r12);
    a.push(r13);
    a.push(r14);
    a.push(r15);
    a.push(rbx);
    a.push(rbp);
    a.push(rsi);
    a.push(rdi);
    a.pushfq();

    a.mov(rax, (void*)post_register_storage.get());
    a.mov(ptr(rax), rsp);

    a.movabs(rsp, aligned_stack);


    a.mov(rdx, rcx); // func param
    a.movabs(rcx, ctx.rcx);
    //a.movabs(rdx, ctx.rdx);
    a.movabs(r8, ctx.r8);
    //a.movabs(r9, ctx.r9);
    const auto size = g_framework->is_dx11() ? g_framework->get_d3d11_rt_size() : g_framework->get_d3d12_rt_size();
    a.mov(r9, (uint32_t)size.x);
    // move w into first stack argument
    a.mov(dword_ptr(rsp, 0x20), (uint32_t)size.y);
    a.movabs(r10, ctx.r10);
    a.movabs(r11, ctx.r11);
    a.movabs(r12, ctx.r12);
    a.movabs(r13, ctx.r13);
    a.movabs(r14, ctx.r14);
    a.movabs(r15, ctx.r15);
    a.movabs(rax, ctx.rax);
    a.movabs(rbx, ctx.rbx);
    a.movabs(rbp, ctx.rbp);
    a.movabs(rsi, ctx.rsi);
    a.movabs(rdi, ctx.rdi);

    // Correct the stack pointers inside the stack we cloned
    // to point to areas within the cloned stack if they were
    // pointing to the original stack.
    for (auto stack_var = 0; stack_var < 0x1000; stack_var += sizeof(void*)) {
        auto stack_var_ptr = (uintptr_t*)(aligned_stack + stack_var);

        if (*stack_var_ptr >= ctx.rsp && *stack_var_ptr < ctx.rsp + 0x1000) {
            spdlog::info("Correcting stack var at 0x{:x}", stack_var);
            *stack_var_ptr = aligned_stack + (*stack_var_ptr - ctx.rsp);
        }
    }

    auto correct_register = [&](auto& reg) {
        if (reg >= ctx.rsp && reg < ctx.rsp + 0x1000) {
            spdlog::info("Correcting Register");
            reg = aligned_stack + (reg - ctx.rsp);
        }

    };
    for (auto insn_byte : g_hook->get_render_target_manager()->texture_create_insn_bytes) {
        a.db(insn_byte);
    }

    a.mov(rsp, post_register_storage.get());
    a.mov(rsp, ptr(rsp));
    //a.mov(rsp, rcx);

    // Pop all of the original registers off of the stack.
    a.popfq();
    a.pop(rdi);
    a.pop(rsi);
    a.pop(rbp);
    a.pop(rbx);
    a.pop(r15);
    a.pop(r14);
    a.pop(r13);
    a.pop(r12);
    a.pop(r11);
    a.pop(r10);
    a.pop(r9);
    a.pop(r8);
    a.pop(rdx);
    a.pop(rcx);

    //a.pop(rsp); // Restore the original stack pointer.
    a.movabs(rsp, (void*)stack_ptr.get());
    a.mov(rsp, ptr(rsp));

    a.ret();

    uintptr_t code_addr{};
    runtime.add(&code_addr, &code);

    spdlog::info("JITed address: {:x}", code_addr);

    //MessageBox(0, "debug now", "debug", 0);

    void (*func)(void* rdx) = (decltype(func))code_addr;

    static FTexture2DRHIRef out{};
    out.texture = nullptr;
    func(&out);*/

    auto call_with_context = [&](uintptr_t func, FTexture2DRHIRef& out) {
        CodeHolder code{};
        JitRuntime runtime{};
        code.init(runtime.environment());

        Assembler a{&code};

        auto post_align_label = a.newLabel();

        a.push(rbx);

        a.mov(rcx, ctx.rcx);
        
        if (!g_hook->get_render_target_manager()->is_pre_texture_call_e8) {
            a.movabs(rdx, (uintptr_t)&out);
        } else {
            a.mov(rdx, ctx.rdx);
        }

        a.mov(r8, ctx.r8);

        const auto size = g_framework->is_dx11() ? g_framework->get_d3d11_rt_size() : g_framework->get_d3d12_rt_size();
        a.mov(r9, (uint32_t)size.x);

        a.sub(rsp, 0x100);
        a.mov(rbx, 0x100);
        a.test(rsp, sizeof(void*));
        a.jz(post_align_label);

        a.sub(rsp, 8);
        a.mov(rbx, 0x108);
        a.bind(post_align_label);

        a.mov(ptr(rsp, 0x20), (uint32_t)size.y);

        for (auto i = 0x28; i < 0x90; i += sizeof(void*)) {
            a.mov(rax, *(uintptr_t*)(ctx.rsp + i));
            a.mov(ptr(rsp, i), rax);
        }

        a.mov(rax, (void*)func);
        a.call(rax);

        a.add(rsp, rbx);
        a.pop(rbx);

        a.ret();

        uintptr_t code_addr{};
        runtime.add(&code_addr, &code);
        void (*jitted_func)() = (decltype(jitted_func))code_addr;

        jitted_func();
    };

    static FTexture2DRHIRef out{};
    static FTexture2DRHIRef shader_out{};

    const auto size = g_framework->is_dx11() ? g_framework->get_d3d11_rt_size() : g_framework->get_d3d12_rt_size();
    const auto stack_args = (uintptr_t*)(ctx.rsp + 0x20);

    spdlog::info("About to call the original!");
    
    if (!rtm->is_pre_texture_call_e8) {
        spdlog::info("Calling register version of texture create");

        void (*func)(
            uintptr_t rhi,
            FTexture2DRHIRef* out,
            uintptr_t command_list,
            uintptr_t w,
            uintptr_t h,
            uintptr_t format,
            uintptr_t mips,
            uintptr_t samples,
            uintptr_t flags,
            uintptr_t create_info,
            uintptr_t additional,
            uintptr_t additional2) = (decltype(func))func_ptr;

        func(ctx.rcx, &out, ctx.r8, size.x, size.y, 2, 
            stack_args[2], stack_args[3], stack_args[4], 
            stack_args[5], stack_args[6], stack_args[7]);

        rtm->ui_target = out.texture;

        if (rtm->texture_hook_ref == nullptr || rtm->texture_hook_ref->texture == nullptr) {
            spdlog::info("Had to set texture hook ref in pre texture hook!");
            rtm->texture_hook_ref = (FTexture2DRHIRef*)ctx.rdx;
        }
    } else {
        spdlog::info("Calling E8 version of texture create");
        
        // check if RCX is near the stack pointer
        // if it is then it's a different form of E8 call that takes the texture in the first parameter.
        if (ctx.rcx != 0 && std::abs((int64_t)ctx.rcx - (int64_t)ctx.rsp) <= 0x300) {
            spdlog::info("Weird form of E8 call detected...");

            // Format
            ctx.r9 = 2; // PF_B8G8R8A8

            void (*func)(
                FTexture2DRHIRef* out,
                uint32_t w,
                uint32_t h,
                uint8_t format,
                uintptr_t mips,
                uintptr_t samples,
                uintptr_t flags,
                uintptr_t a7,
                uintptr_t a8,
                uintptr_t a9,
                uintptr_t additional,
                uintptr_t additional2) = (decltype(func))func_ptr;

            func(&out, (uint32_t)size.x, (uint32_t)size.y, 2,
                stack_args[0], stack_args[1], 
                stack_args[2], stack_args[3],
                stack_args[4],
                stack_args[7], stack_args[8], stack_args[9]);
        } else {
            ctx.r8 = 2; // PF_B8G8R8A8

            void (*func)(
                uint32_t w,
                uint32_t h,
                uint8_t format,
                uintptr_t mips,
                uintptr_t samples,
                uintptr_t flags,
                uintptr_t a7,
                uintptr_t a8,
                uintptr_t a9,
                FTexture2DRHIRef* out,
                FTexture2DRHIRef* shader_out,
                uintptr_t additional,
                uintptr_t additional2) = (decltype(func))func_ptr;

            func((uint32_t)size.x, (uint32_t)size.y, 2, ctx.r9,
                stack_args[0], stack_args[1], 
                stack_args[2], stack_args[3],
                stack_args[4],
                &out, &shader_out,
                stack_args[7], stack_args[8]);
        }

        
        rtm->ui_target = out.texture;
    }

    if (out.texture == nullptr) {
        spdlog::error("Failed to create UI texture!");
    } else {
        spdlog::info("Created UI texture at {:x}", (uintptr_t)out.texture);
    }

    //call_with_context((uintptr_t)func, out);

    spdlog::info("Called the original function!");

    // Cause stuff like the VR ui texture to get recreated.
    VR::get()->reinitialize_renderer();
}

void VRRenderTargetManager_Base::texture_hook_callback(safetyhook::Context& ctx) {
    auto rtm = g_hook->get_render_target_manager();

    spdlog::info("Ref: {:x}", (uintptr_t)rtm->texture_hook_ref);

    auto texture = rtm->texture_hook_ref->texture;

    // happens?
    if (rtm->texture_hook_ref->texture == nullptr) {
        const auto ref = (FTexture2DRHIRef*)ctx.rax;
        texture = ref->texture;
    }

    spdlog::info("last texture index: {}", rtm->last_texture_index);
    spdlog::info("Resulting texture: {:x}", (uintptr_t)texture);
    spdlog::info("Real resource: {:x}", (uintptr_t)texture->get_native_resource());

    rtm->render_target = texture;
    //rtm->ui_target = texture;
    rtm->texture_hook_ref = nullptr;
    ++rtm->last_texture_index;
}

bool VRRenderTargetManager_Base::allocate_render_target_texture(uintptr_t return_address, FTexture2DRHIRef* tex, FTexture2DRHIRef* shader_resource) {
    this->texture_hook_ref = tex;
    this->shader_resource_hook_ref = shader_resource;

    if (!this->set_up_texture_hook) {
        spdlog::info("AllocateRenderTargetTexture retaddr: {:x}", return_address);
        spdlog::info("Scanning for call instr...");

        bool next_call_is_not_the_right_one = false;

        // Now, we need to emulate from where AllocateRenderTargetTexture returns from
        // we will set RAX to false, to get the control flow correct
        // and then keep emulating until we hit the call we want
        // Previously, we were using just straight linear disassembly to do this, and it mostly worked
        // but in one game, there was an unconditional branch after the call instead of flowing
        // directly into the next instruction.
        auto emu = utility::ShemuContext{*utility::get_module_within(return_address)};

        emu.ctx->Registers.RegRax = 0;
        emu.ctx->Registers.RegRip = (ND_UINT64)return_address;
        emu.ctx->MemThreshold = 100;

        while(true) {
            if (emu.ctx->InstructionsCount > 100) {
                break;
            }

            const auto ip = emu.ctx->Registers.RegRip;
            const auto bytes = (uint8_t*)ip;
            const auto decoded = utility::decode_one((uint8_t*)ip);

            // mov dl, 32h
            // seen in UE5 or debug builds?
            if (bytes[0] == 0xB2 && bytes[1] == 0x32) {
                next_call_is_not_the_right_one = true;
            }

            // make sure we are not emulating any instructions that write to memory
            // so we can just set the IP to the next instruction
            if (decoded) {
                const auto is_call = std::string_view{decoded->Mnemonic}.starts_with("CALL");

                if (decoded->MemoryAccess & ND_ACCESS_ANY_WRITE || is_call) {
                    // We are looking for the call instruction
                    // This instruction calls RHICreateTargetableShaderResource2D(TexSizeX, TexSizeY, SceneTargetFormat, 1, TexCreate_None,
                    // TexCreate_RenderTargetable, false, CreateInfo, BufferedRTRHI, BufferedSRVRHI); Which sets up the BufferedRTRHI and
                    // BufferedSRVRHI variables.
                    if (is_call && !next_call_is_not_the_right_one) {
                        const auto post_call = (uintptr_t)ip + decoded->Length;
                        spdlog::info("AllocateRenderTargetTexture post_call: {:x}", post_call - (uintptr_t)*utility::get_module_within((void*)post_call));

                        auto factory = SafetyHookFactory::init();
                        auto builder = factory->acquire();

                        if (*(uint8_t*)ip == 0xE8) {
                            spdlog::info("E8 call found!");
                            this->is_pre_texture_call_e8 = true;
                        } else {
                            spdlog::info("E8 call not found, assuming register call!");
                        }

                        // So we can call the original texture create function again.
                        this->texture_create_insn_bytes.resize(decoded->Length);
                        memcpy(this->texture_create_insn_bytes.data(), (void*)ip, decoded->Length);

                        this->texture_hook = builder.create_mid((void*)post_call, &VRRenderTargetManager::texture_hook_callback);
                        this->pre_texture_hook = builder.create_mid((void*)ip, &VRRenderTargetManager::pre_texture_hook_callback);
                        this->set_up_texture_hook = true;

                        return false;
                    }

                    spdlog::info("Skipping write to memory instruction at {:x}", ip);
                    emu.ctx->Registers.RegRip += decoded->Length;
                    emu.ctx->Instruction = *decoded; // pseudo-emulate the instruction
                    ++emu.ctx->InstructionsCount;

                    if (is_call) {
                        next_call_is_not_the_right_one = false;
                    }
                } else if (emu.emulate() != SHEMU_SUCCESS) { // only emulate the non-memory write instructions
                    emu.ctx->Registers.RegRip += decoded->Length;
                    continue;
                }
            } else {
                break;
            }
        }

        spdlog::error("Failed to find call instruction!");
    }

    return false;
}

bool VRRenderTargetManager::AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
    ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
    FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples) {
    // So, what's happening here is instead of using this method
    // to actually create our textures, we are going to
    // get the return address, scan forward for the next call instruction
    // and insert a midhook after the next call instruction.
    // The purpose of this is to get the texture that is being created
    // by the engine itself after we return false from this function.
    // When we return false from this function, it indicates
    // to the engine that we are letting the engine itself
    // create the texture, rather than us creating it ourselves.
    // This should allow maximum compatibility across engine versions.
    /*const auto dynamic_rhi = *(uintptr_t*)((uintptr_t)sdk::get_ue_module(L"Engine") + 0x3309C50);
    const auto command_list = (uintptr_t)sdk::get_ue_module(L"Engine") + 0x330AE70;
    struct {
        void* bulk_data{nullptr};
        void* rsrc_array{nullptr};

        struct {
            uint32_t color_binding{1};
            float color[4]{};
        } clear_value_binding;

        uint32_t gpu_mask{1};
        bool without_native_rsrc{false};
        const TCHAR* debug_name{"BufferedRT"};
        uint32_t extended_data{};
    } create_info;

    const void (*RHICreateTexture2D_RenderThread)(
        uintptr_t rhi,
        FTexture2DRHIRef* out,
        uintptr_t command_list,
        uint32_t w,
        uint32_t h,
        uint8_t format,
        uint32_t mips,
        uint32_t samples,
        ETextureCreateFlags flags,
        void* create_info) = (*(decltype(RHICreateTexture2D_RenderThread)**)dynamic_rhi)[178];

    *(uint64_t*)&TargetableTextureFlags |= (uint64_t)ETextureCreateFlags::ShaderResource | (uint64_t)Flags;
    RHICreateTexture2D_RenderThread(dynamic_rhi, &OutTargetableTexture, command_list, SizeX, SizeY, 2, NumMips, NumSamples, TargetableTextureFlags, &create_info);

    const auto size = g_framework->is_dx11() ? g_framework->get_d3d11_rt_size() : g_framework->get_d3d12_rt_size();
    RHICreateTexture2D_RenderThread(dynamic_rhi, &OutShaderResourceTexture, command_list, (uint32_t)size.x, (uint32_t)size.y, 2, NumMips, NumSamples, TargetableTextureFlags, &create_info);

    this->render_target = OutTargetableTexture.texture;
    this->ui_target = OutShaderResourceTexture.texture;

    OutShaderResourceTexture.texture = OutTargetableTexture.texture;*/

    m_last_allocate_render_target_return_address = (uintptr_t)_ReturnAddress();
    spdlog::info("AllocateRenderTargetTexture called from: {:x}", m_last_allocate_render_target_return_address - (uintptr_t)*utility::get_module_within((void*)m_last_allocate_render_target_return_address));

    // So, if CalculateRenderTargetSize was *never* called before this function
    // that means we have the virtual index of this function wrong, and we must swap the vtable out.
    // also, if this function was called very close to NeedReallocateDepthTexture, that also means
    // the virtual index is wrong, and we must swap the vtable out.
    const auto is_incorrect_vtable = 
        m_last_calculate_render_size_return_address == 0 ||
        m_last_allocate_render_target_return_address - m_last_needs_reallocate_depth_texture_return_address <= 0x200;

    if (is_incorrect_vtable) {
        // oh no this is the wrong vtable!!!! we need to fix it  nOW!!!
        spdlog::info("AllocateRenderTargetTexture called instead of AllocateDepthTexture! Fixing...");
        spdlog::info("Switching to old render target manager! Incorrect function called!");
        //g_hook->switch_to_old_rendertarget_manager();

        // Do a switcharoo on the vtable of this object to the old one because we will crash if we don't.
        // I've decided against actually switching the entire object over in favor of just vtable
        // swapping for now even though it's kind of a hack.
        const auto fake_object = std::make_unique<VRRenderTargetManager_418>();
        *(void**)this = *(void**)fake_object.get();

        return false;
    }

    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture, &OutShaderResourceTexture);

    //return true;
}

bool VRRenderTargetManager_418::AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags,
        uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples) 
{
    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture, &OutShaderResourceTexture);
}

bool VRRenderTargetManager_Special::AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
    ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
    FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples) 
{
    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture, &OutShaderResourceTexture);
}
