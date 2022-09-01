// dllmain
#include <windows.h>
#include <cstdint>
#include <SafetyHook.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <disasmtypes.h>
#include <bddisasm.h>


#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include "Math.hpp"

struct Rotator {
    float x, y, z;
};

struct FFakeStereoRendering;

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

std::optional<uintptr_t> locate_fake_stereo_rendering_vtable() {
    const auto game = (uintptr_t)utility::get_executable();
    const auto stereo_emulation_string = utility::scan_string((HMODULE)game, L"r.StereoEmulationHeight");

    if (stereo_emulation_string) {
        spdlog::info("Stereo Emulation String: {:x}", (uintptr_t)*stereo_emulation_string);
    } else {
        spdlog::error("Failed to find Stereo Emulation String");
        return std::nullopt;
    }

    const auto fake_stereo_rendering_constructor_mid = utility::scan_reference((HMODULE)game, *stereo_emulation_string);

    if (fake_stereo_rendering_constructor_mid) {
        spdlog::info("Fake Stereo Rendering Constructor Reference: {:x}", (uintptr_t)*fake_stereo_rendering_constructor_mid);
    } else {
        spdlog::error("Failed to find Fake Stereo Rendering Constructor");
        return std::nullopt;
    }

    spdlog::info("Fake Stereo Rendering Constructor (mid reference): {:x}", (uintptr_t)*fake_stereo_rendering_constructor_mid);

    const auto fake_stereo_rendering_constructor = find_function_start(*fake_stereo_rendering_constructor_mid);

    if (!fake_stereo_rendering_constructor) {
        spdlog::error("Failed to find Fake Stereo Rendering Constructor");
        return std::nullopt;
    }

    const auto vtable_ref = utility::scan(*fake_stereo_rendering_constructor, 100, "48 8D 05 ? ? ? ?");

    if (!vtable_ref) {
        spdlog::error("Failed to find Fake Stereo Rendering VTable Reference");
        return std::nullopt;
    }

    const auto vtable = utility::calculate_absolute(*vtable_ref + 3);

    if (!vtable) {
        spdlog::error("Failed to find Fake Stereo Rendering VTable");
        return std::nullopt;
    }

    spdlog::info("Fake Stereo Rendering VTable: {:x}", (uintptr_t)vtable);

    return vtable;
}

const std::optional<uint32_t> get_stereo_view_offset_index(uintptr_t vtable) {
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

std::unique_ptr<safetyhook::InlineHook> g_csvo{};
float g_t{0.0f};

void calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator& view_rotation, const float world_to_meters, Vector3f& view_location) {
    const auto t = cos(g_t);

    if (view_index % 2 == 0) {
        //view_rotation.x += t * 25.0f;
        view_location.x += t * 100.0f;
    } else {
        //view_rotation.x -= t * 25.0f;
        view_location.x -= t * 100.0f;
    }

    g_t += 0.01f;
}

std::unique_ptr<safetyhook::InlineHook> g_cspm{};

Matrix4x4f& calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f& out, const int32_t view_index) {
    //out = glm::identity<Matrix4x4f>();
    return out;
}

void startup_thread(HMODULE poc_module) {
    // create spdlog sink
    spdlog::set_default_logger(spdlog::basic_logger_mt("poc", "poc.log"));
    spdlog::flush_on(spdlog::level::info);
    spdlog::info("POC Entry");

    const auto game = (uintptr_t)utility::get_executable();
    const auto vtable = locate_fake_stereo_rendering_vtable();

    if (!vtable) {
        spdlog::error("Failed to locate Fake Stereo Rendering VTable");
        return;
    }

    const auto stereo_view_offset_index = get_stereo_view_offset_index(*vtable);

    if (!stereo_view_offset_index) {
        spdlog::error("Failed to locate Stereo View Offset Index");
        return;
    }

    const auto stereo_view_offset_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index];
    const auto calculate_stereo_view_offset_func = ((uintptr_t*)*vtable)[*stereo_view_offset_index + 1];
    
    auto factory = SafetyHookFactory::init();
    auto builder = factory->acquire();

    g_csvo = builder.create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
    g_cspm = builder.create_inline((void*)calculate_stereo_view_offset_func, calculate_stereo_projection_matrix);
}

BOOL APIENTRY DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)startup_thread, handle, 0, nullptr);
    }

    return TRUE;
}