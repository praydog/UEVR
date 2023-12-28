#include <chrono>

#include <spdlog/spdlog.h>

#include <dinput.h>

#include "DInputHook.hpp"

DInputHook* g_dinput_hook{nullptr};

constexpr size_t ENUM_DEVICES_VTABLE_INDEX = 4;

DInputHook::DInputHook() {
    SPDLOG_INFO("[DInputHook] Constructing DInputHook");

    g_dinput_hook = this;

    SPDLOG_INFO("[DInputHook] Creating thread");

    std::thread([this]() {
        SPDLOG_INFO("[DInputHook] Entering thread");

        const auto start_time = std::chrono::steady_clock::now();
        HMODULE dinput8{nullptr};

        while (dinput8 == nullptr) {
            dinput8 = GetModuleHandleA("dinput8.dll");

            if (dinput8) {
                SPDLOG_INFO("[DInputHook] dinput8.dll loaded");
                break;
            }

            const auto now = std::chrono::steady_clock::now();

            if (now - start_time > std::chrono::seconds(10)) {
                SPDLOG_ERROR("[DInputHook] Timed out waiting for dinput8.dll to load, aborting hook");
                return;
            }

            SPDLOG_INFO("[DInputHook] Waiting for dinput8.dll to load...");
            Sleep(1000);
        }

        auto create_addr = GetProcAddress(dinput8, "DirectInput8Create");

        if (create_addr == nullptr) {
            SPDLOG_ERROR("[DInputHook] Failed to find DirectInput8Create, aborting hook");
            return;
        }

        SPDLOG_INFO("[DInputHook] Found DirectInput8Create at {:x}", (uintptr_t)create_addr);

        m_create_hook = safetyhook::create_inline(create_addr, (uintptr_t)create_hooked);

        if (!m_create_hook) {
            SPDLOG_ERROR("[DInputHook] Failed to hook DirectInput8Create, aborting hook");
            return;
        }

        SPDLOG_INFO("[DInputHook] Hooked DirectInput8Create");
    }).detach();

    SPDLOG_INFO("[DInputHook] Exiting constructor");
}

HRESULT WINAPI DInputHook::create_hooked(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID* ppvOut,
    LPUNKNOWN punkOuter
) 
{
    SPDLOG_INFO("[DInputHook] DirectInput8Create called");

    const auto og = g_dinput_hook->m_create_hook.original<decltype(&create_hooked)>();
    const auto result = og(hinst, dwVersion, riidltf, ppvOut, punkOuter);

    if (result == DI_OK) {
        SPDLOG_INFO("[DInputHook] DirectInput8Create succeeded");

        if (ppvOut == nullptr) {
            SPDLOG_INFO("[DInputHook] ppvOut is null");
            return result;
        }

        std::scoped_lock _{g_dinput_hook->m_mutex};

        auto iface = (LPDIRECTINPUT8W)*ppvOut;

        if (!g_dinput_hook->m_vtable_hooks.contains(iface)) {
            g_dinput_hook->m_vtable_hooks[iface] = std::make_unique<VtableHook>(iface);
            auto& vthook = g_dinput_hook->m_vtable_hooks[iface];

            if (!vthook->create(iface)) {
                SPDLOG_INFO("[DInputHook] Failed to hook vtable");
                return result;
            }

            // Hook EnumDevices
            if (vthook->hook_method(ENUM_DEVICES_VTABLE_INDEX, DInputHook::enum_devices_hooked)) {
                SPDLOG_INFO("[DInputHook] Hooked IDirectInput8::EnumDevices");
            } else {
                SPDLOG_INFO("[DInputHook] Failed to hook IDirectInput8::EnumDevices");
            
            }
        }
    } else {
        SPDLOG_INFO("[DInputHook] DirectInput8Create failed");
    }

    return result;
}

HRESULT DInputHook::enum_devices_hooked(
    LPDIRECTINPUT8W This,
    DWORD dwDevType,
    LPDIENUMDEVICESCALLBACKW lpCallback,
    LPVOID pvRef,
    DWORD dwFlags
)
{
    SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices called");

    std::scoped_lock _{g_dinput_hook->m_mutex};

    if (!g_dinput_hook->m_vtable_hooks.contains(This)) {
        SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices not hooked");
        return DI_OK;
    }

    auto& vthook = g_dinput_hook->m_vtable_hooks[This];

    if (vthook == nullptr) {
        SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices vtable hook is null");
        return DI_OK;
    }

    auto og = vthook->get_method<decltype(&enum_devices_hooked)>(ENUM_DEVICES_VTABLE_INDEX);

    if (og == nullptr) {
        SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices original method is null");
        return DI_OK;
    }

    SPDLOG_INFO("[DInputHook] Calling original IDirectInput8::EnumDevices");

    auto result = og(This, dwDevType, lpCallback, pvRef, dwFlags);

    SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices returned {}", result);

    if (lpCallback != nullptr) {
        // TODO: Spoof xinput devices
    }

    return result;
}