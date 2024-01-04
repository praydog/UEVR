#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>
#include <array>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>

#include <safetyhook.hpp>

#include <utility/PointerHook.hpp>

class DInputHook {
public:
    DInputHook();
    virtual ~DInputHook() = default;

private:
    static HRESULT WINAPI create_hooked(
        HINSTANCE hinst,
        DWORD dwVersion,
        REFIID riidltf,
        LPVOID* ppvOut,
        LPUNKNOWN punkOuter
    );

    static HRESULT enum_devices_hooked(
        LPDIRECTINPUT8W This,
        DWORD dwDevType,
        LPDIENUMDEVICESCALLBACKW lpCallback,
        LPVOID pvRef,
        DWORD dwFlags
    );

    // This is recursive because apparently EnumDevices
    // can call DirectInput8Create again... wHAT?
    std::recursive_mutex m_mutex{};

    safetyhook::InlineHook m_create_hook{};
    std::unique_ptr<PointerHook> m_enum_devices_hook{};
};