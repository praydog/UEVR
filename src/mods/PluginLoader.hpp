#pragma once

#include <map>
#include <mutex>
#include <shared_mutex>

#include <Windows.h>

#include <safetyhook.hpp>
#include <asmjit/asmjit.h>

#include "Mod.hpp"
#include "uevr/API.h"

namespace uevr {
extern UEVR_RendererData g_renderer_data;
}

extern "C" __declspec(dllexport) UEVR_PluginInitializeParam g_plugin_initialize_param;

class PluginLoader : public Mod {
public:
    static std::shared_ptr<PluginLoader>& get();

    // This is called prior to most UEVR initialization so that all plugins are at least **loaded** early on. UEVR_ramework plugin
    // specifics (`uevr_plugin_initialize`) are still delayed until UEVR_ramework is fully setup.
    void early_init();

    std::string_view get_name() const override { return "PluginLoader"; }
    bool is_advanced_mod() const override { return true; }
    std::optional<std::string> on_initialize_d3d_thread() override;
    void on_draw_ui() override;

    void on_present();
    void on_device_reset() override;
    bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) override;
    void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) override;
    void on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) override;

    void on_post_render_vr_framework_dx11(ID3D11DeviceContext* context, ID3D11Texture2D*, ID3D11RenderTargetView* rtv) override;
    void on_post_render_vr_framework_dx12(ID3D12GraphicsCommandList* command_list, ID3D12Resource* rt, D3D12_CPU_DESCRIPTOR_HANDLE* rtv) override;
    
    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_post_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_pre_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) override;
    void on_post_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) override;
    void on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                             const float world_to_meters, Vector3f* view_location, bool is_double) override;
    void on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                              const float world_to_meters, Vector3f* view_location, bool is_double) override;
    void on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) override;
    void on_post_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) override;
    
public:
    void attempt_unload_plugins();
    void reload_plugins();

    /*using UEVR_OnPresentCb = std::function<std::remove_pointer<::UEVR_OnPresentCb>::type>;
    using UEVR_OnDeviceResetCb = std::function<std::remove_pointer<::UEVR_OnDeviceResetCb>::type>;
    using UEVR_OnMessageCb = std::function<std::remove_pointer<::UEVR_OnMessageCb>::type>;
    using UEVR_OnXInputGetStateCb = std::function<std::remove_pointer<::UEVR_OnXInputGetStateCb>::type>;
    using UEVR_OnXInputSetStateCb = std::function<std::remove_pointer<::UEVR_OnXInputSetStateCb>::type>;*/

    /* VR Renderer */
    //using UEVR_OnPostRenderVRFrameworkDX11Cb = std::function<std::remove_pointer<::UEVR_OnPostRenderVRFrameworkDX11Cb>::type>;
    //using UEVR_OnPostRenderVRFrameworkDX12Cb = std::function<std::remove_pointer<::UEVR_OnPostRenderVRFrameworkDX12Cb>::type>;

    /* Engine specific callbacks */
    //using UEVR_Engine_TickCb = std::function<std::remove_pointer<::UEVR_Engine_TickCb>::type>;
    //using UEVR_Slate_DrawWindow_RenderThreadCb = std::function<std::remove_pointer<::UEVR_Slate_DrawWindow_RenderThreadCb>::type>;
    //using UEVR_Stereo_CalculateStereoViewOffsetCb = std::function<std::remove_pointer<::UEVR_Stereo_CalculateStereoViewOffsetCb>::type>;
    //using UEVR_ViewportClient_DrawCb = std::function<std::remove_pointer<::UEVR_ViewportClient_DrawCb>::type>;

    bool add_on_present(UEVR_OnPresentCb cb);
    bool add_on_device_reset(UEVR_OnDeviceResetCb cb);
    bool add_on_message(UEVR_OnMessageCb cb);
    bool add_on_xinput_get_state(UEVR_OnXInputGetStateCb cb);
    bool add_on_xinput_set_state(UEVR_OnXInputSetStateCb cb);
    bool add_on_post_render_vr_framework_dx11(UEVR_OnPostRenderVRFrameworkDX11Cb cb);
    bool add_on_post_render_vr_framework_dx12(UEVR_OnPostRenderVRFrameworkDX12Cb cb);

    bool add_on_pre_engine_tick(UEVR_Engine_TickCb cb);
    bool add_on_post_engine_tick(UEVR_Engine_TickCb cb);
    bool add_on_pre_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb);
    bool add_on_post_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb);
    bool add_on_pre_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb);
    bool add_on_post_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb);
    bool add_on_pre_viewport_client_draw(UEVR_ViewportClient_DrawCb cb);
    bool add_on_post_viewport_client_draw(UEVR_ViewportClient_DrawCb cb);

    bool remove_callback(void* cb) {
        {
            std::unique_lock lock{m_api_cb_mtx};

            for (auto& pcb_list : m_plugin_callback_lists) {
                auto& cb_list = *pcb_list;
                std::erase_if(cb_list, [cb](auto& cb_func) {
                    return cb_func == cb;
                });
            }
        }

        {
            std::unique_lock lock{ m_ufunction_hooks_mtx };

            for (auto& [_, hook] : m_ufunction_hooks) {
                std::scoped_lock _{hook->mux};
                std::erase_if(hook->pre_callbacks, [cb](auto& cb_func) {
                    return (void*)cb_func == cb;
                });
                std::erase_if(hook->post_callbacks, [cb](auto& cb_func) {
                    return (void*)cb_func == cb;
                });
            }
        }

        return true;
    }

    size_t add_inline_hook(safetyhook::InlineHook&& hook) {
        std::scoped_lock lock{m_mux};

        auto state = std::make_unique<InlineHookState>(std::move(hook));
        m_inline_hooks[++m_inline_hook_idx] = std::move(state);

        return m_inline_hook_idx;
    }

    void remove_inline_hook(size_t idx) {
        std::scoped_lock lock{m_mux};

        if (!m_inline_hooks.contains(idx)) {
            return;
        }

        {
            std::scoped_lock _{m_inline_hooks[idx]->mux};
            m_inline_hooks[idx]->hook.reset();
        }

        m_inline_hooks.erase(idx);
    }

    bool hook_ufunction_ptr(UEVR_UFunctionHandle func, UEVR_UFunction_NativePreFn pre, UEVR_UFunction_NativePostFn post);

