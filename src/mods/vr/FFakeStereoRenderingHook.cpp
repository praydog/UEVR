#include <winternl.h>

#include <spdlog/spdlog.h>
#include <utility/Memory.hpp>
#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include <bddisasm.h>
#include <disasmtypes.h>

#include "../VR.hpp"

#include "FFakeStereoRenderingHook.hpp"

FFakeStereoRenderingHook* g_hook = nullptr;

std::optional<uintptr_t> find_function_start(uintptr_t middle, uintptr_t module = (uintptr_t)utility::get_executable()) {
    const auto middle_rva = middle - module;

    // This function abuses the fact that most non-obfuscated binaries have
    // an exception directory containing a list of function start and end addresses.
    // Get the PE header, and then the exception directory
    const auto dos_header = (PIMAGE_DOS_HEADER)module;
    const auto nt_header = (PIMAGE_NT_HEADERS)((uintptr_t)dos_header + dos_header->e_lfanew);
    const auto exception_directory = (PIMAGE_DATA_DIRECTORY)&nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

    // Get the exception directory RVA and size
    const auto exception_directory_rva = exception_directory->VirtualAddress;
    const auto exception_directory_size = exception_directory->Size;

    // Get the exception directory
    const auto exception_directory_ptr = (PIMAGE_RUNTIME_FUNCTION_ENTRY)((uintptr_t)dos_header + exception_directory_rva);

    // Get the number of entries in the exception directory
    const auto exception_directory_entries = exception_directory_size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);

    for (auto i = 0; i < exception_directory_entries; i++) {
        const auto entry = exception_directory_ptr[i];

        // Check if the middle address is within the range of the function
        if (entry.BeginAddress <= middle_rva && middle_rva <= entry.EndAddress) {
            // Return the start address of the function
            return module + entry.BeginAddress;
        }
    }

    return std::nullopt;
}

