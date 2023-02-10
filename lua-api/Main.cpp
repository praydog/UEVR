// Lua API to expose UEVR functionality to Lua scripts via UE4SS

#include <iostream>

#include <windows.h>
#include <sol/sol.hpp>

#include <uevr/API.hpp>

UEVR_PluginInitializeParam* g_plugin_initialize_param{nullptr};

void log(const std::string& message) {
    std::cout << "[LuaVR] " << message << std::endl;
}

bool setup_plugin_param() {
    const auto unreal_vr_backend = GetModuleHandleA("UnrealVRBackend.dll");

    if (unreal_vr_backend == nullptr) {
        return false;
    }

    g_plugin_initialize_param = (UEVR_PluginInitializeParam*)GetProcAddress(unreal_vr_backend, "g_plugin_initialize_param");

    return g_plugin_initialize_param != nullptr;
}

void test_function() {
    log("Test function called!");
}

int setup_bindings(lua_State* L) {
    sol::state_view lua(L);

    lua.set_function("test_function", test_function);

    lua.new_usertype<UEVR_PluginInitializeParam>("UEVR_PluginInitializeParam",
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

    lua.new_usertype<UEVR_PluginVersion>("UEVR_PluginVersion",
        "major", &UEVR_PluginVersion::major,
        "minor", &UEVR_PluginVersion::minor,
        "patch", &UEVR_PluginVersion::patch
    );

    lua.new_usertype<UEVR_PluginFunctions>("UEVR_PluginFunctions",
        "log_error", &UEVR_PluginFunctions::log_error,
        "log_warn", &UEVR_PluginFunctions::log_warn,
        "log_info", &UEVR_PluginFunctions::log_info,
        "is_drawing_ui", &UEVR_PluginFunctions::is_drawing_ui
    );

    lua.new_usertype<UEVR_RendererData>("UEVR_RendererData",
        "renderer_type", &UEVR_RendererData::renderer_type,
        "device", &UEVR_RendererData::device,
        "swapchain", &UEVR_RendererData::swapchain,
        "command_queue", &UEVR_RendererData::command_queue
    );

    lua.new_usertype<UEVR_SDKFunctions>("UEVR_SDKFunctions",
        "get_uengine", &UEVR_SDKFunctions::get_uengine,
        "set_cvar_int", &UEVR_SDKFunctions::set_cvar_int
    );

    lua.new_usertype<UEVR_SDKData>("UEVR_SDKData",
        "functions", &UEVR_SDKData::functions,
        "callbacks", &UEVR_SDKData::callbacks
    );

    lua.new_usertype<UEVR_VRData>("UEVR_VRData",
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
        "get_lowest_xinput_index", &UEVR_VRData::get_lowest_xinput_index
    );

    lua.new_usertype<UEVR_Vector2f>("UEVR_Vector2f",
        "x", &UEVR_Vector2f::x,
        "y", &UEVR_Vector2f::y
    );

    lua.new_usertype<UEVR_Vector3f>("UEVR_Vector3f",
        "x", &UEVR_Vector3f::x,
        "y", &UEVR_Vector3f::y,
        "z", &UEVR_Vector3f::z
    );

    lua.new_usertype<UEVR_Vector3d>("UEVR_Vector3d",
        "x", &UEVR_Vector3d::x,
        "y", &UEVR_Vector3d::y,
        "z", &UEVR_Vector3d::z
    );

    lua.new_usertype<UEVR_Vector4f>("UEVR_Vector4f",
        "x", &UEVR_Vector4f::x,
        "y", &UEVR_Vector4f::y,
        "z", &UEVR_Vector4f::z,
        "w", &UEVR_Vector4f::w
    );

    lua.new_usertype<UEVR_Quaternionf>("UEVR_Quaternionf",
        "x", &UEVR_Quaternionf::x,
        "y", &UEVR_Quaternionf::y,
        "z", &UEVR_Quaternionf::z,
        "w", &UEVR_Quaternionf::w
    );

    lua.new_usertype<UEVR_Rotatorf>("UEVR_Rotatorf",
        "pitch", &UEVR_Rotatorf::pitch,
        "yaw", &UEVR_Rotatorf::yaw,
        "roll", &UEVR_Rotatorf::roll
    );

    lua.new_usertype<UEVR_Rotatord>("UEVR_Rotatord",
        "pitch", &UEVR_Rotatord::pitch,
        "yaw", &UEVR_Rotatord::yaw,
        "roll", &UEVR_Rotatord::roll
    );

    lua.new_usertype<UEVR_Matrix4x4f>("UEVR_Matrix4x4f",
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

    lua.new_usertype<UEVR_Matrix4x4d>("UEVR_Matrix4x4d",
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

    auto out = lua.create_table();
    out["params"] = g_plugin_initialize_param;

    return out.push(L);
}

// Main exported function that takes in the lua_State*
extern "C" __declspec(dllexport) int luaopen_LuaVR(lua_State* L) {
    log("Initializing LuaVR...");

    if (!setup_plugin_param()) {
        log("Failed to setup plugin param!");
        return 0;
    }

    log("Successfully setup plugin param!");

    return setup_bindings(L);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call, LPVOID reserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}