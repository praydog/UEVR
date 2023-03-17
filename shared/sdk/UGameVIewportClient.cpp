#include <unordered_map>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "EngineModule.hpp"

#include "UGameViewportClient.hpp"

namespace sdk {
std::optional<uintptr_t> UGameViewportClient::get_draw_function() {
    static auto result = []() -> std::optional<uintptr_t> {
        const auto engine_module = sdk::get_ue_module(L"Engine");
        const auto engine_size = utility::get_module_size(engine_module);
        const auto engine_end = engine_module + *engine_size;
        const auto canvas_object_strings = utility::scan_strings(engine_module, L"CanvasObject", true);

        if (canvas_object_strings.empty()) {
            SPDLOG_ERROR("Failed to find CanvasObject string!");
            return std::nullopt;
        }

        // Step 1: Find all of the functions that contain the "CanvasObject" string.
        // In most games, this is usually only one function, UGameViewportClient::Draw.
        // However, sometimes there are multiple very similar functions, but they don't get called
        // so we don't want to return the wrong function.

        // Step 2: Disassemble the functions and look for function calls to the other functions that contain the
        // "CanvasObject" string. This is because games can have multiple viewport types
        // and they will fallback to calling UGameViewportClient::Draw.
        std::vector<uintptr_t> possible_functions{};

        for (const auto canvas_object_string : canvas_object_strings) {
            SPDLOG_INFO("Analyzing CanvasObject string at {:x}", (uintptr_t)canvas_object_string);

            const auto string_refs = utility::scan_displacement_references(engine_module, canvas_object_string);

            if (string_refs.empty()) {
                SPDLOG_INFO(" No string references, continuing on to next string...");
                continue;
            }

            SPDLOG_INFO("Found {} string references", string_refs.size());

            for (auto string_ref : string_refs) {
                SPDLOG_INFO(" Found string reference at {:x}", (uintptr_t)string_ref);

                const auto func = utility::find_virtual_function_start(string_ref);

                if (func) {
                    possible_functions.push_back(*func);
                }
            }
        }

        if (possible_functions.empty()) {
            SPDLOG_ERROR("Failed to find any functions that contain the CanvasObject string!");
            return std::nullopt;
        }
        
        std::optional<uintptr_t> game_viewport_client_draw{};

        if (possible_functions.size() == 1) {
            game_viewport_client_draw = possible_functions[0];
        } else {
            std::unordered_set<uintptr_t> impossible_functions{};
            SPDLOG_INFO("Possible function count is {}, analyzing functions...", possible_functions.size());

            for (const auto possible_function : possible_functions) {
                SPDLOG_INFO("Analyzing possible function at {:x}", (uintptr_t)possible_function);;

                for (const auto other_function : possible_functions) {
                    if (possible_function == other_function) {
                        continue;
                    }
                    
                    // Disassemble the function and all of its possible branches.
                    utility::exhaustive_decode((uint8_t*)other_function, 3000, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
                        // Do not break, there can be multiple calls to the same function or other functions.
                        /*if (result) {
                            return utility::ExhaustionResult::BREAK;
                        }*/

                        if (ix.BranchInfo.IsBranch && !ix.BranchInfo.IsConditional) {
                            if (auto resolved = utility::resolve_displacement(ip); *resolved == possible_function) {
                                impossible_functions.insert(other_function);
                                SPDLOG_INFO("Found possible function within function at {:x} (insn {:x})", (uintptr_t)other_function, ip);
                                return utility::ExhaustionResult::BREAK;
                            }

                            if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                                return utility::ExhaustionResult::STEP_OVER;
                            }
                        }

                        return utility::ExhaustionResult::CONTINUE;
                    });
                }
            }

            if (impossible_functions.empty() || impossible_functions.size() == possible_functions.size()) {
                SPDLOG_ERROR("Falling back to possible function at {:x}", (uintptr_t)possible_functions[0]);
                game_viewport_client_draw = possible_functions[0];
            } else {
                // go through all of the possible functions and find the one that is not in the impossible functions.
                // UGameViewportClient::Draw is guaranteed to never call the other functions
                // it is only the other functions that end up calling UGameViewportClient::Draw.
                for (const auto possible_function : possible_functions) {
                    if (impossible_functions.find(possible_function) == impossible_functions.end()) {
                        SPDLOG_INFO("Found possible function at {:x}", (uintptr_t)possible_function);
                        game_viewport_client_draw = possible_function;
                        break;
                    }
                }
            }
        }

        // Luckily this function is the only one with a CanvasObject string.
        // ADDENDUM: using the previous information, this is not actually true sometimes.
        if (!game_viewport_client_draw) {
            SPDLOG_ERROR("Failed to find UGameViewportClient::Draw function!");
            return std::nullopt;
        }

        SPDLOG_INFO("Found UGameViewportClient::Draw function at {:x}", (uintptr_t)*game_viewport_client_draw);
        return game_viewport_client_draw;
    }();

    return result;
}
}