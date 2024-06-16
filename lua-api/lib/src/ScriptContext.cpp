// This can be considered a binding of the C API.
#include <iostream>
#include <memory>

#include <utility/String.hpp>

#include <windows.h>

#include "ScriptContext.hpp"

namespace uevr {
class ScriptContexts {
public:
    void add(ScriptContext* ctx) {
        std::scoped_lock _{mtx};

        // Check if the context is already in the list
        for (ScriptContext* c : list) {
            if (c == ctx) {
                return;
            }
        }

        list.push_back(ctx);
    }

    void remove(ScriptContext* ctx) {
        std::scoped_lock _{mtx};

        ScriptContext::log("Removing context from list");
        std::erase_if(list, [ctx](ScriptContext* c) {
            return c == ctx;
        });
        ScriptContext::log(std::format("New context count: {}", list.size()));
    }

    template<typename T>
    void for_each(T&& fn) {
        std::scoped_lock _{mtx};
        for (auto& ctx : list) {
            fn(ctx->shared_from_this());
        }
    }

private:
    std::vector<ScriptContext*> list{};
    std::mutex mtx{};
} g_contexts{};

ScriptContext::ScriptContext(lua_State* l, UEVR_PluginInitializeParam* param) 
    : m_lua{l}
{
    std::scoped_lock _{m_mtx};

    g_contexts.add(this);

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
    ScriptContext::log("ScriptContext destructor called");

    // TODO: this probably does not support multiple states
    if (m_plugin_initialize_param != nullptr) {
        for (auto& cb : m_callbacks_to_remove) {
            m_plugin_initialize_param->functions->remove_callback(cb);
        }

        m_callbacks_to_remove.clear();
    }

    g_contexts.remove(this);
}

void ScriptContext::log(const std::string& message) {
    std::cout << "[LuaVR] " << message << std::endl;
    API::get()->log_info("[LuaVR] %s", message.c_str());
}

void ScriptContext::test_function() {
    log("Test function called!");
}

void ScriptContext::setup_callback_bindings() {
    std::scoped_lock _{ m_mtx };

    auto cbs = m_plugin_initialize_param->sdk->callbacks;

    add_callback(cbs->on_pre_engine_tick, on_pre_engine_tick);
    add_callback(cbs->on_post_engine_tick, on_post_engine_tick);
    add_callback(cbs->on_pre_slate_draw_window_render_thread, on_pre_slate_draw_window_render_thread);
    add_callback(cbs->on_post_slate_draw_window_render_thread, on_post_slate_draw_window_render_thread);
    add_callback(cbs->on_pre_calculate_stereo_view_offset, on_pre_calculate_stereo_view_offset);
    add_callback(cbs->on_post_calculate_stereo_view_offset, on_post_calculate_stereo_view_offset);
    add_callback(cbs->on_pre_viewport_client_draw, on_pre_viewport_client_draw);
    add_callback(cbs->on_post_viewport_client_draw, on_post_viewport_client_draw);

    m_lua.new_usertype<UEVR_SDKCallbacks>("UEVR_SDKCallbacks",
        "on_pre_engine_tick", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_pre_engine_tick_callbacks.push_back(fn);
        },
        "on_post_engine_tick", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_post_engine_tick_callbacks.push_back(fn);
        },
        "on_pre_slate_draw_window_render_thread", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_pre_slate_draw_window_render_thread_callbacks.push_back(fn);
        },
        "on_post_slate_draw_window_render_thread", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_post_slate_draw_window_render_thread_callbacks.push_back(fn);
        },
        "on_pre_calculate_stereo_view_offset", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_pre_calculate_stereo_view_offset_callbacks.push_back(fn);
        },
        "on_post_calculate_stereo_view_offset", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_post_calculate_stereo_view_offset_callbacks.push_back(fn);
        },
        "on_pre_viewport_client_draw", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_pre_viewport_client_draw_callbacks.push_back(fn);
        },
        "on_post_viewport_client_draw", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_post_viewport_client_draw_callbacks.push_back(fn);
        },
        "on_frame", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_frame_callbacks.push_back(fn);
        },
        "on_draw_ui", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_draw_ui_callbacks.push_back(fn);
        },
        "on_script_reset", [this](sol::function fn) {
            std::scoped_lock _{ m_mtx };
            m_on_script_reset_callbacks.push_back(fn);
        }
    );
}

