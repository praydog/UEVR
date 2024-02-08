#define NOMINMAX

#include <windows.h>
#include <winternl.h>

#include <asmjit/asmjit.h>
#include <future>

#include <spdlog/spdlog.h>
#include <utility/Memory.hpp>
#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/String.hpp>
#include <utility/Thread.hpp>
#include <utility/Emulation.hpp>
#include <utility/ScopeGuard.hpp>

#include <sdk/EngineModule.hpp>
#include <sdk/UEngine.hpp>
#include <sdk/UGameEngine.hpp>
#include <sdk/CVar.hpp>
#include <sdk/Slate.hpp>
#include <sdk/DynamicRHI.hpp>
#include <sdk/FViewportInfo.hpp>
#include <sdk/Utility.hpp>
#include <sdk/RHICommandList.hpp>
#include <sdk/UGameViewportClient.hpp>
#include <sdk/Globals.hpp>
#include <sdk/FName.hpp>
#include <sdk/UObjectArray.hpp>
#include <sdk/FBoolProperty.hpp>
#include <sdk/FViewport.hpp>

#include <sdk/UGameplayStatics.hpp>
#include <sdk/APawn.hpp>
#include <sdk/APlayerController.hpp>

#include "Framework.hpp"
#include "Mods.hpp"

#include <bdshemu.h>
#include <bddisasm.h>
#include <disasmtypes.h>

#include <sdk/threading/GameThreadWorker.hpp>
#include <sdk/threading/RenderThreadWorker.hpp>
#include <sdk/threading/RHIThreadWorker.hpp>
#include "../VR.hpp"
#include "../../utility/Logging.hpp"

#include "FFakeStereoRenderingHook.hpp"

#include <tracy/Tracy.hpp>

//#define FFAKE_STEREO_RENDERING_LOG_ALL_CALLS

FFakeStereoRenderingHook* g_hook = nullptr;
uint32_t g_frame_count{};

// Scan through function instructions to detect usage of double
// floating point precision instructions.
bool is_using_double_precision(uintptr_t addr) {
    SPDLOG_INFO("Scanning function at {:x} for double precision usage", addr);

    bool result = false;

    utility::exhaustive_decode((uint8_t*)addr, 50, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
        if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
            return utility::ExhaustionResult::STEP_OVER;
        }

        if (ix.Instruction == ND_INS_MOVSD && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG) {
            SPDLOG_INFO("[UE5 Detected] Detected Double precision MOVSD at {:x}", (uintptr_t)ip);
            result = true;
            return utility::ExhaustionResult::BREAK;
        }

        if (ix.Instruction == ND_INS_ADDSD) {
            SPDLOG_INFO("[UE5 Detected] Detected Double precision ADDSD at {:x}", (uintptr_t)ip);
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

void FFakeStereoRenderingHook::on_frame() {
    attempt_hook_game_engine_tick();
    attempt_hook_slate_thread();
    attempt_hook_fsceneview_constructor();

    // TODO: see if this can be threaded; it might not be able to because of TLS or something
    if (!VR::get()->should_skip_uobjectarray_init()) {
        sdk::FName::get_constructor();
        sdk::FName::get_to_string();
        sdk::FUObjectArray::get();
    }

    // Ideally we want to do all hooking
    // from game engine tick. if it fails
    // we will fall back to doing it here.
    if (!m_hooked_game_engine_tick && m_attempted_hook_game_engine_tick) {
        attempt_hooking();
    }
}


void FFakeStereoRenderingHook::on_draw_ui() {
    ZoneScopedN(__FUNCTION__);

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Stereo Hook Options")) {
        m_asynchronous_scan->draw("Asynchronous Code Scanning");
        m_recreate_textures_on_reset->draw("Recreate Textures on Reset");
        m_frame_delay_compensation->draw("Frame Delay Compensation");

        if (m_tracking_system_hook != nullptr) {
            m_tracking_system_hook->on_draw_ui();
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
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

namespace detail{
bool pre_find_engine_tick() {
    ZoneScopedN(__FUNCTION__);
    sdk::UGameEngine::get_tick_address(); // this takes a LONG time to find
    sdk::UGameEngine::get_initialize_hmd_device_address();
    return true;
}
}

void FFakeStereoRenderingHook::attempt_hook_game_engine_tick(uintptr_t return_address) {
    if (m_asynchronous_scan->value()) {
        static std::future<bool> future = std::async(std::launch::async, detail::pre_find_engine_tick);

        // Wait for the future to be valid before attempting to hook
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            future.get();
        } else if (future.valid()) {
            return;
        }
    }

    if (m_hooked_game_engine_tick) {
        return;
    }

    if (return_address == 0 && m_attempted_hook_game_engine_tick) {
        return;
    }
    
    SPDLOG_INFO("Attempting to hook UGameEngine::Tick!");

    m_attempted_hook_game_engine_tick = true;

    auto func = sdk::UGameEngine::get_tick_address();

    if (!func) {
        if (return_address == 0) {
            SPDLOG_ERROR("Cannot hook UGameEngine::Tick");
            return;
        }

        const auto engine_module = sdk::get_ue_module(L"Engine");
        static const auto negative_delta_time_strings = 
            utility::scan_strings(engine_module, L"Negative delta time!");
        
        if (negative_delta_time_strings.empty()) {
            SPDLOG_ERROR("Cannot hook UGameEngine::Tick (Negative delta time! not found)");
            return;
        }

        static std::vector<uintptr_t> negative_delta_time_funcs = [&]() {
            std::vector<uintptr_t> out{};

            for (auto str : negative_delta_time_strings) {
                const auto ref = utility::scan_displacement_reference(engine_module, str);

                if (!ref) {
                    continue;
                }
                //
                const auto func_start = utility::find_virtual_function_start(*ref);

                if (!func_start) {
                    continue;
                }

                SPDLOG_INFO("Negative delta time string function @ {:x}", *func_start);

                out.push_back(*func_start);
            }

            return out;
        }();

        const auto return_address_func = utility::find_virtual_function_start(return_address);

        if (!return_address_func) {
            SPDLOG_ERROR("Return address is not within a valid function!");
            return;
        }

        // Check if the return address is within one of the negative delta time functions.
        // If it is, then it's UGameEngine::Tick. Set func to the return_address_func.
        for (auto potential : negative_delta_time_funcs) {
            if (potential == *return_address_func) {
                SPDLOG_INFO("Found UGameEngine::Tick @ {:x}", *return_address_func);
                func = *return_address_func;
                break;
            }
        }

        if (!func) {
            SPDLOG_ERROR("Return address is not the correct function!");
            return;
        }
    }

    // TODO: move this to a better place
    m_tick_hook = safetyhook::create_inline((void*)*func, +[](sdk::UGameEngine* engine, float delta, bool idle) -> void* {
        ZoneScopedN("UGameEngine::Tick Hook");
        FrameMarkStart("UGameEngine::Tick");

        auto hook = g_hook;
        
        hook->m_in_engine_tick = true;

        utility::ScopeGuard _{[]() {
            g_hook->m_in_engine_tick = false;
            FrameMarkEnd("UGameEngine::Tick");
        }};
        
        static bool once = true;

        if (once) {
            SPDLOG_INFO("First time calling UGameEngine::Tick!");
            once = false;
        }

        if (!g_framework->is_game_data_intialized()) {
            return hook->m_tick_hook.call<void*>(engine, delta, idle);
        }

        hook->attempt_hooking();

        // Best place to run game thread jobs.
        GameThreadWorker::get().execute();

        if (hook->m_ignore_next_engine_tick) {
            hook->m_ignored_engine_delta = delta;
            hook->m_ignore_next_engine_tick = false;
            return nullptr;
        }
        
        g_framework->enable_engine_thread();
        g_framework->run_imgui_frame(false);

        delta += hook->m_ignored_engine_delta;
        hook->m_ignored_engine_delta = 0.0f;

        if (hook->m_tracking_system_hook != nullptr) {
            hook->m_tracking_system_hook->on_pre_engine_tick(engine, delta);
        }

        const auto& mods = g_framework->get_mods()->get_mods();
        for (auto& mod : mods) {
            mod->on_pre_engine_tick(engine, delta);
        }

        const auto result = hook->m_tick_hook.call<void*>(engine, delta, idle);

        for (auto& mod : mods) {
            mod->on_post_engine_tick(engine, delta);
        }

        return result;
    });

    if (!m_tick_hook) {
        SPDLOG_ERROR("Failed to hook UGameEngine::Tick!");
    }

    m_hooked_game_engine_tick = true;

    SPDLOG_INFO("Hooked UGameEngine::Tick!");
}

namespace detail{
bool pre_find_slate_thread() {
    sdk::slate::locate_draw_window_renderthread_fn(); // Can take a while to find
    return true;
}
}

void FFakeStereoRenderingHook::attempt_hook_slate_thread(uintptr_t return_address) {
    if (m_asynchronous_scan->value()) {
        static std::future<bool> future = std::async(std::launch::async, detail::pre_find_slate_thread);

        // Wait for the future to be valid before attempting to hook
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            future.get();
        } else if (future.valid()) {
            return;
        }
    }

    if (m_hooked_slate_thread) {
        return;
    }

    if (return_address == 0 && m_attempted_hook_slate_thread) {
        return;
    }

    SPDLOG_INFO("Attempting to hook FSlateRHIRenderer::DrawWindow_RenderThread!");
    m_attempted_hook_slate_thread = true;

    auto func = sdk::slate::locate_draw_window_renderthread_fn();

    if (!func && return_address == 0) {
        SPDLOG_ERROR("Cannot hook FSlateRHIRenderer::DrawWindow_RenderThread");
        return;
    }

    if (return_address != 0) {
        func = utility::find_function_start_with_call(return_address);

        if (!func) {
            SPDLOG_ERROR("Cannot hook FSlateRHIRenderer::DrawWindow_RenderThread with alternative return address method");
            m_hooked_slate_thread = true; // not actually true but just to stop spamming the scans
            return;
        }

        SPDLOG_INFO("Found FSlateRHIRenderer::DrawWindow_RenderThread with alternative return address method: {:x}", *func);
    }

    m_slate_thread_hook = safetyhook::create_inline((void*)*func, &FFakeStereoRenderingHook::slate_draw_window_render_thread);
    m_hooked_slate_thread = true;

    if (!m_slate_thread_hook) {
        SPDLOG_ERROR("Failed to hook FSlateRHIRenderer::DrawWindow_RenderThread!");
        return;
    }

    SPDLOG_INFO("Hooked FSlateRHIRenderer::DrawWindow_RenderThread!");
}

namespace detail{
bool pre_find_fsceneview_constructor() {
    sdk::FSceneView::get_constructor_address(); // Can take a while to find
    return true;
}
}

void FFakeStereoRenderingHook::attempt_hook_fsceneview_constructor() {
    if (m_attempted_hook_fsceneview_constructor) {
        return;
    }
    
    // just try to find it before ghosting fix is even enabled
    if (m_asynchronous_scan->value()) {
        static std::future<bool> future = std::async(std::launch::async, detail::pre_find_fsceneview_constructor);

        // Wait for the future to be valid before attempting to hook
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            future.get();
        } else if (future.valid()) {
            return;
        }
    }

    auto& vr = VR::get();

    if (!vr->is_ghosting_fix_enabled() && !vr->is_splitscreen_compatibility_enabled() && !vr->is_sceneview_compatibility_enabled()) {
        return;
    }

    utility::ScopeGuard _{[&]() {
        m_attempted_hook_fsceneview_constructor = true;
    }};

    SPDLOG_INFO("Attempting to hook FSceneView::FSceneView constructor!");
    const auto constructor = sdk::FSceneView::get_constructor_address();

    if (!constructor) {
        SPDLOG_ERROR("Cannot hook FSceneView::FSceneView constructor");
        return;
    }

    g_hook->m_sceneview_data.constructor_hook = safetyhook::create_inline(*constructor, (uintptr_t)&sceneview_constructor);

    if (!g_hook->m_sceneview_data.constructor_hook) {
        SPDLOG_ERROR("Failed to hook FSceneView::FSceneView constructor!");
    }
}

bool FFakeStereoRenderingHook::hook() {
    SPDLOG_INFO("Entering FFakeStereoRenderingHook::hook");

    m_tried_hooking = true;

    // Locking the hook monitor mutex stops our code from trying to re-hook DX11 and 12 after
    // Long pauses in code execution, due to us doing massive scans for code in this function.
    std::scoped_lock _{g_framework->get_hook_monitor_mutex()};

    const auto vtable = locate_fake_stereo_rendering_vtable();

    // This happens if games have intentionally removed the stereo initialization functions and stereo emulation classes.
    // So we need to manually create the stereo device.
    if (!vtable) {
        SPDLOG_ERROR("Failed to locate Fake Stereo Rendering VTable, attempting to perform nonstandard hook");

        auto check_file_version = [](uint32_t ms, uint32_t ls) {
            try {
                const auto full_path = utility::get_module_pathw(utility::get_executable());

                if (!full_path) {
                    SPDLOG_ERROR("Failed to get executable path, falling back");
                    return false;
                }

                const auto file_version_size = GetFileVersionInfoSizeW(full_path->c_str(), nullptr);

                if (file_version_size == 0) {
                    SPDLOG_ERROR("Failed to get file version info size, falling back");
                    return false;
                }

                std::vector<uint8_t> file_version_data(file_version_size);
                GetFileVersionInfoW(full_path->c_str(), 0, file_version_size, file_version_data.data());

                UINT size{};
                VS_FIXEDFILEINFO* fixed_file_info{};

                if (VerQueryValueA(file_version_data.data(), "\\", (LPVOID*)&fixed_file_info, &size) && fixed_file_info != nullptr) {
                    SPDLOG_INFO("MS: {:x}, LS: {:x}", fixed_file_info->dwFileVersionMS, fixed_file_info->dwFileVersionLS);

                    if (fixed_file_info->dwFileVersionMS == ms && fixed_file_info->dwFileVersionLS == ls) {
                        SPDLOG_INFO("Found matching executable, attempting to perform nonstandard hook");
                        return true;
                    } else {
                        SPDLOG_INFO("File does not match requested version, falling back");
                    }
                } else {
                    SPDLOG_ERROR("Failed to get file version info, falling back");
                }
            } catch(...) {
                SPDLOG_ERROR("Failed to get file version info, falling back");
            }

            return false;
        };

        // Check for version 4.27.2.0
        if (check_file_version(0x4001B, 0x20000)) {
            return nonstandard_create_stereo_device_hook_4_27();
        }

        // Check for version 4.22.3.0
        if (check_file_version(0x40016, 0x30000)) {
            return nonstandard_create_stereo_device_hook_4_22();
        }

        // Check for version 4.18.3.0
        if (check_file_version(0x40012, 0x30000)) {
            return nonstandard_create_stereo_device_hook_4_18();
        }

        return nonstandard_create_stereo_device_hook();
    }

    return standard_fake_stereo_hook(*vtable);
}

bool FFakeStereoRenderingHook::standard_fake_stereo_hook(uintptr_t vtable) {
    ZoneScopedN(__FUNCTION__);
    SPDLOG_INFO("Performing standard fake stereo hook");

    const auto game = sdk::get_ue_module(L"Engine");
    std::array<uint8_t, 0x1000> og_vtable{};
    memcpy(og_vtable.data(), (void*)vtable, og_vtable.size()); // to perform tests on.

    const auto module_vtable_within = utility::get_module_within(vtable);

    // In 4.18 the destructor virtual doesn't exist or is at the very end of the vtable.
    const auto is_stereo_enabled_index = sdk::is_vfunc_pattern(*(uintptr_t*)vtable, "B0 01") ? 0 : 1;
    const auto is_stereo_enabled_func_ptr = &((uintptr_t*)vtable)[is_stereo_enabled_index];

    SPDLOG_INFO("IsStereoEnabled Index: {}", is_stereo_enabled_index);

    const auto stereo_view_offset_index = get_stereo_view_offset_index(vtable);

    if (!stereo_view_offset_index) {
        SPDLOG_ERROR("Failed to locate Stereo View Offset Index");
        return false;
    }

    const auto stereo_projection_matrix_index = *stereo_view_offset_index + 1;
    const auto is_4_18_or_lower = *stereo_view_offset_index <= 6;

    const auto& stereo_view_offset_func = ((uintptr_t*)vtable)[*stereo_view_offset_index];

    auto render_texture_render_thread_func = utility::find_virtual_function_from_string_ref(game, L"RenderTexture_RenderThread");

    // Seems more robust than simply just checking the vtable index.
    m_uses_old_rendertarget_manager = *stereo_view_offset_index <= 11 && !render_texture_render_thread_func;

    if (!render_texture_render_thread_func) {
        // Fallback scan to checking for the first non-default virtual function (<= 4.18)
        SPDLOG_INFO("Failed to find RenderTexture_RenderThread, falling back to first non-default virtual function");

        for (auto i = 2; i < 10; ++i) {
            const auto func = ((uintptr_t*)vtable)[stereo_projection_matrix_index + i];

            // Some protectors can fool this check, so we also check for the vfunc pattern (emulates the code)
            if (!utility::is_stub_code((uint8_t*)func) && 
                !sdk::is_vfunc_pattern(func, "33 C0") &&
                !sdk::is_vfunc_pattern(func, "32 C0"))
            {
                render_texture_render_thread_func = func;
                break;
            }
        }

        if (!render_texture_render_thread_func) {
            SPDLOG_ERROR("Failed to find RenderTexture_RenderThread");
            return false;
        }
    }

    SPDLOG_INFO("RenderTexture_RenderThread: {:x}", (uintptr_t)*render_texture_render_thread_func);

    // Scan for the function pointer, it should be in the middle of the vtable.
    auto rendertexture_fn_vtable_middle = utility::scan_ptr(vtable + ((stereo_projection_matrix_index + 2) * sizeof(void*)), 50 * sizeof(void*), *render_texture_render_thread_func);

    if (!rendertexture_fn_vtable_middle) {
        SPDLOG_ERROR("Failed to find RenderTexture_RenderThread VTable Middle");
        return false;
    }

    auto rendertexture_fn_vtable_index = (*rendertexture_fn_vtable_middle - vtable) / sizeof(uintptr_t);
    SPDLOG_INFO("RenderTexture_RenderThread VTable Middle: {} {:x}", rendertexture_fn_vtable_index, (uintptr_t)*rendertexture_fn_vtable_middle);

    auto render_target_manager_vtable_index = rendertexture_fn_vtable_index + 1 + (2 * (size_t)is_4_18_or_lower);

    // verify first that the render target manager index is returning a null pointer
    // and if not, scan forward until we run into a vfunc that returns a null pointer
    auto get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

    bool is_4_11 = false;

    //if (!sdk::is_vfunc_pattern(*(uintptr_t*)get_render_target_manager_func_ptr, "33 C0")) {
        //SPDLOG_INFO("Expected GetRenderTargetManager function at index {} does not return null, scanning forward for return nullptr.", render_target_manager_vtable_index);

        for (;;++render_target_manager_vtable_index) {
            get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

            if (IsBadReadPtr(*(void**)get_render_target_manager_func_ptr, 1)) {
                SPDLOG_ERROR("Failed to find GetRenderTargetManager vtable index, a crash is imminent");
                return false;
            }

            if (sdk::is_vfunc_pattern(*(uintptr_t*)get_render_target_manager_func_ptr, "33 C0")) {
                const auto distance_from_rendertexture_fn = render_target_manager_vtable_index - rendertexture_fn_vtable_index;

                // means it's 4.17 I think. 12 means 4.11.
                if (distance_from_rendertexture_fn == 10 || distance_from_rendertexture_fn == 11 || distance_from_rendertexture_fn == 12) {
                    is_4_11 = distance_from_rendertexture_fn == 12;
                    m_rendertarget_manager_embedded_in_stereo_device = true;
                    SPDLOG_INFO("Render target manager appears to be directly embedded in the stereo device vtable");
                } else {
                    // Now this may potentially be the correct index, but we're not quite done yet.
                    // On 4.19 (and possibly others), the index is 1 higher than it should be.
                    // We can tell by checking how many functions in front of this index return null.
                    // if there are two functions in front of this index that return null, we need to add 1 to the index.
                    SPDLOG_INFO("Found potential GetRenderTargetManager function at index {}", render_target_manager_vtable_index);
                    SPDLOG_INFO("Double checking GetRenderTargetManager index...");

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
                            SPDLOG_INFO("Crossed over into another vtable's boundaries, aborting double check");
                            SPDLOG_INFO("Reached end of double check at index {}, {} appears to be the correct index.", i, render_target_manager_vtable_index);
                            break;
                        }

                        if (!sdk::is_vfunc_pattern(func, "33 C0")) {
                            SPDLOG_INFO("Reached end of double check at index {}, {} appears to be the correct index.", i, render_target_manager_vtable_index);
                            break;
                        }

                        if (++count >= 2) {
                            ++render_target_manager_vtable_index;
                            get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

                            SPDLOG_INFO("Adjusted GetRenderTargetManager index to {}", render_target_manager_vtable_index);
                            break;
                        }
                    }

                    SPDLOG_INFO("Distance: {}", distance_from_rendertexture_fn);
                }

                break;
            }
        }
    //} else {
        //SPDLOG_INFO("GetRenderTargetManager function at index {} appears to be valid.", render_target_manager_vtable_index);
    //}
    
    const auto get_stereo_layers_func_ptr = (uintptr_t)(get_render_target_manager_func_ptr + sizeof(void*));

    if (get_render_target_manager_func_ptr == 0) {
        SPDLOG_ERROR("Failed to find GetRenderTargetManager");
        return false;
    }

    if (get_stereo_layers_func_ptr == 0) {
        SPDLOG_ERROR("Failed to find GetStereoLayers");
        return false;
    }

    SPDLOG_INFO("GetRenderTargetManagerptr: {:x}", (uintptr_t)get_render_target_manager_func_ptr);
    SPDLOG_INFO("GetStereoLayersptr: {:x}", (uintptr_t)get_stereo_layers_func_ptr);

    const auto adjust_view_rect_distance = is_4_18_or_lower ? 2 : 3;
    const auto adjust_view_rect_index = *stereo_view_offset_index - adjust_view_rect_distance;

    SPDLOG_INFO("AdjustViewRect Index: {}", adjust_view_rect_index);
    
    auto calculate_stereo_projection_matrix_index = *stereo_view_offset_index + 1;

    // While generally most of the time the stereo projection matrix func is the next one after the stereo view offset func,
    // it's not always the case. We can scan for a call to the tanf function in one of the virtual functions to find it.
    for (auto i = 0; i < 10; ++i) {
        const auto potential_func = ((uintptr_t*)vtable)[calculate_stereo_projection_matrix_index + i];
        if (potential_func == 0 || IsBadReadPtr((void*)potential_func, 1) || utility::is_stub_code((uint8_t*)potential_func)) {
            continue;
        }

        auto ip = (uint8_t*)potential_func;
        if (*(uint8_t*)ip == 0xE9) {
            ip = (uint8_t*)utility::calculate_absolute(potential_func + 1);
            SPDLOG_INFO("Found JMP at {:x}, jumping to {:x}", (uintptr_t)potential_func, (uintptr_t)ip);
        }

        bool found = false;

        SPDLOG_INFO("Scanning {:x}...", (uintptr_t)ip);

        for (auto j = 0; j < 50; ++j) {
            INSTRUX ix{};

            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                SPDLOG_INFO("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            if (ix.Category == ND_CAT_RET || ix.InstructionBytes[0] == 0xE9) {
                SPDLOG_INFO("Encountered RET or JMP at {:x}, aborting scan", (uintptr_t)ip);
                break;
            }

            if (ix.InstructionBytes[0] == 0xE8) {
                auto called_func = (uintptr_t)(ip + ix.Length + (int32_t)ix.RelativeOffset);
                auto inner_ins = utility::decode_one((uint8_t*)called_func);

                SPDLOG_INFO("called {:x}", (uintptr_t)called_func);
                uintptr_t final_func = 0;

                // Fully resolve the pointer jmps until we reach another module.
                while (inner_ins && inner_ins->InstructionBytes[0] == 0xFF && inner_ins->InstructionBytes[1] == 0x25) {
                    const auto called_func_ptr = (uintptr_t*)(called_func + inner_ins->Length + (int32_t)inner_ins->Displacement);
                    const auto called_func_ptr_val = *called_func_ptr;

                    SPDLOG_INFO("called ptr {:x}", (uintptr_t)called_func_ptr_val);

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
                        SPDLOG_INFO("Found CalculateStereoProjectionMatrix: {} {:x}", calculate_stereo_projection_matrix_index + i, potential_func);
                        calculate_stereo_projection_matrix_index += i;
                        found = true;
                        break;
                    } else {
                        SPDLOG_INFO("Function did not call tanf, skipping");
                    }
                } else {
                    SPDLOG_INFO("Failed to resolve inner pointer");
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
    

    SPDLOG_INFO("AdjustViewRect: {:x}", (uintptr_t)adjust_view_rect_func);
    SPDLOG_INFO("CalculateStereoProjectionMatrix: {:x}", (uintptr_t)calculate_stereo_projection_matrix_func);
    SPDLOG_INFO("CalculateStereoViewOffset: {:x}", (uintptr_t)stereo_view_offset_func);
    SPDLOG_INFO("IsStereoEnabled: {:x}", (uintptr_t)*is_stereo_enabled_func_ptr);

    m_has_double_precision = is_using_double_precision(stereo_view_offset_func) || is_using_double_precision(calculate_stereo_projection_matrix_func);

    {
        m_adjust_view_rect_hook = safetyhook::create_inline((void*)adjust_view_rect_func, adjust_view_rect);
        m_calculate_stereo_view_offset_hook_inline = safetyhook::create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
        m_calculate_stereo_projection_matrix_hook = safetyhook::create_inline((void*)calculate_stereo_projection_matrix_func, calculate_stereo_projection_matrix);
    }
    
    if (!m_adjust_view_rect_hook) {
        SPDLOG_ERROR("Failed to create AdjustViewRect hook");
    }

    if (!m_calculate_stereo_view_offset_hook_inline) {
        SPDLOG_ERROR("Failed to create CalculateStereoViewOffset hook, falling back to pointer hook");
        m_calculate_stereo_view_offset_hook_ptr = std::make_unique<PointerHook>((void**)&stereo_view_offset_func, (void*)calculate_stereo_view_offset);
    }

    if (!m_calculate_stereo_projection_matrix_hook) {
        SPDLOG_ERROR("Failed to create CalculateStereoProjectionMatrix hook");
    }

    // This requires a pointer hook because the virtual just returns false
    // compiler optimization makes that function get re-used in a lot of places
    // so it's not feasible to just detour it, we need to replace the pointer in the vtable.
    if (!m_rendertarget_manager_embedded_in_stereo_device) {
        m_render_texture_render_thread_hook = safetyhook::create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);

        if (!m_render_texture_render_thread_hook) {
            SPDLOG_ERROR("Failed to create RenderTexture_RenderThread hook");
        }

        // Seems to exist in 4.18+
        m_get_render_target_manager_hook = std::make_unique<PointerHook>((void**)get_render_target_manager_func_ptr, (void*)&get_render_target_manager_hook);
    } else {
        // When the render target manager is embedded in the stereo device, it just means
        // that all of the virtuals are now part of FFakeStereoRendering
        // instead of being a part of IStereoRenderTargetManager and being returned via GetRenderTargetManager.
        // Only seen in 4.17 and below.
        SPDLOG_INFO("Performing hooks on embedded RenderTargetManager");

        // Scan forward from the alleged RenderTexture_RenderThread function to find the
        // real RenderTexture_RenderThread function, because it is different when the
        // render target manager is embedded in the stereo device.
        // When it's embedded, it seems like it's the first function right after
        // a set of functions that return false sequentially.
        bool prev_function_returned_false = false;

        for (auto i = rendertexture_fn_vtable_index + 1; i < 100; ++i) {
            const auto func = ((uintptr_t*)og_vtable.data())[i];

            if (func == 0 || IsBadReadPtr((void*)func, 3)) {
                SPDLOG_ERROR("Failed to find real RenderTexture_RenderThread");
                return false;
            }
            
            if (sdk::is_vfunc_pattern(func, "32 C0")) {
                prev_function_returned_false = true;
            } else {
                if (prev_function_returned_false) {
                    render_texture_render_thread_func = func;
                    rendertexture_fn_vtable_index = i;
                    m_render_texture_render_thread_hook = safetyhook::create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);
                    if (!m_render_texture_render_thread_hook) {
                        SPDLOG_ERROR("Failed to create RenderTexture_RenderThread hook");
                    }
                    SPDLOG_INFO("Real RenderTexture_RenderThread: {} {:x}", rendertexture_fn_vtable_index, (uintptr_t)*render_texture_render_thread_func);
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
                SPDLOG_ERROR("Failed to find calculate render target size index, falling back to hardcoded index");
                calculate_render_target_size_index = rendertexture_fn_vtable_index - 3;
                break;
            }

            if (sdk::is_vfunc_pattern(func, "C3") || sdk::is_vfunc_pattern(func, "C2 00 00")) {
                SPDLOG_INFO("Dynamically found CalculateRenderTargetSize index: {}", i);
                calculate_render_target_size_index = i;
                break;
            }
        }

        const auto calculate_render_target_size_func_ptr = &((uintptr_t*)vtable)[calculate_render_target_size_index];
        SPDLOG_INFO("CalculateRenderTargetSize index: {}", calculate_render_target_size_index);

        // To be seen if this one needs automated analysis
        const auto need_reallocate_viewport_render_target_index = calculate_render_target_size_index + 1;
        const auto need_reallocate_viewport_render_target_func_ptr = &((uintptr_t*)vtable)[need_reallocate_viewport_render_target_index];

        // To be seen if this one needs automated analysis
        const auto should_use_separate_render_target_index = calculate_render_target_size_index + 2;
        const auto should_use_separate_render_target_func_ptr = &((uintptr_t*)vtable)[should_use_separate_render_target_index];

        // Log a warning if NeedReallocateViewportRenderTarget or ShouldUseSeparateRenderTarget are not
        // functions that plainly return false, but do not fail entirely.
        bool need_reallocate_viewport_render_target_is_bad = false;
        bool should_use_separate_render_target_is_bad = false;

        if (!sdk::is_vfunc_pattern(*need_reallocate_viewport_render_target_func_ptr, "32 C0")) {
            SPDLOG_WARN("NeedReallocateViewportRenderTarget is not a function that returns false");
            need_reallocate_viewport_render_target_is_bad = true;
        }

        if (!sdk::is_vfunc_pattern(*should_use_separate_render_target_func_ptr, "32 C0")) {
            SPDLOG_WARN("ShouldUseSeparateRenderTarget is not a function that returns false");
            should_use_separate_render_target_is_bad = true;
        }

        SPDLOG_INFO("NeedReallocateViewportRenderTarget index: {}", need_reallocate_viewport_render_target_index);
        SPDLOG_INFO("ShouldUseSeparateRenderTarget index: {}", should_use_separate_render_target_index);

        // Scan forward from RenderTexture_RenderThread for the first virtual that returns false
        int32_t allocate_render_target_index = 0;

        for (auto i = rendertexture_fn_vtable_index + 1; i < 100; ++i) {
            const auto func = ((uintptr_t*)og_vtable.data())[i];

            if (func == 0 || IsBadReadPtr((void*)func, 3)) {
                SPDLOG_ERROR("Failed to find allocate render target index, falling back to hardcoded index");
                allocate_render_target_index = render_target_manager_vtable_index + 3;
                break;
            }

            if (sdk::is_vfunc_pattern(func, "32 C0")) {
                SPDLOG_INFO("Dynamically found AllocateRenderTarget index: {}", i);
                allocate_render_target_index = i;
                break;
            }
        }

        const auto allocate_render_target_func_ptr = &((uintptr_t*)vtable)[allocate_render_target_index];
        SPDLOG_INFO("AllocateRenderTarget index: {}", allocate_render_target_index);

        m_embedded_rtm.calculate_render_target_size_hook = 
            std::make_unique<PointerHook>((void**)calculate_render_target_size_func_ptr, +[](void* self, const FViewport& viewport, uint32_t& x, uint32_t& y) {
            #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                SPDLOG_INFO("CalculateRenderTargetSize (embedded)");
            #else
                SPDLOG_INFO_ONCE("CalculateRenderTargetSize (embedded)");
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
                SPDLOG_INFO("AllocateRenderTargetTexture (embedded): {:x}", (uintptr_t)_ReturnAddress());
            #else
                SPDLOG_INFO_ONCE("AllocateRenderTargetTexture (embedded): {:x}", (uintptr_t)_ReturnAddress());
            #endif

                return g_hook->get_render_target_manager()->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &out_texture, &out_shader_resource);
            }
        );
    
        m_embedded_rtm.should_use_separate_render_target_hook = 
            std::make_unique<PointerHook>((void**)should_use_separate_render_target_func_ptr, +[](void* self) -> bool {
            #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                SPDLOG_INFO("ShouldUseSeparateRenderTarget (embedded): {:x}", (uintptr_t)_ReturnAddress());
            #else
                SPDLOG_INFO_ONCE("ShouldUseSeparateRenderTarget (embedded): {:x}", (uintptr_t)_ReturnAddress());
            #endif
            
                auto vr = VR::get();

                if (vr->is_extreme_compatibility_mode_enabled()) {
                    return false;
                }

                if (vr->is_hmd_active() && !vr->is_stereo_emulation_enabled()) {
                    g_hook->get_embedded_rtm().should_use_separate_rt_called = true;
                    return true;
                }

                return false;
            }
        );

        if (!need_reallocate_viewport_render_target_is_bad) {
            m_embedded_rtm.need_reallocate_viewport_render_target_hook = 
                std::make_unique<PointerHook>((void**)need_reallocate_viewport_render_target_func_ptr, +[](void* self, FViewport* viewport) -> bool {
                #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
                    SPDLOG_INFO("NeedReallocateViewportRenderTarget (embedded): {:x}", (uintptr_t)_ReturnAddress());
                #else
                    SPDLOG_INFO_ONCE("NeedReallocateViewportRenderTarget (embedded): {:x}", (uintptr_t)_ReturnAddress());
                #endif

                    if (g_hook->get_render_target_manager()->need_reallocate_view_target(*viewport)) {
                        g_hook->get_embedded_rtm().need_reallocate_viewport_render_target_called = true;
                        g_hook->get_embedded_rtm().last_time_needed_hmd_reallocate = std::chrono::steady_clock::now();
                        return true;
                    }

                    return false;
                }
            );
        }
    }
    
    m_is_stereo_enabled_hook = std::make_unique<PointerHook>((void**)is_stereo_enabled_func_ptr, (void*)&is_stereo_enabled);

    // scan for GetDesiredNumberOfViews function, we use this function to perform AFR if needed
    SPDLOG_INFO("Searching for GetDesiredNumberOfViews function...");
    std::optional<uint32_t> get_desired_number_of_views_index{};

    for (auto i = 1; i < 20; ++i) {
        auto func_ptr = &((uintptr_t*)vtable)[i];

        if (IsBadReadPtr((void*)*func_ptr, sizeof(void*))) {
            SPDLOG_INFO("Could not locate GetDesiredNumberOfViews function, this is okay, not really needed");
            break;
        }

        // pretty consistent patterns
        if (sdk::is_vfunc_pattern(*func_ptr, "0F B6 C2 FF C0 C3") ||
            sdk::is_vfunc_pattern(*func_ptr, "33 C0 84 D2 0F 95 C0 FF C0 C3") || 
            sdk::is_vfunc_pattern(*func_ptr, "84 D2 74 04 8B 41 14 C3 B8 01")) 
        {
            SPDLOG_INFO("Found GetDesiredNumberOfViews function at index: {}", i);
            get_desired_number_of_views_index = i;
            m_get_desired_number_of_views_hook = std::make_unique<PointerHook>((void**)func_ptr, (void*)&get_desired_number_of_views_hook);
            break;
        }
    }

    // If double precision detected, it means it's >= UE 5.0.3
    if (m_has_double_precision && get_desired_number_of_views_index) {
        SPDLOG_INFO("Searching for GetViewPassForIndex function...");

        // Pretty simple, it's at +1, to be seen if this needs automation
        const auto get_view_pass_for_index_index = *get_desired_number_of_views_index + 1;

        auto func_ptr = &((uintptr_t*)vtable)[get_view_pass_for_index_index];

        if (IsBadReadPtr((void*)*func_ptr, sizeof(void*))) {
            SPDLOG_INFO("Could not locate GetViewPassForIndex function. A crash may occur.");
        } else {
            SPDLOG_INFO("Found GetViewPassForIndex function at index: {}", get_view_pass_for_index_index);
            m_get_view_pass_for_index_hook = std::make_unique<PointerHook>((void**)func_ptr, (void*)&get_view_pass_for_index_hook);
        }
    } else if (m_has_double_precision) {
        SPDLOG_INFO("Could not locate GetViewPassForIndex function because GetDesiredNumberOfViews function was not found. A crash may occur.");
    }

    SPDLOG_INFO("Leaving FFakeStereoRenderingHook::hook");

    const auto renderer_module = sdk::get_ue_module(L"Renderer");
    const auto backbuffer_format_cvar = sdk::find_cvar_by_description(L"Defines the default back buffer pixel format.", L"r.DefaultBackBufferPixelFormat", 4, renderer_module);
    m_pixel_format_cvar_found = backbuffer_format_cvar.has_value();

    // In 4.18 this doesn't exist. Not much we can do about that.
    if (backbuffer_format_cvar) {
        SPDLOG_INFO("Backbuffer Format CVar: {:x}", (uintptr_t)*backbuffer_format_cvar);
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0) = 0;   // 8bit RGBA, which is what VR headsets support
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0x4) = 0; // 8bit RGBA, which is what VR headsets support
    } else {
        SPDLOG_ERROR("Failed to find backbuffer format cvar, continuing anyways...");
    }

    // make a shadow copy of FFakeStereoRendering's vtable to get past weird compiler optimizations
    // that cause the hook to not work, reason being that the compiler will optimize
    // if the vtable pointer is equal to the original vtable pointer, and it will
    // not call the hook function, so we make a shadow copy of the vtable
    auto active_stereo_device = locate_active_stereo_rendering_device();
    
    // We need to manually insert a stereo device at this point if it's not already.
    // This is what the "nonstandard" hooks did, but those did not have access to FFakeStereoRendering's vtable.
    // All we need to do in this instance is get the engine offset to the stereo device, create a fake pointer with our own vtable,
    // and just overwrite the engine's (null) stereo device pointer with our fake one.
    // It is very rare that this should need to be done.
    if (!active_stereo_device) {
        SPDLOG_INFO("Attempting to create a stereo device without InitializeHMDDevice...");
        const auto device_offset = sdk::UEngine::get_stereo_rendering_device_offset();

        if (device_offset) {
            auto engine = sdk::UGameEngine::get();

            if (engine != nullptr) {
                m_fallback_device.vtable = (void*)vtable;
                *(uintptr_t*)((uintptr_t)engine + *device_offset) = (uintptr_t)&m_fallback_device;

                active_stereo_device = (uintptr_t)&m_fallback_device;
                s_stereo_rendering_device_offset = *device_offset; // Set it up if it's not already
            }
        } else {
            SPDLOG_ERROR("Could not create a new stereo device, VR may not work!");
        }
    }

    if (active_stereo_device) {
        SPDLOG_INFO("Found active stereo device: {:x}", (uintptr_t)*active_stereo_device);
        SPDLOG_INFO("Overwriting vtable...");

        static std::vector<uintptr_t> shadow_vtable{};
        auto& vtable = *(uintptr_t**)*active_stereo_device;

        for (auto i = 0; i < 100; i++) {
            shadow_vtable.push_back(vtable[i]);
        }

        vtable = shadow_vtable.data();
    } else {
        SPDLOG_INFO("Current stereo device is null, cannot overwrite vtable");
        patch_vtable_checks(); // fallback to patching vtable checks
    }

    setup_view_extensions();
    hook_game_viewport_client();

    m_finished_hooking = true;

    SPDLOG_INFO("Finished hooking FFakeStereoRendering!");

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
    SPDLOG_INFO("Attempting to create a stereo device for the game using nonstandard method");
    m_fallback_vtable.resize(30);

    // Give all of the functions placeholders.
    for (auto i = 0; i < m_fallback_vtable.size(); ++i) {
        m_fallback_vtable[i] = +[](FFakeStereoRendering* stereo) -> void* {
            return nullptr;
        };
    }

    // Actually implement the ones we care about now.
    auto idx = 0;
    //m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> void { SPDLOG_INFO("Destructor called?");  }; // destructor.
    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo) -> bool { 
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("IsStereoEnabled called: {:x}", (uintptr_t)_ReturnAddress());
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
        SPDLOG_INFO("GetTextSafeRegionBounds called");
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
        SPDLOG_INFO("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
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
        SPDLOG_ERROR("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }

    //m_418_detected = true;
    m_special_detected = true;
    m_manually_constructed = true;
    m_fallback_device.vtable = m_fallback_vtable.data();

    auto stereo_rendering_device_offset = sdk::UEngine::get_stereo_rendering_device_offset();
    if (!stereo_rendering_device_offset) {
        stereo_rendering_device_offset = 0xAC8; // fallback for the engine this was originally made for.
    }

    *(uintptr_t*)((uintptr_t)engine + *stereo_rendering_device_offset) = (uintptr_t)&m_fallback_device; // TODO: Automatically find this offset.

    // So the view extension hook will work.
    s_stereo_rendering_device_offset = *stereo_rendering_device_offset;

    hook_game_viewport_client();
    setup_view_extensions();

    SPDLOG_INFO("Finished creating stereo device for the game using nonstandard method");

    m_finished_hooking = true;

    return true;
}

bool FFakeStereoRenderingHook::nonstandard_create_stereo_device_hook_4_27() {
    SPDLOG_INFO("Attempting to create a stereo device for the game using nonstandard method (4.27)");

    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        SPDLOG_ERROR("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }

    m_fallback_vtable.resize(30);

    // Give all of the functions placeholders.
    for (auto i = 0; i < m_fallback_vtable.size(); ++i) {
        m_fallback_vtable[i] = +[](FFakeStereoRendering* stereo) -> void* {
            return nullptr;
        };
    }

    constexpr auto DESTRUCTOR_INDEX = 0;
    constexpr auto IS_STEREO_ENABLED_INDEX = 1;
    constexpr auto IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX = 2;
    constexpr auto ENABLE_STEREO_INDEX = 3;

    constexpr auto GET_DESIRED_NUMBER_OF_VIEWS_INDEX = 4;
    constexpr auto GET_VIEW_PASS_FOR_INDEX_INDEX = 5;
    constexpr auto GET_VIEW_INDEX_FOR_PASS_INDEX = 6;

    constexpr auto DEVICE_IS_STEREO_EYE_PASS_INDEX = 7;
    constexpr auto DEVICE_IS_STEREO_EYE_VIEW_INDEX = 8;
    constexpr auto DEVICE_IS_A_PRIMARY_PASS_INDEX = 9;
    constexpr auto DEVICE_IS_A_PRIMARY_VIEW_INDEX = 10;
    constexpr auto DEVICE_IS_A_SECONDARY_PASS_INDEX = 11;
    constexpr auto DEVICE_IS_A_SECONDARY_VIEW_INDEX = 12;
    constexpr auto DEVICE_IS_AN_ADDITIONAL_PASS_INDEX = 13; // not necessary...?
    constexpr auto DEVICE_IS_AN_ADDITIONAL_VIEW_INDEX = 14; // not necessary...?
    constexpr auto DEVICE_GET_LOD_VIEW_INDEX_INDEX = 15; // not necessary...?

    constexpr auto ADJUST_VIEW_RECT_INDEX = 16;
    constexpr auto CALCULATE_STEREO_VIEW_OFFSET_INDEX = 19;
    constexpr auto CALCULATE_STEREO_PROJECTION_MATRIX_INDEX = 20;
    constexpr auto RENDER_TEXTURE_RENDER_THREAD_INDEX = 22;
    constexpr auto GET_RENDER_TARGET_MANAGER_INDEX = 23;

    auto stereo_rendering_device_offset = sdk::UEngine::get_stereo_rendering_device_offset();
    if (!stereo_rendering_device_offset) {
        stereo_rendering_device_offset = 0xB18; // fallback for the engine this was originally made for.
    }

    static constexpr auto FSCENEVIEW_STEREO_PASS_OFFSET = 0xAF0;
    static auto get_stereo_pass = [](const FSceneView& view) -> EStereoscopicPass {
        return (EStereoscopicPass)*(uint8_t*)((uintptr_t)&view + FSCENEVIEW_STEREO_PASS_OFFSET);
    };

    // Actually implement the ones we care about now.
    m_fallback_vtable[DESTRUCTOR_INDEX] = +[](FFakeStereoRendering* stereo) -> void { SPDLOG_INFO("Destructor called?");  }; // destructor.
    m_fallback_vtable[IS_STEREO_ENABLED_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { 
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("IsStereoEnabled called: {:x}", (uintptr_t)_ReturnAddress());
#endif

        return g_hook->is_stereo_enabled(stereo); 
    }; // IsStereoEnabled

    m_fallback_vtable[IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // IsStereoEnabledOnNextFrame
    m_fallback_vtable[ENABLE_STEREO_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // EnableStereo

    m_fallback_vtable[ADJUST_VIEW_RECT_INDEX] = +[](FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) { 
        return g_hook->adjust_view_rect(stereo, index, x, y, w, h);
    }; // AdjustViewRect

    m_fallback_vtable[CALCULATE_STEREO_VIEW_OFFSET_INDEX] = 
    +[](FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, const float world_to_meters, Vector3f* view_location) {
        return g_hook->calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location);
    }; // CalculateStereoViewOffset

    m_fallback_vtable[CALCULATE_STEREO_PROJECTION_MATRIX_INDEX] = +[](FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
#endif

        if (!g_hook->m_has_double_precision) {
            (*out)[3][2] = 0.1f; // Need to pre-set the Z value to something, otherwise it will be 0.0f & probably break something.
        } else {
            auto dmat = (Matrix4x4d*)out;
            (*dmat)[3][2] = 0.1;
        }

        return g_hook->calculate_stereo_projection_matrix(stereo, out, view_index);
    }; // CalculateStereoProjectionMatrix

    m_fallback_vtable[RENDER_TEXTURE_RENDER_THREAD_INDEX] = 
    +[](FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
        return g_hook->render_texture_render_thread(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    };

    m_fallback_vtable[GET_RENDER_TARGET_MANAGER_INDEX] = +[](FFakeStereoRendering* stereo) { return g_hook->get_render_target_manager_hook(stereo); }; // GetRenderTargetManager

    m_fallback_vtable[GET_DESIRED_NUMBER_OF_VIEWS_INDEX] = +[](FFakeStereoRendering* stereo, bool stereo_enabled) -> int32_t { 
        return g_hook->get_desired_number_of_views_hook(stereo, stereo_enabled); 
    }; // GetDesiredNumberOfViews

    m_fallback_vtable[GET_VIEW_PASS_FOR_INDEX_INDEX] = +[](FFakeStereoRendering* stereo, bool stereo_requested, const uint32_t view_index) -> EStereoscopicPass {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("GetViewPassForIndex called: {:x} {} ", (uintptr_t)_ReturnAddress(), view_index);
    #endif

        return g_hook->get_view_pass_for_index_hook(stereo, stereo_requested, view_index);
    }; // GetViewPassForIndex

    m_fallback_vtable[GET_VIEW_INDEX_FOR_PASS_INDEX] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> int32_t {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("GetViewIndexForPass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        switch (pass) {
            case EStereoscopicPass::eSSP_FULL:
            case EStereoscopicPass::eSSP_PRIMARY:
                return 0;
            
            case EStereoscopicPass::eSSP_SECONDARY:
                return 1;
            
            default:
                SPDLOG_ERROR("Unknown pass: {}", (uint32_t)pass);
                return -1;
        };
    };

    m_fallback_vtable[DEVICE_IS_STEREO_EYE_PASS_INDEX] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsStereoEyePass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        return pass != EStereoscopicPass::eSSP_FULL;
    }; // DeviceIsStereoEyePass

    m_fallback_vtable[DEVICE_IS_STEREO_EYE_VIEW_INDEX] = +[](FFakeStereoRendering* stereo, const FSceneView& view) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsStereoEyeView called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)get_stereo_pass(view));
    #endif

        return get_stereo_pass(view) != EStereoscopicPass::eSSP_FULL;
    }; // DeviceIsStereoEyePass

    m_fallback_vtable[DEVICE_IS_A_PRIMARY_PASS_INDEX] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsAPrimaryPass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        return pass == EStereoscopicPass::eSSP_FULL || pass == EStereoscopicPass::eSSP_PRIMARY;
    }; // DeviceIsAPrimaryPass

    m_fallback_vtable[DEVICE_IS_A_PRIMARY_VIEW_INDEX] = +[](FFakeStereoRendering* stereo, const FSceneView& view) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsAPrimaryView called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)get_stereo_pass(view));
    #endif

        return get_stereo_pass(view) == EStereoscopicPass::eSSP_FULL || get_stereo_pass(view) == EStereoscopicPass::eSSP_PRIMARY;
    }; // DeviceIsAPrimaryPass

    m_fallback_vtable[DEVICE_IS_A_SECONDARY_PASS_INDEX] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsASecondaryPass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        return !(pass == EStereoscopicPass::eSSP_FULL || pass == EStereoscopicPass::eSSP_PRIMARY);
    }; // DeviceIsASecondaryPass

    m_fallback_vtable[DEVICE_IS_A_SECONDARY_VIEW_INDEX] = +[](FFakeStereoRendering* stereo, const FSceneView& view) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("DeviceIsASecondaryView called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)get_stereo_pass(view));
    #endif

        return get_stereo_pass(view) > EStereoscopicPass::eSSP_PRIMARY;
    }; // DeviceIsASecondaryView

    m_special_detected_4_27 = true;
    m_manually_constructed = true;
    m_fallback_device.vtable = m_fallback_vtable.data();

    auto& current_device = *(uintptr_t*)((uintptr_t)engine + *stereo_rendering_device_offset);
    SPDLOG_INFO("Current device: {:x}", current_device);
    current_device = (uintptr_t)&m_fallback_device; // TODO: Automatically find this offset.

    // So the view extension hook will work.
    s_stereo_rendering_device_offset = *stereo_rendering_device_offset;

    hook_game_viewport_client();
    setup_view_extensions();

    SPDLOG_INFO("Finished creating stereo device for the game using nonstandard method");

    m_finished_hooking = true;

    return true;
}

