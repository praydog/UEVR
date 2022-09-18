#include <winternl.h>

#include <spdlog/spdlog.h>
#include <utility/Memory.hpp>
#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/String.hpp>
#include <utility/Thread.hpp>

#include <bddisasm.h>
#include <disasmtypes.h>

#include "../VR.hpp"

#include "FFakeStereoRenderingHook.hpp"

FFakeStereoRenderingHook* g_hook = nullptr;

std::optional<uintptr_t> find_function_start(uintptr_t middle) {
    const auto module = (uintptr_t)*utility::get_module_within(middle);
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
    spdlog::info("Scanning module {} for string reference {}", *utility::get_module_path(module), utility::narrow(str));

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

// Finds a module matching the given name
// in a release build, it would be "-{name}-Win64-Shipping.dll"
// in the UnrealEditor, it would be "UnrealEditor-{name}.dll"
// otherwise, if everything is statically linked, it will just return the executable
HMODULE get_ue_module(const std::wstring& name) {
    const auto current_executable = utility::get_executable();
    const auto exe_name = utility::get_module_path(current_executable);

    if (exe_name && exe_name->ends_with("UnrealEditor.exe")) {
        const auto mod = utility::find_partial_module(L"UnrealEditor-" + name + L".dll");

        if (mod != nullptr) {
            return mod;
        }
    }

    const auto partial_module = utility::find_partial_module(L"-" + name + L"-Win64-Shipping.dll");

    if (partial_module != nullptr) {
        return partial_module;
    }

    return current_executable;
}

void** get_engine() {
    static void** engine = []() -> void** {
        spdlog::info("Attempting to locate GEngine...");

        const auto module = get_ue_module(L"Engine");
        const auto calibrate_tilt_fn = find_function_from_string_ref(L"CALIBRATEMOTION", module);

        if (!calibrate_tilt_fn) {
            spdlog::error("Failed to find CalibrateTilt function!");
            return (void**)nullptr;
        }

        spdlog::info("CalibrateTilt function: {:x}", (uintptr_t)*calibrate_tilt_fn);

        // Use bddisasm to find the first ptr mov into a register
        uint8_t* ip = (uint8_t*)*calibrate_tilt_fn;

        for (auto i = 0; i < 50; ++i) {
            INSTRUX ix{};
            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                spdlog::info("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM && ix.Operands[1].Info.Memory.IsRipRel) {
                const auto offset = ix.Operands[1].Info.Memory.Disp;
                const auto result = (void**)((uint8_t*)ip + ix.Length + offset);

                spdlog::info("Found GEngine at {:x}", (uintptr_t)result);
                return result;
            }

            ip += ix.Length;
        }

        spdlog::error("Failed to find GEngine!");
        return nullptr;
    }();

    return engine;
}

// In some games, likely due to obfuscation, the cvar description is missing
// so we must do an alternative scan for the cvar name itself, which is a bit tougher
// because the cvar name is usually referenced in multiple places, whereas
// the description is only referenced once, in the cvar registration function
std::optional<uintptr_t> find_alternate_cvar_ref(std::wstring_view str, uint32_t known_default, HMODULE module = utility::get_executable()) {
    spdlog::info("Performing alternate scan for cvar \"{}\"", utility::narrow(str));

    const auto str_data = utility::scan_string(module, str.data());

    if (!str_data) {
        spdlog::error("Failed to find string for cvar \"{}\"", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto module_base = (uintptr_t)module;
    const auto module_size = utility::get_module_size(module);
    auto str_ref = utility::scan_reference(module, *str_data);
    std::optional<uintptr_t> result{};

    while (str_ref) {
        // This is a last resort so maybe come up with something more robust later...
        std::array<uint8_t, 6+8> mov_r8d_mov_rsp { 
            0x41, 0xB8, 0x00, 0x00, 0x00, 0x00,
            0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00 
        };

        *(uint32_t*)&mov_r8d_mov_rsp[2] = known_default;
        *(uint32_t*)&mov_r8d_mov_rsp[10] = known_default;

        // Scan for this behind the string reference
        /*
        mov     r8d, 4
        mov     [rsp+20h], 4
        */
        result = utility::scan_data_reverse(*str_ref, 50, mov_r8d_mov_rsp.data(), mov_r8d_mov_rsp.size());

        if (result) {
            spdlog::info("Found alternate cvar reference at {:x}", *result);
            break;
        }

        const auto delta = *module_size - ((*str_ref + 1) - module_base);
        str_ref = utility::scan_reference(*str_ref + 1, delta, *str_data);
    }

    if (!result) {
        spdlog::error("Failed to find alternate cvar reference for \"{}\"", utility::narrow(str.data()));
        return std::nullopt;
    }
    
    return result;
}

std::optional<uintptr_t> resolve_cvar_from_address(uintptr_t start, std::wstring_view str) {
    const auto cvar_creation_ref = utility::scan_mnemonic(start, 100, "CALL");

    if (!cvar_creation_ref) {
        spdlog::error("Failed to find cvar creation reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    auto raw_cvar_ref = utility::scan_mnemonic(*cvar_creation_ref + utility::get_insn_size(*cvar_creation_ref), 100, "CALL");

    if (!raw_cvar_ref) {
        spdlog::error("Failed to find raw cvar reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    spdlog::info("Found raw cvar reference for \"{}\" at {:x}", utility::narrow(str.data()), *raw_cvar_ref);
    const auto decoded_ref = utility::decode_one((uint8_t*)*raw_cvar_ref);

    // we need to check that the reference uses a register in its operand
    // otherwise it's the wrong call. find the next call if it is.
    if (decoded_ref) {
        for (auto i = 0; i < decoded_ref->OperandsCount; ++i) {
            spdlog::info(" Operand type {}: {}", i, decoded_ref->Operands[i].Type);
        }

        if (decoded_ref->OperandsCount == 0 || 
            decoded_ref->Operands[0].Type != ND_OP_MEM || 
            decoded_ref->Operands[0].Info.Memory.Base == ND_REG_NOT_PRESENT)
        {
            spdlog::info("Scanning again, instruction at {:x} doesn't use a register", *raw_cvar_ref);
            raw_cvar_ref = utility::scan_mnemonic(*raw_cvar_ref + utility::get_insn_size(*raw_cvar_ref), 100, "CALL");

            if (raw_cvar_ref) {
                const auto decoded_ref = utility::decode_one((uint8_t*)*raw_cvar_ref);

                for (auto i = 0; i < decoded_ref->OperandsCount; ++i) {
                    spdlog::info(" Operand type {}: {}", i, decoded_ref->Operands[i].Type);
                }

                spdlog::info("Found raw cvar reference for \"{}\" at {:x}", utility::narrow(str.data()), *raw_cvar_ref);
            }
        }
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

std::optional<uintptr_t> find_cvar_by_description(std::wstring_view str, std::wstring_view cvar_name, uint32_t known_default = 0, HMODULE module = utility::get_executable()) {
    auto str_data = utility::scan_string(module, str.data());

    std::optional<uintptr_t> str_ref{};

    // Fallback to alternate scan if the description is missing
    if (!str_data) {
        str_ref = find_alternate_cvar_ref(cvar_name, known_default, module);
    }

    if (!str_data && !str_ref) {
        spdlog::error("Failed to find string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    // This scans for the cvar description string ref.
    if (!str_ref) {
        str_ref = utility::scan_reference(module, *str_data);

        if (!str_ref) {
            spdlog::error("Failed to find reference to string for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        spdlog::info("Found string ref for \"{}\" at {:x}", utility::narrow(str.data()), *str_ref);
    }

    return resolve_cvar_from_address(*str_ref + 4, cvar_name);
}

FFakeStereoRenderingHook::FFakeStereoRenderingHook() {
    g_hook = this;
}

bool FFakeStereoRenderingHook::hook() {
    spdlog::info("Entering FFakeStereoRenderingHook::hook");

    m_tried_hooking = true;

    const auto vtable = locate_fake_stereo_rendering_vtable();

    if (!vtable) {
        spdlog::error("Failed to locate Fake Stereo Rendering VTable, attempting to perform nonstandard hook");
        return nonstandard_create_stereo_device_hook();
    }

    return standard_fake_stereo_hook(*vtable);
}

bool FFakeStereoRenderingHook::standard_fake_stereo_hook(uintptr_t vtable) {
    spdlog::info("Performing standard fake stereo hook");

    const auto game = get_ue_module(L"Engine");

    patch_vtable_checks();

    const auto module_vtable_within = utility::get_module_within(vtable);
    const auto stereo_view_offset_index = get_stereo_view_offset_index(vtable);

    if (!stereo_view_offset_index) {
        spdlog::error("Failed to locate Stereo View Offset Index");
        return false;
    }

    const auto stereo_projection_matrix_index = *stereo_view_offset_index + 1;
    const auto is_4_18 = stereo_view_offset_index <= 6;

    const auto stereo_view_offset_func = ((uintptr_t*)vtable)[*stereo_view_offset_index];

    auto render_texture_render_thread_func = find_function_from_string_ref(L"RenderTexture_RenderThread", game);

    // Seems more robust than simply just checking the vtable index.
    m_uses_old_rendertarget_manager = stereo_view_offset_index <= 11 && !render_texture_render_thread_func;

    if (!render_texture_render_thread_func) {
        // Fallback scan to checking for the first non-default virtual function (4.18)
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
    auto rendertexture_fn_vtable_middle = utility::scan_ptr((HMODULE)game, *render_texture_render_thread_func);

    if (!rendertexture_fn_vtable_middle) {
        spdlog::error("Failed to find RenderTexture_RenderThread VTable Middle");
        return false;
    }

    const auto rendertexture_fn_vtable_index = (*rendertexture_fn_vtable_middle - vtable) / sizeof(uintptr_t);
    spdlog::info("RenderTexture_RenderThread VTable Middle: {} {:x}", rendertexture_fn_vtable_index, (uintptr_t)*rendertexture_fn_vtable_middle);

    auto render_target_manager_vtable_index = rendertexture_fn_vtable_index + 1 + (2 * (size_t)is_4_18);

    // verify first that the render target manager index is returning a null pointer
    // and if not, scan forward until we run into a vfunc that returns a null pointer
    auto get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

    if (!utility::scan(*(uintptr_t*)get_render_target_manager_func_ptr, 3, "33 C0 C3")) {
        spdlog::info("Expected GetRenderTargetManager function at index {} does not return null, scanning forward for return nullptr.", render_target_manager_vtable_index);

        for (;;++render_target_manager_vtable_index) {
            get_render_target_manager_func_ptr = &((uintptr_t*)vtable)[render_target_manager_vtable_index];

            if (IsBadReadPtr(*(void**)get_render_target_manager_func_ptr, 1)) {
                spdlog::error("Failed to find GetRenderTargetManager vtable index, a crash is imminent");
                return false;
            }

            if (utility::scan(*(uintptr_t*)get_render_target_manager_func_ptr, 3, "33 C0 C3")) {
                const auto distance_from_rendertexture_fn = render_target_manager_vtable_index - rendertexture_fn_vtable_index;

                // means it's 4.17 I think.
                if (distance_from_rendertexture_fn == 11) {
                    m_rendertarget_manager_embedded_in_stereo_device = true;
                    spdlog::info("Render target manager appears to be directly embedded in the stereo device vtable");
                } else {
                    spdlog::info("Found potential GetRenderTargetManager function at index {}", render_target_manager_vtable_index);
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

    // In 4.18 the destructor virtual doesn't exist.
    const auto is_stereo_enabled_index = utility::scan(*(uintptr_t*)vtable, 3, "B0 01 C3").value_or(0) == *(uintptr_t*)vtable ? 0 : 1;
    spdlog::info("IsStereoEnabled Index: {}", is_stereo_enabled_index);
    const auto is_stereo_enabled_func_ptr = &((uintptr_t*)vtable)[is_stereo_enabled_index];

    const auto adjust_view_rect_distance = is_4_18 ? 2 : 3;

    const auto adjust_view_rect_index = *stereo_view_offset_index - adjust_view_rect_distance;
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

                    if (module_within && final_func == (uintptr_t)GetProcAddress(*module_within, "tanf")) {
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

    m_adjust_view_rect_hook = builder.create_inline((void*)adjust_view_rect_func, adjust_view_rect);
    m_calculate_stereo_view_offset_hook = builder.create_inline((void*)stereo_view_offset_func, calculate_stereo_view_offset);
    m_calculate_stereo_projection_matrix_hook = builder.create_inline((void*)calculate_stereo_projection_matrix_func, calculate_stereo_projection_matrix);

    // When this is the case, then the CalculateRenderTargetSize index is the RenderTextureRenderThread index.
    if (!m_rendertarget_manager_embedded_in_stereo_device) {
        m_render_texture_render_thread_hook = builder.create_inline((void*)*render_texture_render_thread_func, render_texture_render_thread);
    }

    // This requires a pointer hook because the virtual just returns false
    // compiler optimization makes that function get re-used in a lot of places
    // so it's not feasible to just detour it, we need to replace the pointer in the vtable.
    if (!m_rendertarget_manager_embedded_in_stereo_device) {
        // Seems to exist in 4.18+
        m_get_render_target_manager_hook = std::make_unique<PointerHook>((void**)get_render_target_manager_func_ptr, (void*)&get_render_target_manager_hook);
    } else {
        // When the render target manager is embedded in the stereo device, it just means
        // that all of the virtuals are now part of FFakeStereoRendering
        // instead of being a part of IStereoRenderTargetManager and being returned via GetRenderTargetManager.
        // Only seen in 4.17 and below.
        spdlog::info("Performing hooks on embedded RenderTargetManager");

        // To be seen if any of these need further automated analysis.
        const auto calculate_render_target_size_index = rendertexture_fn_vtable_index + 2;
        const auto calculate_render_target_size_func_ptr = &((uintptr_t*)vtable)[calculate_render_target_size_index];

        const auto should_use_separate_render_target_index = calculate_render_target_size_index + 2;
        const auto should_use_separate_render_target_func_ptr = &((uintptr_t*)vtable)[should_use_separate_render_target_index];

        // This was calculated earlier when we were searching for the GetRenderTargetManager index.
        const auto allocate_render_target_index = render_target_manager_vtable_index + 3;
        const auto allocate_render_target_func_ptr = &((uintptr_t*)vtable)[allocate_render_target_index];

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

                return g_hook->get_render_target_manager()->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &out_texture);
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

    const auto renderer_module = get_ue_module(L"Renderer");
    const auto backbuffer_format_cvar = find_cvar_by_description(L"Defines the default back buffer pixel format.", L"r.DefaultBackBufferPixelFormat", 4, renderer_module);
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

    ++idx; // idk waht this is.

    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) { 
        return g_hook->adjust_view_rect(stereo, index, x, y, w, h);
    }; // AdjustViewRect

    // in this version the index is passed...?
    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, uint32_t index, Vector2f* bounds) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        spdlog::info("GetTextSafeRegionBounds called");
#endif

        bounds->x = 0.75f;
        bounds->y = 0.75f;

        return bounds;
    }; // GetTextSafeRegionBounds

    m_fallback_vtable[idx++] = 
    +[](FFakeStereoRendering* stereo, const int32_t view_index, Rotator* view_rotation, const float world_to_meters, Vector3f* view_location) {
        return g_hook->calculate_stereo_view_offset(stereo, view_index, view_rotation, world_to_meters, view_location);
    }; // CalculateStereoViewOffset

    m_fallback_vtable[idx++] = +[](FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
        spdlog::info("CalculateStereoProjectionMatrix called: {:x} {} {:x}", (uintptr_t)_ReturnAddress(), view_index, (uintptr_t)out);
#endif

        (*out)[3][2] = 10.0f; // Need to pre-set the Z value to something, otherwise it will be 0.0f & probably break something.

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

    auto engine_ptr = get_engine();

    if (engine_ptr == nullptr) {
        spdlog::error("Failed to get engine pointer! Cannot create stereo device!");
        return false;
    }
    
    auto engine = *engine_ptr;

    if (engine == nullptr) {
        spdlog::error("Failed to get engine! Cannot create stereo device!");
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

    const auto engine_dll = get_ue_module(L"Engine");

    auto fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationHeight", engine_dll);

    if (!fake_stereo_rendering_constructor) {
        fake_stereo_rendering_constructor = find_function_from_string_ref(L"r.StereoEmulationFOV", engine_dll);

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
        return std::nullopt;
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

    for (auto ref = utility::scan_reference(*vtable_module_within, *fake_stereo_rendering_vtable); 
        ref.has_value();
        ref = utility::scan_reference((uintptr_t)*ref + 4, (module_end - *ref) - sizeof(void*), *fake_stereo_rendering_vtable)) 
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
    auto engine = get_engine();

    if (engine == nullptr) {
        spdlog::error("Failed to locate GEngine, cannot inject stereo rendering device at runtime.");
        return false;
    }

    static auto enable_stereo_emulation_cvar = []() -> std::optional<uintptr_t> {
        spdlog::info("Attempting to locate r.EnableStereoEmulation cvar...");

        const auto module = get_ue_module(L"Engine");
        const auto str = utility::scan_string(module, L"r.EnableStereoEmulation");

        if (!str) {
            spdlog::error("Failed to find r.EnableStereoEmulation string!");
            return std::nullopt;
        }

        const auto str_ref = utility::scan_reference(module, *str);

        if (!str_ref) {
            spdlog::error("Failed to find r.EnableStereoEmulation string reference!");
            return std::nullopt;
        }

        const auto result = resolve_cvar_from_address(*str_ref + 4, L"r.EnableStereoEmulation");
        if (result) {
            spdlog::info("Found r.EnableStereoEmulation at {:x}", (uintptr_t)*result);
        }

        return result;
    }();

    if (!enable_stereo_emulation_cvar) {
        spdlog::error("Failed to locate r.EnableStereoEmulation cvar, cannot inject stereo rendering device at runtime.");
        return false;
    }

    static auto initialize_hmd_device = []() -> std::optional<uintptr_t> {
        spdlog::info("Searching for InitializeHMDDevice function...");

        const auto module_within = utility::get_module_within(*enable_stereo_emulation_cvar);

        if (!module_within) {
            spdlog::error("Failed to find module containing r.EnableStereoEmulation cvar!");
            return std::nullopt;
        }

        spdlog::info("Module containing r.EnableStereoEmulation cvar: {:x}", (uintptr_t)*module_within);

        const auto enable_stereo_emulation_cvar_ref = utility::scan_reference(*module_within, *enable_stereo_emulation_cvar);

        if (!enable_stereo_emulation_cvar_ref) {
            spdlog::error("Failed to find r.EnableStereoEmulation cvar reference!");
            return std::nullopt;
        }

        spdlog::info("Found r.EnableStereoEmulation cvar reference at {:x}", (uintptr_t)*enable_stereo_emulation_cvar_ref);

        auto result = find_function_start(*enable_stereo_emulation_cvar_ref);

        // scan backwards for the function start until it's no longer some random label within a function, but the function start itself.
        while (result) {
            // This means it's a valid vtable function, and we have found the function start.
            if (utility::scan_ptr(*utility::get_module_within(*result), *result)) {
                break;
            }

            spdlog::info("result was not really the function, scanning again...");
            result = find_function_start(*result - 1);
        }

        if (result) {
            spdlog::info("Found InitializeHMDDevice at {:x}", (uintptr_t)*result);
        }

        return result;
    }();

    if (!initialize_hmd_device) {
        spdlog::error("Failed to locate InitializeHMDDevice function, cannot inject stereo rendering device at runtime.");
        return false;
    }

    spdlog::info("Calling InitializeHMDDevice...");

    utility::ThreadSuspender _{};

    *(int32_t*)(*(uintptr_t*)*enable_stereo_emulation_cvar + 0x0) = 1;
    *(int32_t*)(*(uintptr_t*)*enable_stereo_emulation_cvar + 0x4) = 1;

    const auto fn = (void(*)(void*))*initialize_hmd_device;
    fn(*engine);

    spdlog::info("Called InitializeHMDDevice.");

    return true;
}

bool FFakeStereoRenderingHook::is_stereo_enabled(FFakeStereoRendering* stereo) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("is stereo enabled called!");
#endif
    return !VR::get()->get_runtime()->got_first_sync || VR::get()->is_hmd_active();
}

void FFakeStereoRenderingHook::adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("adjust view rect called! {}", index);
#endif

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

void FFakeStereoRenderingHook::calculate_stereo_view_offset(
    FFakeStereoRendering* stereo, const int32_t view_index, Rotator* view_rotation, 
    const float world_to_meters, Vector3f* view_location)
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate stereo view offset called! {}", view_index);
#endif

    static bool index_starts_from_one = true;

    if (view_index == 2) {
        index_starts_from_one = true;
    } else if (view_index == 0) {
        index_starts_from_one = false;
    }

    const auto true_index = index_starts_from_one ? ((view_index + 1) % 2) : (view_index % 2);

    auto vr = VR::get();

    /*if (view_index % 2 == 1 && VR::get()->get_runtime()->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
        std::scoped_lock _{ vr->get_runtime()->render_mtx };
        spdlog::info("SYNCING!!!");
        //vr->get_runtime()->synchronize_frame();
        vr->update_hmd_state();
    }*/

    if (true_index == 0) {
        std::scoped_lock _{ vr->m_openvr_mtx };
        vr->get_runtime()->consume_events(nullptr);
        vr->update_hmd_state();
    }

    const auto view_mat = glm::yawPitchRoll(
        glm::radians(view_rotation->yaw),
        glm::radians(view_rotation->pitch),
        glm::radians(view_rotation->roll));

    const auto view_mat_inverse = glm::yawPitchRoll(
        glm::radians(-view_rotation->yaw),
        glm::radians(view_rotation->pitch),
        glm::radians(-view_rotation->roll));

    const auto view_quat_inverse = glm::quat {
        view_mat_inverse
    };

    const auto view_quat = glm::quat {
        view_mat
    };

    const auto rotation_offset = vr->get_rotation_offset();
    const auto current_hmd_rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(0)});

    const auto new_rotation = glm::normalize(view_quat_inverse * current_hmd_rotation);
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

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("Finished calculating stereo view offset!");
#endif
}

Matrix4x4f* FFakeStereoRenderingHook::calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate stereo projection matrix called! {} from {:x}", view_index, (uintptr_t)_ReturnAddress());
#endif

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

        float old_znear = (*out)[3][2];
        VR::get()->m_nearz = old_znear;

        VR::get()->get_runtime()->update_matrices(old_znear, 10000.0f);
        *out = VR::get()->get_projection_matrix((VRRuntime::Eye)(true_index));
    } else {
        spdlog::error("CalculateStereoProjectionMatrix returned nullptr!");
    }

#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("Finished calculating stereo projection matrix!");
#endif
    
    return out;
}

void FFakeStereoRenderingHook::render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
    FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size) 
{
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("render texture render thread called!");
#endif

    // spdlog::info("{:x}", (uintptr_t)src_texture->GetNativeResource());

    // maybe the window size is actually a pointer we will find out later.
    // g_hook->m_render_texture_render_thread_hook->call<void*>(stereo, rhi_command_list, backbuffer, src_texture, window_size);
}

void FFakeStereoRenderingHook::init_canvas(FFakeStereoRendering* stereo, FSceneView* view, UCanvas* canvas) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("init canvas called!");
#endif

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
        const auto containing_function = find_function_start(return_address);

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

void VRRenderTargetManager_Base::update_viewport(bool use_separate_rt, const FViewport& vp, class SViewport* vp_widget) {
    //spdlog::info("Widget: {:x}", (uintptr_t)ViewportWidget);
}

void VRRenderTargetManager_Base::calculate_render_target_size(const FViewport& viewport, uint32_t& x, uint32_t& y) {
#ifdef FFAKE_STEREO_RENDERING_LOG_ALL_CALLS
    spdlog::info("calculate render target size called!");
#endif

    spdlog::info("RenderTargetSize Before: {}x{}", x, y);

    x = VR::get()->get_hmd_width() * 2;
    y = VR::get()->get_hmd_height();

    spdlog::info("RenderTargetSize After: {}x{}", x, y);
}

bool VRRenderTargetManager_Base::need_reallocate_view_target(const FViewport& Viewport) {
    /*const auto w = VR::get()->get_hmd_width();
    const auto h = VR::get()->get_hmd_height();

    if (w != this->last_width || h != this->last_height) {
        this->last_width = w;
        this->last_height = h;
        return true;
    }*/

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
    if (g_hook->get_render_target_manager()->is_pre_texture_call_e8) {
        ctx.r8 = 2; // PF_B8G8R8A8
    } else {
        *((uint8_t*)ctx.rsp + 0x28) = 2; // PF_B8G8R8A8
    }
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
    rtm->texture_hook_ref = nullptr;
    ++rtm->last_texture_index;
}

bool VRRenderTargetManager_Base::allocate_render_target_texture(uintptr_t return_address, FTexture2DRHIRef* tex) {
    this->texture_hook_ref = tex;

    if (!this->set_up_texture_hook) {
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
                spdlog::info("AllocateRenderTargetTexture post_call: {:x}", post_call - (uintptr_t)*utility::get_module_within((void*)post_call));

                auto factory = SafetyHookFactory::init();
                auto builder = factory->acquire();

                this->texture_hook = builder.create_mid((void*)post_call, &VRRenderTargetManager::texture_hook_callback);

                if (!g_hook->has_pixel_format_cvar()) {
                    spdlog::info("No pixel format cvar found, setting up pre texture hook...");
                    if (*(uint8_t*)ip == 0xE8) {
                        spdlog::info("E8 call found!");
                        this->is_pre_texture_call_e8 = true;
                    } else {
                        spdlog::info("E8 call not found, assuming register call!");
                    }

                    this->pre_texture_hook = builder.create_mid((void*)ip, &VRRenderTargetManager::pre_texture_hook_callback);
                }

                this->set_up_texture_hook = true;

                return false;
            }

            ip += ix.Length;
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
    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture);
}

bool VRRenderTargetManager_418::AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags,
        uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples) 
{
    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture);
}

bool VRRenderTargetManager_Special::AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
    ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
    FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples) 
{
    return this->allocate_render_target_texture((uintptr_t)_ReturnAddress(), &OutTargetableTexture);
}
