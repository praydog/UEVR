#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>

#include <safetyhook.hpp>
#include <utility/VtableHook.hpp>

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
    std::unordered_map<LPDIRECTINPUT8W, std::unique_ptr<VtableHook>> m_vtable_hooks{};
};