bool FFakeStereoRenderingHook::nonstandard_create_stereo_device_hook_4_22() {
    SPDLOG_INFO("Attempting to create a stereo device for the game using nonstandard method (4.22)");

    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        SPDLOG_ERROR("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }

    m_fallback_vtable.resize(30);

    // Give all of the functions placeholders.
    for (auto i = 0; i < m_fallback_vtable.size(); ++i) {
        m_fallback_vtable[i] = +[](FFakeStereoRendering* stereo) -> void* {
            return nullptr;
        };
    }

    constexpr auto DESTRUCTOR_INDEX = 0;
    constexpr auto IS_STEREO_ENABLED_INDEX = 1;
    constexpr auto IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX = 2;
    constexpr auto ENABLE_STEREO_INDEX = 3;

    constexpr auto GET_DESIRED_NUMBER_OF_VIEWS_INDEX = 4;
    constexpr auto GET_VIEW_PASS_FOR_INDEX_INDEX = 5;
    constexpr auto GET_VIEW_INDEX_FOR_PASS_INDEX = 6;
    constexpr auto IS_STEREO_EYE_PASS_INDEX = 7;

    constexpr auto ADJUST_VIEW_RECT_INDEX = 8;
    constexpr auto CALCULATE_STEREO_VIEW_OFFSET_INDEX = ADJUST_VIEW_RECT_INDEX + 3;
    constexpr auto CALCULATE_STEREO_PROJECTION_MATRIX_INDEX = CALCULATE_STEREO_VIEW_OFFSET_INDEX + 1;
    constexpr auto RENDER_TEXTURE_RENDER_THREAD_INDEX = CALCULATE_STEREO_PROJECTION_MATRIX_INDEX + 2;
    constexpr auto GET_RENDER_TARGET_MANAGER_INDEX = RENDER_TEXTURE_RENDER_THREAD_INDEX + 1;

    auto stereo_rendering_device_offset = sdk::UEngine::get_stereo_rendering_device_offset();
    if (!stereo_rendering_device_offset) {
        stereo_rendering_device_offset = 0xAB8; // fallback for the engine this was originally made for.
    }

    // Actually implement the ones we care about now.
    m_fallback_vtable[DESTRUCTOR_INDEX] = +[](FFakeStereoRendering* stereo) -> void { SPDLOG_INFO("Destructor called?");  }; // destructor.
    m_fallback_vtable[IS_STEREO_ENABLED_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { 
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("IsStereoEnabled called: {:x}", (uintptr_t)_ReturnAddress());
#endif

        return g_hook->is_stereo_enabled(stereo); 
    }; // IsStereoEnabled

    m_fallback_vtable[IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // IsStereoEnabledOnNextFrame
    m_fallback_vtable[ENABLE_STEREO_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // EnableStereo

    m_fallback_vtable[ADJUST_VIEW_RECT_INDEX] = +[](FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) { 
        return g_hook->adjust_view_rect(stereo, index, x, y, w, h);
    }; // AdjustViewRect

    m_fallback_vtable[CALCULATE_STEREO_VIEW_OFFSET_INDEX] = 
    +[](FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, const float world_to_meters, Vector3f* view_location) {
        return g_hook->calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location);
    }; // CalculateStereoViewOffset

    m_fallback_vtable[CALCULATE_STEREO_PROJECTION_MATRIX_INDEX] = +[](FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
#endif

        if (!g_hook->m_has_double_precision) {
            (*out)[3][2] = 0.1f; // Need to pre-set the Z value to something, otherwise it will be 0.0f & probably break something.
        } else {
            auto dmat = (Matrix4x4d*)out;
            (*dmat)[3][2] = 0.1;
        }

        return g_hook->calculate_stereo_projection_matrix(stereo, out, view_index);
    }; // CalculateStereoProjectionMatrix

    m_fallback_vtable[RENDER_TEXTURE_RENDER_THREAD_INDEX] = 
    +[](FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
        return g_hook->render_texture_render_thread(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    };

    m_fallback_vtable[GET_RENDER_TARGET_MANAGER_INDEX] = +[](FFakeStereoRendering* stereo) { return g_hook->get_render_target_manager_hook(stereo); }; // GetRenderTargetManager

    m_fallback_vtable[GET_DESIRED_NUMBER_OF_VIEWS_INDEX] = +[](FFakeStereoRendering* stereo, bool stereo_enabled) -> int32_t { 
        return g_hook->get_desired_number_of_views_hook(stereo, stereo_enabled); 
    }; // GetDesiredNumberOfViews

    m_fallback_vtable[GET_VIEW_PASS_FOR_INDEX_INDEX] = +[](FFakeStereoRendering* stereo, bool stereo_requested, const uint32_t view_index) -> EStereoscopicPass {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("GetViewPassForIndex called: {:x} {} ", (uintptr_t)_ReturnAddress(), view_index);
    #endif

        return g_hook->get_view_pass_for_index_hook(stereo, stereo_requested, view_index);
    }; // GetViewPassForIndex

    m_fallback_vtable[GET_VIEW_INDEX_FOR_PASS_INDEX] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> int32_t {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("GetViewIndexForPass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        switch (pass) {
            case EStereoscopicPass::eSSP_FULL:
            case EStereoscopicPass::eSSP_PRIMARY:
                return 0;
            
            case EStereoscopicPass::eSSP_SECONDARY:
                return 1;
            
            default:
                SPDLOG_ERROR("Unknown pass: {}", (uint32_t)pass);
                return -1;
        };
    };

    m_fallback_vtable[IS_STEREO_EYE_PASS_INDEX ] = +[](FFakeStereoRendering* stereo, const EStereoscopicPass pass) -> bool {
    #ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("IsStereoPass called: {:x} {} ", (uintptr_t)_ReturnAddress(), (uint32_t)pass);
    #endif

        return pass != EStereoscopicPass::eSSP_FULL;
    };

    m_special_detected_4_22 = true;
    m_manually_constructed = true;
    m_fallback_device.vtable = m_fallback_vtable.data();

    auto& current_device = *(uintptr_t*)((uintptr_t)engine + *stereo_rendering_device_offset);
    SPDLOG_INFO("Current device: {:x}", current_device);
    current_device = (uintptr_t)&m_fallback_device; // TODO: Automatically find this offset.

    // So the view extension hook will work.
    s_stereo_rendering_device_offset = *stereo_rendering_device_offset;

    hook_game_viewport_client();
    setup_view_extensions();

    SPDLOG_INFO("Finished creating stereo device for the game using nonstandard method");

    m_finished_hooking = true;

    return true;
}

bool FFakeStereoRenderingHook::nonstandard_create_stereo_device_hook_4_18() {
    SPDLOG_INFO("Attempting to create a stereo device for the game using nonstandard method (4.18)");

    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        SPDLOG_ERROR("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }

    m_fallback_vtable.resize(30);

    // Give all of the functions placeholders.
    for (auto i = 0; i < m_fallback_vtable.size(); ++i) {
        m_fallback_vtable[i] = +[](FFakeStereoRendering* stereo) -> void* {
            return nullptr;
        };
    }

    constexpr auto IS_STEREO_ENABLED_INDEX = 0;
    constexpr auto IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX = 1;
    constexpr auto ENABLE_STEREO_INDEX = 2;

    constexpr auto ADJUST_VIEW_RECT_INDEX = 3;
    constexpr auto CALCULATE_STEREO_VIEW_OFFSET_INDEX = ADJUST_VIEW_RECT_INDEX + 2;
    constexpr auto CALCULATE_STEREO_PROJECTION_MATRIX_INDEX = CALCULATE_STEREO_VIEW_OFFSET_INDEX + 1;
    constexpr auto RENDER_TEXTURE_RENDER_THREAD_INDEX = CALCULATE_STEREO_PROJECTION_MATRIX_INDEX + 3;
    constexpr auto GET_RENDER_TARGET_MANAGER_INDEX = RENDER_TEXTURE_RENDER_THREAD_INDEX + 3;

    auto stereo_rendering_device_offset = sdk::UEngine::get_stereo_rendering_device_offset();
    if (!stereo_rendering_device_offset) {
        stereo_rendering_device_offset = 0xAE8; // fallback for the engine this was originally made for.
    }

    // Actually implement the ones we care about now.
    m_fallback_vtable[IS_STEREO_ENABLED_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { 
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("IsStereoEnabled called: {:x}", (uintptr_t)_ReturnAddress());
#endif

        return g_hook->is_stereo_enabled(stereo); 
    }; // IsStereoEnabled

    m_fallback_vtable[IS_STEREO_ENABLED_ON_NEXT_FRAME_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // IsStereoEnabledOnNextFrame
    m_fallback_vtable[ENABLE_STEREO_INDEX] = +[](FFakeStereoRendering* stereo) -> bool { return g_hook->is_stereo_enabled(stereo); }; // EnableStereo

    m_fallback_vtable[ADJUST_VIEW_RECT_INDEX] = +[](FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) { 
        return g_hook->adjust_view_rect(stereo, index, x, y, w, h);
    }; // AdjustViewRect

    m_fallback_vtable[CALCULATE_STEREO_VIEW_OFFSET_INDEX] = 
    +[](FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, const float world_to_meters, Vector3f* view_location) {
        return g_hook->calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location);
    }; // CalculateStereoViewOffset

    m_fallback_vtable[CALCULATE_STEREO_PROJECTION_MATRIX_INDEX] = +[](FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        SPDLOG_INFO("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
#endif

        if (!g_hook->m_has_double_precision) {
            (*out)[3][2] = 0.1f; // Need to pre-set the Z value to something, otherwise it will be 0.0f & probably break something.
        } else {
            auto dmat = (Matrix4x4d*)out;
            (*dmat)[3][2] = 0.1;
        }

        return g_hook->calculate_stereo_projection_matrix(stereo, out, view_index);
    }; // CalculateStereoProjectionMatrix

    m_fallback_vtable[RENDER_TEXTURE_RENDER_THREAD_INDEX] = 
    +[](FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
        return g_hook->render_texture_render_thread(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    };

    m_fallback_vtable[GET_RENDER_TARGET_MANAGER_INDEX] = +[](FFakeStereoRendering* stereo) { return g_hook->get_render_target_manager_hook(stereo); }; // GetRenderTargetManager

    m_special_detected_4_18 = true;
    m_uses_old_rendertarget_manager = true; // this engine has a funny render target manager.
    m_manually_constructed = true;
    m_fallback_device.vtable = m_fallback_vtable.data();

    auto& current_device = *(uintptr_t*)((uintptr_t)engine + *stereo_rendering_device_offset);
    SPDLOG_INFO("Current device: {:x}", current_device);
    current_device = (uintptr_t)&m_fallback_device; // TODO: Automatically find this offset.

    // So the view extension hook will work.
    s_stereo_rendering_device_offset = *stereo_rendering_device_offset;

    hook_game_viewport_client();
    setup_view_extensions();

    SPDLOG_INFO("Finished creating stereo device for the game using nonstandard method");

    m_finished_hooking = true;

    return true;
}

bool FFakeStereoRenderingHook::hook_game_viewport_client() try {
    SPDLOG_INFO("Attempting to hook UGameViewportClient::Draw...");

    // We need to cache the canvas index before we hook the draw function or else this doesn't work.
    sdk::FViewport::get_debug_canvas_index();
    auto game_viewport_client_draw = sdk::UGameViewportClient::get_draw_function();

    if (!game_viewport_client_draw) {
        SPDLOG_ERROR("Failed to find UGameViewportClient::Draw!");
        m_has_game_viewport_client_draw_hook = false;
        return false;
    }

    m_gameviewportclient_draw_hook = safetyhook::create_inline((void*)*game_viewport_client_draw, &game_viewport_client_draw_hook);
    m_has_game_viewport_client_draw_hook = true;

    if (!m_gameviewportclient_draw_hook) {
        SPDLOG_ERROR("Failed to hook UGameViewportClient::Draw!");
        return false;
    }

    return true;
} catch(std::exception& e) {
    SPDLOG_ERROR("Failed to hook UGameViewportClient: {}", e.what());
    return false;
} catch(...) {
    SPDLOG_ERROR("Failed to hook UGameViewportClient!");
    return false;
}

void FFakeStereoRenderingHook::viewport_draw_hook(void* viewport, bool should_present) {
    ZoneScopedN(__FUNCTION__);

    auto call_orig = [&]() {
        ZoneScopedN("FViewport::Draw");
        g_hook->m_viewport_draw_hook.call(viewport, should_present);
    };

    if (!g_framework->is_game_data_intialized()) {
        call_orig();
        return;
    }

    if (g_hook->m_ignore_next_viewport_draw) {
        g_hook->m_ignore_next_viewport_draw = false;
        return;
    }

    auto vr = VR::get();

    if (!vr->is_hmd_active()) {
        call_orig();
        return;
    }

    static bool once = true;

    if (once) {
        SPDLOG_INFO("FViewport::Draw called for the first time.");
        once = false;
    }

    call_orig();
}

// This function needs some more work for more rigorous filtering
// However it does its job on the relevant titles
// This is only used for the UI compatibility mode.
FRHITexture2D** FFakeStereoRenderingHook::viewport_get_render_target_texture_hook(sdk::FViewport* viewport) {
    const auto retaddr = (uintptr_t)_ReturnAddress();
    static std::unordered_set<uintptr_t> redirected_retaddrs{};
    static std::unordered_set<uintptr_t> call_original_retaddrs{};
    static std::recursive_mutex retaddr_mutex{};
    static bool has_view_family_tex{false};

    SPDLOG_INFO_ONCE("FViewport::GetRenderTargetTexture called!");
    const auto og = g_hook->m_viewport_get_render_target_texture_hook->get_original<decltype(&viewport_get_render_target_texture_hook)>();
    const auto& vr = VR::get();

    if (!vr->is_ahud_compatibility_enabled() || !vr->is_hmd_active() || g_hook->m_slate_draw_window_thread_id == 0) {
        return og(viewport);
    }

    {
        std::scoped_lock _{retaddr_mutex};

        if (call_original_retaddrs.contains(retaddr)) {
            return og(viewport);
        }

        // Hacky way to allow the first texture to go through
        // For the games that are using something other than ViewFamilyTexture as the scene RT.
        if (!call_original_retaddrs.empty() && !redirected_retaddrs.contains(retaddr) && !has_view_family_tex) {
            return og(viewport);
        }

        if (!redirected_retaddrs.contains(retaddr) && !call_original_retaddrs.contains(retaddr)) {
            SPDLOG_INFO("FViewport::GetRenderTargetTexture called from {:x}", retaddr);
            
            // Analyze surrounding code to determine if this is a valid call.
            auto func_start = utility::find_function_start(retaddr);

            if (!func_start) {
                func_start = retaddr;
            }

            // The function that has this string reference should ALWAYS get passed
            // back to the original function, this is the actual scene render target.
            // Everything else we will redirect to the UI render target.
            if (utility::find_string_reference_in_path(*func_start, L"ViewFamilyTexture", false)) {
                SPDLOG_INFO("Found view family texture reference @ {:x}", retaddr);
                call_original_retaddrs.insert(retaddr);
                has_view_family_tex = true;
                return og(viewport);
            }

            // Probably NOT...
            /*if (utility::find_string_reference_in_path(*func_start, L"r.RHICmdAsyncRHIThreadDispatch")) {
                SPDLOG_INFO("Found RHICmdAsyncRHIThreadDispatch reference @ {:x}", retaddr);
                call_original_retaddrs.insert(retaddr);
                return og(viewport);
            }*/

            if (utility::find_string_reference_in_path(*func_start, L"FinalPostProcessColor", false)) {
                SPDLOG_INFO("Found FinalPostProcessColor reference @ {:x}", retaddr);
                call_original_retaddrs.insert(retaddr);
                return og(viewport);
            }

            // TODO? this needs some more rigorous filtering
            // some games are insane and have multiple "UnknownTexture" references...
            if (utility::find_string_reference_in_path(*func_start, L"UnknownTexture", false)) {
                SPDLOG_INFO("Found unknown texture reference @ {:x}", retaddr);
                call_original_retaddrs.insert(retaddr);
                return og(viewport);
            }

            SPDLOG_INFO("Redirecting FViewport::GetRenderTargetTexture call to UI render target @ {:x}", retaddr);
            redirected_retaddrs.insert(retaddr);
        }
    }

    // Finally redirect the call to the UI render target.
    auto& ui_target = g_hook->get_render_target_manager()->get_ui_target();

    if (ui_target != nullptr) {
        return &ui_target;
    }

    return og(viewport);
}

void FFakeStereoRenderingHook::game_viewport_client_draw_hook(sdk::UGameViewportClient* viewport_client, sdk::FViewport* viewport, sdk::FCanvas* canvas, void* a4) {
    ZoneScopedN(__FUNCTION__);

    // UI compatibility mode
    // Tries to redirect calls to GetRenderTargetTexture to point towards our UI
    // texture instead of the scene render target, if it's not the scene itself/the view family texture.
    // This usually isn't needed but sometimes there are bespoke changes to the rendering pipeline
    // or uses of the AHUD class that make it necessary.
    if (g_framework->is_game_data_intialized() && VR::get()->is_ahud_compatibility_enabled() && viewport != nullptr) {
        if (g_hook->m_viewport_get_render_target_texture_hook == nullptr) {
            SPDLOG_INFO("Hooking FViewport::GetRenderTargetTexture...");
            void** vp_vtable = *(void***)viewport;
            g_hook->m_viewport_get_render_target_texture_hook = std::make_unique<PointerHook>(&vp_vtable[1], &viewport_get_render_target_texture_hook);
            SPDLOG_INFO("Hooked FViewport::GetRenderTargetTexture!");
        }
    }

    auto call_orig = [=]() {
        ZoneScopedN("UGameViewportClient::Draw");
        g_hook->m_gameviewportclient_draw_hook.call(viewport_client, viewport, canvas, a4);
    };

    SPDLOG_INFO_ONCE("UGameViewportClient::Draw called for the first time.");

    if (!g_framework->is_game_data_intialized()) {
        call_orig();
        return;
    }

    g_hook->m_in_viewport_client_draw = true;
    g_hook->m_was_in_viewport_client_draw = false;

    utility::ScopeGuard _{ 
        []() { 
            g_hook->m_in_viewport_client_draw = false;
            g_hook->m_was_in_viewport_client_draw = false;
        } 
    };

    auto vr = VR::get();

    if (!vr->is_hmd_active()) {
        call_orig();
        return;
    }

    static uint32_t hook_attempts = 0;
    static bool run_anyways = false;

    if (hook_attempts < 100 && !g_hook->m_hooked_game_engine_tick && g_hook->m_attempted_hook_game_engine_tick) {
        ZoneScopedN("UGameViewportClient::Draw (hook UGameEngine::Tick)");
        SPDLOG_INFO("Performing alternative UGameEngine::Tick hook for synced AFR.");

        ++hook_attempts;

        // Go up the stack and find the viewport draw function.
        constexpr auto max_stack_depth = 100;
        uintptr_t stack[max_stack_depth]{};

        const auto depth = RtlCaptureStackBackTrace(0, max_stack_depth, (void**)&stack, nullptr);

        for (auto i = 0; i < depth; ++i) {
            SPDLOG_INFO("Stack[{}]: {:x}", i, stack[i]);
        }

        for (auto i = 3; i < depth; ++i) {
            const auto ret = stack[i];

            g_hook->attempt_hook_game_engine_tick(ret);

            if (g_hook->m_hooked_game_engine_tick) {
                SPDLOG_INFO("Successfully hooked UGameEngine::Tick for synced AFR.");
                break;
            }
        }
    } else {
        run_anyways = !g_hook->m_hooked_game_engine_tick;
    }

    const auto in_engine_tick = g_hook->m_in_engine_tick;

    if (run_anyways || in_engine_tick) {
        if (g_hook->m_has_view_extension_hook) {
            g_frame_count = vr->get_runtime()->internal_frame_count;
            vr->update_hmd_state(true, vr->get_runtime()->internal_frame_count + 1);
        } else {
            vr->update_hmd_state(false);
        }
    }

    const auto& mods = g_framework->get_mods()->get_mods();

    for (const auto& mod : mods) {
        mod->on_pre_viewport_client_draw(viewport_client, viewport, canvas);
    }

    call_orig();

    // Perform synced eye rendering (synced AFR)
    if (in_engine_tick && vr->is_using_synchronized_afr()) {
        static bool hooked_viewport_draw = false;

        // Hook for FViewport::Draw
        if (g_hook->m_hooked_game_engine_tick && !hooked_viewport_draw) {
            hooked_viewport_draw = true;

            // Go up the stack and find the viewport draw function.
            constexpr auto max_stack_depth = 100;
            uintptr_t stack[max_stack_depth]{};

            const auto depth = RtlCaptureStackBackTrace(0, max_stack_depth, (void**)&stack, nullptr);
            if (depth >= 2) {
                // Log the stack functions
                for (auto i = 0; i < depth; ++i) {
                    SPDLOG_INFO("(Stack[{}]: {:x}", i, stack[i]);
                }

                auto try_hook_index = [&](uint32_t index) -> bool {
                    SPDLOG_INFO("Attempting to locate FViewport::Draw function @ stack[{}]", index);

                    const auto viewport_draw_middle = stack[index];
                    const auto viewport_draw = utility::find_function_start_with_call(viewport_draw_middle);

                    if (!viewport_draw) {
                        SPDLOG_ERROR("Failed to find viewport draw function @ {}", index);
                        return false;
                    }

                    SPDLOG_INFO("Found FViewport::Draw function at {:x}", (uintptr_t)*viewport_draw); 

                    g_hook->m_viewport_draw_hook = safetyhook::create_inline((void*)*viewport_draw, &viewport_draw_hook);

                    if (!g_hook->m_viewport_draw_hook) {
                        SPDLOG_ERROR("Failed to hook FViewport::Draw function!");
                        return false;
                    }

                    return true;
                };

                if (!try_hook_index(1)) {
                    // Fallback to index 3, on some UE4 games the viewport draw function is called from a different stack index.
                    if (!try_hook_index(2)) {
                        SPDLOG_ERROR("Failed to find viewport draw function! Cannot perform synced AFR!");
                    }
                }
            }
        }
    }

    // This is how synchronized AFR works. it forces a world draw
    // on the start of the next engine tick, before the world ticks again.
    // that will allow both views and the world to be drawn in sync with no artifacts.
    if (in_engine_tick && vr->is_using_synchronized_afr() && g_frame_count % 2 == 0) {
        GameThreadWorker::get().enqueue([=]() {
            if (g_hook->m_viewport_draw_hook) {
                const auto viewport_draw = (void (*)(void*, bool))g_hook->m_viewport_draw_hook.target();
                viewport_draw(viewport, true);

                auto& vr = VR::get();
                const auto method = vr->get_synced_sequential_method();
                
                if (method == VR::SyncedSequentialMethod::SKIP_TICK) {
                    g_hook->m_ignore_next_engine_tick = true;
                    //g_hook->m_ignore_next_viewport_draw = true;
                } else if (method == VR::SyncedSequentialMethod::SKIP_DRAW) {
                    g_hook->m_ignore_next_viewport_draw = true;
                }
            }
        });
    }

    for (const auto& mod : mods) {
        mod->on_post_viewport_client_draw(viewport_client, viewport, canvas);
    }
}

static std::array<uintptr_t, 50> g_view_extension_vtable{};
struct SceneViewExtensionAnalyzer;

// Analyzes all of the virtual functions for ISceneViewExtension
// We create the ISceneViewExtension ourselves and overwrite all of the virtual functions
// The class will count how many times each virtual is getting called
// and then when a threshold is reached, it finds the most called one
// the most called one is IsActiveThisFrame which we need to activate the ISceneViewExtension
struct SceneViewExtensionAnalyzer {
    template<int N>
    struct FillVtable {
        static void fill(std::array<uintptr_t, 50>& table);
        static void fill2(std::array<uintptr_t, 50>& table);
    };

    template<>
    struct FillVtable<-1> {
        static void fill(std::array<uintptr_t, 50>& table) {}
        static void fill2(std::array<uintptr_t, 50>& table) {}
    };

    struct AnalyzedFunction {
        uint32_t call_count{0};
        uint32_t frame_count_a2{0};
        uint32_t frame_count_a3{0};
        uint32_t frame_count_offset_a2{0};
        uint32_t frame_count_offset_a3{0};
        uint32_t times_frame_count_correct_a2{0};
        uint32_t times_frame_count_correct_a3{0};
        std::array<uint8_t, 0x100> a2_data{};
        std::array<uint8_t, 0x100> a3_data{};
    };

    static inline std::recursive_mutex dummy_mutex{};
    static inline uint32_t total_call_count{};
    static inline std::unordered_map<uint32_t, AnalyzedFunction> functions{};
    static inline bool has_found_is_active_this_frame_index{false};
    static inline bool has_found_begin_render_viewfamily{false};
    static inline bool index_0_called{false};
    
    static inline uint32_t is_active_this_frame_index{0};
    static inline uint32_t begin_render_viewfamily_index{0};
    static inline uint32_t pre_render_viewfamily_renderthread_index{0};
    static inline uint32_t frame_count_offset{0};

    template<int N>
    static bool analysis_dummy_stage1(ISceneViewExtension* extension, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (N == 0) {
            index_0_called = true;
        }

        if (has_found_is_active_this_frame_index) {
            return false;
        }

        std::scoped_lock _{dummy_mutex};

        auto& func = functions[N];

        ++total_call_count;
        ++functions[N].call_count;

        if (total_call_count >= 50) {
            // Find the most called index, it's going to be IsActiveThisFrame
            uint32_t max_count = 0;
            uint32_t max_index = 0;

            for (const auto& func : functions) {
                const auto count = func.second.call_count;
                const auto index = func.first;

                if (count > max_count) {
                    max_count = count;
                    max_index = index;
                }
            }

            SPDLOG_INFO("[Stage 1] Found most called index to be {} with {} calls", max_index, max_count);

            functions.clear();
            FillVtable<g_view_extension_vtable.size() - 1>::fill2(g_view_extension_vtable);

            // Force the function to return true
            g_view_extension_vtable[max_index] = (uintptr_t)+[](ISceneViewExtension* ext) -> bool {
                return true;
            };

            has_found_is_active_this_frame_index = true;
            is_active_this_frame_index = max_index;
        } else {
            if (functions[N].call_count == 1) {
                SPDLOG_INFO("[Stage 1] ISceneViewExtension Index {} called for the first time!", N);
            }
        }

        return false;
    };

    template<int N>
    static bool analysis_dummy_stage2(ISceneViewExtension* extension, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (has_found_begin_render_viewfamily) {
            return false;
        }

        if (N == 0) {
            index_0_called = true;
        }

        std::scoped_lock _{dummy_mutex};

        if (functions.contains(N)) {
            auto& func = functions[N];

            if (func.call_count++ == 0) {
                SPDLOG_INFO("[Stage 2] SceneViewExtension Index {} called for the first time!", N);
            }

            const auto& last_view_family_data_a2 = func.a2_data;
            const auto view_family_a2 = (uintptr_t)a2;

            if (a2 != 0 && !IsBadReadPtr((void*)a2, 0x100)) {
                for (auto i = 0x10; i < last_view_family_data_a2.size(); i += sizeof(uint32_t)) {
                    const auto a = *(uint32_t*)&last_view_family_data_a2[i];
                    const auto b = *(uint32_t*)&((uint8_t*)view_family_a2)[i];

                    if (b == a + 1 && a >= 10) { // rule out really low frame counts (this could be something else)
                        if (func.frame_count_a2 + 1 == b) {
                            SPDLOG_INFO("[A2] Function index {} Found frame count offset at {:x}, ({})", N, i, b);

                            func.frame_count_offset_a2 = i;
                            ++func.times_frame_count_correct_a2;

                            // func_next is one of the functions ahead of N and has the frame count in a3
                            AnalyzedFunction* func_next = nullptr;
                            uint32_t next_index = 0;

                            for (auto j = N + 1; j < g_view_extension_vtable.size(); j++) {
                                if (functions.contains(j)) {
                                    const auto& next = functions[j];

                                    if (next.times_frame_count_correct_a3 >= 10) {
                                        func_next = &functions[j];
                                        next_index = j;
                                        break;
                                    }
                                }
                            }

                            if (func_next != nullptr) {
                                if (func.times_frame_count_correct_a2 >= 50 && 
                                    func_next->times_frame_count_correct_a3 >= 50 && 
                                    func.frame_count_offset_a2 == func_next->frame_count_offset_a3 &&
                                    std::abs((int32_t)func.frame_count_a2 - (int32_t)func_next->frame_count_a3) <= 3) // In some games, the frame delta is really high but the same offset (so, it's wrong)
                                {
                                    SPDLOG_INFO("Found final frame count offset at {:x}", i);
                                    SPDLOG_INFO("Found BeginRenderViewFamily at index {}", N);
                                    SPDLOG_INFO("Found PreRenderViewFamily_RenderThread at index {}", next_index);
                                    has_found_begin_render_viewfamily = true;
                                    begin_render_viewfamily_index = N;
                                    pre_render_viewfamily_renderthread_index = next_index;

                                    frame_count_offset = i;

                                    setup_view_extension_hook();
                                    return false;
                                }   
                            }
                        }

                        func.frame_count_a2 = b;
                        break;
                    }
                }
            }

            const auto& last_view_family_data_a3 = func.a3_data;
            const auto view_family_a3 = (uintptr_t)a3;

            if (a3 != 0 && !IsBadReadPtr((void*)a3, 0x100)) {
                for (auto i = 0x10; i < last_view_family_data_a3.size(); i += sizeof(uint32_t)) {
                    const auto a = *(uint32_t*)&last_view_family_data_a3[i];
                    const auto b = *(uint32_t*)&((uint8_t*)view_family_a3)[i];

                    if (b == a + 1 && a >= 10) { // rule out really low frame counts (this could be something else)
                        if (func.frame_count_a3 + 1 == b) {
                            SPDLOG_INFO("[A3] Function index {} Found frame count offset at {:x} ({})", N, i, b);
                            ++func.times_frame_count_correct_a3;
                        }

                        func.frame_count_a3 = b;
                        func.frame_count_offset_a3 = i;
                        break;
                    }
                }
            }
        }

        if (a2 != 0 && !IsBadReadPtr((void*)a2, 0x100)) {
            memcpy(functions[N].a2_data.data(), (void*)a2, 0x100);
        }

        if (a3 != 0 && !IsBadReadPtr((void*)a3, 0x100)) {
            memcpy(functions[N].a3_data.data(), (void*)a3, 0x100);
        }

        return false;
    }

    static inline std::recursive_mutex vtable_mutex{};
    static inline std::unordered_map<sdk::FRHICommandBase_New*, void**> original_vtables{};
    static inline std::unordered_map<sdk::FRHICommandBase_New*, uint32_t> cmd_frame_counts{};

    // Meant to be called after analysis has been completed
    static void setup_view_extension_hook() {
        std::scoped_lock _{dummy_mutex};

        SPDLOG_INFO("Setting up BeginRenderViewFamily hook...");

        const auto setup_view_family_index = index_0_called ? 0 : 1;

        g_view_extension_vtable[setup_view_family_index] = (uintptr_t)&FFakeStereoRenderingHook::setup_view_family;
        g_view_extension_vtable[begin_render_viewfamily_index] = (uintptr_t)&FFakeStereoRenderingHook::begin_render_viewfamily;

        if (!index_0_called && (setup_view_family_index + 2) != begin_render_viewfamily_index) {
            g_view_extension_vtable[setup_view_family_index + 2] = (uintptr_t)&FFakeStereoRenderingHook::setup_viewpoint;
        }

        // PreRenderViewFamily_RenderThread
        g_view_extension_vtable[pre_render_viewfamily_renderthread_index] = (uintptr_t)&FFakeStereoRenderingHook::pre_render_viewfamily_renderthread;

        SPDLOG_INFO("Done setting up BeginRenderViewFamily hook!");
    }

    template<int N>
    static void* hooked_command_fn(sdk::FRHICommandBase_New* cmd, sdk::FRHICommandListBase* cmd_list, void* debug_context, void* r9, void* stack_1, void* stack_2, void* stack_3, void* stack_4, void* stack_5, void* stack_6, void* stack_7, void* stack_8) {
        std::scoped_lock _{vtable_mutex};
        //std::scoped_lock __{VR::get()->get_vr_mutex()};

        static bool once = true;

        if (once) {
            SPDLOG_INFO("[ISceneViewExtension] Successfully hijacked command list! {}", N);
            once = false;
        }

        const auto original_vtable = original_vtables[cmd];
        const auto original_func = original_vtable[N];

        const auto func = (decltype(hooked_command_fn<N>)*)original_func;
        const auto frame_count = cmd_frame_counts[cmd];

        auto& vr = VR::get();
        auto runtime = vr->get_runtime();

        auto call_orig = [=]() {
            const auto result = func(cmd, cmd_list, debug_context, r9, stack_1, stack_2, stack_3, stack_4, stack_5, stack_6, stack_7, stack_8);

            if (N == 0) {
                runtime->enqueue_render_poses(frame_count);
            }

            return result;
        };

        if (N != 0) {
            return call_orig();
        }

        // set the vtable back
        *(void**)cmd = original_vtable;
        original_vtables.erase(cmd);
        cmd_frame_counts.erase(cmd);

        RHIThreadWorker::get().execute();

        if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
            if (runtime->is_openxr()) {
                if (g_framework->get_renderer_type() == Framework::RendererType::D3D11) {
                    if (!runtime->got_first_sync || runtime->synchronize_frame(frame_count) != VRRuntime::Error::SUCCESS) {
                        return call_orig();
                    }  
                } else if (runtime->synchronize_frame(frame_count) != VRRuntime::Error::SUCCESS) {
                    return call_orig();
                }

                vr->get_openxr_runtime()->begin_frame();
            } else {
                if (runtime->synchronize_frame(frame_count) != VRRuntime::Error::SUCCESS) {
                    return call_orig();
                }
            }
        }

        return call_orig();
    }

    static void hook_new_rhi_command(sdk::FRHICommandBase_New* last_command, uint32_t frame_count) {
        std::scoped_lock __{vtable_mutex};

        auto runtime = VR::get()->get_runtime();
        runtime->on_pre_render_render_thread(frame_count);

        if (last_command == nullptr || *(void**)last_command == nullptr) {
            SPDLOG_INFO("Cannot hook command with no vtable, falling back to passing current frame count to runtime");
            runtime->enqueue_render_poses(frame_count);
            return;
        }

        // Whichever one gets called first is the winner winner chicken dinner
        static std::array<uintptr_t, 7> new_vtable{
            (uintptr_t)&hooked_command_fn<0>,
            (uintptr_t)&hooked_command_fn<1>,
            (uintptr_t)&hooked_command_fn<2>,
            (uintptr_t)&hooked_command_fn<3>,
            (uintptr_t)&hooked_command_fn<4>,
            (uintptr_t)&hooked_command_fn<5>,
            (uintptr_t)&hooked_command_fn<6>
        };

        cmd_frame_counts[last_command] = frame_count;

        if (original_vtables.contains(last_command) || *(void**)last_command == new_vtable.data()) {
            static auto last_log_time = std::chrono::high_resolution_clock::time_point{};
            const auto now = std::chrono::high_resolution_clock::now();
            
            if (now - last_log_time > std::chrono::seconds(1)) {
                SPDLOG_WARN("Something strange is going on, the vtable is already hooked, maybe previous frame was not rendered?");
                last_log_time = now;
            }

            return;
        }

        original_vtables[last_command] = *(void***)last_command;
        *(void***)last_command = (void**)new_vtable.data();
    }

    static void hook_old_rhi_command(sdk::FRHICommandBase_Old* last_command, uint32_t frame_count) {
        static std::recursive_mutex func_mutex{};
        static std::unordered_map<sdk::FRHICommandBase_Old*, sdk::FRHICommandBase_Old::Func> original_funcs{};
        static std::unordered_map<sdk::FRHICommandBase_Old*, uint32_t> cmd_frame_counts{};

        std::scoped_lock __{func_mutex};

        auto runtime = VR::get()->get_runtime();
        runtime->on_pre_render_render_thread(frame_count);

        cmd_frame_counts[last_command] = frame_count;

        if (original_funcs.contains(last_command)) {
            static auto last_log_time = std::chrono::high_resolution_clock::time_point{};
            const auto now = std::chrono::high_resolution_clock::now();
            
            if (now - last_log_time > std::chrono::seconds(1)) {
                SPDLOG_WARN("Something strange is going on, the function is already hooked, maybe previous frame was not rendered?");
                last_log_time = now;
            }

            return;
        }

        static auto func_override = (sdk::FRHICommandBase_Old::Func)+[](sdk::FRHICommandListBase* cmd_list, sdk::FRHICommandBase_Old* cmd) {
            std::scoped_lock _{func_mutex};
            //std::scoped_lock __{VR::get()->get_vr_mutex()};

            static bool once = true;

            if (once) {
                SPDLOG_INFO("[ISceneViewExtension] Successfully hijacked command list!");
                once = false;
            }

            auto& vr = VR::get();
            auto runtime = vr->get_runtime();

            const auto func = original_funcs[cmd];
            const auto frame_count = cmd_frame_counts[cmd];

            runtime->enqueue_render_poses(frame_count);
            runtime->on_pre_render_rhi_thread(frame_count);

            auto call_orig = [&]() {
                func(*cmd_list, cmd);
            };

            cmd->func = func;
            original_funcs.erase(cmd);
            cmd_frame_counts.erase(cmd);

            RHIThreadWorker::get().execute();

            if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
                if (runtime->is_openxr()) {
                    if (g_framework->get_renderer_type() == Framework::RendererType::D3D11) {
                        if (!runtime->got_first_sync || runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                            return call_orig();
                        }  
                    } else if (runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                        return call_orig();
                    }

                    vr->get_openxr_runtime()->begin_frame();
                } else {
                    if (runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                        return call_orig();
                    }
                }
            }

            return call_orig();
        };

        original_funcs[last_command] = last_command->func;
        last_command->func = func_override;
    }
};

template<int N>
void SceneViewExtensionAnalyzer::FillVtable<N>::fill(std::array<uintptr_t, 50>& table) {
    table[N] = (uintptr_t)&SceneViewExtensionAnalyzer::analysis_dummy_stage1<N>;
    FillVtable<N - 1>::fill(table);
}

template<int N>
void SceneViewExtensionAnalyzer::FillVtable<N>::fill2(std::array<uintptr_t, 50>& table) {
    table[N] = (uintptr_t)&SceneViewExtensionAnalyzer::analysis_dummy_stage2<N>;
    FillVtable<N - 1>::fill2(table);
}

// 4.25something to 4.27
// TODO: Add support for all versions via PDB dumps
constexpr auto INIT_OPTIONS_OFFSET = 0x50;

bool FFakeStereoRenderingHook::is_in_viewport_client_draw() const {
    return m_in_viewport_client_draw && GameThreadWorker::get().is_same_thread();
}

// FSceneView constructor hook
sdk::FSceneView* FFakeStereoRenderingHook::sceneview_constructor(sdk::FSceneView* view, sdk::FSceneViewInitOptions* init_options, void* a3, void* a4) {
    SPDLOG_INFO_ONCE("Called FSceneView constructor for the first time");

    auto& vr = VR::get();

    if (!g_hook->is_in_viewport_client_draw() || !vr->is_hmd_active()) {
        return g_hook->m_sceneview_data.constructor_hook.unsafe_call<sdk::FSceneView*>(view, init_options, a3, a4);
    }

    if (g_hook->m_analyzing_view_extensions || !g_hook->m_has_view_extensions_installed) {
        SPDLOG_INFO_ONCE("FSceneView constructor was called before view extensions were installed, aborting");
        return g_hook->m_sceneview_data.constructor_hook.unsafe_call<sdk::FSceneView*>(view, init_options, a3, a4);
    }

    std::scoped_lock ___{g_hook->m_sceneview_data.mtx};

    const auto is_ue5 = g_hook->has_double_precision();
    auto init_options_ue5 = (sdk::FSceneViewInitOptionsUE5*)init_options;

    const auto has_valid_svsi = is_ue5 ? init_options_ue5->scene_view_state != nullptr : init_options->scene_view_state != nullptr;

    if (has_valid_svsi) {
        if (is_ue5) {
            auto& vio_entry = g_hook->m_sceneview_data.view_init_options_ue5[init_options_ue5->scene_view_state];
            memcpy(&vio_entry, init_options, sizeof(sdk::FSceneViewInitOptionsUE5));
        } else {
            auto& vio_entry = g_hook->m_sceneview_data.view_init_options_ue4[init_options->scene_view_state];
            memcpy(&vio_entry, init_options, sizeof(sdk::FSceneViewInitOptionsUE4));
        }
    }

    auto& known_scene_states = g_hook->m_sceneview_data.known_scene_states;
    auto& last_frame_count = g_hook->m_sceneview_data.last_frame_count;
    auto& last_index = g_hook->m_sceneview_data.last_index;

    if (last_frame_count != g_frame_count || last_index > 1) {
        last_index = 0;
    }

    last_frame_count = g_frame_count;

    const auto true_index = vr->is_using_afr() ? (g_frame_count + last_index) % 2 : last_index;

    if (vr->is_splitscreen_compatibility_enabled() || vr->is_sceneview_compatibility_enabled()) {
        int32_t w = vr->get_hmd_width();
        int32_t h = vr->get_hmd_height();

        int32_t x = 0;
        int32_t y = 0;

        if (!vr->is_using_afr() && true_index == 1) {
            x += w;
        }

        FIntRect view_rect{x, y, x + w, y + h};

        vr->get_runtime()->update_matrices(0.1f, 10000.0f);

        const auto proj_mat = vr->get_projection_matrix((VRRuntime::Eye)(true_index));

        auto& init_options_view_origin = is_ue5 ? *(glm::vec3*)&init_options_ue5->view_origin : init_options->view_origin;
        auto& init_options_view_rect = is_ue5 ? init_options_ue5->view_rect : init_options->view_rect;
        auto& init_options_constrained_view_rect = is_ue5 ? init_options_ue5->constrained_view_rect : init_options->constrained_view_rect;
        auto& init_options_projection_matrix = init_options->projection_matrix;
        auto& init_options_projection_matrix_ue5 = init_options_ue5->projection_matrix;

        auto& init_options_view_rotation_matrix = init_options->view_rotation_matrix;
        auto& init_options_view_rotation_matrix_ue5 = init_options_ue5->view_rotation_matrix;

        const auto conversion_mat = glm::mat4 {
            0, 0, 1, 0,
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1
        };

        const auto conversion_mat_inverse = glm::inverse(conversion_mat);

        // We need to "undo" the operations done to create the rotation matrix so we can get the original angle
        // const auto view_rot_mat = conversion_mat * make_inverse_rot_matrix(euler); <-- this is the result of the conversion
        glm::vec3 euler{};

        if (is_ue5) {
            euler = utility::math::ue_euler_from_rotation_matrix(glm::inverse(conversion_mat_inverse * glm::mat4{init_options_view_rotation_matrix_ue5}));
        } else {
            euler = utility::math::ue_euler_from_rotation_matrix(glm::inverse(conversion_mat_inverse * init_options_view_rotation_matrix));
        }

        auto euler_d = glm::vec<3, double>{euler};
        auto euler_pointer = is_ue5 ? (Rotator<float>*)&euler_d : (Rotator<float>*)&euler;

        g_hook->calculate_stereo_view_offset_(true_index + 1, euler_pointer, 100.0f, &init_options_view_origin);

        if (is_ue5) {
            euler = euler_d;
        }

        const auto view_rot_mat = conversion_mat * utility::math::ue_inverse_rotation_matrix(euler);

        *(FIntRect*)&init_options_view_rect = view_rect;
        *(FIntRect*)&init_options_constrained_view_rect = view_rect;

        if (is_ue5) {
            init_options_view_rotation_matrix_ue5 = view_rot_mat;

            if (!vr->is_using_2d_screen()) {
                init_options_projection_matrix_ue5 = proj_mat;
            }
        } else {
            init_options_view_rotation_matrix = view_rot_mat;

            if (!vr->is_using_2d_screen()) {
                init_options_projection_matrix = proj_mat;
            }
        }

        //init_options_stereo_pass = 0;
    }

    auto& init_options_scene_state = is_ue5 ? init_options_ue5->scene_view_state : init_options->scene_view_state;
    auto& init_options_stereo_pass = is_ue5 ? init_options_ue5->stereo_pass : init_options->stereo_pass;

    bool new_scene_state_inserted_this_frame = false;

    if (init_options_scene_state != nullptr && !g_hook->m_sceneview_data.known_scene_states.contains(init_options_scene_state)) {
        SPDLOG_INFO("Inserting new scene state {:x}", (uintptr_t)init_options_scene_state);
        known_scene_states.insert(init_options_scene_state);
        new_scene_state_inserted_this_frame = true;
    } else if (init_options_scene_state == nullptr) {
        SPDLOG_ERROR_ONCE("Scene state passed to FSceneView constructor is null");

        if ((int32_t)init_options_stereo_pass < 0) {
            SPDLOG_ERROR_ONCE("Stereo pass is negative");
        }
    }

    if (init_options_scene_state != nullptr && !new_scene_state_inserted_this_frame && vr->is_ghosting_fix_enabled() && !known_scene_states.empty() && vr->is_using_afr() && true_index == 1) {
        init_options_stereo_pass = 1;

        // Set the scene state to the one that isn't the current one
        for (auto scene_state : known_scene_states) {
            if (scene_state != init_options_scene_state) {
                SPDLOG_INFO_ONCE("Setting scene state to {:x}", (uintptr_t)scene_state);
                init_options_scene_state = scene_state;
                break;
            }
        }
    }

    last_index++;

    return g_hook->m_sceneview_data.constructor_hook.unsafe_call<sdk::FSceneView*>(view, init_options, a3, a4);
}

void FFakeStereoRenderingHook::setup_view_family(ISceneViewExtension* extension, FSceneViewFamily& view_family) {
    ZoneScopedN("SetupViewFamily");

    static bool once = true;

    if (once) {
        SPDLOG_INFO("Called SetupViewFamily for the first time");
        once = false;
    }

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    //vr->update_hmd_state(true, vr->get_runtime()->internal_frame_count + 1);
}

void FFakeStereoRenderingHook::setup_viewpoint(ISceneViewExtension* extension, void* player_controller, void* view_info) {
    ZoneScopedN("SetupViewPoint");
    SPDLOG_INFO_ONCE("Called SetupViewPoint for the first time");

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    auto& vr = VR::get();

    if (!vr->is_ghosting_fix_enabled() || g_hook->m_fixed_localplayer_view_count) {
        return;
    }

    // Using this as a way to get to the localplayer
    static bool attempted_hook{false};

    // Fix localplayer view count
    if (!attempted_hook) {
        SPDLOG_INFO("Attempting to find caller of ISceneViewExtension::SetupViewPoint");

        attempted_hook = true;
        const auto return_address = (uintptr_t)_ReturnAddress();
        const auto caller = utility::find_virtual_function_start(return_address);

        if (!caller) {
            SPDLOG_ERROR("Failed to find caller of ISceneViewExtension::SetupViewPoint");
            return;
        }

        g_hook->m_localplayer_get_viewpoint_hook = safetyhook::create_inline(*caller, (uintptr_t)&localplayer_setup_viewpoint);
        
        if (!g_hook->m_localplayer_get_viewpoint_hook) {
            SPDLOG_ERROR("Failed to hook ISceneViewExtension::SetupViewPoint");
            return;
        }

        SPDLOG_INFO("Hooked ISceneViewExtension::SetupViewPoint");
    }
}

void FFakeStereoRenderingHook::localplayer_setup_viewpoint(void* localplayer, void* view_info, void* pass) {
    ZoneScopedN("LocalPlayerSetupViewPoint");
    SPDLOG_INFO_ONCE("Called LocalPlayerSetupViewPoint for the first time");

    if (!g_hook->m_fixed_localplayer_view_count) {
        static bool attempted = false;

        if (!attempted) {
            attempted = true;

            if (localplayer != nullptr && !IsBadReadPtr(localplayer, sizeof(void*))) try {
                g_hook->post_init_properties((uintptr_t)localplayer);
            } catch(...) {
                SPDLOG_ERROR("[LocalPlayerSetupViewPoint] Failed to post init properties");
            }
        }
    }

    g_hook->m_localplayer_get_viewpoint_hook.call<void>(localplayer, view_info, pass);
}

void FFakeStereoRenderingHook::begin_render_viewfamily(ISceneViewExtension* extension, FSceneViewFamily& view_family) {
    ZoneScopedN("BeginRenderViewFamily");

    SPDLOG_INFO_ONCE("Called BeginRenderViewFamily for the first time");

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    if (!g_hook->has_engine_tick_hook()) {
        // Alternative place of running game thread work.
        GameThreadWorker::get().execute();
    }

    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    const auto frame_count = *(uint32_t*)((uintptr_t)&view_family + SceneViewExtensionAnalyzer::frame_count_offset);
    //vr->update_hmd_state(true, frame_count);
    auto runtime = vr->get_runtime();
    runtime->internal_frame_count = frame_count;
    runtime->on_pre_render_game_thread(frame_count);

    // This is a HACKHACKHACK to get splitscreen working on around 4.20 to 4.27 something
    // This is completely borked on UE5
    // We can probably do it better inside the sceneview constructor hook, but that needs to be handled with care
    if (vr->is_splitscreen_compatibility_enabled()) {
        // B = dst, A = src
        static auto copy_init_options_from = [](const sdk::FSceneView& a, sdk::FSceneView& b) {
            std::scoped_lock _{g_hook->m_sceneview_data.mtx};
            auto init_options_a = (sdk::FSceneViewInitOptions*)((uintptr_t)&a + INIT_OPTIONS_OFFSET);
            auto init_options_b = (sdk::FSceneViewInitOptions*)((uintptr_t)&b + INIT_OPTIONS_OFFSET);

            auto& cached_init_options = g_hook->m_sceneview_data.view_init_options_ue4;

            if (auto it = cached_init_options.find(init_options_a->scene_view_state); it != cached_init_options.end()) {
                const auto& vio_entry = it->second;
                //memcpy(init_options_b, &vio_entry, sizeof(sdk::FSceneViewInitOptionsUE4));
                init_options_b->view_origin = vio_entry.view_origin;
                init_options_b->view_rotation_matrix = vio_entry.view_rotation_matrix;
                *(FIntRect*)&init_options_b->view_rect = *(FIntRect*)&vio_entry.view_rect;
                *(FIntRect*)&init_options_b->constrained_view_rect = *(FIntRect*)&vio_entry.constrained_view_rect;
                init_options_b->projection_matrix = vio_entry.projection_matrix;
                return;
            }

            // Otherwise just do this crap
            init_options_b->view_origin = init_options_a->view_origin;
            init_options_b->view_rotation_matrix = init_options_a->view_rotation_matrix;
            *(FIntRect*)&init_options_b->view_rect = *(FIntRect*)&init_options_a->view_rect;
            *(FIntRect*)&init_options_b->constrained_view_rect = *(FIntRect*)&init_options_a->constrained_view_rect;
            init_options_b->projection_matrix = init_options_a->projection_matrix;
        };

        auto do_splitscreen = [&](int32_t view_index) {
            int32_t w = vr->get_hmd_width();
            int32_t h = vr->get_hmd_height();

            int32_t x = 0;
            int32_t y = 0;

            const auto true_index = vr->is_using_afr() ? (frame_count + 1) % 2 : view_index;

            if (!vr->is_using_afr() && true_index == 1) {
                x += w;
            }

            auto view = view_family.views.data[view_index % view_family.views.count];

            FIntRect view_rect{x, y, x + w, y + h};

            auto& vr = VR::get();

            VR::get()->get_runtime()->update_matrices(0.1f, 10000.0f);

            const auto proj_mat = VR::get()->get_projection_matrix((VRRuntime::Eye)(true_index));

            std::array<uint8_t, 0x500> init_options_copy{};

            auto init_options = (sdk::FSceneViewInitOptions*)((uintptr_t)view + INIT_OPTIONS_OFFSET);

            auto& init_options_view_origin = init_options->view_origin;
            auto& init_options_view_rotation_matrix = init_options->view_rotation_matrix;
            auto& init_options_view_rect = *(FIntRect*)&init_options->view_rect;
            auto& init_options_constrained_view_rect = *(FIntRect*)&init_options->constrained_view_rect;
            auto& init_options_projection_matrix = init_options->projection_matrix;
            auto& init_options_stereo_pass = init_options->stereo_pass;

            // ADDENDUM: The sceneview constructor hook handles the rotation logic now.
            /*const auto conversion_mat = glm::mat4 {
                0, 0, 1, 0,
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 0, 1
            };

            const auto conversion_mat_inverse = glm::inverse(conversion_mat);*/

            // We need to "undo" the operations done to create the rotation matrix so we can get the original angle
            // const auto view_rot_mat = conversion_mat * make_inverse_rot_matrix(euler); <-- this is the result of the conversion
            //auto euler = utility::math::ue_euler_from_rotation_matrix(glm::inverse(conversion_mat_inverse * init_options_view_rotation_matrix));
            //g_hook->calculate_stereo_view_offset_(true_index + 1, (Rotator<float>*)&euler, 100.0f, &init_options_view_origin);
            //const auto view_rot_mat = conversion_mat * utility::math::ue_inverse_rotation_matrix(euler);
            //init_options_view_rotation_matrix = view_rot_mat;

            init_options_view_rect = view_rect;
            init_options_constrained_view_rect = view_rect;
            init_options_projection_matrix = proj_mat;

            memcpy(init_options_copy.data(), init_options, 0x500);
            view->constructor((sdk::FSceneViewInitOptions*)init_options_copy.data()); // Triggers our hook as well
        };

        const auto requested_index = vr->get_requested_splitscreen_index();
        const auto final_index = std::min<uint32_t>(view_family.views.count - 1, requested_index);
        const auto other_index = final_index != 0 ? 0 : 1;

        if (final_index > 0) {
            if (view_family.views.count > 1) {
                copy_init_options_from(*view_family.views.data[final_index], *view_family.views.data[other_index]);
            }

            if (!vr->is_using_afr()) {
                if (view_family.views.count > 1) {
                    do_splitscreen(other_index);
                } else {
                    do_splitscreen(0);
                }
            } else {
                do_splitscreen(0);
            }
        }
    }

    // If we couldn't find GetDesiredNumberOfViews, we need to set the view count to 1 as a workaround
    // TODO: Check if this can cause a memory leak, I don't know who is resonsible
    // for destroying the views in the array
    // This check might seem kind of arbitrary, but sometimes (rarely) the offset
    // for the views can be wrong so if the count is some sane number
    // then we can assume that the offset is correct
    if (vr->is_using_afr() && view_family.views.count >= 2 && view_family.views.count <= 4) {
        SPDLOG_INFO_ONCE("Setting view count to 1 (from {})", view_family.views.count);
        view_family.views.count = 1;
    }
}

void FFakeStereoRenderingHook::pre_render_viewfamily_renderthread(ISceneViewExtension* extension, sdk::FRHICommandListBase* cmd_list, FSceneViewFamily& view_family) {
    ZoneScopedN("PreRenderViewFamily_RenderThread");

    utility::ScopeGuard _{[]() {
        RenderThreadWorker::get().execute();
    }};
    
    SPDLOG_INFO_ONCE("Called PreRenderViewFamily_RenderThread for the first time");
    
    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    if (vr->is_stereo_emulation_enabled()) {
        return;
    }

    const auto frame_count = *(uint32_t*)((uintptr_t)&view_family + SceneViewExtensionAnalyzer::frame_count_offset);

    static bool is_ue5_rdg_builder = false;
    static uint32_t ue5_command_offset = 0;
    static bool analyzed_root_already = false;
    static bool is_old_command_base = false;

    if (is_ue5_rdg_builder) {
        cmd_list = *(sdk::FRHICommandListBase**)((uintptr_t)cmd_list + ue5_command_offset);
    }

    const auto compensation = g_hook->get_frame_delay_compensation();

    // Using slate's draw window hook is the safest way to do this without
    // false positives on the command list in this function
    // otherwise we can attempt to use the command list here and hook it
    // in the slate hook, a guaranteed proper command list is passed to the function
    // so we can use that to hook the command list
    // The main inspiration for this is UE5.0.3 because it passes an FRDGBuilder
    // which *does* contain the command list in it, but for whatever reason I can't
    // seem to hook it properly, so I'm using the slate hook instead
    // ADDENDUM: For now, I'm only using the slate hook for UE5.0.3.
    // But I'll use it as a fallback as well for when the command list appears to be empty
    // Reason being the slate hook doesn't appear to run every frame, so it's not a perfect solution
    auto enqueue_poses_on_slate_thread = [&]() {
        g_hook->get_slate_thread_worker()->enqueue([=](FRHICommandListImmediate* command_list) {
            static bool once_slate = true;

            if (once_slate) {
                SPDLOG_INFO("Called enqueued function on the Slate thread for the first time! Frame count: {}", frame_count);
                once_slate = false;
            }

            auto l = (sdk::FRHICommandListBase*)command_list;

            if (l != nullptr && l->root != nullptr && ((uintptr_t)l->root & (sizeof(void*) - 1)) == 0) {
                if (!analyzed_root_already) try {
                    if (utility::get_module_within(*(void**)l->root).value_or(nullptr) == nullptr) {
                        SPDLOG_INFO("Old FRHICommandBase detected");
                        is_old_command_base = true;
                    } else {
                        SPDLOG_INFO("New FRHICommandBase detected");
                    }

                    analyzed_root_already = true;
                } catch(...) {
                    SPDLOG_ERROR("Failed to analyze FRHICommandBase");
                    analyzed_root_already = true;
                }

                if (!is_old_command_base) {
                    SceneViewExtensionAnalyzer::hook_new_rhi_command((sdk::FRHICommandBase_New*)l->root, frame_count + compensation);
                } else {
                    SceneViewExtensionAnalyzer::hook_old_rhi_command((sdk::FRHICommandBase_Old*)l->root, frame_count + compensation);
                }
            } else {
                // welp
                vr->get_runtime()->enqueue_render_poses(frame_count + compensation);
            }
        });
    };

    // okay well I think this evaluates to false all the time
    // but apparently it has been working for a LONG TIME so I'm not going to touch this until after release
    // (the else statement still handles everything... fine?)
    const auto has_good_root = 
        cmd_list != nullptr &&
        ((uintptr_t)cmd_list & 1 == 0) &&
        cmd_list->root != nullptr &&
        ((uintptr_t)cmd_list->root & 1 == 0);

    // Hijack the top command in the command list so we can enqueue the render poses on the RHI thread
    if (has_good_root) {
        SPDLOG_INFO_ONCE("Command list root is good");

        if (!analyzed_root_already) try {
            auto root = cmd_list->root;

            auto analyze_for_ue5 = [&]() {
                // Find the real command list.
                is_ue5_rdg_builder = true;
                const auto rdg_builder = (uintptr_t)cmd_list;

                for (auto i = 0x10; i <= 0x100; i += sizeof(void*)) try {
                    const auto value = *(uintptr_t*)(rdg_builder + i);

                    if (value == 0 || IsBadReadPtr((void*)value, sizeof(void*))) {
                        continue;
                    }

                    if (utility::get_module_within((void*)value).has_value()) {
                        continue;
                    }

                    const auto value_deref = *(uintptr_t*)value;

                    if (value_deref == 0 || IsBadReadPtr((void*)value_deref, sizeof(void*))) {
                        continue;
                    }

                    if (utility::get_module_within((void*)value_deref).has_value()) {
                        continue;
                    }

                    const auto root_vtable = *(uintptr_t*)value_deref;

                    if (root_vtable == 0 || IsBadReadPtr((void*)root_vtable, sizeof(void*))) {
                        continue;
                    }

                    if (!utility::get_module_within((void*)root_vtable).has_value()) {
                        continue;
                    }

                    // Check that there is a valid function in the vtable
                    const auto first_function = *(uintptr_t*)root_vtable;

                    if (first_function == 0 || IsBadReadPtr((void*)first_function, sizeof(void*))) {
                        continue;
                    }

                    if (!utility::get_module_within((void*)first_function).has_value()) {
                        continue;
                    }

                    SPDLOG_INFO("Possible UE5 command list found at offset 0x{:x}", i);
                    ue5_command_offset = i;
                    cmd_list = (sdk::FRHICommandListBase*)value;
                    break;
                } catch(...) {
                    spdlog::error("Exception occurred while analyzing UE5 command list");
                }
            };

            // If we read the pointer at the start of the root and it's not a module, then it's the old FRHICommandBase
            // this is because all vtables reside within a module
            if (utility::get_module_within(*(void**)root).value_or(nullptr) == nullptr) {
                // UE5
                if (g_hook->has_double_precision()) {
                    analyze_for_ue5();

                    if (ue5_command_offset == 0) {
                        SPDLOG_ERROR("Failed to find UE5 command list, trying again next frame");
                        return;
                    }
                } else {
                    SPDLOG_INFO("Old FRHICommandBase detected");
                    is_old_command_base = true;
                }
            } else {
                SPDLOG_INFO("New FRHICommandBase detected");
            }

            analyzed_root_already = true;
        } catch(...) {
            SPDLOG_ERROR("Failed to analyze root command");
            analyzed_root_already = true;
        }

        if (g_hook->get_render_target_manager()->is_ue_5_0_3() && g_hook->has_slate_hook()) {
            enqueue_poses_on_slate_thread();
        } else try {
            if (!is_old_command_base) {
                SceneViewExtensionAnalyzer::hook_new_rhi_command((sdk::FRHICommandBase_New*)cmd_list->root, frame_count + compensation);
            } else {
                SceneViewExtensionAnalyzer::hook_old_rhi_command((sdk::FRHICommandBase_Old*)cmd_list->root, frame_count + compensation);
            }
        } catch(...) {
            SPDLOG_INFO_ONCE("Failed to hook command list, falling back to Slate thread hook");

            if (g_hook->has_slate_hook()) {
                enqueue_poses_on_slate_thread();
            } else {
                vr->get_runtime()->enqueue_render_poses(frame_count + compensation);
            }
        }
    } else {
        SPDLOG_INFO_ONCE("Bad root or command list, falling back to Slate thread hook");

        // welp v2
        if (g_hook->has_slate_hook()) {
            enqueue_poses_on_slate_thread();
        } else {
            vr->get_runtime()->enqueue_render_poses(frame_count + compensation);
        }
    }
}

bool FFakeStereoRenderingHook::setup_view_extensions() try {
    SPDLOG_INFO("Attempting to set up view extensions...");

    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        SPDLOG_ERROR("Failed to get engine pointer! Cannot set up view extensions!");
        return false;
    }

    const auto active_stereo_device = locate_active_stereo_rendering_device();

    if (!active_stereo_device || !s_stereo_rendering_device_offset) {
        SPDLOG_ERROR("Failed to locate active stereo rendering device!");
        return false;
    }

    // This is a proof of concept at the moment for newer UE versions
    // older versions may not work or crash.
    // TODO: Figure out older versions.
    constexpr auto weak_ptr_size = sizeof(TWeakPtr<void*>);
    static const auto potential_hmd_device_offset = s_stereo_rendering_device_offset + weak_ptr_size;
    static const uintptr_t potential_hmd_device = (uintptr_t)engine + potential_hmd_device_offset;
    static const uintptr_t potential_view_extensions = (uintptr_t)engine + s_stereo_rendering_device_offset + (weak_ptr_size * 2); // 2 to skip over the XRSystem

    // This can happen if the game left a VR plugin in it
    // Usually this isn't an issue, but some games can leave a valid HMDDevice or XRSystem laying around for whatever reason
    // If this isn't cleaned up, the game will crash because it tries to gather view extensions from the existing device
    // and the view extensions it gathered will cause a crash when calling them. also the HMD device itself can cause a crash, it's not actually initialized.
    if (*(void**)potential_hmd_device != nullptr) {
        // Double check that we're actually replacing a pointer and not an integer or something
        if (!IsBadReadPtr(*(void**)potential_hmd_device, sizeof(void*))) {
            SPDLOG_INFO("Found an existing HMDDevice or XRSystem, nullifying it...");
            static std::vector<uintptr_t> replacement_vtable{};

            for (auto i = 0; i < 200; ++i) {
                replacement_vtable.push_back((uintptr_t)+[]() { return nullptr; });
            }

            //**(void***)potential_hmd_device = replacement_vtable.data();
            *(void**)potential_hmd_device = nullptr;
            m_fixed_localplayer_view_count = true; // If this is already allocated, then there's already a second view for us to use
        }

        if (!IsBadReadPtr(*(void**)(potential_hmd_device + sizeof(void*)), sizeof(void*))) {
            *(void**)(potential_hmd_device + sizeof(void*)) = nullptr;
        }
    }

    m_tracking_system_hook = std::make_unique<IXRTrackingSystemHook>(this, potential_hmd_device_offset);

    // Add a vectored exception handler that catches attempted dereferences of a null XRSystem or HMDDevice
    // The exception handler will then patch out the instructions causing the crash and continue execution
    static std::vector<Patch::Ptr> xrsystem_patches{};
    static std::unordered_set<uintptr_t> ignored_addresses{};

    AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS exception) -> LONG {
        if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            const auto exception_address = exception->ContextRecord->Rip;

            if (ignored_addresses.contains(exception_address)) {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            ignored_addresses.insert(exception_address);

            if (exception_address == 0) {
                SPDLOG_INFO("[Exception Handler] Exception address is null");
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (IsBadReadPtr((void*)exception_address, sizeof(void*))) {
                SPDLOG_INFO("[Exception Handler] Bad read pointer at {:x}", exception_address);
                return EXCEPTION_CONTINUE_SEARCH;
            }

            const auto decoded = utility::decode_one((uint8_t*)exception_address);

            if (!decoded) {
                SPDLOG_ERROR("[Exception Handler] Failed to decode instruction at {:x}", exception_address);
                return EXCEPTION_CONTINUE_SEARCH;
            }

            const auto& op2 = decoded->Operands[1];

            if (decoded->OperandsCount != 2 || 
                 op2.Type != ND_OP_MEM      || 
                !op2.Info.Memory.HasBase)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            SPDLOG_INFO("Encountered attempted dereference of null pointer at {:x}", exception_address);

            // Get the start of the previous instruction
            const auto previous_instruction = utility::resolve_instruction(exception_address - 1);

            if (!previous_instruction) {
                SPDLOG_ERROR("Could not resolve previous instruction at {:x}", exception_address - 1);
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (previous_instruction->instrux.Operands[0].Type != ND_OP_REG ||
                previous_instruction->instrux.Operands[0].Info.Register.Reg != op2.Info.Memory.Base)
            {
                SPDLOG_ERROR("Previous instruction does not use the same register as the dereference");
                return EXCEPTION_CONTINUE_SEARCH;
            }

            const auto prev_op2 = previous_instruction->instrux.Operands[1];

            if (previous_instruction->instrux.OperandsCount < 2 ||
                prev_op2.Type != ND_OP_MEM ||
                !prev_op2.Info.Memory.HasBase)
            {
                SPDLOG_ERROR("Previous instruction is not a memory dereference");
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (!prev_op2.Info.Memory.HasDisp) {
                SPDLOG_ERROR("Previous instruction does not have a displacement");
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (prev_op2.Info.Memory.Disp != potential_hmd_device_offset) {
                SPDLOG_ERROR("Previous instruction is not the XRSystem or HMDDevice dereference");
                return EXCEPTION_CONTINUE_SEARCH;
            }

            SPDLOG_INFO("Found the dereference of the XRSystem or HMDDevice at {:x}", previous_instruction->addr);

            // Patch the initial instruction that caused the crash
            SPDLOG_INFO("Creating first patch...");

            std::vector<int16_t> first_patch{};

            for (auto i = 0; i < decoded->Length; ++i) {
                first_patch.push_back(0x90);
            }

            xrsystem_patches.push_back(Patch::create(exception_address, first_patch));

            const auto next_instruction_addr = exception_address + decoded->Length;
            const auto next_instruction = utility::decode_one((uint8_t*)next_instruction_addr);

            if (!next_instruction) {
                SPDLOG_ERROR("Could not decode next instruction at {:x}", exception_address + decoded->Length);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            if (!std::string_view{next_instruction->Mnemonic}.starts_with("CALL")) {
                SPDLOG_ERROR("Next instruction is not a call, continuing anyways since we patched the dereference");
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // Patch the next instruction if it's a call
            SPDLOG_INFO("Creating second patch...");

            std::vector<int16_t> second_patch{};

            for (auto i = 0; i < next_instruction->Length; ++i) {
                second_patch.push_back(0x90);
            }

            xrsystem_patches.push_back(Patch::create(next_instruction_addr, second_patch));

            SPDLOG_INFO("Finished creating patches, continuing execution. Hopefully we don't crash...");
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    });

    // The TWeakPtr version is for >= 4.11 UE versions
    TWeakPtr<FSceneViewExtensions>& view_extensions_tweakptr = 
        *(TWeakPtr<FSceneViewExtensions>*)potential_view_extensions;

    // This means it's an old version of UE
    // so the view extensions are a TArray and not a TWeakPtr<TArray>
    if (!m_rendertarget_manager_embedded_in_stereo_device) {
        if (view_extensions_tweakptr.reference == nullptr) {
            view_extensions_tweakptr.allocate_naive();
        }
    }

    FSceneViewExtensions& view_extensions = m_rendertarget_manager_embedded_in_stereo_device ?  
                                            *(FSceneViewExtensions*)potential_view_extensions : *view_extensions_tweakptr.reference;

    SPDLOG_INFO("Current ext ptr: {:x}", (uintptr_t)view_extensions.extensions.data);
    SPDLOG_INFO("Current ext count: {}", view_extensions.extensions.count);
    SPDLOG_INFO("Current ext capacity: {}", view_extensions.extensions.capacity);

    // Verifications on the current memory of the FSceneViewExtensions, because pre-4.10 (?) the view extensions array did not actually exist
    if (m_rendertarget_manager_embedded_in_stereo_device) {
        SPDLOG_INFO("Performing verifications on the current memory of the FSceneViewExtensions...");

        const auto& current_view_extensions_ptr_value = view_extensions.extensions;

        // Check if current value is non zero and points to invalid memory
        if (current_view_extensions_ptr_value.data != nullptr && IsBadReadPtr((void*)current_view_extensions_ptr_value.data, sizeof(void*))) {
            SPDLOG_ERROR("Usual view extensions pointer is non-zero but points to invalid memory! Cannot set up view extensions!");
            SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
            return false;
        }

        // Check if count is greater than capacity, which is not possible
        if ((uint32_t)current_view_extensions_ptr_value.count > (uint32_t)current_view_extensions_ptr_value.capacity) {
            SPDLOG_ERROR("Usual view extensions count is greater than capacity! Cannot set up view extensions!");
            SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
            return false;
        }

        // Check if count or capacity is negative, which is not possible
        if ((int32_t)current_view_extensions_ptr_value.count < 0 || (int32_t)current_view_extensions_ptr_value.capacity < 0) {
            SPDLOG_ERROR("Usual view extensions count or capacity is negative! Cannot set up view extensions!");
            SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
            return false;
        }
        
        // Check if the memory at count treated as a pointer points to valid memory, which is not possible
        const auto count_as_ptr = *(void**)&current_view_extensions_ptr_value.count;
        if (count_as_ptr != nullptr && !IsBadReadPtr(count_as_ptr, sizeof(void*))) {
            SPDLOG_ERROR("Usual view extensions count is actually a pointer to valid memory! Cannot set up view extensions!");
            SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
            return false;
        }

        // Check if the data pointer is null but capacity is greater than 0, which is not possible
        if (current_view_extensions_ptr_value.data == nullptr && current_view_extensions_ptr_value.capacity > 0) {
            SPDLOG_INFO("Usual view extensions data pointer is null but capacity is greater than 0! Cannot set up view extensions!");
            SPDLOG_INFO("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
        }

        // Check if the data pointer is non-null but the capacity is 0, which is not possible
        if (current_view_extensions_ptr_value.data != nullptr && current_view_extensions_ptr_value.capacity == 0) {
            SPDLOG_ERROR("Usual view extensions data pointer is non-null but capacity is 0! Cannot set up view extensions!");
            SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
            return false;
        }

        // Check if any current entries in the array within the count are invalid, which is not possible
        if (current_view_extensions_ptr_value.data != nullptr) {
            for (auto i = 0; i < current_view_extensions_ptr_value.count; ++i) {
                const auto ext = current_view_extensions_ptr_value.data[i].reference;

                if (IsBadReadPtr((void*)ext, sizeof(void*))) {
                    SPDLOG_ERROR("Usual view extensions array contains an invalid entry! Cannot set up view extensions!");
                    SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
                    return false;
                }

                const auto ext_vtable = *(void**)ext;

                if (IsBadReadPtr((void*)ext_vtable, sizeof(void*))) {
                    SPDLOG_ERROR("Usual view extensions array contains an entry with an invalid vtable! Cannot set up view extensions!");
                    SPDLOG_ERROR("This may mean that the UE version is very old and this method of hooking the view extensions is not supported.");
                    return false;
                }
            }
        }
    }

    // Allocate a completely new array if the current one is null or empty
    if (view_extensions.extensions.data == nullptr || view_extensions.extensions.data[0].reference == nullptr || view_extensions.extensions.count == 0) {
        SPDLOG_INFO("Allocating new view extensions array...");

        auto& exts = view_extensions.extensions;

        // Allocate a bunch more than necessary to prevent crashes when the engine tries to add new entries
        const auto new_capacity = 32;

        exts.data = new TWeakPtr<ISceneViewExtension>[new_capacity]{};
        exts.count = 0;
        exts.capacity = new_capacity;

        ZeroMemory(exts.data, sizeof(TWeakPtr<ISceneViewExtension>) * new_capacity);
        exts.data[exts.count++].allocate_naive();
    } else if (view_extensions.extensions.data != nullptr && view_extensions.extensions.count <= view_extensions.extensions.capacity) {
        auto& exts = view_extensions.extensions;

        // TODO: Use FMemory::Realloc (or whatever its called) instead of new/delete cuz game crashes when reallocating/closing the game
        if (exts.count == exts.capacity) {
            SPDLOG_INFO("Extending view extensions array...");

            const auto new_capacity = exts.capacity * 4;
            const auto old_capacity = exts.capacity;
            auto new_exts = new TWeakPtr<ISceneViewExtension>[new_capacity];

            ZeroMemory(new_exts, sizeof(TWeakPtr<ISceneViewExtension>) * new_capacity);
            memcpy(new_exts, exts.data, sizeof(TWeakPtr<ISceneViewExtension>) * old_capacity);

            // dont delete it cuz its owned by the games allocator... for now
            //delete[] exts.data;

            exts.data = new_exts;
            exts.capacity = new_capacity;
        } else {
            SPDLOG_INFO("Allocating new view extension entry onto existing array...");
        }

        exts.data[exts.count++].allocate_naive();
    } else {
        SPDLOG_INFO("None of the previous conditions were met, so we're not allocating a new view extensions array");
    }

    if (view_extensions.extensions.count > 0 && view_extensions.extensions.data != nullptr) {
        // Replace the vtable of the first entry
        auto& entry = view_extensions.extensions.data[view_extensions.extensions.count-1];

        if (entry.reference == nullptr) {
            SPDLOG_ERROR("Failed to get first view extension entry!");
            return false;
        }

        auto& vtable = *(uintptr_t**)entry.reference;

        g_hook->m_analyze_view_extensions_start_time = std::chrono::high_resolution_clock::now();
        g_hook->m_analyzing_view_extensions = true;

        if (!m_rendertarget_manager_embedded_in_stereo_device) {
            SceneViewExtensionAnalyzer::FillVtable<g_view_extension_vtable.size()-1>::fill(g_view_extension_vtable);
        } else {
            // Skip straight to stage 2.
            SPDLOG_INFO("Skipping view extension stage 1...");
            SceneViewExtensionAnalyzer::FillVtable<g_view_extension_vtable.size()-1>::fill2(g_view_extension_vtable);
        }

        // Will get called when the view extensions are finally hooked.
        RenderThreadWorker::get().enqueue([this]() {
            this->m_analyzing_view_extensions = false;
            this->m_has_view_extensions_installed = true;
        });

        // overwrite the vtable
        vtable = g_view_extension_vtable.data();
        m_has_view_extension_hook = true;
    } else {
        // TODO: Allocate a new one.
        m_has_view_extension_hook = false;

        SPDLOG_INFO("Failed to set up view extensions! (not yet implemented to allocate a new one)");
    }

    return true;
} catch(...) {
    SPDLOG_ERROR("Unknown exception while setting up view extensions!");
    return false;
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
            SPDLOG_ERROR("Failed to find FFakeStereoRendering constructor");
            return std::nullopt;
        }
    }

    if (!fake_stereo_rendering_constructor) {
        SPDLOG_ERROR("Failed to find FFakeStereoRendering constructor");
        return std::nullopt;
    }

    SPDLOG_INFO("FFakeStereoRendering constructor: {:x}", (uintptr_t)*fake_stereo_rendering_constructor);
    cached_result = *fake_stereo_rendering_constructor;

    return *fake_stereo_rendering_constructor;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_fake_stereo_rendering_vtable() {
    static std::optional<uintptr_t> cached_result{};

    if (cached_result) {
        return cached_result;
    }

    if (g_hook->m_manually_constructed) {
        cached_result = *(uintptr_t*)((uintptr_t)sdk::UGameEngine::get() + s_stereo_rendering_device_offset);
        return cached_result;
    }

    const auto fake_stereo_rendering_constructor = locate_fake_stereo_rendering_constructor();

    if (!fake_stereo_rendering_constructor) {
        // If this happened, then that's bad news, the UE version is probably extremely old
        // so we have to use this fallback method.
        SPDLOG_INFO("Failed to locate FFakeStereoRendering constructor, using fallback method");
        const auto initialize_hmd_device = sdk::UEngine::get_initialize_hmd_device_address();

        if (!initialize_hmd_device) {
            SPDLOG_ERROR("Failed to find FFakeStereoRendering VTable via fallback method");
            return std::nullopt;
        }

        // To be seen if this needs to be adjusted. At first glance it doesn't look very reliable.
        // maybe perform emulation or something in the future?
        const auto instruction = utility::scan_disasm(*initialize_hmd_device, 100, "48 8D 05 ? ? ? ?");

        if (!instruction) {
            SPDLOG_ERROR("Failed to find FFakeStereoRendering VTable via fallback method (2)");
            return std::nullopt;
        }

        const auto result = utility::calculate_absolute(*instruction + 3);

        if (!result) {
            SPDLOG_ERROR("Failed to find FFakeStereoRendering VTable via fallback method (3)");
            return std::nullopt;
        }

        SPDLOG_INFO("FFakeStereoRendering VTable: {:x}", (uintptr_t)result);
        cached_result = result;

        return result;
    }

    const auto vtable_ref = utility::scan(*fake_stereo_rendering_constructor, 100, "48 8D 05 ? ? ? ?");

    if (!vtable_ref) {
        SPDLOG_ERROR("Failed to find FFakeStereoRendering VTable Reference");
        return std::nullopt;
    }

    const auto vtable = utility::calculate_absolute(*vtable_ref + 3);

    if (!vtable) {
        SPDLOG_ERROR("Failed to find FFakeStereoRendering VTable");
        return std::nullopt;
    }

    SPDLOG_INFO("FFakeStereoRendering VTable: {:x}", (uintptr_t)vtable);
    cached_result = vtable;

    return vtable;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_active_stereo_rendering_device() {
    auto engine = (uintptr_t)sdk::UEngine::get();

    if (engine == 0) {
        SPDLOG_ERROR("GEngine does not appear to be instantiated, cannot verify stereo rendering device is setup.");
        return std::nullopt;
    }

    SPDLOG_INFO("Checking engine pointers for StereoRenderingDevice...");
    auto fake_stereo_device_vtable = locate_fake_stereo_rendering_vtable();

    if (!fake_stereo_device_vtable) {
        SPDLOG_ERROR("Failed to locate fake stereo rendering device vtable, cannot verify stereo rendering device is setup.");
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
            SPDLOG_INFO("Reached end of engine pointers at offset {:x}", i);
            break;
        }

        const auto ptr = *(uintptr_t*)addr_of_ptr;

        if (ptr == 0 || IsBadReadPtr((void*)ptr, sizeof(void*))) {
            continue;
        }

        auto potential_vtable = *(uintptr_t*)ptr;

        if (potential_vtable == *fake_stereo_device_vtable) {
            SPDLOG_INFO("Found fake stereo rendering device at offset {:x} -> {:x}", i, ptr);
            s_stereo_rendering_device_offset = i;
            return ptr;
        }
    }

    SPDLOG_ERROR("Failed to find stereo rendering device");
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
            SPDLOG_INFO("VFunc at index {} contains a jmp, resolving...", i);
            func = utility::calculate_absolute(func + 1);
        }

        bool found = false;
        uint32_t xmm_register_usage_count = 0;

        // We do an exhaustive decode (disassemble all possible code paths) that correctly follows the control flow
        // because some games are obfuscated and do huge jumps across gaps of junk code.
        // so we can't just linearly scan forward as the disassembler will fail at some point.
        utility::exhaustive_decode((uint8_t*)func, 50, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
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
            SPDLOG_INFO("Found Stereo View Offset Index: {}", i);
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
    SPDLOG_INFO("Attempting to patch inlined vtable checks...");

    const auto fake_stereo_rendering_constructor = locate_fake_stereo_rendering_constructor();
    const auto fake_stereo_rendering_vtable = locate_fake_stereo_rendering_vtable();

    if (!fake_stereo_rendering_constructor || !fake_stereo_rendering_vtable) {
        SPDLOG_ERROR("Cannot patch vtables, constructor or vtable not found!");
        return false;
    }

    const auto vtable_module_within = utility::get_module_within(*fake_stereo_rendering_vtable);
    const auto module_size = utility::get_module_size(*vtable_module_within);
    const auto module_end = (uintptr_t)*vtable_module_within + *module_size;

    SPDLOG_INFO("{:x} {:x} {:x}", *fake_stereo_rendering_vtable, (uintptr_t)*vtable_module_within, *module_size);

    for (auto ref = utility::scan_displacement_reference(*vtable_module_within, *fake_stereo_rendering_vtable); 
        ref.has_value();
        ref = utility::scan_displacement_reference((uintptr_t)*ref + 4, (module_end - *ref) - sizeof(void*), *fake_stereo_rendering_vtable)) 
    {
        const auto distance_from_constructor = *ref - *fake_stereo_rendering_constructor;

        // We don't want to mess with the one within the constructor.
        if (distance_from_constructor < 0x100) {
            SPDLOG_INFO("Skipping vtable reference within constructor");
            continue;
        }

        // Change the bytes to be some random number
        // this causes the vtable check to fail and will call the function within the vtable.
        DWORD old{};
        VirtualProtect((void*)*ref, 4, PAGE_EXECUTE_READWRITE, &old);
        *(uint32_t*)*ref = 0x12345678;
        VirtualProtect((void*)*ref, 4, old, &old);
        SPDLOG_INFO("Patched vtable check at {:x}", (uintptr_t)*ref);
    }

    SPDLOG_INFO("Finished patching inlined vtable checks.");
    return true;
}

bool FFakeStereoRenderingHook::attempt_runtime_inject_stereo() {
    // This attempts to create a new StereoRenderingDevice in the GEngine
    // if it doesn't already exist via using -emulatestereo.
    auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        SPDLOG_ERROR("Failed to locate GEngine, cannot inject stereo rendering device at runtime.");
        return false;
    }

    static auto enable_stereo_emulation_cvar = sdk::vr::get_enable_stereo_emulation_cvar();

    if (!locate_active_stereo_rendering_device()) {
        SPDLOG_INFO("Calling InitializeHMDDevice...");

        //utility::ThreadSuspender _{};

        engine->initialize_hmd_device();

        SPDLOG_INFO("Called InitializeHMDDevice.");

        if (!locate_active_stereo_rendering_device()) {
            SPDLOG_INFO("Previous call to InitializeHMDDevice did not setup the stereo rendering device, attempting to call again...");

            auto patch_emulate_stereo_flag = []() {
                //SPDLOG_ERROR("Failed to locate r.EnableStereoEmulation cvar, next call may fail.");
                SPDLOG_INFO("r.EnableStereoEmulation cvar not found, using fallback method of forcing -emulatestereo flag.");
                
                const auto emulate_stereo_string_ref = sdk::UGameEngine::get_emulatestereo_string_ref_address();

                if (emulate_stereo_string_ref) {
                    const auto resolved = utility::resolve_instruction(*emulate_stereo_string_ref);

                    if (resolved) {
                        // Scan forward for a call instruction, this call checks the command line for "emulatestereo".
                        const auto call = utility::scan_disasm(resolved->addr, 20, "E8 ? ? ? ?");

                        if (call) {
                            // Patch the instruction to mov al, 1
                            SPDLOG_INFO("Patching instruction at {:x} to mov al, 1", (uintptr_t)*call);
                            static auto patch = Patch::create(*call, { 0xB0, 0x01, 0x90, 0x90, 0x90 });
                        }
                    }
                }
            };

            // We don't call this before because the cvar will not be set up
            // until it's referenced once. after we set this we need to call the function again.
            if (enable_stereo_emulation_cvar) {
                try {
                    enable_stereo_emulation_cvar->set<int>(1);
                } catch(...) {
                    SPDLOG_ERROR("Access violation occurred when writing to r.EnableStereoEmulation, the address may be incorrect!");
                    patch_emulate_stereo_flag();
                }
            } else {
                //SPDLOG_ERROR("Failed to locate r.EnableStereoEmulation cvar, next call may fail.");
                patch_emulate_stereo_flag();
            }

            SPDLOG_INFO("Calling InitializeHMDDevice... AGAIN");

            engine->initialize_hmd_device();

            SPDLOG_INFO("Called InitializeHMDDevice again.");
        }

        if (locate_active_stereo_rendering_device()) {
            SPDLOG_INFO("Stereo rendering device setup successfully.");
        } else {
            SPDLOG_ERROR("Failed to setup stereo rendering device.");
            return false;
        }
    } else {
        SPDLOG_INFO("Not necessary to call InitializeHMDDevice, stereo rendering device is already setup.");
        m_fixed_localplayer_view_count = true; // Everything was set up beforehand, we don't need to do anything, so just set it to true.
    }

    return true;
}

bool FFakeStereoRenderingHook::is_stereo_enabled(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("is stereo enabled called!");
#else
    SPDLOG_INFO_ONCE("is stereo enabled called!");
#endif

    // wait!!!
    if (!g_framework->is_game_data_intialized()) {
        return false;
    }

    if (g_hook->m_sceneview_data.inside_post_init_properties) {
        g_hook->set_should_recreate_textures(true);
        return true;
    }
    
    /*if (g_hook->m_analyzing_view_extensions) {
        const auto now = std::chrono::high_resolution_clock::now();

        if (now - g_hook->m_analyze_view_extensions_start_time > std::chrono::seconds(15)) {
            SPDLOG_INFO("Timed out waiting for view extensions to be analyzed.");
            g_hook->m_analyzing_view_extensions = false;
        }

        return false;
    }*/

    static std::atomic<bool> last_state = false;
    auto hook = g_hook;

    // The best way to enable stereo rendering without causing crashes
    // while also allowing the desktop view to initially display
    // if the HMD is not on at the start. It only allows
    // stereo to be enabled if it starts from the first call to IsStereoEnabled inside UGameViewportClient::Draw.
    if (hook->m_has_game_viewport_client_draw_hook) {
        if (GameThreadWorker::get().is_same_thread()) {
            if (hook->m_in_viewport_client_draw && !hook->m_was_in_viewport_client_draw) {
                const auto is_hmd_active = VR::get()->is_hmd_active();

                if (!last_state && is_hmd_active) {
                    VR::get()->wait_for_present();
                    hook->set_should_recreate_textures(true);
                }

                last_state = is_hmd_active;
            }

            hook->m_was_in_viewport_client_draw = hook->m_in_viewport_client_draw;
        }

        return last_state;
    }

    static uint32_t count = 0;

    // Forcefully return true the first few times to let stuff initialize.
    if (count < 50) {
        if (count == 0) {
            hook->set_should_recreate_textures(true);
        }

        ++count;
        last_state = true;
        return true;
    }

    const auto result = !VR::get()->get_runtime()->got_first_sync || VR::get()->is_hmd_active();

    if (result && !last_state) {
        hook->set_should_recreate_textures(true);
    }

    last_state = result;

    return result;
}

void FFakeStereoRenderingHook::adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("adjust view rect called! {}", index);
    SPDLOG_INFO(" x: {}, y: {}, w: {}, h: {}", *x, *y, *w, *h);
#else
    SPDLOG_INFO_ONCE("adjust view rect called! {}", index);
    SPDLOG_INFO_ONCE(" x: {}, y: {}, w: {}, h: {}", *x, *y, *w, *h);
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

    // The purpose of this is to prevent the game from crashing in IDirect3D12CommandList::Close
    // Because the game will try to copy a texture region that is out of bounds.
    if (g_hook->m_skip_next_adjust_view_rect) {
        *x = 0;
        *y = 0;
        *w = std::min<uint32_t>(VR::get()->get_hmd_width(), *w);
        *h = std::min<uint32_t>(VR::get()->get_hmd_height(), *h);
        g_hook->m_skip_next_adjust_view_rect = false;
        g_hook->m_skip_next_adjust_view_rect_count = 1;
        return;
    }

    if (g_hook->m_skip_next_adjust_view_rect_count > 0) {
        *x = 0;
        *y = 0;
        *w = std::min<uint32_t>(VR::get()->get_hmd_width(), *w);
        *h = std::min<uint32_t>(VR::get()->get_hmd_height(), *h);
        --g_hook->m_skip_next_adjust_view_rect_count;
        return;
    }

    if (VR::get()->is_stereo_emulation_enabled()) {
        *w *= 2;
    } else {
        *w = VR::get()->get_hmd_width() * 2;
        *h = VR::get()->get_hmd_height();
    }


    *w = *w / 2;

    const auto true_index = index_starts_from_one ? ((index + 1) % 2) : (index % 2);
    *x += *w * true_index;
}

__forceinline void FFakeStereoRenderingHook::calculate_stereo_view_offset(
    FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation, 
    const float world_to_meters, Vector3f* view_location)
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("calculate stereo view offset called! {}", view_index);
#else
    SPDLOG_INFO_ONCE("calculate stereo view offset called! {}", view_index);
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    auto vr = VR::get();
    //std::scoped_lock _{vr->get_vr_mutex()};

    static bool index_starts_from_one = true;
    static bool index_was_ever_two = false;
    static bool index_was_ever_negative = false;

    if (view_index == -1) {
        index_was_ever_negative = true;
        SPDLOG_INFO_ONCE("calculate stereo view offset called with view index -1 (INDEX_NONE), ignoring.");
        return;
    }

    // This is eSSP_FULL, we don't care. It will cause the view to become monoscopic if we do anything.
    if (index_was_ever_two && view_index == 0) {
        SPDLOG_INFO_ONCE("calculate stereo view offset called with view index 0 after 2, ignoring.");
        return;
    }

    vr->set_world_to_meters(world_to_meters);

    if (view_index == 2) {
        index_starts_from_one = true;
        index_was_ever_two = true;
    } else if (view_index == 0 && !index_was_ever_two) {
        index_starts_from_one = false;
    }

    const auto is_full_pass = view_index == 0 && !index_was_ever_two && !index_was_ever_negative;

    auto true_index = index_starts_from_one ? ((view_index + 1) % 2) : (view_index % 2);
    const auto has_double_precision = g_hook->m_has_double_precision;
    const auto rot_d = (Rotator<double>*)view_rotation;

    if (vr->is_using_afr() && !is_full_pass) {
        true_index = g_frame_count % 2;

        if (!vr->is_using_synchronized_afr()) {
            if (g_hook->m_has_double_precision) {
                if (true_index == 1) {
                    *rot_d = g_hook->m_last_afr_rotation_double;
                } else {
                    g_hook->m_last_afr_rotation_double = *rot_d;
                }
            } else {
                if (true_index == 1) {
                    *view_rotation = g_hook->m_last_afr_rotation;
                } else {
                    g_hook->m_last_afr_rotation = *view_rotation;
                }
            }
        }
    }

    if (true_index == 0 && !is_full_pass) {
        //vr->wait_for_present();
        
        if (!g_hook->m_has_view_extension_hook && !g_hook->m_has_game_viewport_client_draw_hook) {
            vr->update_hmd_state();
        }
    }

    /*if (view_index % 2 == 1 && VR::get()->get_runtime()->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
        std::scoped_lock _{ vr->get_runtime()->render_mtx };
        SPDLOG_INFO("SYNCING!!!");
        //vr->get_runtime()->synchronize_frame();
        vr->update_hmd_state();
    }*/

    // if we were unable to hook UGameEngine::Tick, we can run our game thread jobs here instead.
    if (!is_full_pass && !g_hook->m_has_view_extension_hook && g_hook->m_attempted_hook_game_engine_tick && !g_hook->m_hooked_game_engine_tick) {
        GameThreadWorker::get().execute();
    }

    if (vr->is_sceneview_compatibility_enabled() && !g_hook->m_inside_manual_view_offset) {
        return;
    }

    const auto& mods = g_framework->get_mods()->get_mods();

    if (!is_full_pass) {
        for (auto& mod : mods) {
            mod->on_pre_calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location, g_hook->m_has_double_precision);
        }
    }

    const auto view_d = (Vector3d*)view_location;

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

    auto vqi_norm = glm::normalize(view_quat_inverse);

    // Decoupled Pitch
    if (vr->is_decoupled_pitch_enabled()) {
        vr->set_pre_flattened_rotation(vqi_norm);
        vqi_norm = utility::math::flatten(vqi_norm);
    }

    const auto camera_forward_offset = vr->get_camera_forward_offset();
    const auto camera_right_offset = vr->get_camera_right_offset();
    const auto camera_up_offset = vr->get_camera_up_offset();
    const auto camera_forward = quat_converter * (vqi_norm * glm::vec3{0, 0, camera_forward_offset});
    const auto camera_right = quat_converter * (vqi_norm * glm::vec3{-camera_right_offset, 0, 0});
    const auto camera_up = quat_converter * (vqi_norm * glm::vec3{0, -camera_up_offset, 0});

    const auto world_scale = world_to_meters * vr->get_world_scale();

    if (has_double_precision) {
        *view_d += camera_forward;
        *view_d += camera_right;
        *view_d += camera_up;
    } else {
        *view_location += camera_forward;
        *view_location += camera_right;
        *view_location += camera_up;
    }

    // Don't apply any headset transformations
    // if we have stereo emulation mode enabled
    // it is only for debugging purposes
    if (!vr->is_stereo_emulation_enabled()) {
        const auto is_2d_screen = vr->is_using_2d_screen();

        const auto rotation_offset = vr->get_rotation_offset();
        const auto current_hmd_rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(0)});
        const auto current_eye_rotation_offset = glm::normalize(glm::quat{vr->get_eye_transform(true_index)});

        const auto new_rotation = glm::normalize(vqi_norm * current_hmd_rotation * current_eye_rotation_offset);
        const auto eye_offset = glm::vec3{vr->get_eye_offset((VRRuntime::Eye)(true_index))};


        const auto standing_delta = vr->get_position(0) - vr->get_standing_origin();
        const auto standing_delta_flat = glm::vec3{standing_delta.x, 0, standing_delta.z};

        const auto pos = glm::vec3{rotation_offset * standing_delta};
        const auto pos_flat = glm::vec3{rotation_offset * standing_delta_flat};

        const auto head_offset = quat_converter * (vqi_norm * (pos * world_scale));
        const auto head_offset_flat = quat_converter * (vqi_norm * (pos_flat * world_scale));
        const auto eye_separation = quat_converter * (glm::normalize(new_rotation) * (eye_offset * world_scale));

        if (!has_double_precision) {
            if (!is_2d_screen) {
                *view_location -= head_offset;
            }

            *view_location -= eye_separation;
        } else {
            if (!is_2d_screen) {
                *view_d -= head_offset;
            }

            *view_d -= eye_separation;
        }

        if (!is_2d_screen) {
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
        }

        // Roomscale movement
        // only do it on the right eye pass
        // if we did it on the left, there would be eye desyncs when the right eye is rendered
        if (true_index == 1 && (vr->is_roomscale_enabled() || vr->is_aim_pawn_control_rotation_enabled())) {
            const auto world = sdk::UEngine::get()->get_world();

            if (const auto controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0); controller != nullptr) {
                const auto pawn = controller->get_acknowledged_pawn();

                static bool was_pawn_rotation_enabled = false;

                if (pawn != nullptr && vr->is_aim_pawn_control_rotation_enabled()) {
                    auto camera_component = (sdk::UObject*)pawn->get_camera_component();

                    if (camera_component != nullptr && camera_component->get_class() != nullptr) {
                        static const auto boolprop = (sdk::FBoolProperty*)camera_component->get_class()->find_property(L"bUsePawnControlRotation");

                        if (boolprop != nullptr) {
                            boolprop->set_value_in_object(camera_component, true);
                            was_pawn_rotation_enabled = true;
                        }
                    }
                } else if (pawn != nullptr && was_pawn_rotation_enabled) {
                    auto camera_component = (sdk::UObject*)pawn->get_camera_component();

                    if (camera_component != nullptr && camera_component->get_class() != nullptr) {
                        static const auto boolprop = (sdk::FBoolProperty*)camera_component->get_class()->find_property(L"bUsePawnControlRotation");

                        if (boolprop != nullptr) {
                            boolprop->set_value_in_object(camera_component, false);
                            was_pawn_rotation_enabled = false;
                        }
                    }
                }

                if (pawn != nullptr && vr->is_roomscale_enabled()) {
                    const auto pawn_pos = pawn->get_actor_location();
                    const auto new_pos = pawn_pos - head_offset_flat;

                    pawn->set_actor_location(new_pos, false, false);

                    // Recenter the standing origin
                    auto current_standing_origin = vr->get_standing_origin();
                    const auto hmd_pos = vr->get_position(0);
                    // dont touch the Y axis
                    current_standing_origin.x = hmd_pos.x;
                    current_standing_origin.z = hmd_pos.z;
                    vr->set_standing_origin(current_standing_origin);
                }
            }
        }

        // Process snapturn    
        vr->process_snapturn();
    }

    if (!is_full_pass) {
        for (auto& mod : mods) {
            mod->on_post_calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location, g_hook->m_has_double_precision);
        }

        if (true_index == 0) {
            if (has_double_precision) {
                g_hook->m_last_rotation_double = *rot_d;
            } else {
                g_hook->m_last_rotation = *view_rotation;
            }
        }

        // Modify Player Control Rotation
        if (true_index == 1 && vr->is_aim_modify_player_control_rotation_enabled() && vr->is_any_aim_method_active()) {
            if (g_hook->m_tracking_system_hook != nullptr) {
                g_hook->m_tracking_system_hook->manual_update_control_rotation();
            }
        }
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("Finished calculating stereo view offset!");
#else
    SPDLOG_INFO_ONCE("Finished calculating stereo view offset!");
#endif
}

__forceinline Matrix4x4f* FFakeStereoRenderingHook::calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("calculate stereo projection matrix called! {} from {:x}", view_index, (uintptr_t)_ReturnAddress() - (uintptr_t)utility::get_module_within((uintptr_t)_ReturnAddress()).value_or(nullptr));
#else
    SPDLOG_INFO_ONCE("calculate stereo projection matrix called! {} from {:x}", view_index, (uintptr_t)_ReturnAddress() - (uintptr_t)utility::get_module_within((uintptr_t)_ReturnAddress()).value_or(nullptr));
#endif

    auto& vr = VR::get();

    // Only call PostInitProperties if ghosting fix enabled or native stereo is being used.
    // Also, if we don't have a hook on GetDesiredNumberOfViews, we need to call PostInitProperties
    //if (!vr->is_using_afr() || vr->is_ghosting_fix_enabled() || !g_hook->m_get_desired_number_of_views_hook) {
    if (!vr->should_skip_post_init_properties()) {
        if (!g_hook->m_fixed_localplayer_view_count) {
            if (!g_hook->m_calculate_stereo_projection_matrix_post_hook) {
                const auto return_address = (uintptr_t)_ReturnAddress();
                SPDLOG_INFO("Inserting midhook after CalculateStereoProjectionMatrix... @ {:x}", return_address);

                constexpr auto max_stack_depth = 100;
                uintptr_t stack[max_stack_depth]{};

                const auto depth = RtlCaptureStackBackTrace(0, max_stack_depth, (void**)&stack, nullptr);

                for (int i = 0; i < depth; i++) {
                    g_hook->m_projection_matrix_stack.push_back(stack[i]);
                    SPDLOG_INFO(" {:x}", (uintptr_t)stack[i]);
                }

                g_hook->m_calculate_stereo_projection_matrix_post_hook = safetyhook::create_mid((void*)return_address, &FFakeStereoRenderingHook::post_calculate_stereo_projection_matrix);

                if (!g_hook->m_calculate_stereo_projection_matrix_post_hook) {
                    SPDLOG_ERROR("Failed to insert midhook after CalculateStereoProjectionMatrix!");
                }
            }
        } else if (g_hook->m_calculate_stereo_projection_matrix_post_hook) {
            SPDLOG_INFO("Removing midhook after CalculateStereoProjectionMatrix, job is done...");
            g_hook->m_calculate_stereo_projection_matrix_post_hook = {};
            g_hook->m_get_projection_data_pre_hook = {};
        }   
    }

    if (!g_framework->is_game_data_intialized()) {
        if (g_hook->m_calculate_stereo_projection_matrix_hook) {
            return g_hook->m_calculate_stereo_projection_matrix_hook.call<Matrix4x4f*>(stereo, out, view_index);
        }

        return out;
    }

    static bool index_starts_from_one = true;
    static bool index_was_ever_two = false;

    // This is eSSP_FULL, we don't care. It will cause the view to become monoscopic if we do anything.
    // or maybe we should, this could be used for WorldToScreen.
    /*if (index_was_ever_two && view_index == 0) {
        SPDLOG_INFO_ONCE("Index was ever two, and now it's zero. This is eSSP_FULL, we don't care. It will cause the view to become monoscopic if we do anything.");
        return out;
    }*/

    if (view_index == 2) {
        index_starts_from_one = true;
        index_was_ever_two = true;
    } else if (view_index == 0) {
        index_starts_from_one = false;
    }

    // Can happen if we hooked this differently.
    if (g_hook->m_calculate_stereo_projection_matrix_hook) {
        g_hook->m_calculate_stereo_projection_matrix_hook.call<Matrix4x4f*>(stereo, out, view_index);
    } else {
        if (g_hook->m_has_double_precision) {
            (*out)[3][2] = sdk::globals::get_near_clipping_plane();
        } else {
            (*(Matrix4x4d*)out)[3][2] = (double)sdk::globals::get_near_clipping_plane();
        }
    }

    if (VR::get()->is_using_2d_screen()) {
        float fov = 90.0f; // todo, get from FMinimalViewInfo

        const float width = VR::get()->get_hmd_width();
        const float height = VR::get()->get_hmd_height();
        const float half_fov = glm::radians(fov) / 2.0f;
        const float xs = 1.0f / glm::tan(half_fov);
        const float ys = width / glm::tan(half_fov) / height;
        const float near_z = sdk::globals::get_near_clipping_plane();

        if (g_hook->m_has_double_precision) {
            (*(Matrix4x4d*)out) = Matrix4x4d {
                xs, 0.0, 0.0, 0.0,
                0.0, ys, 0.0, 0.0,
                0.0, 0.0, 0.0, 1.0,
                0.0, 0.0, near_z, 0.0
            };
        } else {
            *out = Matrix4x4f {
                xs, 0.0f, 0.0f, 0.0f,
                0.0f, ys, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, near_z, 0.0f
            };
        }

        return out;
    }

    // SPDLOG_INFO("NearZ: {}", old_znear);

    if (out != nullptr) {
        auto true_index = index_starts_from_one ? ((view_index + 1) % 2) : (view_index % 2);
    
        if (vr->is_using_afr()) {
            true_index = g_frame_count % 2;
        }

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
        SPDLOG_ERROR("CalculateStereoProjectionMatrix returned nullptr!");
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("Finished calculating stereo projection matrix!");
#else
    SPDLOG_INFO_ONCE("Finished calculating stereo projection matrix!");
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
    SPDLOG_INFO("render texture render thread called!");
#else
    SPDLOG_INFO_ONCE("render texture render thread called!");
#endif


    if (!g_hook->is_slate_hooked() && g_hook->has_attempted_to_hook_slate()) {
        SPDLOG_INFO("Attempting to hook SlateRHIRenderer::DrawWindow_RenderThread using RenderTexture_RenderThread return address...");
        const auto return_address = (uintptr_t)_ReturnAddress();
        SPDLOG_INFO(" Return address: {:x}", return_address);
        g_hook->attempt_hook_slate_thread(return_address);
    }

    /*const auto return_address = (uintptr_t)_ReturnAddress();
    const auto slate_cvar_usage_location = sdk::vr::get_slate_draw_to_vr_render_target_usage_location();

    if (slate_cvar_usage_location) {
        const auto distance_from_usage = (intptr_t)(return_address - *slate_cvar_usage_location);

        if (distance_from_usage <= 0x200) {
            //SPDLOG_INFO("Ret: {:x} Distance: {:x}", return_address, distance_from_usage);

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

    // SPDLOG_INFO("{:x}", (uintptr_t)src_texture->GetNativeResource());

    // maybe the window size is actually a pointer we will find out later.
    /*if (g_hook->m_render_texture_render_thread_hook) {
        g_hook->m_render_texture_render_thread_hook->call<void*>(stereo, rhi_command_list, backbuffer, src_texture, window_size);
    }*/
}

void FFakeStereoRenderingHook::init_canvas(FFakeStereoRendering* stereo, FSceneView* view, UCanvas* canvas) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("init canvas called!");
#else
    SPDLOG_INFO_ONCE("init canvas called!");
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
        SPDLOG_INFO("Searching for FSceneView and UCanvas offsets...");
        SPDLOG_INFO("Canvas: {:x}", (uintptr_t)canvas);

        const auto return_address = (uintptr_t)_ReturnAddress();
        const auto containing_function = utility::find_function_start(return_address);

        SPDLOG_INFO("Found containing function at {:x}", *containing_function);

        auto find_offsets = [](uintptr_t start, uintptr_t end) -> bool {
            for (auto ip = (uintptr_t)start; ip < end + 0x100;) {
                const auto ix = utility::decode_one((uint8_t*)ip);

                if (!ix) {
                    SPDLOG_ERROR("Failed to decode instruction at {:x}", ip);
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
                            
                            SPDLOG_INFO("Found at {:x}", ip);
                            SPDLOG_INFO("Found FSceneView ViewProjectionMatrix offset: {:x}", fsceneview_viewproj_offset);
                            SPDLOG_INFO("Found UCanvas ViewProjectionMatrix offset: {:x}", ucanvas_viewproj_offset);
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
                SPDLOG_ERROR("Failed to find offsets!");
                return;
            }
        }
    }

    //*(Matrix4x4f*)((uintptr_t)view + fsceneview_viewproj_offset) = VR::get()->get_projection_matrix(VRRuntime::Eye::LEFT);
    *(Matrix4x4f*)((uintptr_t)canvas + ucanvas_viewproj_offset) = *(Matrix4x4f*)((uintptr_t)view + fsceneview_viewproj_offset);
}

uint32_t FFakeStereoRenderingHook::get_desired_number_of_views_hook(FFakeStereoRendering* stereo, bool is_stereo_enabled) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("get desired number of views hook called!");
#else
    SPDLOG_INFO_ONCE("get desired number of views hook called!");
#endif

    auto& vr = VR::get();

    if (g_hook->m_sceneview_data.inside_post_init_properties) {
        return 2;
    }

    if (!is_stereo_enabled || (vr->is_using_afr() && !vr->is_splitscreen_compatibility_enabled())) {
        // We need to know about the second scene state to fix ghosting, so set the view count to 2
        // after we know about it, we can continue returning 1.
        if (is_stereo_enabled && vr->is_ghosting_fix_enabled() && vr->is_using_afr() &&
            g_hook->m_sceneview_data.known_scene_states.size() < 2 && g_hook->m_fixed_localplayer_view_count &&
            !!g_hook->m_sceneview_data.constructor_hook && g_hook->m_has_view_extensions_installed)
        {
            // Only works correctly if view extensions are installed, so we can reset the view count to 1 without crashing
            return 2;
        }

        return 1;
    }

    return 2;
}

// Only really necessary for 5.0.3 because for some reason negative view index gets passed into it
// but 5.0.3 doesn't account for this and thinks it's a secondary pass
// so the purpose of the hook (mostly) is to make those return eSSP_FULL to fix a crash
EStereoscopicPass FFakeStereoRenderingHook::get_view_pass_for_index_hook(FFakeStereoRendering* stereo, bool stereo_requested, int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("get view pass for index hook called! {} {}", stereo_requested, view_index);
#else
    SPDLOG_INFO_ONCE("get view pass for index hook called! {} {}", stereo_requested, view_index);
#endif

    // On 5.0.3 this check is not here, it was only added in 5.1
    // So we need to imitate it here to prevent a crash
    if (!stereo_requested || view_index < 0) {
        return EStereoscopicPass::eSSP_FULL;
    }

    return view_index % 2 == 0 ? EStereoscopicPass::eSSP_PRIMARY : EStereoscopicPass::eSSP_SECONDARY;
}

IStereoRenderTargetManager* FFakeStereoRenderingHook::get_render_target_manager_hook(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("get render target manager hook called!");
#else
    SPDLOG_INFO_ONCE("get render target manager hook called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return nullptr;
    }

    auto vr = VR::get();

    if (vr->is_stereo_emulation_enabled() || vr->is_extreme_compatibility_mode_enabled()) {
        return nullptr;
    }

    if (!vr->get_runtime()->got_first_poses || vr->is_hmd_active()) {
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
    SPDLOG_INFO("get stereo layers hook called!");
#else
    SPDLOG_INFO_ONCE("get stereo layers hook called!");
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
    SPDLOG_INFO("post calculate stereo projection matrix called!");
#else
    SPDLOG_INFO_ONCE("post calculate stereo projection matrix called!");
#endif

    if (g_hook->m_fixed_localplayer_view_count || g_hook->m_hooked_alternative_localplayer_scan) {
        return;
    }

    auto vfunc = utility::find_virtual_function_start(g_hook->m_calculate_stereo_projection_matrix_post_hook.target_address());

    if (!vfunc) {
        // attempt to hook GetProjectionData instead to get the localplayer
        SPDLOG_INFO("Failed to find virtual function start for CalculateStereoProjectionMatrix, attempting to hook GetProjectionData instead...");

        if (!g_hook->m_projection_matrix_stack.empty() && g_hook->m_projection_matrix_stack.size() >= 3) {
            const auto post_get_projection_data = g_hook->m_projection_matrix_stack[2];

            const auto get_projection_data_candidate_1 = utility::find_function_start_with_call(post_get_projection_data);
            const auto get_projection_data_candidate_2 = utility::find_virtual_function_start(post_get_projection_data);

            // Select whichever one is closest to post_get_projection_data
            std::optional<uintptr_t> get_projection_data{};

            if (get_projection_data_candidate_1 && get_projection_data_candidate_2) {
                const auto candidate_1_distance = std::abs((int64_t)post_get_projection_data - (int64_t)*get_projection_data_candidate_1);
                const auto candidate_2_distance = std::abs((int64_t)post_get_projection_data - (int64_t)*get_projection_data_candidate_2);

                if (candidate_1_distance < candidate_2_distance) {
                    get_projection_data = get_projection_data_candidate_1;
                } else {
                    get_projection_data = get_projection_data_candidate_2;
                }
            } else if (get_projection_data_candidate_1) {
                get_projection_data = get_projection_data_candidate_1;
            } else if (get_projection_data_candidate_2) {
                get_projection_data = get_projection_data_candidate_2;
            } else {
                // emergency fallback
                SPDLOG_INFO("Failed to find GetProjectionData, falling back to emergency fallback (this may not work)");
                get_projection_data = utility::find_function_start(post_get_projection_data);
            }

            if (get_projection_data) {
                SPDLOG_INFO("Successfully found GetProjectionData at {:x}", *get_projection_data);

                g_hook->m_hooked_alternative_localplayer_scan = true;

                g_hook->m_get_projection_data_pre_hook = safetyhook::create_mid((void*)*get_projection_data, &FFakeStereoRenderingHook::pre_get_projection_data);
                g_hook->m_projection_matrix_stack.clear();

                if (g_hook->m_get_projection_data_pre_hook) {
                    SPDLOG_INFO("Successfully hooked GetProjectionData");
                    return;
                } else {
                    SPDLOG_ERROR("Failed to hook GetProjectionData");
                }
            } else {
                SPDLOG_ERROR("Failed to find GetProjectionData!");
            }
        }
    }

    if (!vfunc) {
        SPDLOG_INFO("Could not find function via normal means, scanning for int3s...");

        const auto ref = utility::scan_reverse(g_hook->m_calculate_stereo_projection_matrix_post_hook.target_address(), 0x2000, "CC CC CC");

        if (ref) {
            vfunc = *ref + 3;
        }

        if (!vfunc) {
            g_hook->m_fixed_localplayer_view_count = true;
            SPDLOG_ERROR("Failed to find virtual function start for post calculate_stereo_projection_matrix!");
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
            SPDLOG_INFO("Decoding failed with error {:x}!", (uint32_t)status);
            break;
        }

        if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_REG && ix.Operands[1].Info.Register.Reg == NDR_RCX) {
            SPDLOG_INFO("Found assignment of RCX to storage register at {:x} ({})!", (uintptr_t)ip, ix.Operands[0].Info.Register.Reg);
            found_register = ix.Operands[0].Info.Register.Reg;
            break;
        }

        ip += ix.Length;
    }

    if (!found_register) {
        g_hook->m_fixed_localplayer_view_count = true;
        SPDLOG_ERROR("Failed to find assignment of RCX to storage register!");
        return;
    }

    const auto localplayer = *register_to_context[found_register.value_or(0)];
    SPDLOG_INFO("Local player: {:x}", localplayer);

    if (localplayer == 0) {
        g_hook->m_fixed_localplayer_view_count = true;
        SPDLOG_ERROR("Failed to find local player, cannot call PostInitProperties!");
        return;
    }

    g_hook->post_init_properties(localplayer);
}

void FFakeStereoRenderingHook::pre_get_projection_data(safetyhook::Context& ctx) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("pre get projection data called!");
#else
    SPDLOG_INFO_ONCE("pre get projection data called!");
#endif

    if (g_hook->m_fixed_localplayer_view_count) {
        return;
    }

    const auto localplayer = ctx.rcx;
    SPDLOG_INFO("Local player: {:x}", localplayer);

    if (localplayer == 0) {
        g_hook->m_fixed_localplayer_view_count = true;
        SPDLOG_ERROR("Failed to find local player, cannot call PostInitProperties!");
        return;
    }

    g_hook->post_init_properties(localplayer);
}

