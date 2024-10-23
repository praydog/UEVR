#pragma once

#include <iostream>
#include <memory>
#include <shared_mutex>

#include "ScriptPrerequisites.hpp"
#include <uevr/API.hpp>

#include <vector>

namespace uevr {
class ScriptContext : public std::enable_shared_from_this<ScriptContext> {
public:
    static std::shared_ptr<ScriptContext> create(lua_State* l, UEVR_PluginInitializeParam* param = nullptr) {
        auto ctx = std::shared_ptr<ScriptContext>(new ScriptContext(l, param));
        ctx->initialize();

        return ctx;
    }

    static std::shared_ptr<ScriptContext> create(std::shared_ptr<sol::state> l, UEVR_PluginInitializeParam* param = nullptr) {
        auto ctx = std::shared_ptr<ScriptContext>(new ScriptContext(l->lua_state(), param));
        ctx->initialize(l);

        return ctx;
    }

    ScriptContext() = delete;
    virtual ~ScriptContext();

    int setup_bindings();
    void setup_callback_bindings();

    bool valid() {
        return m_plugin_initialize_param != nullptr;
    }

    auto& lua() {
        return m_lua;
    }

    UEVR_PluginInitializeParam* plugin_initialize_param() {
        return m_plugin_initialize_param;
    }

    sol::protected_function_result handle_protected_result(sol::protected_function_result result) {
        if (result.valid()) {
            return result;
        }

        sol::script_default_on_error(m_lua.lua_state(), std::move(result));
        return result;
    }

    static void log(const std::string& message);
    void log_error(const std::string& message) {
        log(message);

        std::unique_lock _{m_script_error_mutex};
        m_last_script_error_state.e = message;
        m_last_script_error_state.t = std::chrono::system_clock::now();
    }

    template<typename T1, typename T2>
    void add_callback(T1&& adder, T2&& cb) {
        std::scoped_lock _{m_mtx};

        if (m_plugin_initialize_param != nullptr) {
            adder(cb);
            s_callbacks_to_remove.push_back((void*)cb);
        }
    }

    auto& get_mutex() {
        return m_mtx;
    }

    void script_reset() {
        std::scoped_lock _{m_mtx};

        for (auto& cb : m_on_script_reset_callbacks) try {
            handle_protected_result(cb());
        } catch (const std::exception& e) {
            log_error("Exception in on_script_reset: " + std::string(e.what()));
        } catch (...) {
            log_error("Unknown exception in on_script_reset");
        }
    }

    void frame() {
        std::scoped_lock _{m_mtx};

        for (auto& cb : m_on_frame_callbacks) try {
            handle_protected_result(cb());
        } catch (const std::exception& e) {
            log_error("Exception in on_frame: " + std::string(e.what()));
        } catch (...) {
            log_error("Unknown exception in on_frame");
        }
    }

    void draw_ui() {
        std::scoped_lock _{m_mtx};

        for (auto& cb : m_on_draw_ui_callbacks) try {
            handle_protected_result(cb());
        } catch (const std::exception& e) {
            log_error("Exception in on_draw_ui: " + std::string(e.what()));
        } catch (...) {
            log_error("Unknown exception in on_draw_ui");
        }
    }

    void dispatch_event(std::string_view event_name, std::string_view event_data) {
        std::scoped_lock _{m_mtx};

        for (auto& cb : m_on_lua_event_callbacks) try {
            handle_protected_result(cb(event_name, event_data));
        } catch (const std::exception& e) {
            log_error("Exception in on_lua_event: " + std::string(e.what()));
        } catch (...) {
            log_error("Unknown exception in on_lua_event");
        }
    }

    struct ScriptErrorState {
        std::string e{};
        std::chrono::system_clock::time_point t{};
    };

    auto get_last_script_error() const {
        std::shared_lock _{m_script_error_mutex};
        return m_last_script_error_state;
    }

private:
    // Private constructor to prevent direct instantiation
    ScriptContext(lua_State* l, UEVR_PluginInitializeParam* param = nullptr);
    void initialize(std::shared_ptr<sol::state> l = nullptr);

    static inline std::vector<void*> s_callbacks_to_remove{};
    static inline std::mutex s_callbacks_to_remove_mtx{};

    sol::state_view m_lua;
    std::shared_ptr<sol::state> m_lua_shared{}; // This allows us to keep the state alive (if it was created by ScriptState)
    ScriptErrorState m_last_script_error_state{};
    mutable std::shared_mutex m_script_error_mutex{};

    std::recursive_mutex m_mtx{};
    UEVR_PluginInitializeParam* m_plugin_initialize_param{nullptr};
    std::vector<sol::protected_function> m_on_xinput_get_state_callbacks{};
    std::vector<sol::protected_function> m_on_xinput_set_state_callbacks{};
    std::vector<sol::protected_function> m_on_pre_engine_tick_callbacks{};
    std::vector<sol::protected_function> m_on_post_engine_tick_callbacks{};
    std::vector<sol::protected_function> m_on_pre_slate_draw_window_render_thread_callbacks{};
    std::vector<sol::protected_function> m_on_post_slate_draw_window_render_thread_callbacks{};
    std::vector<sol::protected_function> m_on_early_calculate_stereo_view_offset_callbacks{};
    std::vector<sol::protected_function> m_on_pre_calculate_stereo_view_offset_callbacks{};
    std::vector<sol::protected_function> m_on_post_calculate_stereo_view_offset_callbacks{};
    std::vector<sol::protected_function> m_on_pre_viewport_client_draw_callbacks{};
    std::vector<sol::protected_function> m_on_post_viewport_client_draw_callbacks{};
    std::vector<sol::protected_function> m_on_lua_event_callbacks{};

    // Custom UEVR callbacks
    std::vector<sol::protected_function> m_on_frame_callbacks{};
    std::vector<sol::protected_function> m_on_draw_ui_callbacks{};
    std::vector<sol::protected_function> m_on_script_reset_callbacks{};

    struct UFunctionHookState {
        std::vector<sol::protected_function> pre_hooks{};
        std::vector<sol::protected_function> post_hooks{};
    };

    std::shared_mutex m_ufunction_hooks_mtx{};
    std::unordered_map<uevr::API::UFunction*, std::unique_ptr<UFunctionHookState>> m_ufunction_hooks{};
    static bool global_ufunction_pre_handler(uevr::API::UFunction* fn, uevr::API::UObject* obj, void* params, void* result);
    static void global_ufunction_post_handler(uevr::API::UFunction* fn, uevr::API::UObject* obj, void* params, void* result);

    static void on_xinput_get_state(uint32_t* retval, uint32_t user_index, void* state);
    static void on_xinput_set_state(uint32_t* retval, uint32_t user_index, void* vibration);
    static void on_pre_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds);
    static void on_post_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds);
    static void on_pre_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info);
    static void on_post_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info);
    static void on_early_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double);
    static void on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double);
    static void on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double);
    static void on_pre_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas);
    static void on_post_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas);
    static void on_frame();
    static void on_draw_ui();
    static void on_script_reset();
    static void on_lua_event(std::string_view event_name, std::string_view event_data);
};
}