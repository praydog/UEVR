#include <chrono>
#include <spdlog/spdlog.h>

#include <safetyhook/Factory.hpp>

#include "Framework.hpp"
#include "Mods.hpp"
#include "XInputHook.hpp"

XInputHook* g_hook{nullptr};

XInputHook::XInputHook() {
    g_hook = this;
    spdlog::info("[XInputHook] Entry");

    const auto start_time = std::chrono::system_clock::now();
    HMODULE xinput_dll{};

    while (xinput_dll == nullptr) {
        xinput_dll = GetModuleHandleA("xinput1_4.dll");
        if (xinput_dll == nullptr) {
            xinput_dll = GetModuleHandleA("xinput1_3.dll");
        }

        if (xinput_dll == nullptr) {
            const auto elapsed_time = std::chrono::system_clock::now() - start_time;
            if (elapsed_time > std::chrono::seconds(5)) {
                spdlog::error("[XInputHook] Failed to find xinput dll");
                return;
            }
        }
    }

    auto factory = safetyhook::Factory::init();
    auto builder = factory->acquire();

    const auto get_state_fn = (void*)GetProcAddress(xinput_dll, "XInputGetState");
    const auto set_state_fn = (void*)GetProcAddress(xinput_dll, "XInputSetState");

    if (get_state_fn != nullptr) {
        m_xinput_get_state_hook = builder.create_inline(get_state_fn, get_state_hook);
    } else {
        spdlog::error("[XInputHook] Failed to find XInputGetState");
    }

    if (set_state_fn != nullptr) {
        m_xinput_set_state_hook = builder.create_inline(set_state_fn, set_state_hook);
    } else {
        spdlog::error("[XInputHook] Failed to find XInputSetState");
    }

    spdlog::info("[XInputHook] Done");
}

uint32_t XInputHook::get_state_hook(uint32_t user_index, XINPUT_STATE* state) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_get_state_hook->call<uint32_t>(user_index, state);
    }

    auto ret = g_hook->m_xinput_get_state_hook->call<uint32_t>(user_index, state);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_get_state(&ret, user_index, state);
    }

    return ret;
}

uint32_t XInputHook::set_state_hook(uint32_t user_index, XINPUT_VIBRATION* vibration) {
    if (!g_framework->is_ready()) {
        return g_hook->m_xinput_set_state_hook->call<uint32_t>(user_index, vibration);
    }

    auto ret = g_hook->m_xinput_set_state_hook->call<uint32_t>(user_index, vibration);

    const auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_xinput_set_state(&ret, user_index, vibration);
    }

    return ret;
}