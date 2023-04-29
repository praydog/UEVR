#include <array>
#include <string_view>

#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>
#include <utility/Emulation.hpp>

#include <bdshemu.h>

#include "threading/GameThreadWorker.hpp"
#include "EngineModule.hpp"
#include "Utility.hpp"

#include "CVar.hpp"


namespace sdk {
std::optional<uintptr_t> find_alternate_cvar_ref(std::wstring_view str, uint32_t known_default, HMODULE module) {
    SPDLOG_INFO("Performing alternate scan for cvar \"{}\"", utility::narrow(str));

    const auto str_data = utility::scan_string(module, str.data());

    if (!str_data) {
        SPDLOG_ERROR("Failed to find string for cvar \"{}\"", utility::narrow(str.data()));
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
            SPDLOG_INFO("Found alternate cvar reference at {:x}", *result);
            break;
        }

        const auto delta = *module_size - ((*str_ref + 1) - module_base);
        str_ref = utility::scan_displacement_reference(*str_ref + 1, delta, *str_data);
    }

    if (!result) {
        SPDLOG_ERROR("Failed to find alternate cvar reference for \"{}\"", utility::narrow(str.data()));
        return std::nullopt;
    }
    
    return result;
}

std::optional<uintptr_t> resolve_cvar_from_address(uintptr_t start, std::wstring_view str, bool stop_at_first_mov) {
    const auto cvar_creation_ref = utility::scan_mnemonic(start, 100, "CALL");

    if (!cvar_creation_ref) {
        SPDLOG_ERROR("Failed to find cvar creation reference for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    // means it's not inlined like we hoped
    const auto is_e8_call = *(uint8_t*)*cvar_creation_ref == 0xE8;
    const auto is_ff_call = *(uint8_t*)*cvar_creation_ref == 0xFF;

    if (!is_e8_call) {
        SPDLOG_INFO("Not E8 call");

        if (is_ff_call) {
            SPDLOG_INFO("FF call");

            // This is the call to the cvar interface so it should be easier
            // we can emulate forward from the call and track the rax register
            // and which global it finally gets assigned to
            const auto post_call = *cvar_creation_ref + utility::get_insn_size(*cvar_creation_ref);
            
            utility::ShemuContext emu{*utility::get_module_within(post_call)};

            constexpr uint64_t RAX_MAGIC_NUMBER = 0x1337BEEF;

            emu.ctx->MemThreshold = 5;
            emu.ctx->Registers.RegRax = RAX_MAGIC_NUMBER;
            emu.ctx->Registers.RegRip = post_call;

            bool has_two_calls = false;

            while(true) {
                if (emu.ctx->InstructionsCount > 100) {
                    break;
                }

                const auto ip = emu.ctx->Registers.RegRip;
                const auto decoded = utility::decode_one((uint8_t*)ip);

                // make sure we are not emulating any instructions that write to memory
                // so we can just set the IP to the next instruction
                if (decoded) {
                    const auto is_call = std::string_view{decoded->Mnemonic}.starts_with("CALL");

                    if (decoded->MemoryAccess & ND_ACCESS_ANY_WRITE || is_call) {
                        SPDLOG_INFO("Skipping write to memory instruction at {:x}", ip);
                        emu.ctx->Registers.RegRip += decoded->Length;
                        emu.ctx->Instruction = *decoded; // pseudo-emulate the instruction
                        ++emu.ctx->InstructionsCount;

                        if (is_call) {
                            // reset the rax register to the magic number
                            emu.ctx->Registers.RegRax = RAX_MAGIC_NUMBER;
                            has_two_calls = true;
                        }
                    } else if (emu.emulate() != SHEMU_SUCCESS) { // only emulate the non-memory write instructions
                        emu.ctx->Registers.RegRip += decoded->Length;
                        continue;
                    }
                } else {
                    break;
                }

                const auto& ix = emu.ctx->Instruction;

                if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG) {
                    // We can treat the registers struct in the context as an array
                    // and then use the enum value as an index
                    const auto reg_value = ((uint64_t*)&emu.ctx->Registers.RegRax)[ix.Operands[1].Info.Register.Reg];

                    if (reg_value == RAX_MAGIC_NUMBER) {
                        // We found the assignment to the global
                        // so we can just return the address of the global
                        // which is the displacement of the mov instruction
                        // plus the address of the instruction
                        auto result = (uintptr_t)(ip + ix.Length + ix.Operands[0].Info.Memory.Disp);

                        if (!has_two_calls) {
                            result += sizeof(void*);
                        }

                        SPDLOG_INFO("Found cvar for \"{}\" at {:x} (referenced at {:x})", utility::narrow(str.data()), result, emu.ctx->Registers.RegRip);
                        return result;
                    }
                }
            }

            SPDLOG_ERROR("Failed to find cvar reference for {}", utility::narrow(str.data()));
        } else {
            auto raw_cvar_ref = !stop_at_first_mov ? 
                                utility::scan_mnemonic(*cvar_creation_ref + utility::get_insn_size(*cvar_creation_ref), 100, "CALL") : cvar_creation_ref;

            if (!raw_cvar_ref) {
                SPDLOG_ERROR("Failed to find raw cvar reference for {}", utility::narrow(str.data()));
                return std::nullopt;
            }

            SPDLOG_INFO("Found raw cvar reference for \"{}\" at {:x}", utility::narrow(str.data()), *raw_cvar_ref);
            const auto decoded_ref = utility::decode_one((uint8_t*)*raw_cvar_ref);

            // we need to check that the reference uses a register in its operand
            // otherwise it's the wrong call. find the next call if it is.
            if (decoded_ref && !stop_at_first_mov) {
                for (auto i = 0; i < decoded_ref->OperandsCount; ++i) {
                    SPDLOG_INFO(" Operand type {}: {}", i, decoded_ref->Operands[i].Type);
                }

                if (decoded_ref->OperandsCount == 0 || 
                    decoded_ref->Operands[0].Type != ND_OP_MEM || 
                    !decoded_ref->Operands[0].Info.Memory.HasBase)
                {
                    SPDLOG_INFO("Scanning again, instruction at {:x} doesn't use a register", *raw_cvar_ref);
                    raw_cvar_ref = utility::scan_mnemonic(*raw_cvar_ref + utility::get_insn_size(*raw_cvar_ref), 100, "CALL");

                    if (raw_cvar_ref) {
                        const auto decoded_ref = utility::decode_one((uint8_t*)*raw_cvar_ref);

                        for (auto i = 0; i < decoded_ref->OperandsCount; ++i) {
                            SPDLOG_INFO(" Operand type {}: {}", i, decoded_ref->Operands[i].Type);
                        }

                        SPDLOG_INFO("Found raw cvar reference for \"{}\" at {:x}", utility::narrow(str.data()), *raw_cvar_ref);
                    }
                }
            }

            // Look for a mov {ptr}, rax
            auto ip = (uint8_t*)*raw_cvar_ref;

            const auto ip_after_call = ip + utility::get_insn_size(*raw_cvar_ref);

            for (auto i = 0; i < 100; ++i) {
                INSTRUX ix{};
                const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

                if (!ND_SUCCESS(status)) {
                    SPDLOG_ERROR("Failed to decode instruction for {}", utility::narrow(str.data()));
                    return std::nullopt;
                }

                if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_REG &&
                    ix.Operands[1].Info.Register.Reg == NDR_RAX) 
                {
                    return (uintptr_t)(ip + ix.Length + ix.Operands[0].Info.Memory.Disp);
                }

                ip += ix.Length;
            }

            SPDLOG_ERROR("Failed to find cvar for {}", utility::narrow(str.data()));
        }
    } else {
        SPDLOG_INFO("Cvar creation is not inlined, performing alternative search...");

        auto lea_rcx_ref = utility::scan_mnemonic(start, 100, "LEA");

        if (!lea_rcx_ref || *lea_rcx_ref >= *cvar_creation_ref) {
            return std::nullopt;
        }

        auto decoded = utility::decode_one((uint8_t*)*lea_rcx_ref);

        if (!decoded) {
            SPDLOG_ERROR("Failed to decode instruction for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        if (decoded->Operands[0].Type != ND_OP_REG || decoded->Operands[0].Info.Register.Reg != NDR_RCX) {
            SPDLOG_ERROR("Failed to find lea rcx for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        const auto cvar = utility::resolve_displacement(*lea_rcx_ref);

        if (!cvar) {
            SPDLOG_ERROR("Failed to resolve displacement for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        if (stop_at_first_mov) {
            return *cvar + 8;
        } else {
            return *cvar + 0x10;
        }
    }

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
        SPDLOG_ERROR("Failed to find string for {}", utility::narrow(str.data()));
        return std::nullopt;
    }

    // This scans for the cvar description string ref.
    if (!str_ref) {
        str_ref = utility::scan_displacement_reference(module, *str_data);

        if (!str_ref) {
            SPDLOG_ERROR("Failed to find reference to string for {}", utility::narrow(str.data()));
            return std::nullopt;
        }

        SPDLOG_INFO("Found string ref for \"{}\" at {:x}", utility::narrow(str.data()), *str_ref);
    }

    return resolve_cvar_from_address(*str_ref + 4, cvar_name, stop_at_first_mov);
}

std::optional<ConsoleVariableDataWrapper> find_cvar_data(std::wstring_view module_name, std::wstring_view name, bool stop_at_first_mov) {
    SPDLOG_INFO("Attempting to locate {} {} cvar", utility::narrow(module_name.data()), utility::narrow(name.data()));

    const auto module = sdk::get_ue_module(module_name.data());
    const auto str = utility::scan_string(module, name.data());

    if (!str) {
        SPDLOG_ERROR("Failed to find {} string!", utility::narrow(name.data()));
        return std::nullopt;
    }

    const auto str_ref = utility::scan_displacement_reference(module, *str);

    if (!str_ref) {
        SPDLOG_ERROR("Failed to find {} string reference!");
        return std::nullopt;
    }

    const auto result = sdk::resolve_cvar_from_address(*str_ref + 4, name.data(), stop_at_first_mov);

    if (result) {
        SPDLOG_INFO("Found {} at {:x}", utility::narrow(name.data()), (uintptr_t)*result);
    }

    return result;
}

IConsoleVariable** find_cvar(std::wstring_view module_name, std::wstring_view name, bool stop_at_first_mov) {
    SPDLOG_INFO("Attempting to locate {} {} cvar", utility::narrow(module_name.data()), utility::narrow(name.data()));

    const auto module = sdk::get_ue_module(module_name.data());
    const auto str = utility::scan_string(module, name.data());

    if (!str) {
        SPDLOG_ERROR("Failed to find {} string!", utility::narrow(name.data()));
        return nullptr;
    }

    const auto str_ref = utility::scan_displacement_reference(module, *str);

    if (!str_ref) {
        SPDLOG_ERROR("Failed to find {} string reference!");
        return nullptr;
    }

    auto result = sdk::resolve_cvar_from_address(*str_ref + 4, name.data(), stop_at_first_mov);

    if (result) {
        *result -= sizeof(void*);
        SPDLOG_INFO("Found {} at {:x}", utility::narrow(name.data()), (uintptr_t)*result);
    }

    return (IConsoleVariable**)*result;
}

std::optional<ConsoleVariableDataWrapper> find_cvar_data_cached(std::wstring_view module_name, std::wstring_view name, bool stop_at_first_mov) {
    static std::unordered_map<std::wstring, uintptr_t> cache{};

    // The module name is irrelevant here, what are the chances that two modules have the same cvar name?
    if (auto it = cache.find(name.data()); it != cache.end()) {
        if (it->second == 0) {
            return std::nullopt;
        }

        return cache[name.data()];
    }

    const auto result = find_cvar_data(module_name, name, stop_at_first_mov);

    if (result) {
        cache[name.data()] = (uintptr_t)result->address();
    } else {
        cache[name.data()] = 0;
    }

    return result;
}

IConsoleVariable** find_cvar_cached(std::wstring_view module_name, std::wstring_view name, bool stop_at_first_mov) {
    static std::unordered_map<std::wstring, IConsoleVariable**> cache{};

    // The module name is irrelevant here, what are the chances that two modules have the same cvar name?
    if (auto it = cache.find(name.data()); it != cache.end()) {
        return it->second;
    }

    const auto result = find_cvar(module_name, name, stop_at_first_mov);

    cache[name.data()] = result;

    return result;
}

bool set_cvar_data_int(std::wstring_view module, std::wstring_view name, int value, bool stop_at_first_mov) try {
    auto cvar_data = find_cvar_data_cached(module, name, stop_at_first_mov);

    if (!cvar_data) {
        return false;
    }

    auto cvar = cvar_data->get<int>();

    if (cvar == nullptr) {
        return false;
    }

    cvar->set(value);
    return true;
} catch (...) {
    static std::unordered_map<std::wstring, bool> bad_cvars{};

    if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
        bad_cvars[name.data()] = true;
        SPDLOG_ERROR("Failed to set {} cvar (data) to {}!", utility::narrow(name.data()), value);
    }

    // whatever.
    return false;
}

bool set_cvar_data_float(std::wstring_view module, std::wstring_view name, float value, bool stop_at_first_mov) try {
    auto cvar_data = find_cvar_data_cached(module, name, stop_at_first_mov);

    if (!cvar_data) {
        return false;
    }

    auto cvar = cvar_data->get<float>();

    if (cvar == nullptr) {
        return false;
    }

    cvar->set(value);
    return true;
} catch (...) {
    static std::unordered_map<std::wstring, bool> bad_cvars{};

    if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
        bad_cvars[name.data()] = true;
        SPDLOG_ERROR("Failed to set {} cvar (data) to {}!", utility::narrow(name.data()), value);
    }

    // whatever.
    return false;
}

bool set_cvar_int(std::wstring_view module, std::wstring_view name, int value, bool stop_at_first_mov) try {
    auto cvarpp = find_cvar_cached(module, name, stop_at_first_mov);

    if (cvarpp == nullptr || *cvarpp == nullptr) {
        return false;
    }

    // yoink we're using the UE4 cvar setter yep thats right we're not using the console manager because we're so freaking cool
    GameThreadWorker::get().enqueue([=]() {
        try {
            (*cvarpp)->Set(std::to_wstring(value).data());
        } catch (...) {
            static std::unordered_map<std::wstring, bool> bad_cvars{};

            if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
                bad_cvars[name.data()] = true;
                SPDLOG_ERROR("Failed to set {} cvar to {}!", utility::narrow(name.data()), value);
            }
        }
    });

    // we're so cool we don't even need to check if it worked
    return true;
} catch (...) {
    static std::unordered_map<std::wstring, bool> bad_cvars{};

    if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
        bad_cvars[name.data()] = true;
        SPDLOG_ERROR("Failed to set {} cvar to {}!", utility::narrow(name.data()), value);
    }

    // whatever.
    return false;
}

