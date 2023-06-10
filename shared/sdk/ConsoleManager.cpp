#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>

#include "EngineModule.hpp"

#include "ConsoleManager.hpp"

namespace detail {
sdk::FConsoleManager** try_find_console_manager(const std::wstring& string_candidate) {
    SPDLOG_INFO("Finding IConsoleManager...");

    const auto now = std::chrono::steady_clock::now();

    const auto core_module = sdk::get_ue_module(L"Core");
    const auto candidate_string = utility::scan_string(core_module, string_candidate.c_str());

    if (!candidate_string) {
        SPDLOG_ERROR("Failed to find {} string", utility::narrow(string_candidate));
        return nullptr;
    }

    const auto candidate_stringref = utility::scan_displacement_reference(core_module, *candidate_string);

    if (!candidate_stringref) {
        SPDLOG_ERROR("Failed to find {} stringref", utility::narrow(string_candidate));
        return nullptr;
    }

    SPDLOG_INFO("Found {} stringref: {:x}", utility::narrow(string_candidate), (uintptr_t)*candidate_stringref);

    const auto containing_function = utility::find_function_start_with_call(*candidate_stringref);

    if (!containing_function) {
        SPDLOG_ERROR("Failed to find containing function");
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

        // hinges on IConsoleManager actually being constructed and assigned
        if (!IsBadReadPtr((void*)*displacement, sizeof(void*)) && !IsBadReadPtr(*(void**)*displacement, sizeof(void*))) {
            global_variable_references[*displacement]++;

            if (!highest_global_variable_reference || global_variable_references[*displacement] > std::get<1>(*highest_global_variable_reference)) {
                highest_global_variable_reference = std::make_tuple(*displacement, global_variable_references[*displacement]);
            }
        }

        return utility::ExhaustionResult::CONTINUE;
    });

    if (!highest_global_variable_reference) {
        SPDLOG_ERROR("Failed to find any references to global variables");
        return nullptr;
    }

    SPDLOG_INFO("Found IConsoleManager**: {:x}", (uintptr_t)std::get<0>(*highest_global_variable_reference));
    SPDLOG_INFO("Points to IConsoleManager*: {:x}", *(uintptr_t*)std::get<0>(*highest_global_variable_reference));

    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count();
    SPDLOG_INFO("Took {}ms to find IConsoleManager", diff);

    return (sdk::FConsoleManager**)std::get<0>(*highest_global_variable_reference);
}
}

namespace sdk {
FConsoleManager* FConsoleManager::get() {
    static auto result = []() -> FConsoleManager** {
        std::vector<std::wstring> candidates {
            L"r.DumpingMovie",
            L"vr.pixeldensity"
        };

        const auto now = std::chrono::steady_clock::now();

        for (const auto& candidate : candidates) {
            spdlog::info("Trying to find IConsoleManager with candidate: {}", utility::narrow(candidate));
            auto result = detail::try_find_console_manager(candidate);

            if (result) {
                spdlog::info("Took {}ms to search through all candidates", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count());
                return result;
            }
        }

        spdlog::error("Failed to find IConsoleManager");

        return nullptr;
    }();

    if (result == nullptr) {
        return nullptr;
    }

    return *result;
}
}