void FFakeStereoRenderingHook::post_init_properties(uintptr_t localplayer) {
    SPDLOG_INFO("Searching for PostInitProperties virtual function...");

    std::optional<uint32_t> idx{};
    const auto engine = sdk::UEngine::get_lvalue();

    if (engine == nullptr) {
        SPDLOG_ERROR("Cannot proceed without engine!");
        return;
    }

    const auto vtable = *(uintptr_t**)localplayer;

    if (vtable == nullptr || IsBadReadPtr((void*)vtable, sizeof(void*))) {
        SPDLOG_ERROR("Cannot proceed, vtable for so-called \"local player\" is invalid!");
        return;
    }

    INSTRUX ix{};

    for (auto i = 1; i < 25; ++i) {
        if (idx) {
            break;
        }

        SPDLOG_INFO("Analyzing index {}...", i);

        const auto vfunc = vtable[i];

        if (vfunc == 0 || IsBadReadPtr((void*)vfunc, 1)) {
            SPDLOG_ERROR("Encountered invalid vfunc at index {}!", i);
            break;
        }

        SPDLOG_INFO("Scanning vfunc at index {} ({:x})...", i, vfunc);

        utility::exhaustive_decode((uint8_t*)vfunc, 25, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (idx) {
                return utility::ExhaustionResult::BREAK;
            }

            if (const auto disp = utility::resolve_displacement(ip); disp) {
                // the second expression catches UE dynamic/debug builds
                if (*disp == (uintptr_t)engine || 
                    (!IsBadReadPtr((void*)*disp, sizeof(void*)) && *(uintptr_t*)*disp == (uintptr_t)*engine)) 
                {
                    SPDLOG_INFO("Found PostInitProperties at {} {:x}!", i, (uintptr_t)vfunc);
                    idx = i;
                    return utility::ExhaustionResult::BREAK;
                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });
    }

    if (!idx) {
        SPDLOG_ERROR("Failed to find PostInitProperties virtual function! A crash may occur!");
    }

    // Now call PostInitProperties.
    // The purpose of this is setting up the view for the other eye.
    // Just creating the StereoRenderingDevice does not automatically do it, so we have to do it manually.
    // Usually the game just calls this function near startup after calling InitializeHMDDevice.
    if (idx) {
        SPDLOG_INFO("Calling PostInitProperties on local player!");

        // Get PEB and set debugger present
        auto peb = (PEB*)__readgsqword(0x60);

        const auto old = peb->BeingDebugged;
        peb->BeingDebugged = true;

        // If the exception count exceeds a certain amount, we need to un-nop the function call because it was supposed to return a pointer.
        static auto exception_count = 0;
        static std::vector<Patch::Ptr> patches{};
        static std::vector<uintptr_t> patch_locations{};

        static std::vector<Patch::Ptr> assert_patches{};
        const void (*post_init_properties)(uintptr_t) = (*(decltype(post_init_properties)**)localplayer)[*idx];

        // Scan through all of the branches of PostInitProperties to find any assertions
        // The assertion we're looking for is easily identified by a string that it loads in RCX, named "!Reference"
        // If we dont do this, there's a possibility that the game will crash at some point or cause some sort of corruption
        utility::exhaustive_decode((uint8_t*)post_init_properties, 100, [](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (ix.Operands[1].Type == ND_OP_MEM) {
                const auto referenced_addr = utility::resolve_displacement(ip);

                if (referenced_addr) try {
                    if (std::string_view{(const char*)*referenced_addr}.starts_with("!Reference")) {
                        // Scan forward and patch out the first call or jmp we run into
                        utility::exhaustive_decode((uint8_t*)ip, 10, [](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
                            if (*(uint8_t*)ip == 0xE8) {
                                SPDLOG_INFO("Patching assertion at {:x}!", ip);
                                assert_patches.push_back(Patch::create(ip, { 0x90, 0x90, 0x90, 0x90, 0x90 }));
                                return utility::ExhaustionResult::BREAK;
                            }

                            if (*(uint8_t*)ip == 0xE9) {
                                SPDLOG_INFO("Patching assertion at {:x}!", ip);
                                assert_patches.push_back(Patch::create(ip, { 0xC3 }));
                                return utility::ExhaustionResult::BREAK;
                            }

                            if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                                SPDLOG_INFO("Patching assertion at {:x}!", ip);
                                std::vector<int16_t> nop{};
                                for (auto i = 0; i < ix.Length; ++i) {
                                    nop.push_back(0x90);
                                }

                                assert_patches.push_back(Patch::create(ip, nop));
                                return utility::ExhaustionResult::BREAK;
                            }

                            return utility::ExhaustionResult::CONTINUE;
                        });
                    }
                } catch(...) {

                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        // set up a handler to skip int3 assertions
        // we do this because debug builds assert when the views are already setup.
        const auto seh_handler = [](PEXCEPTION_POINTERS info) -> LONG {
            ++exception_count;

            if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
                SPDLOG_INFO("Skipping int3 breakpoint at {:x}!", info->ContextRecord->Rip);
                const auto insn = utility::decode_one((uint8_t*)info->ContextRecord->Rip);

                if (insn) {
                    spdlog::info("Skipping {} bytes!", insn->Length);
                    info->ContextRecord->Rip += insn->Length;

                    // Nop out the next function call.
                    // It logs and does some other stuff and causes a crash later on.
                    // To be seen if this will cause any issues, does not appear to (on 4.9 debug builds)
                    const auto call = utility::scan_disasm((uintptr_t)info->ContextRecord->Rip, 20, "E8 ? ? ? ?");

                    if (call) {
                        patch_locations.push_back(*call);
                        patches.emplace_back(Patch::create(*call, {0x90, 0x90, 0x90, 0x90, 0x90}));
                    }

                    return EXCEPTION_CONTINUE_EXECUTION;
                }
                
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            SPDLOG_INFO("Encountered exception {:x} at {:x}!", info->ExceptionRecord->ExceptionCode, info->ContextRecord->Rip);

            // This happens if we removed a call that shouldn't have been removed.
            if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && !patches.empty()) {
                SPDLOG_WARN("Access violation at {:x}! Removing patch at {:x}!", info->ContextRecord->Rip, patch_locations.back());

                exception_count = 0;
                info->ContextRecord->Rip = patch_locations.back();
                patches.pop_back();
                patch_locations.pop_back();
            } else {
                const auto insn = utility::decode_one((uint8_t*)info->ContextRecord->Rip);

                if (insn) {
                    info->ContextRecord->Rip += insn->Length;
                } else {
                    info->ContextRecord->Rip += 1;
                }
            }

            // yolo? idk xd
            return EXCEPTION_CONTINUE_EXECUTION;
        };

        const auto exception_handler = AddVectoredExceptionHandler(1, seh_handler);

        m_sceneview_data.inside_post_init_properties = true;
        post_init_properties(localplayer);
        m_sceneview_data.inside_post_init_properties = false;

        SPDLOG_INFO("PostInitProperties called!");

        // remove the handler
        RemoveVectoredExceptionHandler(exception_handler);
        peb->BeingDebugged = old;
    }

    g_hook->m_sceneview_data.known_scene_states.clear();
    g_hook->m_fixed_localplayer_view_count = true;
}

void* FFakeStereoRenderingHook::slate_draw_window_render_thread(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info, 
                                                                void* elements, void* params, void* unk1, void* unk2) 
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("SlateRHIRenderer::DrawWindow_RenderThread called!");
#else
    SPDLOG_INFO_ONCE("SlateRHIRenderer::DrawWindow_RenderThread called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return g_hook->m_slate_thread_hook.call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);
    }

    g_hook->get_slate_thread_worker()->execute((FRHICommandListImmediate*)command_list);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_pre_slate_draw_window(renderer, command_list, viewport_info);
    }

    g_hook->m_inside_slate_draw_window = true;
    g_hook->m_slate_draw_window_thread_id = GetCurrentThreadId();

    auto call_orig = [&]() {
        auto ret = g_hook->m_slate_thread_hook.call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);

        for (auto& mod : mods) {
            mod->on_post_slate_draw_window(renderer, command_list, viewport_info);
        }

        g_hook->m_inside_slate_draw_window = false;

        return ret;
    };


    auto vr = VR::get();

    if (!vr->is_hmd_active() || vr->is_stereo_emulation_enabled()) {
        return call_orig();
    }

    const auto ui_target = g_hook->get_render_target_manager()->get_ui_target();

    if (ui_target == nullptr) {
        SPDLOG_INFO_EVERY_N_SEC(1, "No UI target, skipping!");
        return call_orig();
    }

    const auto viewport_rt_provider = viewport_info->get_rt_provider(g_hook->get_render_target_manager()->get_render_target());

    if (viewport_rt_provider == nullptr) {
        SPDLOG_INFO_EVERY_N_SEC(1, "No viewport RT provider, skipping!");
        return call_orig();
    }

    const auto slate_resource = viewport_rt_provider->get_viewport_render_target_texture();

    if (slate_resource == nullptr) {
        SPDLOG_INFO_EVERY_N_SEC(1, "No slate resource, skipping!");
        return call_orig();
    }
    
    // Replace the texture with one we have control over.
    // This isolates the UI to render on our own texture separate from the scene.
    const auto old_texture = slate_resource->get_mutable_resource();
    slate_resource->get_mutable_resource() = ui_target;

    // To be seen if we need to resort to a MidHook on this function if the parameters
    // are wildly different between UE versions.
    const auto ret = g_hook->m_slate_thread_hook.call<void*>(renderer, command_list, viewport_info, elements, params, unk1, unk2);

    // Restore the old texture.
    slate_resource->get_mutable_resource() = old_texture;

    for (auto& mod : mods) {
        mod->on_post_slate_draw_window(renderer, command_list, viewport_info);
    }
    
    // After this we copy over the texture and clear it in the present hook. doing it here just seems to crash sometimes.
    SPDLOG_INFO_ONCE("SlateRHIRenderer::DrawWindow_RenderThread finished!");

    return ret;
}