std::optional<uintptr_t> find_function_from_string_ref(std::wstring_view str, HMODULE module = utility::get_executable()) {
    const auto str_data = utility::scan_string(module, str.data());

    if (!str_data) {
        spdlog::error("Failed to find string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto str_ref = utility::scan_reference(module, *str_data);

    if (!str_ref) {
        spdlog::error("Failed to find reference to string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto func_start = find_function_start(*str_ref);

    if (!func_start) {
        spdlog::error("Failed to find function start for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    return func_start;
}

std::optional<uintptr_t> find_cvar_by_description(std::wstring_view str) {
    auto found_in_module = utility::get_executable();
    auto str_data = utility::scan_string(utility::get_executable(), str.data());

    if (!str_data) {
        found_in_module = utility::find_partial_module(L"-Renderer-Win64-Shipping.dll");

        if (found_in_module != nullptr) {
            str_data = utility::scan_string(found_in_module, str.data());
        }
    }

    if (!str_data) {
        spdlog::error("Failed to find string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto str_ref = utility::scan_reference(found_in_module, *str_data);

    if (!str_ref) {
        spdlog::error("Failed to find reference to string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto cvar_creation_ref = utility::scan_mnemonic(*str_ref, 100, "CALL");

    if (!cvar_creation_ref) {
        spdlog::error("Failed to find cvar creation reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto raw_cvar_ref = utility::scan_mnemonic(*cvar_creation_ref + utility::get_insn_size(*cvar_creation_ref), 100, "CALL");

    if (!raw_cvar_ref) {
        spdlog::error("Failed to find raw cvar reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    // Look for a mov {ptr}, rax
    auto ip = (uint8_t*)*raw_cvar_ref;

    for (auto i = 0; i < 100; ++i) {
        INSTRUX ix{};
        const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

        if (!ND_SUCCESS(status)) {
            spdlog::error("Failed to decode instruction for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG &&
            ix.Operands[1].Info.Register.Reg == NDR_RAX) 
        {
            return (uintptr_t)(ip + ix.Length + ix.Operands[0].Info.Memory.Disp);
        }

        ip += ix.Length;
    }

    spdlog::error("Failed to find cvar for {}", utility::narrow(str.data()));
    return std::nullopt;
}

FFakeStereoRenderingHook::FFakeStereoRenderingHook() {
    g_hook = this;
    hook();
}

bool FFakeStereoRenderingHook::hook() {
    spdlog::info("Entering FFakeStereoRenderingHook::hook");

    const auto game = (uintptr_t)utility::get_executable();
    const auto engine_dll = utility::find_partial_module(L"-Engine-Win64-Shipping.dll");
    const auto vtable = locate_fake_stereo_rendering_vtable();

    if (!vtable) {
        spdlog::error("Failed to locate Fake Stereo Rendering VTable");
        return false;
    }

    const auto module_vtable_within = utility::get_module_within(*vtable);
    const auto stereo_view_offset_index = get_stereo_view_offset_index(*vtable);

    if (!stereo_view_offset_index) {
        spdlog::error("Failed to locate Stereo View Offset Index");
        return false;
    }

    const auto stereo_projection_matrix_index = *stereo_view_offset_index + 1;
    const auto is_4_18 = stereo_view_offset_index <= 6;

    const auto stereo_view_offset_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index];

    auto render_texture_render_thread_func = find_function_from_string_ref(L"RenderTexture_RenderThread");

    if (!render_texture_render_thread_func && engine_dll != nullptr) {
         render_texture_render_thread_func = find_function_from_string_ref(L"RenderTexture_RenderThread", engine_dll);
    }

    if (!render_texture_render_thread_func) {
        // Fallback scan to checking for the first non-default virtual function (4.18)
        spdlog::info("Failed to find RenderTexture_RenderThread, falling back to first non-default virtual function");

        for (auto i = 2; i < 10; ++i) {
            const auto func = ((uintptr_t*)*vtable)[stereo_projection_matrix_index + i];

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
    auto rendertexture_fn_vtable_middle = utility::scan_ptr((HMODULE)game, *render_texture_render_thread_func);

    if (!rendertexture_fn_vtable_middle && engine_dll != nullptr) {
        rendertexture_fn_vtable_middle = utility::scan_ptr((HMODULE)engine_dll, *render_texture_render_thread_func);
    }

    if (!rendertexture_fn_vtable_middle) {
        spdlog::error("Failed to find RenderTexture_RenderThread VTable Middle");
        return false;
    }

    spdlog::info("RenderTexture_RenderThread VTable Middle: {:x}", (uintptr_t)*rendertexture_fn_vtable_middle);

    // This is NOT confirmed to work on 4.18, more investigation is needed!!!!!
    const auto get_render_target_manager_func_ptr =
        (uintptr_t)(*rendertexture_fn_vtable_middle + sizeof(void*) + (sizeof(void*) * 4 * (size_t)is_4_18));

    if (!get_render_target_manager_func_ptr) {
        spdlog::error("Failed to find GetRenderTargetManager");
        return false;
    }

    spdlog::info("GetRenderTargetManagerptr: {:x}", (uintptr_t)get_render_target_manager_func_ptr);

    // In 4.18 the destructor virtual doesn't exist.
    const auto is_stereo_enabled_index = utility::scan(*(uintptr_t*)*vtable, 3, "B0 01 C3").value_or(0) == *(uintptr_t*)*vtable ? 0 : 1;
    spdlog::info("IsStereoEnabled Index: {}", is_stereo_enabled_index);
    const auto is_stereo_enabled_func_ptr = &((uintptr_t*)*vtable)[is_stereo_enabled_index];

    const auto adjust_view_rect_distance = is_4_18 ? 2 : 3;

    const auto adjust_view_rect_index = *stereo_view_offset_index - adjust_view_rect_distance;
    auto calculate_stereo_projection_matrix_index = *stereo_view_offset_index + 1;

    // While generally most of the time the stereo projection matrix func is the next one after the stereo view offset func,
    // it's not always the case. We can scan for a call to the tanf function in one of the virtual functions to find it.
    for (auto i = 0; i < 10; ++i) {
        const auto potential_func = ((uintptr_t*)*vtable)[calculate_stereo_projection_matrix_index + i];
        if (potential_func == 0 || utility::is_stub_code((uint8_t*)potential_func) || IsBadReadPtr((void*)potential_func, 1)) {
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

            if (ix.InstructionBytes[0] == 0xE8) {
                const auto called_func = (uintptr_t)(ip + ix.Length + (int32_t)ix.RelativeOffset);
                const auto inner_ins = utility::decode_one((uint8_t*)called_func);

                spdlog::info("called {:x}", (uintptr_t)called_func);

                // Check if this function is jmping into the "tanf" export in ucrtbase.dll
                if (inner_ins) {
                    if (inner_ins->InstructionBytes[0] == 0xFF && inner_ins->InstructionBytes[1] == 0x25) {
                        const auto called_func_ptr = (uintptr_t*)(called_func + inner_ins->Length + (int32_t)inner_ins->Displacement);
                        const auto called_func_ptr_val = *called_func_ptr;

                        spdlog::info("called ptr {:x}", (uintptr_t)called_func_ptr_val);

                        const auto module_within = utility::get_module_within(called_func_ptr_val);

                        if (module_within && called_func_ptr_val == (uintptr_t)GetProcAddress(*module_within, "tanf")) {
                            spdlog::info("Found CalculateStereoProjectionMatrix: {} {:x}", calculate_stereo_projection_matrix_index + i, called_func);
                            calculate_stereo_projection_matrix_index += i;
                            found = true;
                            break;
                        } else {
                            spdlog::info("Function did not call tanf, skipping");
                        }
                    } else {
                        spdlog::info("Inner instruction was not a jmp [{}]", inner_ins->Mnemonic);
                    }
                } else {
                    spdlog::info("Failed to decode inner instruction");
                }
            }

            ip += ix.Length;
        }

        if (found) {
            break;
        }
    }

    // const auto init_canvas_index = *stereo_view_offset_index + 4;

    const auto adjust_view_rect_func = ((uintptr_t*)*vtable)[adjust_view_rect_index];
    const auto calculate_stereo_projection_matrix_func = ((uintptr_t*)*vtable)[calculate_stereo_projection_matrix_index];
    // const auto render_texture_render_thread_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index + 3];

    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    spdlog::info("AdjustViewRect: {:x}", (uintptr_t)adjust_view_rect_func);
    spdlog::info("CalculateStereoProjectionMatrix: {:x}", (uintptr_t)calculate_stereo_projection_matrix_func);
    spdlog::info("IsStereoEnabled: {:x}", (uintptr_t)*is_stereo_enabled_func_ptr);

    m_adjust_view_rect_hook = builder.create_inline((void*)adjust_view_rect_func, adjust_view_rect);
    m_calculate_stereo_view_offset_hook = builder.create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
    m_calculate_stereo_projection_matrix_hook = builder.create_inline((void*)calculate_stereo_projection_matrix_func, calculate_stereo_projection_matrix);
    m_render_texture_render_thread_hook = builder.create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);

    // This requires a pointer hook because the virtual just returns false
    // compiler optimization makes that function get re-used in a lot of places
    // so it's not feasible to just detour it, we need to replace the pointer in the vtable.
    m_get_render_target_manager_hook = std::make_unique<PointerHook>((void**)get_render_target_manager_func_ptr, (void*)&get_render_target_manager_hook);
    m_is_stereo_enabled_hook = std::make_unique<PointerHook>((void**)is_stereo_enabled_func_ptr, (void*)&is_stereo_enabled);

    spdlog::info("Leaving FFakeStereoRenderingHook::hook");

    const auto backbuffer_format_cvar = find_cvar_by_description(L"Defines the default back buffer pixel format.");

    // In 4.18 this doesn't exist. Not much we can do about that.
    if (backbuffer_format_cvar) {
        spdlog::info("Backbuffer Format CVar: {:x}", (uintptr_t)*backbuffer_format_cvar);
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0) = 0;   // 8bit RGBA, which is what VR headsets support
        *(int32_t*)(*(uintptr_t*)*backbuffer_format_cvar + 0x4) = 0; // 8bit RGBA, which is what VR headsets support
    } else {
        spdlog::error("Failed to find backbuffer format cvar, continuing anyways...");
    }

    spdlog::info("Finished hooking FFakeStereoRendering!");

    return true;
}

std::optional<uintptr_t> FFakeStereoRenderingHook::locate_fake_stereo_rendering_vtable() {
    const auto engine_dll = utility::find_partial_module(L"-Engine-Win64-Shipping.dll");

    spdlog::info("Engine: {:p}, size {:x}", (void*)engine_dll, *utility::get_module_size(engine_dll));

    auto fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationHeight");

    if (!fake_stereo_rendering_constructor && engine_dll != nullptr) {
        fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationHeight", engine_dll);
    }

    if (!fake_stereo_rendering_constructor) {
        fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationFOV");

        if (!fake_stereo_rendering_constructor && engine_dll != nullptr) {
            fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationFOV", engine_dll);
        }

        if (!fake_stereo_rendering_constructor) {
            spdlog::error("Failed to find FFakeStereoRendering constructor");
            return std::nullopt;
        }
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

    return vtable;
}

std::optional<uint32_t> FFakeStereoRenderingHook::get_stereo_view_offset_index(uintptr_t vtable) {
    for (auto i = 0; i < 30; ++i) {
        const auto func = ((uintptr_t*)vtable)[i];

        if (func == 0) {
            continue;
        }

        auto ip = (uint8_t*)func;
        uint32_t xmm_register_usage_count = 0;

        for (auto j = 0; j < 1000; ++j) {
            if (*ip == 0xC3 || *ip == 0xCC || *ip == 0xE9) {
                break;
            }

            INSTRUX ix{};
            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                spdlog::info("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            ip += ix.Length;

            char txt[ND_MIN_BUF_SIZE]{};
            NdToText(&ix, 0, sizeof(txt), txt);

            if (std::string_view{txt}.find("xmm") != std::string_view::npos) {
                ++xmm_register_usage_count;

                if (xmm_register_usage_count >= 10) {
                    spdlog::info("Found Stereo View Offset Index: {}", i);
                    return i;
                }
            }
        }
    }

    return std::nullopt;
}

bool FFakeStereoRenderingHook::is_stereo_enabled(FFakeStereoRendering* stereo) {
    return !VR::get()->get_runtime()->got_first_sync || VR::get()->is_hmd_active();
}

void FFakeStereoRenderingHook::adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) {
    *w = VR::get()->get_hmd_width() * 2;
    *h = VR::get()->get_hmd_height();

    *w = *w / 2;
    *x += *w * (index % 2); // on some versions the index is actually the pass... figure out how to detect that
}

void FFakeStereoRenderingHook::calculate_stereo_view_offset(
    FFakeStereoRendering* stereo, const int32_t view_index, Rotator* view_rotation, 
    const float world_to_meters, Vector3f* view_location)
{
    const auto view_mat = glm::yawPitchRoll(
        glm::radians(view_rotation->yaw),
        glm::radians(view_rotation->pitch),
        glm::radians(view_rotation->roll));

    const auto view_mat_inverse = glm::yawPitchRoll(
        glm::radians(-view_rotation->yaw),
        glm::radians(view_rotation->pitch),
        glm::radians(view_rotation->roll));

    const auto view_quat_inverse = glm::quat {
        view_mat_inverse
    };

    const auto view_quat = glm::quat {
        view_mat
    };

    auto vr = VR::get();
    const auto rotation_offset = vr->get_rotation_offset();
    const auto current_hmd_rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(0)});

    const auto new_rotation = glm::normalize(view_quat_inverse * current_hmd_rotation);

    const auto true_index = ((view_index) % 2);
    const auto eye_offset = glm::vec3{vr->get_eye_offset((VRRuntime::Eye)(true_index))};

    const auto pos = glm::vec3{rotation_offset * ((vr->get_position(0) - vr->get_standing_origin()))};

    const auto quat_asdf = glm::quat{Matrix4x4f {
        0, 0, -1, 0,
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0, 1
    }};

    *view_location -= quat_asdf * (glm::normalize(view_quat_inverse) * (pos * world_to_meters));
    *view_location -= quat_asdf * (glm::normalize(new_rotation) * (eye_offset * world_to_meters));

    const auto euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

    view_rotation->pitch = euler.x;
    view_rotation->yaw = euler.y;
    view_rotation->roll = euler.z;
}

Matrix4x4f* FFakeStereoRenderingHook::calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
    g_hook->m_calculate_stereo_projection_matrix_hook->call<Matrix4x4f*>(stereo, out, view_index);

    // spdlog::info("NearZ: {}", old_znear);

    if (out != nullptr) {
        float old_znear = (*out)[3][2];
        VR::get()->m_nearz = old_znear;
        VR::get()->get_runtime()->update_matrices(old_znear, 10000.0f);
        *out = VR::get()->get_projection_matrix((VRRuntime::Eye)(view_index % 2));
    }
    return out;
}

void FFakeStereoRenderingHook::render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
    FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
    // spdlog::info("{:x}", (uintptr_t)src_texture->GetNativeResource());

    // maybe the window size is actually a pointer we will find out later.
    // g_hook->m_render_texture_render_thread_hook->call<void*>(stereo, rhi_command_list, backbuffer, src_texture, window_size);
}

IStereoRenderTargetManager* FFakeStereoRenderingHook::get_render_target_manager_hook(FFakeStereoRendering* stereo) {
    if (!VR::get()->get_runtime()->got_first_poses || VR::get()->is_hmd_active()) {
        return &g_hook->m_rtm;
    }

    return nullptr;
}

void VRRenderTargetManager::CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) {
    InOutSizeX = VR::get()->get_hmd_width() * 2;
    InOutSizeY = VR::get()->get_hmd_height();

    spdlog::info("RenderTargetSize: {}x{}", InOutSizeX, InOutSizeY);
}

void VRRenderTargetManager::texture_hook_callback(safetyhook::Context& ctx) {
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
    spdlog::info("Real resource: {:x}", (uintptr_t)texture->GetNativeResource());

    rtm->render_target = texture;
    rtm->texture_hook_ref = nullptr;
    ++rtm->last_texture_index;
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
    this->texture_hook_ref = &OutTargetableTexture;

    if (!this->set_up_texture_hook) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        spdlog::info("AllocateRenderTargetTexture retaddr: {:x}", return_address);

        spdlog::info("Scanning for call instr...");
        auto ip = (uint8_t*)return_address;

        for (auto i = 0; i < 100; ++i) {
            INSTRUX ix{};
            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                spdlog::error("Decoding failed with error {:x}!", (uint32_t)status);
                return false;
            }

            // We are looking for the call instruction
            // This instruction calls RHICreateTargetableShaderResource2D(TexSizeX, TexSizeY, SceneTargetFormat, 1, TexCreate_None,
            // TexCreate_RenderTargetable, false, CreateInfo, BufferedRTRHI, BufferedSRVRHI); Which sets up the BufferedRTRHI and
            // BufferedSRVRHI variables.
            if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                const auto post_call = (uintptr_t)ip + ix.Length;
                spdlog::info("AllocateRenderTargetTexture post_call: {:x}", post_call);

                auto factory = SafetyHookFactory::init();
                auto builder = factory->acquire();

                this->texture_hook = builder.create_mid((void*)post_call, &VRRenderTargetManager::texture_hook_callback);
                this->set_up_texture_hook = true;

                return false;
            }

            ip += ix.Length;
        }

        spdlog::error("Failed to find call instruction!");
    }

    return false;
}
