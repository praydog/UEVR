#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>

#include "EngineModule.hpp"

#include "UGameEngine.hpp"

namespace sdk {
std::optional<uintptr_t> UGameEngine::get_tick_address() {
    static auto addr = []() -> uintptr_t {
        spdlog::info("UGameEngine::get_tick_address: scanning for address");

        const auto module = sdk::get_ue_module(L"Engine");
        const auto result = utility::find_virtual_function_from_string_ref(module, L"causeevent=", true); // present for a very long time

        if (!result) {
            spdlog::error("Failed to find UGameEngine::Tick!");
            return 0;
        }

        const auto engine = UEngine::get();

        if (engine != nullptr) {
            // Double check via the vtable that this function is actually UGameEngine::Tick
            const auto vtable = *(uintptr_t**)engine;
            bool exists = false;

            if (vtable != nullptr && !IsBadReadPtr(vtable, sizeof(void*))) {
                spdlog::info("Double checking UGameEngine::Tick via vtable...");

                for (auto i = 0; i < 200; ++i) {
                    if (IsBadReadPtr(&vtable[i], sizeof(void*))) {
                        break;
                    }

                    const auto fn = vtable[i];

                    if (fn == *result) {
                        spdlog::info("UGameEngine::Tick: found at vtable index {}", i);
                        exists = true;
                        break;
                    }
                }
            }

            if (!exists) {
                spdlog::error("UGameEngine::Tick: vtable check failed!");
                return 0;
            }
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