// INTERNAL USE ONLY!!!!
__declspec(noinline) void VRRenderTargetManager::CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager::CalculateRenderTargetSize called!");

    m_last_calculate_render_size_return_address = (uintptr_t)_ReturnAddress();

    VRRenderTargetManager_Base::calculate_render_target_size(Viewport, InOutSizeX, InOutSizeY);
}

__declspec(noinline) bool VRRenderTargetManager::NeedReAllocateDepthTexture(const void* DepthTarget) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager::NeedReAllocateDepthTexture called!");

    m_last_needs_reallocate_depth_texture_return_address = (uintptr_t)_ReturnAddress();

    if (this->depth_analysis_passed) {
        return VRRenderTargetManager_Base::need_reallocate_depth_texture(DepthTarget);
    }

    return false;
}

__declspec(noinline) bool VRRenderTargetManager::NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager::NeedReAllocateShadingRateTexture called!");

    const auto return_address = (uintptr_t)_ReturnAddress();
    const auto diff = return_address - m_last_calculate_render_size_return_address;

    if (diff <= 0x50) {
        // We need to switch the FFakeStereoRenderingHook's render target manager
        // to the old one NOW or we will crash. Reason being what was actually called
        // is the GetNumberOfBufferedFrames function, not NeedReAllocateShadingRateTexture.
        SPDLOG_INFO("Switching to old render target manager! Incorrect function called!");
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
    SPDLOG_INFO_ONCE("VRRenderTargetManager_Base::update_viewport called! {} {:x} {:x}", use_separate_rt, (uintptr_t)&vp, (uintptr_t)vp_widget);

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    //SPDLOG_INFO("Widget: {:x}", (uintptr_t)ViewportWidget);
}

