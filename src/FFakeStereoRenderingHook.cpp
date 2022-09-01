#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>

#include <disasmtypes.h>
#include <bddisasm.h>

#include "FFakeStereoRenderingHook.hpp"

FFakeStereoRenderingHook* g_hook = nullptr;

std::optional<uintptr_t> find_function_start(uintptr_t middle) {
    const auto middle_rva = middle - (uintptr_t)utility::get_executable();

    // This function abuses the fact that most non-obfuscated binaries have
    // an exception directory containing a list of function start and end addresses.
    // Get the PE header, and then the exception directory
    const auto dos_header = (PIMAGE_DOS_HEADER)utility::get_executable();
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
            return (uintptr_t)utility::get_executable() + entry.BeginAddress;
        }
    }

    return std::nullopt;
}

std::optional<uintptr_t> find_function_from_string_ref(std::wstring_view str) {
    const auto str_data = utility::scan_string(utility::get_executable(), str.data());

    if (!str_data) {
        spdlog::error("Failed to find string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto str_ref = utility::scan_reference(utility::get_executable(), *str_data);

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

FFakeStereoRenderingHook::FFakeStereoRenderingHook() {
    g_hook = this;
    hook();
}

void FFakeStereoRenderingHook::calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator& view_rotation, const float world_to_meters, Vector3f& view_location) {
    static float s_t{0.0f};
    const auto t = cos(s_t);

    if (view_index % 2 == 0) {
        //view_rotation.x += t * 25.0f;
        view_location.x += t * 100.0f;
    } else {
        //view_rotation.x -= t * 25.0f;
        view_location.x -= t * 100.0f;
    }

    s_t += 0.01f;
}

Matrix4x4f* FFakeStereoRenderingHook::calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f& out, const int32_t view_index) {
    // TODO: grab from VR runtime
    return g_hook->m_calculate_stereo_projection_matrix_hook->call<Matrix4x4f*>(stereo, out, view_index);
}

void FFakeStereoRenderingHook::render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) {
    /*spdlog::info("hello");

    spdlog::info("{}", utility::rtti::get_type_info(src_texture->GetNativeResource())->name());

    // maybe the window size is actually a pointer we will find out later.
    g_rtrt->call<void*>(stereo, rhi_command_list, backbuffer, src_texture, window_size);*/
}

IStereoRenderTargetManager* FFakeStereoRenderingHook::get_render_target_manager(FFakeStereoRendering* stereo) {
    spdlog::info("is this working?");
    return &g_hook->m_rtm;
}

bool FFakeStereoRenderingHook::hook() {
    const auto game = (uintptr_t)utility::get_executable();
    const auto vtable = locate_fake_stereo_rendering_vtable();

    if (!vtable) {
        spdlog::error("Failed to locate Fake Stereo Rendering VTable");
        return false;
    }

    const auto stereo_view_offset_index = get_stereo_view_offset_index(*vtable);

    if (!stereo_view_offset_index) {
        spdlog::error("Failed to locate Stereo View Offset Index");
        return false;
    }

    const auto stereo_view_offset_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index];
    const auto calculate_stereo_view_offset_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index + 1];
    //const auto render_texture_render_thread_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index + 3];

    const auto render_texture_render_thread_func = find_function_from_string_ref(L"RenderTexture_RenderThread");

    if (!render_texture_render_thread_func) {
        spdlog::error("Failed to find RenderTexture_RenderThread");
        return false;
    }

    spdlog::info("RenderTexture_RenderThread: {:x}", (uintptr_t)*render_texture_render_thread_func);

    // Scan for the function pointer, it should be in the middle of the vtable.
    const auto rendertexture_fn_vtable_middle = utility::scan_ptr((HMODULE)game, *render_texture_render_thread_func);

    if (!rendertexture_fn_vtable_middle) {
        spdlog::error("Failed to find RenderTexture_RenderThread VTable Middle");
        return false;
    }

    spdlog::info("RenderTexture_RenderThread VTable Middle: {:x}", (uintptr_t)*rendertexture_fn_vtable_middle);

    const auto get_render_target_manager_func_ptr = (uintptr_t)(*rendertexture_fn_vtable_middle + sizeof(void*));

    if (!get_render_target_manager_func_ptr) {
        spdlog::error("Failed to find GetRenderTargetManager");
        return false;
    }

    spdlog::info("GetRenderTargetManagerptr: {:x}", (uintptr_t)get_render_target_manager_func_ptr);

    const auto init_canvas_index = *stereo_view_offset_index + 4;
    
    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    m_calculate_stereo_view_offset_hook = builder.create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
    m_calculate_stereo_projection_matrix_hook = builder.create_inline((void*)calculate_stereo_view_offset_func, calculate_stereo_projection_matrix);
    //g_rtrt = builder.create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);

    m_get_render_target_manager_hook = std::make_unique<PointerHook>((void**)get_render_target_manager_func_ptr, (void*)&get_render_target_manager);

    return true;
}
std::optional<uintptr_t> FFakeStereoRenderingHook::locate_fake_stereo_rendering_vtable() {
    auto fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationHeight");

    if (!fake_stereo_rendering_constructor) {
        fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationFOV");

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

        for (auto j=0; j < 1000; ++j) {
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
