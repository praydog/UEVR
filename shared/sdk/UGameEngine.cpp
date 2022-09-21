#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>

#include "EngineModule.hpp"

#include "UGameEngine.hpp"

namespace sdk {
std::optional<uintptr_t> UGameEngine::get_tick_address() {
    static auto addr = []() -> uintptr_t {
        spdlog::info("UGameEngine::get_tick_address: scanning for address");

        const auto module = sdk::get_ue_module(L"Engine");
        const auto result = utility::find_virtual_function_from_string_ref(module, L"causeevent="); // present for a very long time

        if (!result) {
            spdlog::error("Failed to find UGameEngine::Tick!");
            return 0;
        }

        spdlog::info("UGameEngine::Tick: {:x}", (uintptr_t)*result);

        return *result;
    }();

    if (addr == 0) {
        return std::nullopt;
    }

    return addr;
}
}