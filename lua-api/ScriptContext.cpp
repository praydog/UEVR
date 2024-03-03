// This can be considered a binding of the C API.
#include <iostream>
#include <memory>

#include <utility/String.hpp>

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

    m_lua.new_usertype<UEVR_UObjectHookFunctions>("UEVR_UObjectHookFunctions",
        "activate", &UEVR_UObjectHookFunctions::activate,
        "exists", &UEVR_UObjectHookFunctions::exists,
        "get_first_object_by_class", &UEVR_UObjectHookFunctions::get_first_object_by_class,
        "get_first_object_by_class_name", &UEVR_UObjectHookFunctions::get_first_object_by_class_name
        // The other functions are really C-oriented so... we will just wrap the C++ API for the rest
    );

    m_lua.new_usertype<UEVR_SDKData>("UEVR_SDKData",
        "functions", &UEVR_SDKData::functions,
        "callbacks", &UEVR_SDKData::callbacks,
        "uobject", &UEVR_SDKData::uobject,
        "uobject_array", &UEVR_SDKData::uobject_array,
        "ffield", &UEVR_SDKData::ffield,
        "fproperty", &UEVR_SDKData::fproperty,
        "ustruct", &UEVR_SDKData::ustruct,
        "uclass", &UEVR_SDKData::uclass,
        "ufunction", &UEVR_SDKData::ufunction,
        "uobject_hook", &UEVR_SDKData::uobject_hook,
        "ffield_class", &UEVR_SDKData::ffield_class,
        "fname", &UEVR_SDKData::fname,
        "console", &UEVR_SDKData::console
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

    // TODO: Add operators to these types
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

    m_lua.new_usertype<uevr::API::FName>("UEVR_FName",
        "to_string", &uevr::API::FName::to_string
    );

    m_lua.new_usertype<uevr::API::UObject>("UEVR_UObject",
        "static_class", &uevr::API::UObject::static_class,
        "get_fname", &uevr::API::UObject::get_fname,
        "get_full_name", &uevr::API::UObject::get_full_name,
        "is_a", &uevr::API::UObject::is_a,
        "get_class", &uevr::API::UObject::get_class,
        "get_outer", &uevr::API::UObject::get_outer
    );

    m_lua.new_usertype<uevr::API::UStruct>("UEVR_UStruct",
        sol::base_classes, sol::bases<uevr::API::UObject>(),
        "static_class", &uevr::API::UStruct::static_class,
        "get_super_struct", &uevr::API::UStruct::get_super_struct,
        "get_super", &uevr::API::UStruct::get_super,
        "find_function", &uevr::API::UStruct::find_function,
        "get_child_properties", &uevr::API::UStruct::get_child_properties
    );

    m_lua.new_usertype<uevr::API::UClass>("UEVR_UClass",
        sol::base_classes, sol::bases<uevr::API::UStruct, uevr::API::UObject>(),
        "static_class", &uevr::API::UClass::static_class,
        "get_class_default_object", &uevr::API::UClass::get_class_default_object,
        "get_objects_matching", &uevr::API::UClass::get_objects_matching<uevr::API::UObject>,
        "get_first_object_matching", &uevr::API::UClass::get_first_object_matching<uevr::API::UObject>
    );

    m_lua.new_usertype<uevr::API::UFunction>("UEVR_UFunction",
        sol::base_classes, sol::bases<uevr::API::UStruct, uevr::API::UObject>(),
        "static_class", &uevr::API::UFunction::static_class,
        "call", &uevr::API::UFunction::call,
        "get_native_function", &uevr::API::UFunction::get_native_function
    );

    m_lua.new_usertype<uevr::API::FField>("UEVR_FField",
        "get_next", &uevr::API::FField::get_next,
        "get_fname", &uevr::API::FField::get_fname,
        "get_class", &uevr::API::FField::get_class
    );

    m_lua.new_usertype<uevr::API::FFieldClass>("UEVR_FFieldClass",
        "get_fname", &uevr::API::FFieldClass::get_fname,
        "get_name", &uevr::API::FFieldClass::get_name
    );

    m_lua.new_usertype<uevr::API::FConsoleManager>("UEVR_FConsoleManager",
        "get_console_objects", &uevr::API::FConsoleManager::get_console_objects,
        "find_object", &uevr::API::FConsoleManager::find_object,
        "find_variable", &uevr::API::FConsoleManager::find_variable,
        "find_command", &uevr::API::FConsoleManager::find_command
    );

    m_lua.new_usertype<uevr::API>("UEVR_API",
        "sdk", &uevr::API::sdk,
        "find_uobject", &uevr::API::find_uobject<uevr::API::UObject>,
        "get_engine", &uevr::API::get_engine,
        "get_player_controller", &uevr::API::get_player_controller,
        "get_local_pawn", &uevr::API::get_local_pawn,
        "spawn_object", &uevr::API::spawn_object,
        "execute_command", &uevr::API::execute_command,
        "execute_command_ex", &uevr::API::execute_command_ex,
        "get_uobject_array", &uevr::API::get_uobject_array,
        "get_console_manager", &uevr::API::get_console_manager
    );

    m_lua.new_usertype<uevr::API::IConsoleObject>("UEVR_IConsoleObject",
        "as_command", &uevr::API::IConsoleObject::as_command
    );

    m_lua.new_usertype<uevr::API::IConsoleVariable>("UEVR_IConsoleVariable",
        sol::base_classes, sol::bases<uevr::API::IConsoleObject>(),
        "set", [](sol::this_state s, uevr::API::IConsoleVariable* self, sol::object value) {
            if (value.is<int>()) {
                self->set(value.as<int>());
            } else if (value.is<float>()) {
                self->set(value.as<float>());
            } else if (value.is<std::wstring>()) {
                self->set(value.as<std::wstring>());
            } else if (value.is<std::string>()) {
                const auto str = utility::widen(value.as<std::string>());
                self->set(str);
            } else {
                throw sol::error("Invalid type for IConsoleVariable::set");
            }
        },
        "set_float", [](uevr::API::IConsoleVariable& self, float value) {
            self.set(value);
        },
        "set_int", [](uevr::API::IConsoleVariable& self, int value) {
            self.set(value);
        },
        "set_ex", &uevr::API::IConsoleVariable::set_ex,
        "get_int", &uevr::API::IConsoleVariable::get_int,
        "get_float", &uevr::API::IConsoleVariable::get_float
    );

    m_lua.new_usertype<uevr::API::IConsoleCommand>("UEVR_IConsoleCommand",
        sol::base_classes, sol::bases<uevr::API::IConsoleObject>(),
        "execute", &uevr::API::IConsoleCommand::execute
    );

    setup_callback_bindings();

    auto out = m_lua.create_table();
    out["params"] = m_plugin_initialize_param;
    out["api"] = uevr::API::get().get();

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