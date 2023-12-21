#include <unordered_set>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/String.hpp>
#include <utility/Module.hpp>
#include <utility/ScopeGuard.hpp>

#include <tracy/Tracy.hpp>

#include "EngineModule.hpp"

#include "UObjectHashTables.hpp"

namespace sdk {
FUObjectHashTables* FUObjectHashTables::get() {
    static auto result = []() -> FUObjectHashTables* {
        ZoneScopedN("sdk::UObjectHashTables::get static init");
        SPDLOG_INFO("[UObjectHashTables::get] Finding UObjectHashTables...");

        const auto core_uobject = sdk::get_ue_module(L"CoreUObject");

        if (core_uobject == nullptr) {
            return nullptr;
        }

        const auto str = utility::scan_string(core_uobject, L"/Temp/%s", true);

        if (!str) {
            SPDLOG_ERROR("[FUObjectHashTables::get] Failed to find /Temp/%s");
            return nullptr;
        }

        const auto str_ref = utility::scan_displacement_reference(core_uobject, *str);

        if (!str_ref) {
            SPDLOG_ERROR("[FUObjectHashTables::get] Failed to find /Temp/%s reference");
            return nullptr;
        }

        // Skip next call, it's never it.
        const auto start_1 = *str_ref + 4;
        const auto next_call = utility::scan_mnemonic(start_1, 100, "CALL");

        if (!next_call) {
            SPDLOG_ERROR("[FUObjectHashTables::get] Failed to find next call");
            return nullptr;
        }

        const auto start_2 = *next_call + utility::decode_one((uint8_t*)*next_call).value_or(INSTRUX{}).Length;
        bool found = false;

        const auto rtl_initialize_critical_section = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlInitializeCriticalSection");

        auto resolve_arbitrary_call = [](uintptr_t addr, const INSTRUX& ix) -> std::optional<uintptr_t> {
            auto fn = utility::resolve_displacement(addr);

            if (!fn) {
                return std::nullopt;
            }

            uint8_t* ip = (uint8_t*)addr;

            if (ix.IsRipRelative && ip[0] == 0xFF && ip[1] == 0x15) {
                if (*fn != 0 && fn != addr && !IsBadReadPtr((void*)*fn, sizeof(void*))) {
                    const auto real_dest = *(uintptr_t*)*fn;

                    if (real_dest != 0 && real_dest != (uintptr_t)ip && !IsBadReadPtr((void*)real_dest, sizeof(void*))) {
                        fn = real_dest;
                    }
                }
            }

            return fn;
        };

        auto analyze_instr = [&](utility::ExhaustionContext& ctx, std::unordered_map<ND_UINT32, uint64_t>& register_states) -> std::optional<utility::ExhaustionResult> {
            if (found) {
                return utility::ExhaustionResult::BREAK;
            }

            const auto mnem = std::string_view{ctx.instrux.Mnemonic};

            if (mnem.starts_with("LEA") || mnem.starts_with("MOV")) {
                // Keep track of last GPR load (from memory)
                if (ctx.instrux.Operands[0].Type == ND_OP_REG) {
                    if (mnem.starts_with("LEA")) {
                        if (auto disp = utility::resolve_displacement(ctx.addr); disp.has_value() && ctx.instrux.IsRipRelative) {
                            register_states[ctx.instrux.Operands[0].Info.Register.Reg] = *disp;
                            //SPDLOG_INFO("[UObjectHashTables::get] Found GPR load: 0x{:X} ({:x} rel)", ctx.addr, ctx.addr - (uintptr_t)core_uobject);
                            return utility::ExhaustionResult::CONTINUE;
                        }

                        register_states.erase(ctx.instrux.Operands[0].Info.Register.Reg);
                    }

                    if (mnem.starts_with("MOV")) {
                        register_states.erase(ctx.instrux.Operands[0].Info.Register.Reg);

                        if (ctx.instrux.Operands[1].Type == ND_OP_REG && register_states.contains(ctx.instrux.Operands[1].Info.Register.Reg)) {
                            register_states[ctx.instrux.Operands[0].Info.Register.Reg] = register_states[ctx.instrux.Operands[1].Info.Register.Reg];
                        }
                    }
                }
            } else if (mnem.starts_with("ADD") || mnem.starts_with("SUB")) {
                if (ctx.instrux.Operands[0].Type == ND_OP_REG) {
                    // If the original register is clobbered in any way, it's not the right structure.
                    // The original memory address loaded into RCX is always used as the base address for the hash table.
                    register_states.erase(ctx.instrux.Operands[0].Info.Register.Reg);
                }
            } else if (mnem.starts_with("CALL")) {
                // If next call lands in InitializeCriticalSection, we're done
                auto fn = resolve_arbitrary_call(ctx.addr, ctx.instrux);

                if (!fn) {
                    return utility::ExhaustionResult::STEP_OVER;
                }

                if (*fn == (uintptr_t)&InitializeCriticalSection || *fn == (uintptr_t)rtl_initialize_critical_section) {
                    if (register_states.contains(NDR_RCX)) {
                        const auto rcx = register_states[NDR_RCX];

                        if (!IsBadReadPtr((void*)rcx, sizeof(void*)) && utility::get_module_within(rcx) == core_uobject) {
                            found = true;
                            return utility::ExhaustionResult::BREAK;
                        }
                    }

                    return utility::ExhaustionResult::STEP_OVER;
                }
            }

            return std::nullopt;
        };

        std::unordered_set<uintptr_t> seen_ips{};

        // Recursive function analyzer. Used on every CALL instruction to linearly keep track of register states.
        // We could use just exhaustive_decode, but that is not a linear disassembly, it adds CALL instructions to a branch list
        // and steps over them, which is not what we want.
        auto analyze_fn = [&](this const auto& self, uintptr_t start, std::unordered_map<ND_UINT32, uint64_t>& register_states) -> void {
            utility::exhaustive_decode((uint8_t*)start, 100, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
                if (found || seen_ips.contains(ctx.addr)) {
                    return utility::ExhaustionResult::BREAK;
                }

                seen_ips.insert(ctx.addr);

                const auto mnem = std::string_view{ctx.instrux.Mnemonic};

                if (auto exhaustion_result = analyze_instr(ctx, register_states); exhaustion_result.has_value()) {
                    return *exhaustion_result;
                }

                if (mnem.starts_with("CALL")) {
                    auto fn = resolve_arbitrary_call(ctx.addr, ctx.instrux);

                    if (fn) {
                        const auto valid_fn = !seen_ips.contains(*fn) && !utility::find_pointer_in_path(*fn, &GlobalMemoryStatusEx);

                        if (valid_fn) {
                            self(*fn, register_states);

                            if (found) {
                                return utility::ExhaustionResult::BREAK;
                            }
                        }
                    }

                    // Just clear all register states on call
                    // too lazy to clear volatile registers
                    register_states.clear();

                    return utility::ExhaustionResult::STEP_OVER;
                }

                return utility::ExhaustionResult::CONTINUE;
            });
        };

        std::unordered_map<ND_UINT32, uint64_t> register_states{};
        analyze_fn(start_2, register_states);

        if (found && register_states.contains(NDR_RCX)) {
            const auto result = (FUObjectHashTables*)register_states[NDR_RCX];
            SPDLOG_INFO("[FUObjectHashTables::get] Found UObjectHashTables: 0x{:X} ({:x} rel)", (uintptr_t)result, (uintptr_t)result - (uintptr_t)core_uobject);
            return result;
        }

        SPDLOG_ERROR("[FUObjectHashTables::get] Failed to find UObjectHashTables");
        return nullptr;
    }();

    return result;
}
}