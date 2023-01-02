#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>

#include "EngineModule.hpp"
#include "CVar.hpp"
#include "Slate.hpp"

namespace sdk {
namespace slate {
std::optional<uintptr_t> locate_draw_window_renderthread_fn() {
    static auto result = []() -> std::optional<uintptr_t> {
        spdlog::info("Attempting to locate SlateRHIRenderer::DrawWindow_RenderThread");

        const auto module = sdk::get_ue_module(L"SlateRHIRenderer");

        if (!module) {
            spdlog::error("Failed to get module");
            return std::nullopt;
        }

        // This is in the middle of the function. We need to scan backwards until
        // we run into something that looks like the start of a function.
        const auto draw_to_vr_rt_usage_location = sdk::vr::get_slate_draw_to_vr_render_target_usage_location();

        if (!draw_to_vr_rt_usage_location) {
            spdlog::error("Failed to get draw to vr rt usage location, cannot continue");
            return std::nullopt;
        }

        for (auto func = utility::find_function_start(*draw_to_vr_rt_usage_location);
            func.has_value();
            func = utility::find_function_start(*func - 1))
        {
            spdlog::info("Checking if {:x} is SlateRHIRenderer::DrawWindow_RenderThread", *func);
            auto ref = utility::scan_displacement_reference(module, *func);

            if (!ref) {
                // Fallback scan for obfuscated binaries
                spdlog::info("Performing fallback scan for obfuscated binaries");

                // Function wrapper that just jmps into the function we are looking for
                // protected/obfuscated binaries usually generate this.
                try {
                    ref = utility::scan_relative_reference_strict(module, *func, "E9");

                    if (ref) {
                        ref = utility::scan_displacement_reference(module, *ref - 1);
                    }
                } catch(...) {

                }

                if (!ref) {
                    spdlog::info(" No reference found");
                    continue;
                }
            }

            spdlog::info(" Displacement found at {:x}", *ref);

            const auto resolved = utility::resolve_instruction(*ref);

            if (!resolved) {
                spdlog::info(" Could not resolve instruction");
                continue;
            }

            if (std::string_view{resolved->instrux.Mnemonic}.starts_with("CALL")) {
                spdlog::info("Found SlateRHIRenderer::DrawWindow_RenderThread at {:x}", *func);
                return *func;
            }
        }

        spdlog::error("Failed to locate SlateRHIRenderer::DrawWindow_RenderThread");
        return std::nullopt;
    }();

    return result;
}
}
}