bool set_cvar_float(std::wstring_view module, std::wstring_view name, float value, bool stop_at_first_mov) try {
    auto cvarpp = find_cvar_cached(module, name, stop_at_first_mov);

    if (cvarpp == nullptr || *cvarpp == nullptr) {
        return false;
    }

    // yoink we're using the UE4 cvar setter yep thats right we're not using the console manager because we're so freaking cool
    GameThreadWorker::get().enqueue([=]() {
        try {
            (*cvarpp)->Set(std::to_wstring(value).data());
        } catch (...) {
            static std::unordered_map<std::wstring, bool> bad_cvars{};

            if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
                bad_cvars[name.data()] = true;
                SPDLOG_ERROR("Failed to set {} cvar to {}!", utility::narrow(name.data()), value);
            }
        }
    });

    // we're so cool we don't even need to check if it worked
    return true;
} catch (...) {
    // whatever.
    return false;
}


std::optional<int> get_cvar_int(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov) try {
    auto cvarpp = find_cvar_cached(module, name, stop_at_first_mov);

    if (cvarpp == nullptr || *cvarpp == nullptr) {
        return std::nullopt;
    }

    return (*cvarpp)->GetInt();
} catch(...) {
    static std::unordered_map<std::wstring, bool> bad_cvars{};

    if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
        bad_cvars[name.data()] = true;
        SPDLOG_ERROR("Failed to read {} cvar!", utility::narrow(name.data()));
    }

    // whatever.
    return std::nullopt;
}

