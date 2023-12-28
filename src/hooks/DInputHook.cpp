#include <chrono>

#include <spdlog/spdlog.h>
#include <utility/String.hpp>

#include "Framework.hpp"
#include "mods/VR.hpp"
#include "utility/Logging.hpp"

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
    SPDLOG_INFO_EVERY_N_SEC(5, "[DInputHook] DirectInput8Create called");

    const auto og = g_dinput_hook->m_create_hook.original<decltype(&create_hooked)>();
    const auto result = og(hinst, dwVersion, riidltf, ppvOut, punkOuter);

    if (result == DI_OK) {
        if (ppvOut == nullptr) {
            SPDLOG_INFO("[DInputHook] ppvOut is null");
            return result;
        }

        std::scoped_lock _{g_dinput_hook->m_mutex};

        auto iface = (LPDIRECTINPUT8W)*ppvOut;

        if (iface != nullptr) {
            // Its not necessary to make a full blown vtable hook for this because
            // the vtable will always be the same for IDirectInput8W
            // This way we prevent a memory leak from creating many vtable hook instances
            static decltype(DInputHook::m_original_vtable) vt{
                (*(uintptr_t**)iface)[0],
                (*(uintptr_t**)iface)[1],
                (*(uintptr_t**)iface)[2],
                (*(uintptr_t**)iface)[3],
                (uintptr_t)enum_devices_hooked,
                (*(uintptr_t**)iface)[5],
                (*(uintptr_t**)iface)[6],
                (*(uintptr_t**)iface)[7],
                (*(uintptr_t**)iface)[8],
                (*(uintptr_t**)iface)[9],
                (*(uintptr_t**)iface)[10],
                (*(uintptr_t**)iface)[11],
                (*(uintptr_t**)iface)[12],
                (*(uintptr_t**)iface)[13],
                (*(uintptr_t**)iface)[14],
            };

            if (g_dinput_hook->m_original_vtable[0] == 0) {
                memcpy(g_dinput_hook->m_original_vtable.data(), *(uintptr_t**)iface, sizeof(vt));
            }
            
            *(uintptr_t**)iface = vt.data();
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
    SPDLOG_INFO_EVERY_N_SEC(5, "[DInputHook] IDirectInput8::EnumDevices called");

    std::scoped_lock _{g_dinput_hook->m_mutex};

    const auto og = (decltype(&enum_devices_hooked))g_dinput_hook->m_original_vtable[ENUM_DEVICES_VTABLE_INDEX];

    if (og == 0) {
        SPDLOG_INFO("[DInputHook] IDirectInput8::EnumDevices original method is null");
        return DI_OK;
    }

    // We dont care about these other ones, so just call the original
    if (dwDevType != DI8DEVCLASS_GAMECTRL) {
        auto result = og(This, dwDevType, lpCallback, pvRef, dwFlags);
        return result;
    }

    // the purpose of this is to stop some games from spamming calls to EnumDevices
    // without a real controller connected, which causes the game to drop to single digit FPS
    auto should_call_original = g_framework->is_ready() && !VR::get()->is_using_controllers_within(std::chrono::seconds(5));

    if (should_call_original) {
        auto result = og(This, dwDevType, lpCallback, pvRef, dwFlags);
        return result;
    }

    return DI_OK;
}