sol::object prop_to_object(sol::this_state s, uevr::API::UObject& self, const std::wstring& name) {
    const auto c = self.get_class();

    if (c == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    const auto desc = c->find_property(name.c_str());

    if (desc == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    const auto propc = desc->get_class();

    if (propc == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    const auto name_hash = utility::hash(propc->get_fname()->to_string());

    switch (name_hash) {
    case L"BoolProperty"_fnv:
        return sol::make_object(s, self.get_bool_property(name));
    case L"FloatProperty"_fnv:
        return sol::make_object(s, self.get_property<float>(name));
    case L"DoubleProperty"_fnv:
        return sol::make_object(s, self.get_property<double>(name));
    case L"IntProperty"_fnv:
        return sol::make_object(s, self.get_property<int32_t>(name));
    case L"UIntProperty"_fnv:
    case L"UInt32Property"_fnv:
        return sol::make_object(s, self.get_property<uint32_t>(name));
    case L"NameProperty"_fnv:
        return sol::make_object(s, self.get_property<uevr::API::FName>(name));
    case L"ObjectProperty"_fnv:
        return sol::make_object(s, self.get_property<uevr::API::UObject*>(name));
    case L"ArrayProperty"_fnv:
    {
        const auto inner_prop = ((uevr::API::FArrayProperty*)desc)->get_inner();

        if (inner_prop == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto inner_c = inner_prop->get_class();

        if (inner_c == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto inner_name_hash = utility::hash(inner_c->get_fname()->to_string());

        switch (inner_name_hash) {
        case "ObjectProperty"_fnv:
        {
            const auto& arr = self.get_property<uevr::API::TArray<uevr::API::UObject*>>(name);

            if (arr.data == nullptr || arr.count == 0) {
                return sol::make_object(s, sol::lua_nil);
            }

            auto lua_arr = std::vector<uevr::API::UObject*>{};

            for (size_t i = 0; i < arr.count; ++i) {
                lua_arr.push_back(arr.data[i]);
            }

            return sol::make_object(s, lua_arr);
        }
        // TODO: Add support for other types
        };

        return sol::make_object(s, sol::lua_nil);
    }
    };

    return sol::make_object(s, sol::lua_nil);
}

void set_property(sol::this_state s, uevr::API::UObject& self, const std::wstring& name, sol::object value) {
    const auto c = self.get_class();

    if (c == nullptr) {
        return;
    }

    const auto desc = c->find_property(name.c_str());

    if (desc == nullptr) {
        return;
    }

    const auto propc = desc->get_class();

    if (propc == nullptr) {
        return;
    }

    const auto name_hash = utility::hash(propc->get_fname()->to_string());

    switch (name_hash) {
    case L"BoolProperty"_fnv:
        self.set_bool_property(name, value.as<bool>());
        return;
    case L"FloatProperty"_fnv:
        self.get_property<float>(name) = value.as<float>();
        return;
    case L"DoubleProperty"_fnv:
        self.get_property<double>(name) = value.as<double>();
        return;
    case L"IntProperty"_fnv:
        self.get_property<int32_t>(name) = value.as<int32_t>();
        return;
    case L"UIntProperty"_fnv:
    case L"UInt32Property"_fnv:
        self.get_property<uint32_t>(name) = value.as<uint32_t>();
        return;
    case L"NameProperty"_fnv:
        //return sol::make_object(s, self.get_property<uevr::API::FName>(name));
        throw sol::error("Setting FName properties is not supported (yet)");
    case L"ObjectProperty"_fnv:
        self.get_property<uevr::API::UObject*>(name) = value.as<uevr::API::UObject*>();
        return;
    case L"ArrayProperty"_fnv:
        throw sol::error("Setting array properties is not supported (yet)");
    };

    // NONE
}

int ScriptContext::setup_bindings() {
    m_lua.registry()["uevr_context"] = this;

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
        "get_address", [](uevr::API::UObject& self) {
            return (uintptr_t)&self;
        },
        "static_class", &uevr::API::UObject::static_class,
        "get_fname", &uevr::API::UObject::get_fname,
        "get_full_name", &uevr::API::UObject::get_full_name,
        "is_a", &uevr::API::UObject::is_a,
        "get_class", &uevr::API::UObject::get_class,
        "get_outer", &uevr::API::UObject::get_outer,
        "get_bool_property", &uevr::API::UObject::get_bool_property,
        "get_float_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<float>(name);
        },
        "get_double_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<double>(name);
        },
        "get_int_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<int32_t>(name);
        },
        "get_uint_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<uint32_t>(name);
        },
        "get_fname_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<uevr::API::FName>(name);
        },
        "get_uobject_property", [](uevr::API::UObject& self, const std::wstring& name) {
            return self.get_property<uevr::API::UObject*>(name);
        },
        "get_property", [](sol::this_state s, uevr::API::UObject& self, const std::wstring& name) -> sol::object {
            return prop_to_object(s, self, name);
        },
        "set_property", &set_property,
        sol::meta_function::index, [](sol::this_state s, uevr::API::UObject& self, sol::object index_obj) -> sol::object {
            if (!index_obj.is<std::string>()) {
                return sol::make_object(s, sol::lua_nil);
            }

            const auto name = utility::widen(index_obj.as<std::string>());
            return prop_to_object(s, self, name);
        },
        sol::meta_function::new_index, [](sol::this_state s, uevr::API::UObject& self, sol::object index_obj, sol::object value) {
            if (!index_obj.is<std::string>()) {
                return;
            }

            const auto name = utility::widen(index_obj.as<std::string>());
            set_property(s, self, name, value);
        }
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

    m_lua.new_usertype<uevr::API::FUObjectArray>("UEVR_FUObjectArray",
        "get", &uevr::API::FUObjectArray::get,
        "is_chunked", &uevr::API::FUObjectArray::is_chunked,
        "is_inlined", &uevr::API::FUObjectArray::is_inlined,
        "get_objects_offset", &uevr::API::FUObjectArray::get_objects_offset,
        "get_item_distance", &uevr::API::FUObjectArray::get_item_distance,
        "get_object_count", &uevr::API::FUObjectArray::get_object_count,
        "get_objects_ptr", &uevr::API::FUObjectArray::get_objects_ptr,
        "get_object", &uevr::API::FUObjectArray::get_object,
        "get_item", &uevr::API::FUObjectArray::get_item
    );

    m_lua.new_usertype<uevr::API::UObjectHook>("UEVR_UObjectHook",
        "activate", &uevr::API::UObjectHook::activate,
        "exists", &uevr::API::UObjectHook::exists,
        "is_disabled", &uevr::API::UObjectHook::is_disabled,
        "set_disabled", &uevr::API::UObjectHook::set_disabled,
        "get_first_object_by_class", [](uevr::API::UClass* c, sol::object allow_default_obj) {
            bool allow_default = false;
            if (allow_default_obj.is<bool>()) {
                allow_default = allow_default_obj.as<bool>();
            }
            return uevr::API::UObjectHook::get_first_object_by_class(c, allow_default);
        },
        "get_objects_by_class", [](uevr::API::UClass* c, sol::object allow_default_obj) {
            bool allow_default = false;
            if (allow_default_obj.is<bool>()) {
                allow_default = allow_default_obj.as<bool>();
            }
            return uevr::API::UObjectHook::get_objects_by_class(c, allow_default);
        },
        "get_or_add_motion_controller_state", &uevr::API::UObjectHook::get_or_add_motion_controller_state,
        "get_motion_controller_state", &uevr::API::UObjectHook::get_motion_controller_state
    );

    m_lua.new_usertype<uevr::API>("UEVR_API",
        "sdk", &uevr::API::sdk,
        "find_uobject", [](uevr::API* api, const std::string& name) {
            return api->find_uobject<uevr::API::UObject>(utility::widen(name));
        },
        "get_engine", &uevr::API::get_engine,
        "get_player_controller", &uevr::API::get_player_controller,
        "get_local_pawn", &uevr::API::get_local_pawn,
        "spawn_object", &uevr::API::spawn_object,
        "execute_command", [](uevr::API* api, const std::string& s) { api->execute_command(utility::widen(s).data()); },
        "get_uobject_array", &uevr::API::get_uobject_array,
        "get_console_manager", &uevr::API::get_console_manager
    );

    setup_callback_bindings();

    auto out = m_lua.create_table();
    out["params"] = m_plugin_initialize_param;
    out["api"] = uevr::API::get().get();
    out["types"] = m_lua.create_table_with(
        "UObject", m_lua["UEVR_UObject"],
        "UStruct", m_lua["UEVR_UStruct"],
        "UClass", m_lua["UEVR_UClass"],
        "UFunction", m_lua["UEVR_UFunction"],
        "FField", m_lua["UEVR_FField"],
        "FFieldClass", m_lua["UEVR_FFieldClass"],
        "FConsoleManager", m_lua["UEVR_FConsoleManager"],
        "IConsoleObject", m_lua["UEVR_IConsoleObject"],
        "IConsoleVariable", m_lua["UEVR_IConsoleVariable"],
        "IConsoleCommand", m_lua["UEVR_IConsoleCommand"],
        "FName", m_lua["UEVR_FName"],
        "FUObjectArray", m_lua["UEVR_FUObjectArray"],
        "UObjectHook", m_lua["UEVR_UObjectHook"]
    );
    
    out["plugin_callbacks"] = m_plugin_initialize_param->callbacks;
    out["sdk"] = m_plugin_initialize_param->sdk;

    return out.push(m_lua.lua_state());
}

