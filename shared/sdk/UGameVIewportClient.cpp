#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "EngineModule.hpp"

#include "UGameViewportClient.hpp"

namespace sdk {
std::optional<uintptr_t> UGameViewportClient::get_draw_function() {
    static auto result = []() -> std::optional<uintptr_t> {
        const auto engine_module = sdk::get_ue_module(L"Engine");
        const auto canvas_object_strings = utility::scan_strings(engine_module, L"CanvasObject", true);

        if (canvas_object_strings.empty()) {
            SPDLOG_ERROR("Failed to find CanvasObject string!");
            return std::nullopt;
        }

        std::optional<uintptr_t> game_viewport_client_draw{};

        for (const auto canvas_object_string : canvas_object_strings) {
            SPDLOG_INFO("Analyzing CanvasObject string at {:x}", (uintptr_t)canvas_object_string);

            const auto string_ref = utility::scan_displacement_reference(engine_module, canvas_object_string);

            if (!string_ref) {
                SPDLOG_INFO(" No string reference, continuing on to next string...");
                continue;
            }

            const auto func = utility::find_virtual_function_start(*string_ref);

            if (func) {
                game_viewport_client_draw = func;
                break;
            }
        }

        // Luckily this function is the only one with a CanvasObject string.
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