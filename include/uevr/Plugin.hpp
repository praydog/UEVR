/*
This file (Plugin.hpp) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Helper header to easily instantiate a plugin
// and get some initial callbacks setup
// the user can inherit from the Plugin class
// and set uevr::g_plugin to their plugin instance
#pragma once

#include <windows.h>
#include <Xinput.h>

#include <d3d11.h>
#include <d3d12.h>

#include "API.hpp"

namespace uevr {
class Plugin;

namespace detail {
    static inline ::uevr::Plugin* g_plugin{nullptr};
}

class Plugin {
public:
    Plugin() { detail::g_plugin = this; }

    virtual ~Plugin() = default;

    // Main plugin callbacks
    virtual void on_dllmain() {}
    virtual void on_initialize() {}
    virtual void on_present() {}
    virtual void on_post_render_vr_framework_dx11(ID3D11DeviceContext* context, ID3D11Texture2D* texture, ID3D11RenderTargetView* rtv) {}
    virtual void on_post_render_vr_framework_dx12(ID3D12GraphicsCommandList* command_list, ID3D12Resource* rt, D3D12_CPU_DESCRIPTOR_HANDLE* rtv) {}
    virtual void on_device_reset() {}
    virtual bool on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) { return true; }
    virtual void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {}
    virtual void on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {}

    // Game/Engine callbacks
    virtual void on_pre_engine_tick(API::UGameEngine* engine, float delta) {}
    virtual void on_post_engine_tick(API::UGameEngine* engine, float delta) {}
    virtual void on_pre_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {}
    virtual void on_post_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {}
    virtual void on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, 
                                                     UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {};
    virtual void on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, 
                                                      UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {};

    virtual void on_pre_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle) {}
    virtual void on_post_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle) {}

protected:
};
}

extern "C" __declspec(dllexport) void uevr_plugin_required_version(UEVR_PluginVersion* version) {
    version->major = UEVR_PLUGIN_VERSION_MAJOR;
    version->minor = UEVR_PLUGIN_VERSION_MINOR;
    version->patch = UEVR_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool uevr_plugin_initialize(const UEVR_PluginInitializeParam* param) {
    auto& api = uevr::API::initialize(param);
    uevr::detail::g_plugin->on_initialize();

    auto callbacks = param->callbacks;
    auto sdk_callbacks = param->sdk->callbacks;

    callbacks->on_device_reset([]() {
        uevr::detail::g_plugin->on_device_reset();
    });

    callbacks->on_present([]() {
        uevr::detail::g_plugin->on_present();
    });

    callbacks->on_post_render_vr_framework_dx11([](void* context, void* texture, void* rtv) {
        uevr::detail::g_plugin->on_post_render_vr_framework_dx11((ID3D11DeviceContext*)context, (ID3D11Texture2D*)texture, (ID3D11RenderTargetView*)rtv);
    });

    callbacks->on_post_render_vr_framework_dx12([](void* command_list, void* rt, void* rtv) {
        uevr::detail::g_plugin->on_post_render_vr_framework_dx12((ID3D12GraphicsCommandList*)command_list, (ID3D12Resource*)rt, (D3D12_CPU_DESCRIPTOR_HANDLE*)rtv);
    });

    callbacks->on_message([](void* hwnd, unsigned int msg, unsigned long long wparam, long long lparam) {
        return uevr::detail::g_plugin->on_message((HWND)hwnd, msg, wparam, lparam);
    });

    callbacks->on_xinput_get_state([](unsigned int* retval, unsigned int user_index, void* state) {
        uevr::detail::g_plugin->on_xinput_get_state(retval, user_index, (XINPUT_STATE*)state);
    });

    callbacks->on_xinput_set_state([](unsigned int* retval, unsigned int user_index, void* vibration) {
        uevr::detail::g_plugin->on_xinput_set_state(retval, user_index, (XINPUT_VIBRATION*)vibration);
    });

    sdk_callbacks->on_pre_engine_tick([](UEVR_UGameEngineHandle engine, float delta) {
        uevr::detail::g_plugin->on_pre_engine_tick((uevr::API::UGameEngine*)engine, delta);
    });

    sdk_callbacks->on_post_engine_tick([](UEVR_UGameEngineHandle engine, float delta) {
        uevr::detail::g_plugin->on_post_engine_tick((uevr::API::UGameEngine*)engine, delta);
    });

    sdk_callbacks->on_pre_slate_draw_window_render_thread([](UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
        uevr::detail::g_plugin->on_pre_slate_draw_window(renderer, viewport_info);
    });

    sdk_callbacks->on_post_slate_draw_window_render_thread([](UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
        uevr::detail::g_plugin->on_post_slate_draw_window(renderer, viewport_info);
    });

    sdk_callbacks->on_pre_calculate_stereo_view_offset([](UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, 
                                                          UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
        uevr::detail::g_plugin->on_pre_calculate_stereo_view_offset(device, view_index, world_to_meters, position, rotation, is_double);
    });

    sdk_callbacks->on_post_calculate_stereo_view_offset([](UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, 
                                                           UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
        uevr::detail::g_plugin->on_post_calculate_stereo_view_offset(device, view_index, world_to_meters, position, rotation, is_double);
    });

    sdk_callbacks->on_pre_viewport_client_draw([](UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
        uevr::detail::g_plugin->on_pre_viewport_client_draw(viewport_client, viewport, canvas);
    });

    sdk_callbacks->on_post_viewport_client_draw([](UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
        uevr::detail::g_plugin->on_post_viewport_client_draw(viewport_client, viewport, canvas);
    });

    return true;
}

BOOL APIENTRY DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        uevr::detail::g_plugin->on_dllmain();
    }

    return TRUE;
}
