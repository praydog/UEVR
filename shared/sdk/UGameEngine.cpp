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

        auto fallback_search = [&]() -> std::optional<uintptr_t> {
            spdlog::error("Failed to find UGameEngine::Tick using normal method, trying fallback");
            const auto engine = UEngine::get();

            if (engine != nullptr) {
                // Not find_virtual_function_from_string_ref because it can fail on obfuscated executables
                // where the virtual func is actually a jmp wrapper to the real function that this string is in.
                const auto uengine_tick_pure_virtual = utility::find_function_from_string_ref(module, L"UEngine::Tick", true);

                if (!uengine_tick_pure_virtual) {
                    spdlog::error("Failed to find UEngine::Tick pure virtual");
                    return std::nullopt;
                }

                spdlog::info("UEngine::Tick: {:x} (pure virtual)", (uintptr_t)*uengine_tick_pure_virtual);

                auto uengine_tick_vtable_middle = utility::scan_ptr(module, *uengine_tick_pure_virtual);

                // If this has happened then we are dealing with an obfuscated executable
                // it's not the end of the world, we can still find the address
                // through additional legwork.
                if (!uengine_tick_vtable_middle) {
                    uint32_t insn_size = 0;
                    auto func_call = utility::scan_displacement_reference(module, *uengine_tick_pure_virtual);

                    if (!func_call) {
                        func_call = utility::scan_relative_reference_strict(module, *uengine_tick_pure_virtual, "E9"); // jmp

                        if (!func_call) {
                            spdlog::error("Failed to find UEngine::Tick vtable middle");
                            return std::nullopt;
                        }

                        insn_size = 1;
                    } else {
                        insn_size = *func_call - utility::resolve_instruction(*func_call)->addr;
                    }

                    spdlog::info("UEngine::Tick reference (call or jmp): {:x}", (uintptr_t)*func_call);

                    uengine_tick_vtable_middle = utility::scan_ptr(module, *func_call - insn_size);

                    if (!uengine_tick_vtable_middle) {
                        spdlog::error("Failed to find UEngine::Tick vtable middle even after finding a reference to it");
                        return std::nullopt;
                    }
                }

                spdlog::info("UEngine::Tick: {:x} (vtable middle)", (uintptr_t)*uengine_tick_vtable_middle);

                // Keep going backwards through the vtable until we find an instruction that references the pointer
                for (auto i = 1; i < 200; ++i) {
                    const auto& fn = *(uintptr_t*)(*uengine_tick_vtable_middle - (i * sizeof(void*)));

                    if (fn == 0 || IsBadReadPtr((void*)fn, sizeof(void*))) {
                        spdlog::error("Reached end of vtable during backwards search");
                        break;
                    }

                    // If a reference is found to this address, AND it's a valid instruction
                    // then we have found the start of vtable.
                    if (utility::scan_displacement_reference(module, (uintptr_t)&fn)) {
                        spdlog::info("UGameEngine::Tick: found at vtable index {}", i);
                        const auto real_engine_vtable = *(uintptr_t**)engine;
                        const auto actual_fn = real_engine_vtable[i];
                        spdlog::info("UGameEngine::Tick: {:x}", actual_fn);
                        return actual_fn;
                    }
                }
            }

            spdlog::error("UGameEngine::Tick: failed to find address via fallback");
            return std::nullopt;
        };

        if (!result) {
            return fallback_search().value_or(0);
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
                return fallback_search().value_or(0);
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