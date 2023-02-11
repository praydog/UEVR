#include <chrono>
#include <spdlog/spdlog.h>
#include <utility/String.hpp>

#include <safetyhook/Factory.hpp>

#include "utility/SafetyHookSafeFactory.hpp"

#include "Framework.hpp"
#include "Mods.hpp"
#include "XInputHook.hpp"

XInputHook* g_hook{nullptr};

XInputHook::XInputHook() {
    g_hook = this;
    spdlog::info("[XInputHook] Entry");

    static auto find_dll = [](const std::string& name) -> HMODULE {
        const auto start_time = std::chrono::system_clock::now();
        auto found_dll = GetModuleHandleA(name.c_str());

        while (found_dll == nullptr) {
            if (found_dll = GetModuleHandleA(name.c_str()); found_dll != nullptr) {
                // Load it from the system directory instead, it might be hooked
                /*wchar_t system_dir[MAX_PATH]{};
                if (GetSystemDirectoryW(system_dir, MAX_PATH) != 0) {
                    const auto new_dir = (std::wstring{system_dir} + L"\\" + utility::widen(name));

                    spdlog::info("[XInputHook] Loading {} from {}", name, utility::narrow(new_dir));
                    const auto new_dll = LoadLibraryW(new_dir.c_str());

                    if (new_dll != nullptr) {
                        found_dll = LoadLibraryW(new_dir.c_str());
                    }
                }*/

                break;
            }

            const auto elapsed_time = std::chrono::system_clock::now() - start_time;

            if (elapsed_time > std::chrono::seconds(5)) {
                spdlog::error("[XInputHook] Failed to find {} after 5 seconds", name);
                return nullptr;
            }

            std::this_thread::yield();
        }

        return found_dll;
    };

    auto perform_hooks_1_4 = [&]() {
        const auto xinput_1_4_dll = find_dll("xinput1_4.dll");

        if (xinput_1_4_dll != nullptr) {
            std::scoped_lock _{g_framework->get_hook_monitor_mutex()};

            auto factory = SafetyHookSafeFactory::init();
            auto builder = factory->acquire();

            const auto get_state_fn = (void*)GetProcAddress(xinput_1_4_dll, "XInputGetState");
            const auto set_state_fn = (void*)GetProcAddress(xinput_1_4_dll, "XInputSetState");

            if (get_state_fn != nullptr) {
                m_xinput_1_4_get_state_hook = builder.create_inline(get_state_fn, get_state_hook_1_4);
            } else {
                spdlog::error("[XInputHook] Failed to find XInputGetState");
            }

            if (set_state_fn != nullptr) {
                m_xinput_1_4_set_state_hook = builder.create_inline(set_state_fn, set_state_hook_1_4);
            } else {
                spdlog::error("[XInputHook] Failed to find XInputSetState");
            }
        }

         spdlog::info("[XInputHook] Done (1_4)");
    };

    auto perform_hooks_1_3 = [&]() {
        const auto xinput_1_3_dll = find_dll("xinput1_3.dll");

        if (xinput_1_3_dll != nullptr) {
            std::scoped_lock _{g_framework->get_hook_monitor_mutex()};

            auto factory = SafetyHookSafeFactory::init();
            auto builder = factory->acquire();

            const auto get_state_fn = (void*)GetProcAddress(xinput_1_3_dll, "XInputGetState");
            const auto set_state_fn = (void*)GetProcAddress(xinput_1_3_dll, "XInputSetState");

            if (get_state_fn != nullptr) {
                m_xinput_1_3_get_state_hook = builder.create_inline(get_state_fn, get_state_hook_1_3);
            } else {
                spdlog::error("[XInputHook] Failed to find XInputGetState");
            }

            if (set_state_fn != nullptr) {
                m_xinput_1_3_set_state_hook = builder.create_inline(set_state_fn, set_state_hook_1_3);
            } else {
                spdlog::error("[XInputHook] Failed to find XInputSetState");
            }
        }

        spdlog::info("[XInputHook] Done (1_3)");
    };

    // We use a thread because this may possibly take a while to search for the DLLs
    // and it shouldn't cause any issues anyway
    spdlog::info("[XInputHook] Starting hook thread");
    m_hook_thread_1_4 = std::make_unique<std::jthread>(perform_hooks_1_4);
    m_hook_thread_1_3 = std::make_unique<std::jthread>(perform_hooks_1_3);
    spdlog::info("[XInputHook] Hook thread started");
}

uint32_t XInputHook::get_state_hook_1_4(uint32_t user_index, XINPUT_STATE* state) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_1_4_get_state_hook->call<uint32_t>(user_index, state);
    }

    auto ret = g_hook->m_xinput_1_4_get_state_hook->call<uint32_t>(user_index, state);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_get_state(&ret, user_index, state);
    }

    return ret;
}

uint32_t XInputHook::set_state_hook_1_4(uint32_t user_index, XINPUT_VIBRATION* vibration) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_1_4_set_state_hook->call<uint32_t>(user_index, vibration);
    }

    auto ret = g_hook->m_xinput_1_4_set_state_hook->call<uint32_t>(user_index, vibration);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_set_state(&ret, user_index, vibration);
    }

    return ret;
}

uint32_t XInputHook::get_state_hook_1_3(uint32_t user_index, XINPUT_STATE* state) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_1_3_get_state_hook->call<uint32_t>(user_index, state);
    }

    auto ret = g_hook->m_xinput_1_3_get_state_hook->call<uint32_t>(user_index, state);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_get_state(&ret, user_index, state);
    }

    return ret;
}

uint32_t XInputHook::set_state_hook_1_3(uint32_t user_index, XINPUT_VIBRATION* vibration) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_1_3_set_state_hook->call<uint32_t>(user_index, vibration);
    }

    auto ret = g_hook->m_xinput_1_3_set_state_hook->call<uint32_t>(user_index, vibration);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_set_state(&ret, user_index, vibration);
    }

    return ret;
}