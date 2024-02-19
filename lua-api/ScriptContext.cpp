// This can be considered a binding of the C API.
#include <iostream>
#include <memory>

#include <windows.h>

#include "ScriptContext.hpp"

std::shared_ptr<ScriptContext> g_script_context{};

std::shared_ptr<ScriptContext> ScriptContext::get() {
    return g_script_context;
}

std::shared_ptr<ScriptContext> ScriptContext::reinitialize(lua_State* l, UEVR_PluginInitializeParam* param) {
    g_script_context.reset();
    g_script_context = std::make_shared<ScriptContext>(l, param);
    return g_script_context;
}

ScriptContext::ScriptContext(lua_State* l, UEVR_PluginInitializeParam* param) 
    : m_lua{l} 
{
    std::scoped_lock _{m_mtx};

    if (param != nullptr) {
        m_plugin_initialize_param = param;
        uevr::API::initialize(m_plugin_initialize_param);
        return;
    }
    
    const auto unreal_vr_backend = GetModuleHandleA("UEVRBackend.dll");

    if (unreal_vr_backend == nullptr) {
        return;
    }

    m_plugin_initialize_param = (UEVR_PluginInitializeParam*)GetProcAddress(unreal_vr_backend, "g_plugin_initialize_param");
    uevr::API::initialize(m_plugin_initialize_param);
}

ScriptContext::~ScriptContext() {
    std::scoped_lock _{m_mtx};

    if (m_plugin_initialize_param != nullptr) {
        for (auto& cb : m_callbacks_to_remove) {
            m_plugin_initialize_param->functions->remove_callback(cb);
        }

        m_callbacks_to_remove.clear();
    }
}

void ScriptContext::log(const std::string& message) {
    std::cout << "[LuaVR] " << message << std::endl;
}

void ScriptContext::test_function() {
    log("Test function called!");
}

void ScriptContext::setup_callback_bindings() {
    std::scoped_lock _{ m_mtx };

    auto cbs = m_plugin_initialize_param->sdk->callbacks;

    g_script_context->add_callback(cbs->on_pre_engine_tick, on_pre_engine_tick);
    g_script_context->add_callback(cbs->on_post_engine_tick, on_post_engine_tick);
    g_script_context->add_callback(cbs->on_pre_slate_draw_window_render_thread, on_pre_slate_draw_window_render_thread);
    g_script_context->add_callback(cbs->on_post_slate_draw_window_render_thread, on_post_slate_draw_window_render_thread);
    g_script_context->add_callback(cbs->on_pre_calculate_stereo_view_offset, on_pre_calculate_stereo_view_offset);
    g_script_context->add_callback(cbs->on_post_calculate_stereo_view_offset, on_post_calculate_stereo_view_offset);
    g_script_context->add_callback(cbs->on_pre_viewport_client_draw, on_pre_viewport_client_draw);
    g_script_context->add_callback(cbs->on_post_viewport_client_draw, on_post_viewport_client_draw);

    m_lua.new_usertype<UEVR_SDKCallbacks>("UEVR_SDKCallbacks",
        "on_pre_engine_tick", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_pre_engine_tick_callbacks.push_back(fn);
        },
        "on_post_engine_tick", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_post_engine_tick_callbacks.push_back(fn);
        },
        "on_pre_slate_draw_window_render_thread", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_pre_slate_draw_window_render_thread_callbacks.push_back(fn);
        },
        "on_post_slate_draw_window_render_thread", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_post_slate_draw_window_render_thread_callbacks.push_back(fn);
        },
        "on_pre_calculate_stereo_view_offset", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_pre_calculate_stereo_view_offset_callbacks.push_back(fn);
        },
        "on_post_calculate_stereo_view_offset", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_post_calculate_stereo_view_offset_callbacks.push_back(fn);
        },
        "on_pre_viewport_client_draw", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_pre_viewport_client_draw_callbacks.push_back(fn);
        },
        "on_post_viewport_client_draw", [](sol::function fn) {
            std::scoped_lock _{ g_script_context->m_mtx };
            g_script_context->m_on_post_viewport_client_draw_callbacks.push_back(fn);
        }
    );
}

