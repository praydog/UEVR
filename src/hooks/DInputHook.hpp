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

    std::mutex m_mutex{};

    safetyhook::InlineHook m_create_hook{};
    std::array<uintptr_t, 15> m_original_vtable{};
};