void VRRenderTargetManager_Base::calculate_render_target_size(const FViewport& viewport, uint32_t& x, uint32_t& y) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager_Base::calculate_render_target_size called!");

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    SPDLOG_INFO("calculate render target size called!");
#endif

    if (!g_framework->is_game_data_intialized()) {
        return;
    }

    SPDLOG_INFO("RenderTargetSize Before: {}x{}", x, y);

    x = VR::get()->get_hmd_width() * 2;
    y = VR::get()->get_hmd_height();

    SPDLOG_INFO("RenderTargetSize After: {}x{}", x, y);
}

bool VRRenderTargetManager_Base::need_reallocate_view_target(const FViewport& Viewport) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager_Base::need_reallocate_view_target called!");

    if (!g_framework->is_game_data_intialized()) {
        return false;
    }

    if (!m_attempted_find_force_separate_rt) try {
        m_attempted_find_force_separate_rt = true;

        // Go up the stack until we find something that isn't in our module.
        const auto our_module = g_framework->get_framework_module();
        constexpr auto max_stack_depth = 100;
        uintptr_t stack[max_stack_depth]{};

        const auto depth = RtlCaptureStackBackTrace(0, max_stack_depth, (void**)&stack, nullptr);

        std::optional<uintptr_t> ret_addr{};
        std::optional<HMODULE> module_within{};

        for (auto i = 0; i < depth; ++i) {
            SPDLOG_INFO("Stack[{}]: {:x}", i, stack[i]);

            module_within = utility::get_module_within(stack[i]);

            if (!module_within) {
                continue;
            }

            if (*module_within != our_module) {
                ret_addr = stack[i];
                break;
            }
        }

        // Emulate from the return address and find a memory write
        // this should contain the offset to the force separate rt bool.
        if (ret_addr) {
            SPDLOG_INFO("Found return address: {:x}", *ret_addr);

            utility::ShemuContext ctx{*module_within};
            ctx.ctx->Registers.RegRip = *ret_addr;
            ctx.ctx->Registers.RegRax = 1; // As if we're returning true from this function.

            utility::emulate(*module_within, *ret_addr, 100, ctx, [this](const utility::ShemuContextExtended& ctx) -> utility::ExhaustionResult {
                SPDLOG_INFO("Emulating instruction: {:x}", ctx.ctx->ctx->Registers.RegRip);

                if (ctx.next.writes_to_memory) {
                    const auto& ix = ctx.next.ix;
                    if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG) {
                        // We're looking for a mov [reg1+N], reg2
                        const auto& op0 = ix.Operands[0];

                        // Needs a register
                        if (!op0.Info.Memory.HasBase || op0.Info.Memory.IsRipRel) {
                            return utility::ExhaustionResult::STEP_OVER;
                        }

                        // Needs a displacement
                        if (!op0.Info.Memory.HasDisp) {
                            return utility::ExhaustionResult::STEP_OVER;
                        }

                        // We don't want a stack based register
                        if (op0.Info.Memory.Base == NDR_RSP || op0.Info.Memory.Base == NDR_RBP) {
                            return utility::ExhaustionResult::STEP_OVER;
                        }

                        if (op0.Info.Memory.Disp > 0 && op0.Info.Memory.Disp < 0x2000) {
                            m_viewport_force_separate_rt_offset = op0.Info.Memory.Disp;
                            SPDLOG_INFO("Found force separate rt offset: {:x}", *m_viewport_force_separate_rt_offset);
                            return utility::ExhaustionResult::BREAK;
                        }
                    }

                    SPDLOG_INFO("Stepping over...");

                    return utility::ExhaustionResult::STEP_OVER;
                }

                if (std::string_view{ctx.next.ix.Mnemonic}.starts_with("CALL")) {
                    // We need to break out of this, we should've found the offset before the call.
                    SPDLOG_ERROR("Failed to find force separate rt offset! Encountered call at {:x}", ctx.ctx->ctx->Registers.RegRip);
                    return utility::ExhaustionResult::BREAK;
                }

                return utility::ExhaustionResult::CONTINUE;
            });
        }
    } catch(...) { // if we dont find it, it's fine, not very many games require it.
        SPDLOG_ERROR("Failed to find force separate rt offset! (Exception)");
    }

    const auto w = VR::get()->get_hmd_width();
    const auto h = VR::get()->get_hmd_height();

    if (w != this->last_width || h != this->last_height || g_hook->should_recreate_textures()) {
        SPDLOG_INFO("Reallocating view target! {} {} -> {} {}", this->last_width, this->last_height, w, h);

        this->last_width = w;
        this->last_height = h;
        this->wants_depth_reallocate = true;
        g_hook->set_should_recreate_textures(false);
        return true;
    }

    return false;
}