std::optional<float> get_cvar_float(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov) try {
    auto cvarpp = find_cvar_cached(module, name, stop_at_first_mov);

    if (cvarpp == nullptr || *cvarpp == nullptr) {
        return std::nullopt;
    }

    return (*cvarpp)->GetFloat();
} catch (...) {
    static std::unordered_map<std::wstring, bool> bad_cvars{};

    if (auto it = bad_cvars.find(name.data()); it == bad_cvars.end()) {
        bad_cvars[name.data()] = true;
        SPDLOG_ERROR("Failed to read {} cvar!", utility::narrow(name.data()));
    }

    // whatever.
    return std::nullopt;
}

std::optional<ConsoleVariableDataWrapper> vr::get_enable_stereo_emulation_cvar() {
    static auto enable_stereo_emulation_cvar = []() -> std::optional<ConsoleVariableDataWrapper> {
        return find_cvar_data(L"Engine", L"r.EnableStereoEmulation");
    }();

    return enable_stereo_emulation_cvar;
}

std::optional<ConsoleVariableDataWrapper> vr::get_slate_draw_to_vr_render_target_real_cvar() {
    static auto cvar = []() -> std::optional<ConsoleVariableDataWrapper> {
        return find_cvar_data(L"SlateRHIRenderer", L"Slate.DrawToVRRenderTarget", true);
    }();

    return cvar;
}

