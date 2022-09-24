#include <array>
#include <string_view>

#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>

#include "EngineModule.hpp"

#include "CVar.hpp"


namespace sdk {
std::optional<uintptr_t> find_alternate_cvar_ref(std::wstring_view str, uint32_t known_default, HMODULE module) {
    spdlog::info("Performing alternate scan for cvar \"{}\"", utility::narrow(str));

    const auto str_data = utility::scan_string(module, str.data());

    if (!str_data) {
        spdlog::error("Failed to find string for cvar \"{}\"", utility::narrow(str.data()));
        return std::nullopt;
    }

    const auto module_base = (uintptr_t)module;
    const auto module_size = utility::get_module_size(module);
    auto str_ref = utility::scan_displacement_reference(module, *str_data);
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
        str_ref = utility::scan_displacement_reference(*str_ref + 1, delta, *str_data);
    }

    if (!result) {
        spdlog::error("Failed to find alternate cvar reference for \"{}\"", utility::narrow(str.data()));
        return std::nullopt;
    }
    
    return result;
}

std::optional<uintptr_t> resolve_cvar_from_address(uintptr_t start, std::wstring_view str, bool stop_at_first_mov) {
    const auto cvar_creation_ref = utility::scan_mnemonic(start, 100, "CALL");

    if (!cvar_creation_ref) {
        spdlog::error("Failed to find cvar creation reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    auto raw_cvar_ref = !stop_at_first_mov ? 
                        utility::scan_mnemonic(*cvar_creation_ref + utility::get_insn_size(*cvar_creation_ref), 100, "CALL") : cvar_creation_ref;

    if (!raw_cvar_ref) {
        spdlog::error("Failed to find raw cvar reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    spdlog::info("Found raw cvar reference for \"{}\" at {:x}", utility::narrow(str.data()), *raw_cvar_ref);
    const auto decoded_ref = utility::decode_one((uint8_t*)*raw_cvar_ref);

    // we need to check that the reference uses a register in its operand
    // otherwise it's the wrong call. find the next call if it is.
    if (decoded_ref && !stop_at_first_mov) {
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

std::optional<uintptr_t> find_cvar_by_description(std::wstring_view str, std::wstring_view cvar_name, uint32_t known_default, HMODULE module, bool stop_at_first_mov) {
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
        str_ref = utility::scan_displacement_reference(module, *str_data);

        if (!str_ref) {
            spdlog::error("Failed to find reference to string for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        spdlog::info("Found string ref for \"{}\" at {:x}", utility::narrow(str.data()), *str_ref);
    }

    return resolve_cvar_from_address(*str_ref + 4, cvar_name, stop_at_first_mov);
}

std::optional<uintptr_t> vr::get_enable_stereo_emulation_cvar() {
    static auto enable_stereo_emulation_cvar = []() -> std::optional<uintptr_t> {
        spdlog::info("Attempting to locate r.EnableStereoEmulation cvar...");

        const auto module = sdk::get_ue_module(L"Engine");
        const auto str = utility::scan_string(module, L"r.EnableStereoEmulation");

        if (!str) {
            spdlog::error("Failed to find r.EnableStereoEmulation string!");
            return std::nullopt;
        }

        const auto str_ref = utility::scan_displacement_reference(module, *str);

        if (!str_ref) {
            spdlog::error("Failed to find r.EnableStereoEmulation string reference!");
            return std::nullopt;
        }

        const auto result = sdk::resolve_cvar_from_address(*str_ref + 4, L"r.EnableStereoEmulation");
        if (result) {
            spdlog::info("Found r.EnableStereoEmulation at {:x}", (uintptr_t)*result);
        }

        return result;
    }();

    return enable_stereo_emulation_cvar;
}

std::optional<uintptr_t> vr::get_slate_draw_to_vr_render_target_real_cvar() {
    static auto cvar = []() -> std::optional<uintptr_t> {
        spdlog::info("Attempting to locate Slate.DrawToVRRenderTarget cvar...");

        const auto module = sdk::get_ue_module(L"SlateRHIRenderer");
        const auto str = utility::scan_string(module, L"Slate.DrawToVRRenderTarget");

        if (!str) {
            spdlog::error("Failed to find Slate.DrawToVRRenderTarget string!");
            return std::nullopt;
        }

        const auto str_ref = utility::scan_displacement_reference(module, *str);

        if (!str_ref) {
            spdlog::error("Failed to find Slate.DrawToVRRenderTarget string reference!");
            return std::nullopt;
        }

        const auto result = sdk::resolve_cvar_from_address(*str_ref + 4, L"Slate.DrawToVRRenderTarget", true);

        if (result) {
            spdlog::info("Found Slate.DrawToVRRenderTarget at {:x}", (uintptr_t)*result);
        }

        return result;
    }();

    return cvar;
}

std::optional<uintptr_t> vr::get_slate_draw_to_vr_render_target_cvar() {
    const auto cvar = get_slate_draw_to_vr_render_target_real_cvar();

    if (!cvar) {
        return std::nullopt;
    }

    return *cvar + sizeof(void*);
}

std::optional<uintptr_t> vr::get_slate_draw_to_vr_render_target_usage_location() {
    static auto result = []() -> std::optional<uintptr_t> {
        spdlog::info("Scanning for Slate.DrawToVRRenderTarget usage...");

        const auto cvar = get_slate_draw_to_vr_render_target_real_cvar();

        if (!cvar) {
            return std::nullopt;
        }

        const auto module = sdk::get_ue_module(L"SlateRHIRenderer");
        const auto module_size = utility::get_module_size(module).value_or(0);
        const auto module_end = (uintptr_t)module + module_size;

        for (auto ref = utility::scan_displacement_reference(module, *cvar); 
            ref; 
            ref = utility::scan_displacement_reference(*ref + 1, (module_end - (*ref + 1)), *cvar)) 
        {
            spdlog::info("Checking if Slate.DrawToVRRenderTarget is used at {:x}", *ref);

            const auto resolved = utility::resolve_instruction(*ref);
            if (!resolved) {
                spdlog::error("Failed to resolve instruction at {:x}", *ref);
                continue;
            }

            if (resolved->instrux.Operands[0].Type == ND_OP_MEM && resolved->instrux.Operands[1].Type == ND_OP_REG) {
                spdlog::info("Instruction at {:x} is not a usage of Slate.DrawToVRRenderTarget", *ref);
                continue; // this is NOT what we want
            }

            // check if the distance to the nearest ret is far away, which means
            // it's the slate function we're looking for
            uint32_t count = 0;

            auto ix = utility::decode_one((uint8_t*)resolved->addr);

            for (auto ip = (uint8_t*)resolved->addr; ix = utility::decode_one(ip); ip += ix->Length) {
                if (std::string_view{ix->Mnemonic}.starts_with("RET") || std::string_view{ix->Mnemonic}.starts_with("INT3")) {
                    spdlog::info("Found RET at {:x}", (uintptr_t)ip);
                    break;
                }

                if (ix->Instruction == ND_INS_JMPFI || ix->Instruction == ND_INS_JMPFD) {
                    spdlog::info("Found JMPFI/JMPFD at {:x}", (uintptr_t)ip);
                    break;
                }

                ++count;

                if (count >= 50) {
                    spdlog::info("Located Slate.DrawToVRRenderTarget usage at {:x}", (uintptr_t)resolved->addr);
                    return resolved->addr;
                }
            }
        }

        spdlog::error("Failed to locate Slate.DrawToVRRenderTarget usage!");

        return std::nullopt;
    }();

    return result;
}
}