int ScriptContext::setup_bindings() {
    m_lua.set_function("test_function", ScriptContext::test_function);

    m_lua.new_usertype<UEVR_PluginInitializeParam>("UEVR_PluginInitializeParam",
        "uevr_module", &UEVR_PluginInitializeParam::uevr_module,
        "version", &UEVR_PluginInitializeParam::version,
        "functions", &UEVR_PluginInitializeParam::functions,
        "callbacks", &UEVR_PluginInitializeParam::callbacks,
        "renderer", &UEVR_PluginInitializeParam::renderer,
        "vr", &UEVR_PluginInitializeParam::vr,
        "openvr", &UEVR_PluginInitializeParam::openvr,
        "openxr", &UEVR_PluginInitializeParam::openxr,
        "sdk", &UEVR_PluginInitializeParam::sdk
    );

    m_lua.new_usertype<UEVR_PluginVersion>("UEVR_PluginVersion",
        "major", &UEVR_PluginVersion::major,
        "minor", &UEVR_PluginVersion::minor,
        "patch", &UEVR_PluginVersion::patch
    );

    m_lua.new_usertype<UEVR_PluginFunctions>("UEVR_PluginFunctions",
        "log_error", &UEVR_PluginFunctions::log_error,
        "log_warn", &UEVR_PluginFunctions::log_warn,
        "log_info", &UEVR_PluginFunctions::log_info,
        "is_drawing_ui", &UEVR_PluginFunctions::is_drawing_ui
    );

    m_lua.new_usertype<UEVR_RendererData>("UEVR_RendererData",
        "renderer_type", &UEVR_RendererData::renderer_type,
        "device", &UEVR_RendererData::device,
        "swapchain", &UEVR_RendererData::swapchain,
        "command_queue", &UEVR_RendererData::command_queue
    );

    m_lua.new_usertype<UEVR_SDKFunctions>("UEVR_SDKFunctions",
        "get_uengine", &UEVR_SDKFunctions::get_uengine,
        "set_cvar_int", &UEVR_SDKFunctions::set_cvar_int,
        "get_uobject_array", &UEVR_SDKFunctions::get_uobject_array,
        "get_player_controller", &UEVR_SDKFunctions::get_player_controller,
        "get_local_pawn", &UEVR_SDKFunctions::get_local_pawn,
        "spawn_object", &UEVR_SDKFunctions::spawn_object,

        "execute_command", &UEVR_SDKFunctions::execute_command,
        "execute_command_ex", &UEVR_SDKFunctions::execute_command_ex,

        "get_console_manager", &UEVR_SDKFunctions::get_console_manager
    );

    m_lua.new_usertype<UEVR_SDKData>("UEVR_SDKData",
        "functions", &UEVR_SDKData::functions,
        "callbacks", &UEVR_SDKData::callbacks
    );

    m_lua.new_usertype<UEVR_VRData>("UEVR_VRData",
        "is_runtime_ready", &UEVR_VRData::is_runtime_ready,
        "is_openvr", &UEVR_VRData::is_openvr,
        "is_openxr", &UEVR_VRData::is_openxr,
        "is_hmd_active", &UEVR_VRData::is_hmd_active,
        "get_standing_origin", &UEVR_VRData::get_standing_origin,
        "get_rotation_offset", &UEVR_VRData::get_rotation_offset,
        "set_standing_origin", &UEVR_VRData::set_standing_origin,
        "set_rotation_offset", &UEVR_VRData::set_rotation_offset,
        "get_hmd_index", &UEVR_VRData::get_hmd_index,
        "get_left_controller_index", &UEVR_VRData::get_left_controller_index,
        "get_right_controller_index", &UEVR_VRData::get_right_controller_index,
        "get_pose", &UEVR_VRData::get_pose,
        "get_transform", &UEVR_VRData::get_transform,
        "get_eye_offset", &UEVR_VRData::get_eye_offset,
        "get_ue_projection_matrix", &UEVR_VRData::get_ue_projection_matrix,
        "get_left_joystick_source", &UEVR_VRData::get_left_joystick_source,
        "get_right_joystick_source", &UEVR_VRData::get_right_joystick_source,
        "get_action_handle", &UEVR_VRData::get_action_handle,
        "is_action_active", &UEVR_VRData::is_action_active,
        "get_joystick_axis", &UEVR_VRData::get_joystick_axis,
        "trigger_haptic_vibration", &UEVR_VRData::trigger_haptic_vibration,
        "is_using_controllers", &UEVR_VRData::is_using_controllers,
        "get_lowest_xinput_index", &UEVR_VRData::get_lowest_xinput_index,
        "recenter_view", &UEVR_VRData::recenter_view,
        "recenter_horizon", &UEVR_VRData::recenter_horizon,
        "get_aim_method", &UEVR_VRData::get_aim_method,
        "set_aim_method", &UEVR_VRData::set_aim_method,
        "is_aim_allowed", &UEVR_VRData::is_aim_allowed,
        "set_aim_allowed", &UEVR_VRData::set_aim_allowed
    );

    m_lua.new_usertype<UEVR_Vector2f>("UEVR_Vector2f",
        "x", &UEVR_Vector2f::x,
        "y", &UEVR_Vector2f::y
    );

    m_lua.new_usertype<UEVR_Vector3f>("UEVR_Vector3f",
        "x", &UEVR_Vector3f::x,
        "y", &UEVR_Vector3f::y,
        "z", &UEVR_Vector3f::z
    );

    m_lua.new_usertype<UEVR_Vector3d>("UEVR_Vector3d",
        "x", &UEVR_Vector3d::x,
        "y", &UEVR_Vector3d::y,
        "z", &UEVR_Vector3d::z
    );

    m_lua.new_usertype<UEVR_Vector4f>("UEVR_Vector4f",
        "x", &UEVR_Vector4f::x,
        "y", &UEVR_Vector4f::y,
        "z", &UEVR_Vector4f::z,
        "w", &UEVR_Vector4f::w
    );

    m_lua.new_usertype<UEVR_Quaternionf>("UEVR_Quaternionf",
        "x", &UEVR_Quaternionf::x,
        "y", &UEVR_Quaternionf::y,
        "z", &UEVR_Quaternionf::z,
        "w", &UEVR_Quaternionf::w
    );

    m_lua.new_usertype<UEVR_Rotatorf>("UEVR_Rotatorf",
        "pitch", &UEVR_Rotatorf::pitch,
        "yaw", &UEVR_Rotatorf::yaw,
        "roll", &UEVR_Rotatorf::roll
    );

    m_lua.new_usertype<UEVR_Rotatord>("UEVR_Rotatord",
        "pitch", &UEVR_Rotatord::pitch,
        "yaw", &UEVR_Rotatord::yaw,
        "roll", &UEVR_Rotatord::roll
    );

    m_lua.new_usertype<UEVR_Matrix4x4f>("UEVR_Matrix4x4f",
        sol::meta_function::index, [](sol::this_state s, UEVR_Matrix4x4f& lhs, sol::object index_obj) -> sol::object {
            if (!index_obj.is<int>()) {
                return sol::make_object(s, sol::lua_nil);
            }

            const auto index = index_obj.as<int>();

            if (index >= 4) {
                return sol::make_object(s, sol::lua_nil);
            }

            return sol::make_object(s, &lhs.m[index]);
        }
    );

    m_lua.new_usertype<UEVR_Matrix4x4d>("UEVR_Matrix4x4d",
        sol::meta_function::index, [](sol::this_state s, UEVR_Matrix4x4d& lhs, sol::object index_obj) -> sol::object {
            if (!index_obj.is<int>()) {
                return sol::make_object(s, sol::lua_nil);
            }

            const auto index = index_obj.as<int>();

            if (index >= 4) {
                return sol::make_object(s, sol::lua_nil);
            }

            return sol::make_object(s, &lhs.m[index]);
        }
    );

    setup_callback_bindings();

    auto out = m_lua.create_table();
    out["params"] = m_plugin_initialize_param;

    return out.push(m_lua.lua_state());
}