std::optional<uintptr_t> vr::get_slate_draw_to_vr_render_target_usage_location() {
    static auto result = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Scanning for Slate.DrawToVRRenderTarget usage...");

        const auto cvar = get_slate_draw_to_vr_render_target_real_cvar();

        if (!cvar) {
            return std::nullopt;
        }

        const auto module = sdk::get_ue_module(L"SlateRHIRenderer");
        const auto module_size = utility::get_module_size(module).value_or(0);
        const auto module_end = (uintptr_t)module + module_size;

        for (int32_t i=-1; i < 2; ++i) {
            const auto cvar_addr = (uintptr_t)cvar->address() + (i * sizeof(void*));

            for (auto ref = utility::scan_displacement_reference(module, cvar_addr); 
                ref; 
                ref = utility::scan_displacement_reference(*ref + 1, (module_end - (*ref + 1)), cvar_addr)) 
            {
                SPDLOG_INFO("Checking if Slate.DrawToVRRenderTarget is used at {:x}", *ref);

                const auto resolved = utility::resolve_instruction(*ref);
                if (!resolved) {
                    SPDLOG_ERROR("Failed to resolve instruction at {:x}", *ref);
                    continue;
                }

                if (resolved->instrux.Operands[0].Type == ND_OP_MEM && resolved->instrux.Operands[1].Type == ND_OP_REG) {
                    SPDLOG_INFO("Instruction at {:x} is not a usage of Slate.DrawToVRRenderTarget", *ref);
                    continue; // this is NOT what we want
                }

                // check if the distance to the nearest ret is far away, which means
                // it's the slate function we're looking for
                uint32_t count = 0;

                auto ix = utility::decode_one((uint8_t*)resolved->addr);

                for (auto ip = (uint8_t*)resolved->addr; ix = utility::decode_one(ip); ip += ix->Length) {
                    if (std::string_view{ix->Mnemonic}.starts_with("RET") || std::string_view{ix->Mnemonic}.starts_with("INT3")) {
                        SPDLOG_INFO("Found RET at {:x}", (uintptr_t)ip);
                        break;
                    }

                    if (ix->Instruction == ND_INS_JMPFI || ix->Instruction == ND_INS_JMPFD) {
                        SPDLOG_INFO("Found JMPFI/JMPFD at {:x}", (uintptr_t)ip);
                        break;
                    }

                    ++count;

                    if (count >= 50) {
                        SPDLOG_INFO("Located Slate.DrawToVRRenderTarget usage at {:x}", (uintptr_t)resolved->addr);
                        return resolved->addr;
                    }
                }
            }
        }

        SPDLOG_ERROR("Failed to locate Slate.DrawToVRRenderTarget usage!");

        return std::nullopt;
    }();

    return result;
}

