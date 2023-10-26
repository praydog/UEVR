#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>

#include <tracy/Tracy.hpp>

#include "EngineModule.hpp"
#include "Globals.hpp"

namespace sdk {
namespace globals {
float& get_near_clipping_plane() {
    // There's a few string references we can use here,
    // There's the console command, "r.SetNearClipPlane",
    // But there's also, "bEnableOnScreenDebugMessages", GNearClippingPlane is set shortly after that
    // We'll use the latter for now, we can use r.SetNearClipPlane as a fallback later on
    static float* result = []() -> float* {
        ZoneScopedN("sdk::globals::get_near_clipping_plane static init");
        SPDLOG_INFO("Attempting to find GNearClippingPlane");

        const auto engine = sdk::get_ue_module(L"Engine");
        const auto bEnableOnScreenDebugMessages_str = utility::scan_string(engine, L"bEnableOnScreenDebugMessages", true);

        if (!bEnableOnScreenDebugMessages_str) {
            SPDLOG_ERROR("Failed to find bEnableOnScreenDebugMessages string, cannot find GNearClippingPlane");
            return nullptr;
        }

        const auto bEnableOnScreenDebugMessages_ref = utility::scan_displacement_reference(engine, *bEnableOnScreenDebugMessages_str);

        if (!bEnableOnScreenDebugMessages_ref) {
            SPDLOG_ERROR("Failed to find reference to bEnableOnScreenDebugMessages string, cannot find GNearClippingPlane");
            return nullptr;
        }

        spdlog::info("Found reference to bEnableOnScreenDebugMessages at {:x}", *bEnableOnScreenDebugMessages_ref);

        const auto bEnableOnScreenDebugMessages_instr = utility::resolve_instruction(*bEnableOnScreenDebugMessages_ref);

        if (!bEnableOnScreenDebugMessages_instr) {
            SPDLOG_ERROR("Failed to find instruction containing reference to bEnableOnScreenDebugMessages string, cannot find GNearClippingPlane");
            return nullptr;
        }

        // Scan forward until we run into something that looks like "movss cs:ptr, xmm0" where ptr is the address of GNearClippingPlane
        // const auto movss = utility::scan_disasm(*bEnableOnScreenDebugMessages_instr, 20, "F3 0F 11 05 ? ? ? ?");
        // above is a naive way of doing it, we'll do an exhaustive decode instead that looks at the instruction disassembly
        bool found = false;
        float* result = nullptr;

        utility::exhaustive_decode((uint8_t*)bEnableOnScreenDebugMessages_instr->addr, 50, [&](const INSTRUX& instr, uintptr_t ip) -> utility::ExhaustionResult {
            if (found) {
                return utility::ExhaustionResult::BREAK;
            }
            
            if (std::string_view{instr.Mnemonic}.starts_with("MOVSS") && instr.Operands[0].Type == ND_OP_MEM && instr.Operands[1].Type == ND_OP_REG) {
                SPDLOG_INFO("Found MOVSS at {:x}", ip);

                found = true;
                const auto resolved = utility::resolve_displacement(ip);

                if (resolved) {
                    result = (float*)*resolved;
                    SPDLOG_INFO("Found GNearClippingPlane at {:x}", (uintptr_t)result);
                } else {
                    SPDLOG_ERROR("Failed to resolve displacement for GNearClippingPlane");
                }

                return utility::ExhaustionResult::BREAK;
            }

            if (std::string_view{instr.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER; // we dont care about calls
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        // Fallback for modular builds
        std::optional<uintptr_t> last_ptr_store{};
        std::optional<uint32_t> last_ptr_stored_register{};
        utility::exhaustive_decode((uint8_t*)bEnableOnScreenDebugMessages_instr->addr, 50, [&](const INSTRUX& instr, uintptr_t ip) -> utility::ExhaustionResult {
            if (found) {
                return utility::ExhaustionResult::BREAK;
            }
    
            // Look for something like a "mov rax, cs:ptr", keep track of it for later
            if (std::string_view{instr.Mnemonic}.starts_with("MOV") && instr.Operands[0].Type == ND_OP_REG && 
                instr.Operands[1].Type == ND_OP_MEM && instr.Operands[1].Info.Memory.IsRipRel)
            {
                SPDLOG_INFO("Found MOV at {:x}", ip);

                const auto orig_store = last_ptr_store;
                last_ptr_store = utility::resolve_displacement(ip);

                if (!last_ptr_store) {
                    last_ptr_store = orig_store;
                    SPDLOG_ERROR("Failed to resolve displacement for MOV at {:x}", ip);
                    return utility::ExhaustionResult::CONTINUE;
                }

                if (!IsBadReadPtr((void*)*last_ptr_store, sizeof(uintptr_t)) && !IsBadReadPtr(*(void**)*last_ptr_store, sizeof(uintptr_t))) { // means it's a modular build
                    last_ptr_store = *(uintptr_t*)*last_ptr_store;

                    if (!IsBadReadPtr((void*)*last_ptr_store, sizeof(uintptr_t)) && !IsBadReadPtr(*(void**)*last_ptr_store, sizeof(uintptr_t))) { // Means it's now the wrong pointer, it should just be a float at this point, not a valid pointer
                        last_ptr_store = orig_store;
                        return utility::ExhaustionResult::CONTINUE;
                    }
                }

                last_ptr_stored_register = instr.Operands[0].Info.Register.Reg;
            }

            if (last_ptr_store && last_ptr_stored_register) {
                if (std::string_view{instr.Mnemonic}.starts_with("MOV") && instr.Operands[0].Type == ND_OP_MEM && instr.Operands[0].Info.Memory.HasBase) {
                    SPDLOG_INFO("Found mov with mem and base at {:x}", ip);

                    if (instr.Operands[0].Info.Memory.Base == *last_ptr_stored_register) {
                        SPDLOG_INFO("Found mov into GNearClippingPlane at {:x}", ip);

                        found = true;
                        result = (float*)*last_ptr_store;
                        return utility::ExhaustionResult::BREAK;
                    }
                }
            }

            if (std::string_view{instr.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER; // we dont care about calls
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (result == nullptr) {
            SPDLOG_ERROR("Failed to find GNearClippingPlane");
        } else try {
            SPDLOG_INFO("GNearClippingPlane is {}", *result);
        } catch(...) {

        }

        return result;
    }();

    if (result == nullptr) {
        static float dummy = 1.0f;
        return dummy;
    }

    return *result;
}
}
}