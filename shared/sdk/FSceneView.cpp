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
        const auto instanced_strings = utility::scan_strings(module, L"vr.InstancedStereo");
        const auto translucent_strings = utility::scan_strings(module, L"r.TranslucentSortPolicy");

        if (instanced_strings.empty() || translucent_strings.empty()) {
            SPDLOG_ERROR("[FSceneView] Failed to find string references for FSceneView constructor");
            return std::nullopt;
        }

        SPDLOG_INFO("[FSceneView] Found string references for FSceneView constructor");

        // We need to find the function that contains both of these strings
        std::vector<uintptr_t> instanced_string_refs{};
        std::vector<uintptr_t> translucent_string_refs{};
        //const auto instanced_string_refs = utility::scan_displacement_references(module, *instanced_string);
        //const auto translucent_string_refs = utility::scan_displacement_references(module, *translucent_string);

        for (const auto& instanced_string : instanced_strings) {
            const auto instanced_string_refs_ = utility::scan_displacement_references(module, instanced_string);

            instanced_string_refs.insert(instanced_string_refs.end(), instanced_string_refs_.begin(), instanced_string_refs_.end());
        }

        for (const auto& translucent_string : translucent_strings) {
            const auto translucent_string_refs_ = utility::scan_displacement_references(module, translucent_string);

            translucent_string_refs.insert(translucent_string_refs.end(), translucent_string_refs_.begin(), translucent_string_refs_.end());
        }

        if (instanced_string_refs.empty() || translucent_string_refs.empty()) {
            SPDLOG_ERROR("[FSceneView] Failed to find references for FSceneView constructor");
            return std::nullopt;
        }

        std::vector<uintptr_t> instanced_functions{};

        for (const auto& instanced_ref : instanced_string_refs) {
            const auto instanced_func = utility::find_function_start_with_call(instanced_ref);

            if (!instanced_func) {
                continue;
            }

            instanced_functions.push_back(*instanced_func);
        }

        for (const auto& translucent_ref : translucent_string_refs) {
            const auto translucent_func = utility::find_function_start_with_call(translucent_ref);

            if (!translucent_func) {
                continue;
            }

            for (const auto& instanced_func : instanced_functions) {
                if (instanced_func == *translucent_func) {
                    SPDLOG_INFO("[FSceneView] Found FSceneView constructor at 0x{:x}", instanced_func);
                    return instanced_func;
                }
            }
        }

        SPDLOG_ERROR("[FSceneView] Failed to find FSceneView constructor");
        return std::nullopt;
    }();

    return fn_addr;
}
}