#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>

#include "EngineModule.hpp"

#include "ConsoleManager.hpp"

namespace sdk {
FConsoleManager* FConsoleManager::get() {
    static auto result = []() -> FConsoleManager** {
        SPDLOG_INFO("Finding IConsoleManager...");

        const auto core_module = sdk::get_ue_module(L"Core");
        const auto r_dumping_movie_string = utility::scan_string(core_module, L"r.DumpingMovie");

        if (!r_dumping_movie_string) {
            SPDLOG_ERROR("Failed to find r.DumpingMovie string");
            return nullptr;
        }

        const auto r_dumping_movie_stringref = utility::scan_displacement_reference(core_module, *r_dumping_movie_string);

        if (!r_dumping_movie_stringref) {
            SPDLOG_ERROR("Failed to find r.DumpingMovie stringref");
            return nullptr;
        }

        SPDLOG_INFO("Found r.DumpingMovie stringref: {:x}", (uintptr_t)*r_dumping_movie_stringref);

        const auto containing_function = utility::find_function_start_with_call(*r_dumping_movie_stringref);

        if (!containing_function) {
            SPDLOG_ERROR("Failed to find containing function");
            return nullptr;
        }

        // Check how many references there are to this function, if it's greater than say... 20, this is
        // IConsoleManager::SetupSingleton
        // If not, we can just disassemble the function looking for references to global variables
        const auto function_references = utility::scan_displacement_references(core_module, *containing_function, std::optional<size_t>{20});

        if (function_references.empty()) {
            SPDLOG_ERROR("Failed to find any references to containing function");
            return nullptr;
        }

        if (function_references.size() < 20) {
            SPDLOG_ERROR("Found too few references to containing function");
            return nullptr;
        }
        
        // Disassemble the function and look for references to global variables
        std::unordered_map<uintptr_t, size_t> global_variable_references{};
        std::optional<std::tuple<uintptr_t, size_t>> highest_global_variable_reference{};

        utility::exhaustive_decode((uint8_t*)*containing_function, 20, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            const auto displacement = utility::resolve_displacement(ip);

            if (!displacement) {
                return utility::ExhaustionResult::CONTINUE;
            }

            global_variable_references[*displacement]++;

            if (!highest_global_variable_reference || global_variable_references[*displacement] > std::get<1>(*highest_global_variable_reference)) {
                highest_global_variable_reference = std::make_tuple(*displacement, global_variable_references[*displacement]);
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (!highest_global_variable_reference) {
            SPDLOG_ERROR("Failed to find any references to global variables");
            return nullptr;
        }

        SPDLOG_INFO("Found IConsoleManager**: {:x}", (uintptr_t)std::get<0>(*highest_global_variable_reference));
        SPDLOG_INFO("Points to IConsoleManager*: {:x}", *(uintptr_t*)std::get<0>(*highest_global_variable_reference));

        return (FConsoleManager**)std::get<0>(*highest_global_variable_reference);
    }();

    if (result == nullptr) {
        return nullptr;
    }

    return *result;
}
}