void ScriptContext::on_pre_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_pre_engine_tick_callbacks) try {
            ctx->handle_protected_result(fn(engine, delta_seconds));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_pre_engine_tick: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_pre_engine_tick");
        }
    });
}

void ScriptContext::on_post_engine_tick(UEVR_UGameEngineHandle engine, float delta_seconds) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_post_engine_tick_callbacks) try {
            ctx->handle_protected_result(fn(engine, delta_seconds));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_post_engine_tick: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_post_engine_tick");
        }
    });
}

void ScriptContext::on_pre_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_pre_slate_draw_window_render_thread_callbacks) try {
            ctx->handle_protected_result(fn(renderer, viewport_info));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_pre_slate_draw_window_render_thread: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_pre_slate_draw_window_render_thread");
        }
    });
}

void ScriptContext::on_post_slate_draw_window_render_thread(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_post_slate_draw_window_render_thread_callbacks) try {
            ctx->handle_protected_result(fn(renderer, viewport_info));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_post_slate_draw_window_render_thread: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_post_slate_draw_window_render_thread");
        }
    });
}

void ScriptContext::on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_pre_calculate_stereo_view_offset_callbacks) try {
            ctx->handle_protected_result(fn(device, view_index, world_to_meters, position, rotation, is_double));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_pre_calculate_stereo_view_offset: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_pre_calculate_stereo_view_offset");
        }
    });
}