namespace rendering {
std::optional<ConsoleVariableDataWrapper> get_one_frame_thread_lag_cvar() {
    static auto cvar = []() -> std::optional<ConsoleVariableDataWrapper> {
        return find_cvar_data(L"Engine", L"r.OneFrameThreadLag");
    }();

    return cvar;
}
}
}

namespace sdk {
void IConsoleVariable::Set(const wchar_t* value, uint32_t set_by_flags) {
    const auto info = locate_vtable_indices();

    if (!info) {
        static bool once = true;

        if (once) {
            SPDLOG_ERROR("s_set_vtable_index is not initialized! Cannot call IConsoleVariable::Set()!");
            once = false;
        }

        return;
    }

    using SetFn = void(__thiscall*)(IConsoleVariable*, const wchar_t*, uint32_t);
    const auto func = (*(SetFn**)this)[info->set_vtable_index];

    func(this, value, set_by_flags);
}

int32_t IConsoleVariable::GetInt() {
    const auto info = locate_vtable_indices();

    if (!info) {
        static bool once = true;

        if (once) {
            SPDLOG_ERROR("s_get_int_vtable_index is not initialized! Cannot call IConsoleVariable::GetInt()!");
            once = false;
        }

        return 0;
    }

    using GetIntFn = int32_t(__thiscall*)(IConsoleVariable*);
    const auto func = (*(GetIntFn**)this)[info->get_int_vtable_index];

    return func(this);
}

float IConsoleVariable::GetFloat() {
    const auto info = locate_vtable_indices();

    if (!info) {
        static bool once = true;

        if (once) {
            SPDLOG_ERROR("s_get_float_vtable_index is not initialized! Cannot call IConsoleVariable::GetFloat()!");
            once = false;
        }

        return 0.0f;
    }

    using GetFloatFn = float(__thiscall*)(IConsoleVariable*);
    const auto func = (*(GetFloatFn**)this)[info->get_float_vtable_index];

    return func(this);
}

std::optional<IConsoleVariable::VtableInfo> IConsoleVariable::locate_vtable_indices() {
    std::scoped_lock _{ s_vtable_mutex };

    const auto vtable = *(uintptr_t**)this;

    if (s_vtable_infos.contains(vtable)) {
        return s_vtable_infos[vtable];
    }

    // Search the vtable for the last function that returns nullptr (AsConsoleCommand)
    // in most cases the +2 function will be the Set function (the +1 function is Release)
    // from there, GetInt, GetFloat, etc will be the next subsequent functions
    // THIS MUST BE CALLED FROM AN ACTUAL IConsoleVariable INSTANCE, NOT A CONSOLE COMMAND!!!
    SPDLOG_INFO("Locating IConsoleVariable vtable indices...");
    std::optional<uint32_t> previous_nullptr_index{};

    SPDLOG_INFO("Vtable: {:x}", (uintptr_t)vtable);

    for (auto i = 0; i < 20; ++i) {
        const auto func = vtable[i];

        if (func == 0 || IsBadReadPtr((void*)func, 1)) {
            SPDLOG_ERROR("Reached end of IConsoleVariable vtable at index {}", i);
            break;
        }

        if (is_vfunc_pattern(func, "33 C0") ||
            is_vfunc_pattern(func, "48 89 4C 24 08 33 C0")) // Weird variant that moves rcx into rsp+8 for... reasons?
        {
            previous_nullptr_index = i;
        } else if (previous_nullptr_index) {
            auto destructor_index = *previous_nullptr_index + 1;
            const auto module_within = utility::get_module_within(func);

            // Verify and go forward a bit to look for something that looks like the destructor
            // Usually all the calls we want will be right after the destructor
            // Most games won't need this automatic analysis, but there
            // are some games which have modified the cvar vtables
            SPDLOG_INFO("Starting from {}, looking for destructor...", destructor_index);
            SPDLOG_INFO("Looking for {:x}...", vtable[0]);
            for (auto j = destructor_index; j < destructor_index + 5; ++j) {
                // Emulate each function and check if instruction pointer lands inside vtable[0]
                // If it does, then we've found the destructor
                SPDLOG_INFO("Emulating vtable[{}]...", j);
                const auto fn = vtable[j];

                if (fn == 0 || fn == vtable[0]) { // if fn == vtable[0], we're in some other vtable
                    SPDLOG_ERROR("Reached end of IConsoleVariable vtable at index {}", j);
                    break;
                }

                utility::ShemuContext emu{ *module_within };

                emu.ctx->Registers.RegRcx = (ND_UINT64)this;
                emu.ctx->Registers.RegRip = (ND_UINT64)fn;
                emu.ctx->MemThreshold = 100;
                
                bool prev_was_branch = false;
                bool found_destructor = false;

                do {
                    SPDLOG_INFO("Emulating instruction at {:x}...", emu.ctx->Registers.RegRip);

                    const auto ip = emu.ctx->Registers.RegRip;

                    if (ip == 0 || IsBadReadPtr((void*)emu.ctx->Registers.RegRip, 4)) {
                        break;
                    }

                    if (ip == vtable[0]) {
                        SPDLOG_INFO("Found destructor at index {}", j);
                        destructor_index = j;
                        found_destructor = true;
                        break;
                    }

                    const auto bytes = (uint8_t*)ip;
                    const auto decoded = utility::decode_one((uint8_t*)ip);

                    if (!decoded) {
                        SPDLOG_ERROR("Failed to decode instruction at {:x}", ip);
                        break;
                    }

                    if (decoded->MemoryAccess & ND_ACCESS_ANY_WRITE) {
                        SPDLOG_INFO("Skipping write to memory instruction at {:x} ({:x} bytes, landing at {:x})", ip, decoded->Length, ip + decoded->Length);
                        emu.ctx->Registers.RegRip += decoded->Length;
                        emu.ctx->Instruction = *decoded; // pseudo-emulate the instruction
                        ++emu.ctx->InstructionsCount;
                        prev_was_branch = false;
                        continue;
                    }

                    prev_was_branch = emu.ctx->InstructionsCount == 0 || emu.ctx->Instruction.BranchInfo.IsBranch;

                    if (prev_was_branch) {
                        SPDLOG_INFO("Branch!");
                    }
                } while(emu.emulate() == SHEMU_SUCCESS && emu.ctx->InstructionsCount < 50);

                if (found_destructor) {
                    break;
                }
            }

            auto& vtable_info = s_vtable_infos[vtable];

            vtable_info.set_vtable_index = destructor_index + 1;
            auto potential_get_int_index = vtable_info.set_vtable_index + 1;

            bool already_skipped = false;

            // means this function is GetBool or something like that.
            if (is_vfunc_pattern(vtable[potential_get_int_index], "83 79")) {
                SPDLOG_INFO("GetBool detected, skipping ahead...");
                potential_get_int_index += 1;
            } else {
                utility::exhaustive_decode((uint8_t*)vtable[potential_get_int_index], 100, [&potential_get_int_index, &already_skipped](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
                    if (already_skipped) {
                        return utility::ExhaustionResult::BREAK;
                    }
                    
                    const uint8_t* bytes = (uint8_t*)ip;

                    // setnz al
                    if (bytes[0] == 0x0F && bytes[1] == 0x95 && bytes[2] == 0xC0) {
                        SPDLOG_INFO("GetBool detected, skipping ahead...");
                        potential_get_int_index += 1;
                        already_skipped = true;
                        return utility::ExhaustionResult::BREAK;
                    }

                    return utility::ExhaustionResult::CONTINUE;
                });
            }

            vtable_info.get_int_vtable_index = potential_get_int_index;
            vtable_info.get_float_vtable_index = vtable_info.get_int_vtable_index + 1;
            SPDLOG_INFO("Encountered final nullptr at index {}", *previous_nullptr_index);
            SPDLOG_INFO("IConsoleVariable::Set vtable index: {}", vtable_info.set_vtable_index);
            SPDLOG_INFO("IConsoleVariable::GetInt vtable index: {}", vtable_info.get_int_vtable_index);
            SPDLOG_INFO("IConsoleVariable::GetFloat vtable index: {}", vtable_info.get_float_vtable_index);

            return vtable_info;
        }
    }

    SPDLOG_ERROR("Failed to locate IConsoleVariable::Set vtable index!");

    // TODO: verify that the destructor
    // hasnt been randomly inserted at the end
    // which will fuck everything up
    return std::nullopt;
}
}