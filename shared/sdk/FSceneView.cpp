#include <unordered_set>
#include <cstdint>

#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "Utility.hpp"
#include "EngineModule.hpp"
#include "FSceneView.hpp"

namespace sdk {
void FSceneView::constructor(const FSceneViewInitOptions* options) {
    const auto fn_addr = get_constructor_address();

    if (!fn_addr) {
        return;
    }

    const auto fn = (void* (*)(FSceneView*, const FSceneViewInitOptions*))*fn_addr;

    fn(this, options);
}

std::optional<uintptr_t> FSceneView::get_constructor_address() {
    static auto fn_addr = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Searching for FSceneView constructor...");

        const auto module = sdk::get_ue_module(L"Engine");

        if (module == nullptr) {
            return std::nullopt;
        }

        // We need to find the string references "vr.InstancedStereo" and "r.TranslucentSortPolicy"
        // These two strings will reside together within the same function (the constructor)
        // ADDENDUM: We are only finding the references for "r.TranslucentSortPolicy" now
        // we are still making use of "vr.InstancedStereo" but we are checking whether instructions
        // reference data that == L"vr.InstancedStereo" instead of checking for the string reference itself
        const auto translucent_strings = utility::scan_strings(module, L"r.TranslucentSortPolicy");

        if (translucent_strings.empty()) {
            SPDLOG_ERROR("[FSceneView] Failed to find string references for FSceneView constructor");
            return std::nullopt;
        }

        SPDLOG_INFO("[FSceneView] Found string references for FSceneView constructor");

        std::vector<uintptr_t> translucent_string_refs{};

        for (const auto& translucent_string : translucent_strings) {
            SPDLOG_INFO("[FSceneView] Found r.TranslucentSortPolicy string at 0x{:x}", translucent_string);

            const auto translucent_string_refs_ = utility::scan_displacement_references(module, translucent_string);

            translucent_string_refs.insert(translucent_string_refs.end(), translucent_string_refs_.begin(), translucent_string_refs_.end());
        }

        if (translucent_string_refs.empty()) {
            SPDLOG_ERROR("[FSceneView] Failed to find references for FSceneView constructor");
            return std::nullopt;
        }

        // For use with a fallback method
        std::vector<uintptr_t> translucent_functions{};

        for (const auto& translucent_ref : translucent_string_refs) {
            SPDLOG_INFO("[FSceneView] Found r.TranslucentSortPolicy reference at 0x{:x}", translucent_ref);

            const auto translucent_func = utility::find_function_start_with_call(translucent_ref);

            if (!translucent_func) {
                continue;
            }

            translucent_functions.push_back(*translucent_func);
        }

        // previously we naively checked if the vr.InstancedStereo string ref was
        // in the same function as one of the r.TranslucentSortPolicy string refs
        // however, at some point, UE decided to
        // move the vr.InstancedStereo string ref inside of its own function
        // however, this function is still called from the constructor
        // so we can exhaustively disassemble all code paths from the r.TranslucentSortPolicy string refs
        // until we find a reference to the vr.InstancedStereo string along the way
        // this is kind of a scorched earth method, but it works
        SPDLOG_INFO("[FSceneView] Exhaustively searching for FSceneView constructor");

        std::unordered_set<uintptr_t> seen_ips{};

        for (const auto& translucent_function : translucent_functions) {
            SPDLOG_INFO("[FSceneView] Exhaustively searching for references to vr.InstancedStereo in 0x{:x}", translucent_function);

            bool is_correct_function = false;

            utility::exhaustive_decode((uint8_t*)translucent_function, 1000, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
                if (seen_ips.contains(ip) || is_correct_function) {
                    return utility::ExhaustionResult::BREAK;
                }

                seen_ips.insert(ip);

                // Looking for something like "lea rdx, "vr.InstancedStereo""
                // but we will assume it can be any kind of instruction that references the string
                const auto displacement = utility::resolve_displacement(ip);

                if (!displacement) {
                    return utility::ExhaustionResult::CONTINUE;
                }

                // Directly check the data at the displacement instead
                // because modular builds have the string in a different DLL
                // and hardcoding which DLL its in seems sloppy
                try {
                    const auto potential_string = (wchar_t*)*displacement;

                    if (IsBadReadPtr(potential_string, sizeof(wchar_t) * 2)) {
                        return utility::ExhaustionResult::CONTINUE;
                    }

                    if (std::wstring_view{ potential_string } == L"vr.InstancedStereo") {
                        SPDLOG_INFO("[FSceneView] Found correct displacement at 0x{:x}", ip);
                        is_correct_function = true;
                        return utility::ExhaustionResult::BREAK;
                    }
                } catch(...) {

                }

                return utility::ExhaustionResult::CONTINUE;
            });

            if (is_correct_function) {
                SPDLOG_INFO("[FSceneView] Found FSceneView constructor at 0x{:x}", translucent_function);
                return translucent_function;
            }
        }

        SPDLOG_ERROR("[FSceneView] Failed to find FSceneView constructor");
        return std::nullopt;
    }();

    return fn_addr;
}
}