void ScriptContext::on_pre_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds) {
    std::scoped_lock _{ g_script_context->m_mtx };
    for (auto& fn : g_script_context->m_on_pre_engine_tick_callbacks) try {
        g_script_context->handle_protected_result(fn(engine, delta_seconds));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_pre_engine_tick: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_pre_engine_tick");
    }
}

void ScriptContext::on_post_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_post_engine_tick_callbacks) try {
        g_script_context->handle_protected_result(fn(engine, delta_seconds));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_post_engine_tick: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_post_engine_tick");
    }
}

void ScriptContext::on_pre_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_pre_slate_draw_window_render_thread_callbacks) try {
        g_script_context->handle_protected_result(fn(renderer, viewport_info));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_pre_slate_draw_window_render_thread: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_pre_slate_draw_window_render_thread");
    }
}

void ScriptContext::on_post_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_post_slate_draw_window_render_thread_callbacks) try {
        g_script_context->handle_protected_result(fn(renderer, viewport_info));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_post_slate_draw_window_render_thread: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_post_slate_draw_window_render_thread");
    }
}

void ScriptContext::on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_pre_calculate_stereo_view_offset_callbacks) try {
        g_script_context->handle_protected_result(fn(device, view_index, world_to_meters, position, rotation, is_double));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_pre_calculate_stereo_view_offset: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_pre_calculate_stereo_view_offset");
    }
}

void ScriptContext::on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_post_calculate_stereo_view_offset_callbacks) try {
        g_script_context->handle_protected_result(fn(device, view_index, world_to_meters, position, rotation, is_double));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_post_calculate_stereo_view_offset: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_post_calculate_stereo_view_offset");
    }
}

void ScriptContext::on_pre_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_pre_viewport_client_draw_callbacks) try {
        g_script_context->handle_protected_result(fn(viewport_client, viewport, canvas));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_pre_viewport_client_draw: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_pre_viewport_client_draw");
    }
}

void ScriptContext::on_post_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
    std::scoped_lock _{ g_script_context->m_mtx };

    for (auto& fn : g_script_context->m_on_post_viewport_client_draw_callbacks) try {
        g_script_context->handle_protected_result(fn(viewport_client, viewport, canvas));
    } catch (const std::exception& e) {
        ScriptContext::log("Exception in on_post_viewport_client_draw: " + std::string(e.what()));
    } catch (...) {
        ScriptContext::log("Unknown exception in on_post_viewport_client_draw");
    }
}