bool VRRenderTargetManager_Base::need_reallocate_depth_texture(const void* DepthTarget) {
    SPDLOG_INFO_ONCE("VRRenderTargetManager_Base::need_reallocate_depth_texture called!");

    if (!g_framework->is_game_data_intialized()) {
        return false;
    }

    if (this->wants_depth_reallocate) {
        SPDLOG_INFO("Reallocating depth texture!");

        this->wants_depth_reallocate = false;
        return true;
    }

    return false;
}

void VRRenderTargetManager_Base::pre_texture_hook_callback(safetyhook::Context& ctx) {
    SPDLOG_INFO("PreTextureHook called! {}", ctx.r8);

    // maybe do some work later to bruteforce the registers/offsets for these
    // a la emulation or something more rudimentary
    // since it always seems to access a global right before, which
    // refers to the current pixel format, which we can overwrite (which may not be safe)
    // so we could just follow how the global is being written to registers or the stack
    // and then just overwrite the registers/stack with our own values
    auto rtm = g_hook->get_render_target_manager();

    if (!rtm->allocate_texture_called) {
        SPDLOG_ERROR("AllocateTexture not called yet! (PreTextureHook)");
        return;
    }

    if (!g_hook->has_pixel_format_cvar()) {
        if (g_hook->get_render_target_manager()->is_pre_texture_call_e8) {
            //ctx.r8 = 2; // PF_B8G8R8A8 // decided not to actually set it here, we need to double check when it's actually called
        } else if (!rtm->is_using_texture_desc) {
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

    SPDLOG_INFO("Attempting to JIT a function to call the original function!");

    const auto ix = utility::decode_one(rtm->texture_create_insn_bytes.data(), rtm->texture_create_insn_bytes.size());

    if (!ix) {
        SPDLOG_ERROR("Failed to decode instruction!");
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
            SPDLOG_ERROR("Failed to emulate instruction!: {} RIP: {:x}", emu_ctx.status, emu_ctx.ctx->Registers.RegRip);
            return;
        }
    
        SPDLOG_INFO("Emu landed at {:x}", emu_ctx.ctx->Registers.RegRip);
        func_ptr = emu_ctx.ctx->Registers.RegRip;
    } else {
        const auto target = g_hook->get_render_target_manager()->pre_texture_hook.target_address();
        func_ptr = target + 5 + *(int32_t*)&rtm->texture_create_insn_bytes.data()[1];
    }

    SPDLOG_INFO("Function pointer: {:x}", func_ptr);

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
            SPDLOG_INFO("Correcting stack var at 0x{:x}", stack_var);
            *stack_var_ptr = aligned_stack + (*stack_var_ptr - ctx.rsp);
        }
    }

    auto correct_register = [&](auto& reg) {
        if (reg >= ctx.rsp && reg < ctx.rsp + 0x1000) {
            SPDLOG_INFO("Correcting Register");
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

    SPDLOG_INFO("JITed address: {:x}", code_addr);

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

    SPDLOG_INFO("About to call the original!");
    
    if (!rtm->is_pre_texture_call_e8) {
        SPDLOG_INFO("Calling register version of texture create");

        if (rtm->is_using_texture_desc && rtm->is_version_greq_5_1) {
            if (ctx.r9 == 0 || IsBadReadPtr((void*)ctx.r9, sizeof(void*))) {
                SPDLOG_INFO("Possible UE 5.0.3 detected, not 5.1 or above");
                rtm->is_using_texture_desc = false;
                rtm->is_version_5_0_3 = true;
                rtm->is_version_greq_5_1;
            }
        }

        if (rtm->is_using_texture_desc && rtm->is_version_greq_5_1) {
            SPDLOG_INFO("Calling UE5 texture desc version of texture create");

            void (*func)(
                uintptr_t rhi,
                FTexture2DRHIRef* out,
                uintptr_t command_list,
                uintptr_t desc,
                uintptr_t stack_0, // Stack dummies in-case this is the wrong function
                uintptr_t stack_1,
                uintptr_t stack_2,
                uintptr_t stack_3,
                uintptr_t stack_4,
                uintptr_t stack_5,
                uintptr_t stack_6,
                uintptr_t stack_7,
                uintptr_t stack_8) = (decltype(func))func_ptr;

            // Scan for the render target width and height in the desc
            // and replace it with the desktop resolution (This is for the UI texture)
            const auto scan_x = VR::get()->get_hmd_width() * 2;
            const auto scan_y = VR::get()->get_hmd_height();

            std::optional<int32_t> width_offset{};
            std::optional<int32_t> height_offset{};

            int32_t old_width{};
            int32_t old_height{};

            for (auto i = 0; i < 0x100; ++i) {
                auto& x = *(int32_t*)(ctx.r9 + i);
                auto& y = *(int32_t*)(ctx.r9 + i + 4);

                if (x == scan_x && y == scan_y) {
                    SPDLOG_INFO("UE5: Found render target width and height at offset: {:x}", i);

                    width_offset = i;
                    height_offset = i + 4;

                    old_width = x;
                    old_height = y;

                    x = size.x;
                    y = size.y;
                    break;
                }
            }

            func(ctx.rcx, &out, ctx.r8, ctx.r9,
                stack_args[0], stack_args[1], 
                stack_args[2], stack_args[3],
                stack_args[4],
                stack_args[5], stack_args[6],
                stack_args[7], stack_args[8]
            );

            if (width_offset && height_offset) {
                auto& x = *(int32_t*)(ctx.r9 + *width_offset);
                auto& y = *(int32_t*)(ctx.r9 + *height_offset);

                x = old_width;
                y = old_height;
            }

            if (rtm->texture_hook_ref == nullptr || rtm->texture_hook_ref->texture == nullptr) {
                SPDLOG_INFO("Had to set texture hook ref in pre texture hook!");
                rtm->texture_hook_ref = (FTexture2DRHIRef*)ctx.rdx;
            }
        } else if (rtm->is_using_texture_desc) { // extremely rare.
            SPDLOG_INFO("Calling UE4 texture desc version of texture create");

            void (*func)(
                uintptr_t rhi,
                uintptr_t desc,
                TRefCountPtr<IPooledRenderTarget>* out,
                uintptr_t name // wchar_t*
            ) = (decltype(func))func_ptr;

            // Scan for the render target width and height in the desc
            // and replace it with the desktop resolution (This is for the UI texture)
            const auto scan_x = VR::get()->get_hmd_width() * 2;
            const auto scan_y = VR::get()->get_hmd_height();

            std::optional<int32_t> width_offset{};
            std::optional<int32_t> height_offset{};

            int32_t old_width{};
            int32_t old_height{};

            for (auto i = 0; i < 0x100; ++i) {
                auto& x = *(int32_t*)(ctx.rdx + i);
                auto& y = *(int32_t*)(ctx.rdx + i + 4);

                if (x == scan_x && y == scan_y) {
                    SPDLOG_INFO("UE4: Found render target width and height at offset: {:x}", i);

                    width_offset = i;
                    height_offset = i + 4;

                    old_width = x;
                    old_height = y;

                    x = size.x;
                    y = size.y;
                    break;
                }
            }

            static TRefCountPtr<IPooledRenderTarget> real_out{};

            func(ctx.rcx, ctx.rdx, &real_out, ctx.r9);

            if (real_out.reference != nullptr) {
                const auto& tex = real_out.reference->item.texture;
                const auto& shader = real_out.reference->item.srt;
                out.texture = tex.texture;
                shader_out.texture = shader.texture;
            }

            if (width_offset && height_offset) {
                auto& x = *(int32_t*)(ctx.rdx + *width_offset);
                auto& y = *(int32_t*)(ctx.rdx + *height_offset);

                x = old_width;
                y = old_height;
            }

            if (rtm->texture_hook_ref == nullptr || rtm->texture_hook_ref->texture == nullptr) {
                SPDLOG_INFO("Had to set texture hook ref in pre texture hook!");
                rtm->texture_hook_ref = (FTexture2DRHIRef*)ctx.r8;
            }
        } else { // most common version.
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

            if (rtm->texture_hook_ref == nullptr || rtm->texture_hook_ref->texture == nullptr) {
                SPDLOG_INFO("Had to set texture hook ref in pre texture hook!");
                rtm->texture_hook_ref = (FTexture2DRHIRef*)ctx.rdx;
            }
        }

        rtm->ui_target = out.texture;
    } else {
        SPDLOG_INFO("Calling E8 version of texture create");
        
        // check if RCX is near the stack pointer
        // if it is then it's a different form of E8 call that takes the texture in the first parameter.
        if (ctx.rcx != 0 && std::abs((int64_t)ctx.rcx - (int64_t)ctx.rsp) <= 0x300) {
            SPDLOG_INFO("Weird form of E8 call detected...");

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

            std::optional<int> previous_stack_found_index{};
            std::optional<int> previous_stack_repeating_index{};

            std::optional<int> texture_argument_index{};
            std::optional<int> shader_argument_index{};

            for (auto i = 0; i < 10; ++i) {
                const auto stack_ptr = stack_args[i];

                if (std::abs((int64_t)stack_ptr - (int64_t)ctx.rsp) <= 0x300) {
                    if (previous_stack_found_index && *previous_stack_found_index == i - 1) {
                        previous_stack_repeating_index = i;
                    }

                    previous_stack_found_index = i;
                    SPDLOG_INFO("Stack pointer found at arg index {} ({} stack)", i + 4, i);
                } else if (previous_stack_repeating_index && *previous_stack_repeating_index == i - 1) {
                    texture_argument_index = i - 2;
                    shader_argument_index = i - 1;
                    SPDLOG_INFO("Texture argument may be at index {} ({} stack)", *texture_argument_index + 4, *texture_argument_index);
                    SPDLOG_INFO("Shader argument may be at index {} ({} stack)", *shader_argument_index + 4, *shader_argument_index);
                    break;
                }
            }

            if (!texture_argument_index && !shader_argument_index) {
                // operate on a wild guess (hardcoded function signature)
                SPDLOG_INFO("Calling E8 version of texture create with hardcoded function signature");

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
            } else {
                // dynamically generate the function call
                SPDLOG_INFO("Calling E8 version of texture create with dynamically generated function signature");

                void (*func)(
                    uint32_t w,
                    uint32_t h,
                    uint8_t format,
                    uintptr_t mips,
                    uintptr_t stack_0,
                    uintptr_t stack_1,
                    uintptr_t stack_2,
                    uintptr_t stack_3,
                    uintptr_t stack_4,
                    uintptr_t stack_5,
                    uintptr_t stack_6,
                    uintptr_t stack_7,
                    uintptr_t stack_8) = (decltype(func))func_ptr;

                std::array<uintptr_t, 9> cloned_stack{};
                for (auto i = 0; i < 9; ++i) {
                    cloned_stack[i] = stack_args[i];
                }

                cloned_stack[*texture_argument_index] = (uintptr_t)&out;
                cloned_stack[*shader_argument_index] = (uintptr_t)&shader_out;

                func((uint32_t)size.x, (uint32_t)size.y, 2, ctx.r9,
                    cloned_stack[0], cloned_stack[1], 
                    cloned_stack[2], cloned_stack[3],
                    cloned_stack[4],
                    cloned_stack[5], cloned_stack[6],
                    cloned_stack[7], cloned_stack[8]);

                if (rtm->texture_hook_ref == nullptr || rtm->texture_hook_ref->texture == nullptr) {
                    SPDLOG_INFO("Had to set texture hook ref in pre texture hook!");
                    rtm->texture_hook_ref = (FTexture2DRHIRef*)stack_args[*texture_argument_index];
                }
            }
        }

        rtm->ui_target = out.texture;
    }

    if (out.texture == nullptr) {
        SPDLOG_ERROR("Failed to create UI texture!");
    } else {
        SPDLOG_INFO("Created UI texture at {:x}", (uintptr_t)out.texture);
    }

    //call_with_context((uintptr_t)func, out);

    SPDLOG_INFO("Called the original function!");

    // Cause stuff like the VR ui texture to get recreated.
    VR::get()->reinitialize_renderer();
}

void VRRenderTargetManager_Base::texture_hook_callback(safetyhook::Context& ctx) {
    auto rtm = g_hook->get_render_target_manager();

    SPDLOG_INFO("Post texture hook called!");
    SPDLOG_INFO(" Ref: {:x}", (uintptr_t)rtm->texture_hook_ref);

    if (!rtm->allocate_texture_called) {
        g_hook->set_should_recreate_textures(true);
        rtm->render_target = nullptr;
        rtm->texture_hook_ref = nullptr;

        SPDLOG_INFO("[Post texture hook] Allocate texture was not called, skipping...");
        return;
    }

    rtm->allocate_texture_called = false;

    // very rare...
    if (rtm->is_using_texture_desc && !rtm->is_version_greq_5_1) {
        const auto pooled_rt_container = (TRefCountPtr<IPooledRenderTarget>*)rtm->texture_hook_ref;

        if (pooled_rt_container != nullptr && pooled_rt_container->reference != nullptr) {
            rtm->texture_hook_ref = &pooled_rt_container->reference->item.texture;
        }
    }

    FRHITexture2D* texture = nullptr;

    if (rtm->texture_hook_ref != nullptr) {
        texture = rtm->texture_hook_ref->texture;

        // happens?
        if (texture == nullptr) {
            SPDLOG_INFO(" Texture is null, trying to get it from RAX...");

            const auto ref = (FTexture2DRHIRef*)ctx.rax;

            if (!IsBadReadPtr(ref, sizeof(void*)) && !IsBadReadPtr(ref->texture, sizeof(void*))) {
                texture = ref->texture;
            } else {
                SPDLOG_ERROR(" RAX is bad! Can't get texture!");
            }
        }

        if (texture != nullptr) {
            SPDLOG_INFO(" Resulting texture: {:x}", (uintptr_t)texture);
            SPDLOG_INFO(" Real resource: {:x}", (uintptr_t)texture->get_native_resource());
        } else {
            SPDLOG_INFO(" Texture is still null!");
        }
    }

    SPDLOG_INFO(" last texture index: {}", rtm->last_texture_index);

    rtm->render_target = texture;
    //rtm->ui_target = texture;
    rtm->texture_hook_ref = nullptr;
    ++rtm->last_texture_index;
}

// This is a very special fix for cases where engine modifications
// can add a second call to UpdateViewportRHI right before the place we expect it to get called
// The fact that they get called back-to-back over and over causes huge performance problems
// because the viewport texture keeps getting recreated over and over.
// This hook attempts to only allow the last call to UpdateViewportRHI inside of EnqueueBeginRenderFrame to do anything
// Usually there's only one call to UpdateViewportRHI inside of EnqueueBeginRenderFrame, but (very rarely) there can be two.
__declspec(noinline) void FFakeStereoRenderingHook::update_viewport_rhi_hook(void* viewport, size_t destroyed, size_t new_size_x, size_t new_size_y, size_t new_window_mode, size_t preferred_pixel_format) {
    auto call_orig = [&]() {
        g_hook->m_update_viewport_rhi_hook->get_original<void(*)(void*, size_t, size_t, size_t, size_t, size_t)>()(viewport, destroyed, new_size_x, new_size_y, new_window_mode, preferred_pixel_format);
    };

    SPDLOG_INFO_ONCE("UpdateViewportRHI (embedded): {:x}", (uintptr_t)_ReturnAddress());

    const auto hmd_active = VR::get()->is_hmd_active();
    static bool modified_use_separate_rt = false;

    if (!hmd_active) {
        if (modified_use_separate_rt) {
            modified_use_separate_rt = false;

            const auto rtm = g_hook->get_render_target_manager();

            if (rtm != nullptr) {
                if (const auto offset = rtm->get_viewport_force_separate_rt_offset()) {
                    SPDLOG_INFO_ONCE("Resetting bUseSeparateRenderTarget to false!");
                    auto& use_separate_rt = *(bool*)((uintptr_t)viewport + (*offset - 1));
                    use_separate_rt = false;
                }
            }
        }

        call_orig();
        return;
    }

    struct FunctionInfo {
        std::vector<uintptr_t> return_addrs{}; // in order of call
        size_t count{0};
    };

    static std::mutex mtx{};
    static std::unordered_map<uintptr_t, uintptr_t> functions_within{};
    static std::unordered_map<uintptr_t, FunctionInfo> function_infos{};

    {
        std::scoped_lock _{mtx};

        const auto return_addr = (uintptr_t)_ReturnAddress();
        auto function_within = functions_within.find(return_addr);

        if (function_within == functions_within.end()) {
            const auto result = utility::find_virtual_function_start(return_addr);

            if (result) {
                functions_within[return_addr] = *result;
            } else {
                functions_within[return_addr] = 0;
            }

            function_within = functions_within.find(return_addr);

            if (function_within->second != 0) {
                ++function_infos[function_within->second].count;
            }

            function_infos[function_within->second].return_addrs.push_back(return_addr);

            SPDLOG_INFO("Added new call of UpdateViewportRHI to function {:x} (count: {})", function_within->second, function_infos[function_within->second].count);
        }

        if (function_within->second == 0) {
            SPDLOG_INFO_ONCE("Could not find vfunc start for call of UpdateViewportRHI, calling original.");
            call_orig();
            return;
        }

        const auto& function_info = function_infos[function_within->second]; 

        // We only care about corrections where UpdateViewportRHI is called more than once in the same function.
        if (function_info.count <= 1 || function_info.return_addrs.empty()) {
            call_orig();
            return;
        }

        if (!g_hook->m_rendertarget_manager_embedded_in_stereo_device) {
            const auto rtm = g_hook->get_render_target_manager();

            if (rtm != nullptr) {
                if (const auto offset = rtm->get_viewport_force_separate_rt_offset()) {
                    auto& should_force_separate_rt = *(bool*)((uintptr_t)viewport + *offset);
                    auto& use_separate_rt = *(bool*)((uintptr_t)viewport + (*offset - 1));

                    if (!should_force_separate_rt) {
                        SPDLOG_INFO_ONCE("UpdateViewportRHI was called without should_force_separate_rt being set to true, skipping.");
                        should_force_separate_rt = true;
                        use_separate_rt = true;
                        modified_use_separate_rt = true;
                        return; // NO!!!!!!!!!!!!!!!!!!!
                    }
                }
            }
        } else {      
            // We only want the last function to be called.
            // We don't need to call the original here because it will get called by the last function.
            // if we call the original here, it will cause performance issues.
            if (function_info.return_addrs.back() != return_addr) {
                return;
            }
        }
    }

    
    if (!g_hook->m_rendertarget_manager_embedded_in_stereo_device) {
        call_orig();
        return;
    }

    auto& rtm = g_hook->get_embedded_rtm();

    static std::chrono::steady_clock::time_point last_time_hmd_active{};
    static bool hmd_was_active = false;
    bool should_call_orig = false;

    if (hmd_active && !hmd_was_active) {
        last_time_hmd_active = std::chrono::steady_clock::now();
        hmd_was_active = true;
    } else if (!hmd_active) {
        hmd_was_active = false;
        should_call_orig = true;
    }

    if (hmd_active) {
        should_call_orig = std::chrono::steady_clock::now() - last_time_hmd_active <= std::chrono::milliseconds(2000);
        //should_call_orig = should_call_orig || (std::chrono::steady_clock::now() - rtm.last_time_needed_hmd_reallocate <= std::chrono::milliseconds(2000));
    }

    if (should_call_orig) {
        rtm.should_use_separate_rt_called = false;
        rtm.need_reallocate_viewport_render_target_called = false;
        call_orig();
        return;
    }

    if (!rtm.should_use_separate_rt_called) {
        SPDLOG_INFO_ONCE("Skipping UpdateViewportRHI (embedded) because ShouldUseSeparateRenderTarget() was not called!");
        return; // Do not call at all.
    }

    if (!rtm.need_reallocate_viewport_render_target_called) {
        const auto need_reallocate = g_hook->get_render_target_manager()->need_reallocate_view_target(*(FViewport*)viewport);

        if (!need_reallocate) {
            SPDLOG_INFO_ONCE("Skipping UpdateViewportRHI (embedded) because NeedReallocateViewportRenderTarget() was not called and we don't need to reallocate anyway!");
            rtm.should_use_separate_rt_called = false;
            return; // Do not call at all.
        }

        SPDLOG_INFO_ONCE("We need to reallocate the viewport render target even though NeedReallocateViewportRenderTarget() was not called!");
        //rtm.last_time_needed_hmd_reallocate = std::chrono::steady_clock::now();
    }

    call_orig();
    rtm.should_use_separate_rt_called = false;
    rtm.need_reallocate_viewport_render_target_called = false;
}

void FFakeStereoRenderingHook::attempt_hook_update_viewport_rhi(uintptr_t return_address) {
    if (/*!m_rendertarget_manager_embedded_in_stereo_device ||*/ m_special_detected || m_attempted_hook_update_viewport_rhi) {
        return;
    }

    m_attempted_hook_update_viewport_rhi = true;

    if (m_update_viewport_rhi_hook == nullptr) {
        SPDLOG_INFO("Attempting to hook UpdateViewportRHI...");

        const auto init_dynamic_rhi = utility::find_virtual_function_start(return_address);

        if (init_dynamic_rhi) {
            SPDLOG_INFO("Found InitDynamicRHI: {:x}", *init_dynamic_rhi);

            const auto init_dynamic_rhi_ptr = utility::scan_ptr(*utility::get_module_within(*init_dynamic_rhi), *init_dynamic_rhi);
            if (!init_dynamic_rhi_ptr) {
                SPDLOG_ERROR("Failed to find InitDynamicRHI pointer!");
                return;
            }

            const auto update_viewport_rhi_ptr = *init_dynamic_rhi_ptr - (sizeof(void*) * 2);

            if (*(void**)update_viewport_rhi_ptr == nullptr || IsBadReadPtr(*(void**)update_viewport_rhi_ptr, sizeof(void*))) {
                SPDLOG_ERROR("Failed to find UpdateViewportRHI!");
                return;
            }

            // Make sure this is no displacement reference to this. This can mean we accidentally found the vtable for IViewportRenderTargetProvider
            // The vfunc pointer should be in the middle of the vtable, not the start.
            if (utility::scan_displacement_reference(*utility::get_module_within(*init_dynamic_rhi), update_viewport_rhi_ptr)) {
                SPDLOG_ERROR("Found displacement reference to UpdateViewportRHI, this is probably the vtable for IViewportRenderTargetProvider, aborting!");
                return;
            }

            m_update_viewport_rhi_hook = std::make_unique<PointerHook>((void**)update_viewport_rhi_ptr, &update_viewport_rhi_hook);
        } else {
            SPDLOG_ERROR("Failed to find InitDynamicRHI, cannot hook UpdateViewportRHI!");
        }
    }
}

bool VRRenderTargetManager_Base::allocate_render_target_texture(uintptr_t return_address, FTexture2DRHIRef* tex, FTexture2DRHIRef* shader_resource) {
    this->texture_hook_ref = tex;
    this->shader_resource_hook_ref = shader_resource;
    this->allocate_texture_called = true;

    if (!this->set_up_texture_hook) {
        ZoneScopedN("VRRenderTargetManager_Base::allocate_render_target_texture initialization");
        SPDLOG_INFO("AllocateRenderTargetTexture retaddr: {:x}", return_address);

        g_hook->attempt_hook_update_viewport_rhi(return_address);

        SPDLOG_INFO("Scanning for call instr...");

        bool next_call_is_not_the_right_one = false;

        auto is_string_nearby = [](uintptr_t addr, std::wstring_view str) {
            const auto addr_module = utility::get_module_within(addr);
            if (!addr_module) {
                return false;
            }

            const auto module_size = utility::get_module_size(*addr_module);
            const auto module_end = (uintptr_t)*addr_module + *module_size - 0x1000;

            // Find all possible strings, not just the first one
            for (auto str_addr = utility::scan_string(*addr_module, str.data(), true); 
                str_addr.has_value(); 
                str_addr = utility::scan_string(*str_addr + 1, (module_end - (*str_addr + 1)), str.data(), true)) 
            {
                const auto string_ref = utility::scan_displacement_reference(*addr_module, (uintptr_t)*str_addr);

                if (string_ref) {
                    const auto string_ref_func_start = utility::find_function_start((uintptr_t)*string_ref);
                    const auto return_addr_func_start = utility::find_function_start(addr);

                    SPDLOG_INFO("String ref func start: {:x}", (uintptr_t)*string_ref_func_start);
                    SPDLOG_INFO("Return addr func start: {:x}", (uintptr_t)*return_addr_func_start);

                    if (string_ref_func_start && return_addr_func_start && *string_ref_func_start == *return_addr_func_start) {
                        return true;
                    }
                }
            }

            return false;
        };

        // This string is present in UE5 (>= 5.1) and used when using texture descriptors to create textures.
        // that means this is UE5 and the function will take a texture descriptor instead of a bunch of arguments.
        if (is_string_nearby(return_address, L"BufferedRT")) {
            SPDLOG_INFO("Found string ref for BufferedRT, this is UE5!");
            this->is_using_texture_desc = true;
            this->is_version_greq_5_1 = true;
        }

        // Present in a specific game or game(s), somewhere around 4.8-4.12 (?)
        // indicates that texture descriptors are being used.
        if (is_string_nearby(return_address, L"SceneViewBuffer")) {
            SPDLOG_INFO("Found string ref for SceneViewBuffer, texture descriptors are being used!");
            this->is_using_texture_desc = true;
            this->is_version_greq_5_1 = false;

            next_call_is_not_the_right_one = true; // not seen a case where this isn't true (yet)
        }

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

        const std::vector<std::string> bad_patterns_before_call = {
            "B2 32", // mov dl, 32h, (seen in UE5 debug/dev builds)
            "B2 2A", // mov dl, 2Ah, (seen in UE4.23 debug/dev builds)
            "B2 2B", // mov dl, 2Bh, (seen in UE4.25 debug/dev builds)
            "BA 2F 00 00 00", // mov edx, 2Fh (seen in UE5 debug/dev builds)
            "F6 85 ? ? ? ? 05", // test byte ptr [rbp+?], 5 (seen in UE5 debug/dev builds)
        };

        while(true) {
            if (emu.ctx->InstructionsCount > 200) {
                break;
            }

            const auto ip = emu.ctx->Registers.RegRip;
            const auto bytes = (uint8_t*)ip;
            const auto decoded = utility::decode_one((uint8_t*)ip);

            if (ip != 0) {
                for (const auto& pattern : bad_patterns_before_call) {
                    if (utility::scan(ip, 100, pattern).value_or(0) == ip) {
                        SPDLOG_INFO("Found bad pattern before call, skipping next call: {:x} ({})", ip, pattern);
                        next_call_is_not_the_right_one = true;
                        break;
                    }
                }
            }
            
            if (!next_call_is_not_the_right_one) try {
                const auto addr = utility::resolve_displacement(ip);

                if (addr && !IsBadReadPtr((void*)*addr, 12) && std::wstring_view{(const wchar_t*)*addr}.starts_with(L"BufferedRT")) {
                    this->is_using_texture_desc = true;
                    this->is_version_greq_5_1 = true;

                    SPDLOG_INFO("Found usage of string \"BufferedRT\" while analyzing AllocateRenderTargetTexture!");
                }
            } catch(...) {

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
                    if (is_call && !next_call_is_not_the_right_one && bytes[0] == 0xE8) try {
                        // Analyze some of the instructions inside the call first
                        // If it has a mov eax, 0x800, then returns, we can skip this function
                        const auto fn = utility::calculate_absolute(ip + 1);
                        SPDLOG_INFO("Analyzing call at {:x} to {:x}", ip, fn);

                        if (auto result = utility::scan(fn, 10, "41 B8 30 00 00 00"); result.has_value() && *result == fn) {
                            SPDLOG_INFO("First instruction is a mov r8d, 30h, skipping this call!");
                            next_call_is_not_the_right_one = true;
                        } else if (auto result = utility::scan(fn, 50, "B8 00 08 00 00 C3"); result.has_value()) {
                            SPDLOG_INFO("First few instructions are a mov eax, 800h, ret, skipping this call!");
                            next_call_is_not_the_right_one = true;
                        }
                    } catch(...) {
                        SPDLOG_INFO("Failed to analyze call at {:x}", ip);
                    }

                    if (is_call && !next_call_is_not_the_right_one && bytes[0] == 0xFF && bytes[1] == 0x15) {
                        // well this definitely is not the right one, indirect calls have never called the function we wanted (I think)
                        SPDLOG_INFO("Found indirect call @ {:x}, skipping", ip);
                        next_call_is_not_the_right_one = true;
                    }

                    if (is_call && !next_call_is_not_the_right_one) {
                        const auto post_call = (uintptr_t)ip + decoded->Length;
                        SPDLOG_INFO("AllocateRenderTargetTexture post_call: {:x}, rel {:x}", post_call, post_call - (uintptr_t)*utility::get_module_within((void*)post_call));

                        if (*(uint8_t*)ip == 0xE8) {
                            SPDLOG_INFO("E8 call found!");
                            this->is_pre_texture_call_e8 = true;
                        } else {
                            SPDLOG_INFO("E8 call not found, assuming register call!");
                        }

                        // So we can call the original texture create function again.
                        this->texture_create_insn_bytes.resize(decoded->Length);
                        memcpy(this->texture_create_insn_bytes.data(), (void*)ip, decoded->Length);

                        this->texture_hook = safetyhook::create_mid((void*)post_call, &VRRenderTargetManager::texture_hook_callback);
                        this->pre_texture_hook = safetyhook::create_mid((void*)ip, &VRRenderTargetManager::pre_texture_hook_callback);
                        this->set_up_texture_hook = true;

                        return false;
                    }

                    SPDLOG_INFO("Skipping write to memory instruction at {:x} ({:x} bytes, landing at {:x})", ip, decoded->Length, ip + decoded->Length);
                    emu.ctx->Registers.RegRip += decoded->Length;
                    emu.ctx->Instruction = *decoded; // pseudo-emulate the instruction
                    ++emu.ctx->InstructionsCount;

                    if (is_call) {
                        next_call_is_not_the_right_one = false;
                    }
                } else if (emu.emulate() != SHEMU_SUCCESS) { // only emulate the non-memory write instructions
                    SPDLOG_INFO("Emulation failed at {:x} ({:x} bytes, landing at {:x})", ip, decoded->Length, ip + decoded->Length);
                    // instead of just adding it onto the RegRip, we need to use the ip we had previously from the decode
                    // because the emulator can move the instruction pointer after emulate() is called
                    emu.ctx->Registers.RegRip = ip + decoded->Length;
                    continue;
                }
            } else {
                break;
            }
        }

        SPDLOG_ERROR("Failed to find call instruction!");
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
    SPDLOG_INFO("AllocateRenderTargetTexture called from: {:x}", m_last_allocate_render_target_return_address - (uintptr_t)*utility::get_module_within((void*)m_last_allocate_render_target_return_address));

    // So, if CalculateRenderTargetSize was *never* called before this function
    // that means we have the virtual index of this function wrong, and we must swap the vtable out.
    // also, if this function was called very close to NeedReallocateDepthTexture, that also means
    // the virtual index is wrong, and we must swap the vtable out.
    const auto is_incorrect_vtable = 
        m_last_calculate_render_size_return_address == 0 ||
        m_last_allocate_render_target_return_address - m_last_needs_reallocate_depth_texture_return_address <= 0x200;

    if (is_incorrect_vtable) {
        // oh no this is the wrong vtable!!!! we need to fix it  nOW!!!
        SPDLOG_INFO("AllocateRenderTargetTexture called instead of AllocateDepthTexture! Fixing...");
        SPDLOG_INFO("Switching to old render target manager! Incorrect function called!");
        //g_hook->switch_to_old_rendertarget_manager();

        // Do a switcharoo on the vtable of this object to the old one because we will crash if we don't.
        // I've decided against actually switching the entire object over in favor of just vtable
        // swapping for now even though it's kind of a hack.
        const auto fake_object = std::make_unique<VRRenderTargetManager_418>();
        *(void**)this = *(void**)fake_object.get();

        return false;
    }

    this->depth_analysis_passed = true;

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