void ScriptContext::on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle device, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_post_calculate_stereo_view_offset_callbacks) try {
            ctx->handle_protected_result(fn(device, view_index, world_to_meters, position, rotation, is_double));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_post_calculate_stereo_view_offset: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_post_calculate_stereo_view_offset");
        }
    });
}

void ScriptContext::on_pre_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_pre_viewport_client_draw_callbacks) try {
            ctx->handle_protected_result(fn(viewport_client, viewport, canvas));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_pre_viewport_client_draw: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_pre_viewport_client_draw");
        }
    });
}

void ScriptContext::on_post_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_post_viewport_client_draw_callbacks) try {
            ctx->handle_protected_result(fn(viewport_client, viewport, canvas));
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_post_viewport_client_draw: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_post_viewport_client_draw");
        }
    });
}

void ScriptContext::on_frame() {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_frame_callbacks) try {
            ctx->handle_protected_result(fn());
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_frame: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_frame");
        }
    });
}

void ScriptContext::on_draw_ui() {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_draw_ui_callbacks) try {
            ctx->handle_protected_result(fn());
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_draw_ui: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_draw_ui");
        }
    });
}

void ScriptContext::on_script_reset() {
    g_contexts.for_each([=](auto ctx) {
        std::scoped_lock _{ ctx->m_mtx };

        for (auto& fn : ctx->m_on_script_reset_callbacks) try {
            ctx->handle_protected_result(fn());
        } catch (const std::exception& e) {
            ScriptContext::log("Exception in on_script_reset: " + std::string(e.what()));
        } catch (...) {
            ScriptContext::log("Unknown exception in on_script_reset");
        }
    });
}
}