private:
    std::shared_mutex m_api_cb_mtx;
    std::vector<UEVR_OnPresentCb> m_on_present_cbs{};
    std::vector<UEVR_OnDeviceResetCb> m_on_device_reset_cbs{};
    std::vector<UEVR_OnPostRenderVRFrameworkDX11Cb> m_on_post_render_vr_framework_dx11_cbs{};
    std::vector<UEVR_OnPostRenderVRFrameworkDX12Cb> m_on_post_render_vr_framework_dx12_cbs{};
    std::vector<UEVR_OnMessageCb> m_on_message_cbs{};
    std::vector<UEVR_OnXInputGetStateCb> m_on_xinput_get_state_cbs{};
    std::vector<UEVR_OnXInputSetStateCb> m_on_xinput_set_state_cbs{};

    std::vector<UEVR_Engine_TickCb> m_on_pre_engine_tick_cbs{};
    std::vector<UEVR_Engine_TickCb> m_on_post_engine_tick_cbs{};
    std::vector<UEVR_Slate_DrawWindow_RenderThreadCb> m_on_pre_slate_draw_window_render_thread_cbs{};
    std::vector<UEVR_Slate_DrawWindow_RenderThreadCb> m_on_post_slate_draw_window_render_thread_cbs{};
    std::vector<UEVR_Stereo_CalculateStereoViewOffsetCb> m_on_pre_calculate_stereo_view_offset_cbs{};
    std::vector<UEVR_Stereo_CalculateStereoViewOffsetCb> m_on_post_calculate_stereo_view_offset_cbs{};
    std::vector<UEVR_ViewportClient_DrawCb> m_on_pre_viewport_client_draw_cbs{};
    std::vector<UEVR_ViewportClient_DrawCb> m_on_post_viewport_client_draw_cbs{};

    using generic_std_function = void*;

    std::vector<std::vector<generic_std_function>*> m_plugin_callback_lists{
        // Plugin
        (std::vector<generic_std_function>*)&m_on_present_cbs,
        (std::vector<generic_std_function>*)&m_on_device_reset_cbs,

        // VR Renderer
        (std::vector<generic_std_function>*)&m_on_post_render_vr_framework_dx11_cbs,
        (std::vector<generic_std_function>*)&m_on_post_render_vr_framework_dx12_cbs,

        // Windows CBs
        (std::vector<generic_std_function>*)&m_on_message_cbs,
        (std::vector<generic_std_function>*)&m_on_xinput_get_state_cbs,
        (std::vector<generic_std_function>*)&m_on_xinput_set_state_cbs,

        // SDK
        (std::vector<generic_std_function>*)&m_on_pre_engine_tick_cbs,
        (std::vector<generic_std_function>*)&m_on_post_engine_tick_cbs,
        (std::vector<generic_std_function>*)&m_on_pre_slate_draw_window_render_thread_cbs,
        (std::vector<generic_std_function>*)&m_on_post_slate_draw_window_render_thread_cbs,
        (std::vector<generic_std_function>*)&m_on_pre_calculate_stereo_view_offset_cbs,
        (std::vector<generic_std_function>*)&m_on_post_calculate_stereo_view_offset_cbs,
        (std::vector<generic_std_function>*)&m_on_pre_viewport_client_draw_cbs,
        (std::vector<generic_std_function>*)&m_on_post_viewport_client_draw_cbs
    };

private:
    std::recursive_mutex m_mux{};
    std::map<std::string, HMODULE> m_plugins{};
    std::map<std::string, std::string> m_plugin_load_errors{};
    std::map<std::string, std::string> m_plugin_load_warnings{};

    struct InlineHookState {
        InlineHookState(safetyhook::InlineHook&& hook)
            : hook{std::move(hook)}
        {
        }

        safetyhook::InlineHook hook{};
        std::mutex mux{};
    };

    //std::vector<InlineHookState> m_inline_hooks{};
    std::unordered_map<size_t, std::unique_ptr<InlineHookState>> m_inline_hooks{};
    size_t m_inline_hook_idx{0};

    asmjit::JitRuntime m_jit_runtime{};

    struct UFunctionHookState {
        using Intermediary = void(*)(UEVR_UObjectHandle, void*, void*, sdk::UFunction*);
        Intermediary jitted_pre{};

        std::unique_ptr<PointerHook> hook{};
        std::vector<UEVR_UFunction_NativePreFn> pre_callbacks{};
        std::vector<UEVR_UFunction_NativePostFn> post_callbacks{};
        std::recursive_mutex mux{};

        void remove_callbacks() {
            std::scoped_lock _{mux};

            pre_callbacks.clear();
            post_callbacks.clear();
        }
    };
    std::shared_mutex m_ufunction_hooks_mtx{};
    std::unordered_map<sdk::UFunction*, std::unique_ptr<UFunctionHookState>> m_ufunction_hooks{};
    static void ufunction_hook_intermediary(UEVR_UObjectHandle obj, void* frame, void* result, sdk::UFunction* func);
};
