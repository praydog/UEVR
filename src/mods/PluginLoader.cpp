#include <filesystem>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <openvr.h>

#include "Framework.hpp"
#include "uevr/API.h"

#include <utility/String.hpp>
#include <utility/Module.hpp>

#include <sdk/UEngine.hpp>
#include <sdk/CVar.hpp>

#include "VR.hpp"

#include "PluginLoader.hpp"

UEVR_PluginVersion g_plugin_version{
    UEVR_PLUGIN_VERSION_MAJOR, UEVR_PLUGIN_VERSION_MINOR, UEVR_PLUGIN_VERSION_PATCH};

namespace uevr {
UEVR_RendererData g_renderer_data{
    UEVR_RENDERER_D3D12, nullptr, nullptr, nullptr
};
}

namespace uevr {
void log_error(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::error("[Plugin] {}", str);
}
void log_warn(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::warn("[Plugin] {}", str);
}
void log_info(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::info("[Plugin] {}", str);
}
bool is_drawing_ui() {
    return g_framework->is_drawing_ui();
}
bool remove_callback(void* cb) {
    return PluginLoader::get()->remove_callback(cb);
}
}

namespace uevr {
bool on_present(UEVR_OnPresentCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_present(cb);
}

bool on_device_reset(UEVR_OnDeviceResetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_device_reset(cb);
}

bool on_message(UEVR_OnMessageCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_message(cb);
}

bool on_xinput_get_state(UEVR_OnXInputGetStateCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_xinput_get_state(cb);
}

bool on_xinput_set_state(UEVR_OnXInputSetStateCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_xinput_set_state(cb);
}
}

UEVR_PluginCallbacks g_plugin_callbacks {
    uevr::on_present,
    uevr::on_device_reset,
    uevr::on_message,
    uevr::on_xinput_get_state,
    uevr::on_xinput_set_state
};

UEVR_PluginFunctions g_plugin_functions {
    uevr::log_error,
    uevr::log_warn,
    uevr::log_info,
    uevr::is_drawing_ui,
    uevr::remove_callback
};

UEVR_SDKFunctions g_sdk_functions {
    []() -> UEVR_UEngineHandle {
        return (UEVR_UEngineHandle)sdk::UEngine::get();
    },
    [](const char* module_name, const char* name, int value) -> void {
        static std::unordered_map<std::string, sdk::TConsoleVariableData<int>**> cvars{};

        auto set_cvar = [](sdk::TConsoleVariableData<int>** cvar, int value) {
            if (cvar != nullptr && *cvar != nullptr) {
                (*cvar)->set(value);
            }
        };

        if (!cvars.contains(name)) {
            const auto cvar = sdk::find_cvar_data(utility::widen(module_name), utility::widen(name));

            if (cvar) {
                cvars[name] = (sdk::TConsoleVariableData<int>**)cvar->address();
                set_cvar((sdk::TConsoleVariableData<int>**)cvar->address(), value);
            }
        } else {
            set_cvar(cvars[name], value);
        }
    }
};

namespace uevr {
bool on_pre_engine_tick(UEVR_Engine_TickCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_engine_tick(cb);
}

bool on_post_engine_tick(UEVR_Engine_TickCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_engine_tick(cb);
}

bool on_pre_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_slate_draw_window_render_thread(cb);
}

bool on_post_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_slate_draw_window_render_thread(cb);
}

bool on_pre_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_calculate_stereo_view_offset(cb);
}

bool on_post_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_calculate_stereo_view_offset(cb);
}

bool on_pre_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_viewport_client_draw(cb);
}

bool on_post_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_viewport_client_draw(cb);
}
}

UEVR_SDKCallbacks g_sdk_callbacks {
    uevr::on_pre_engine_tick,
    uevr::on_post_engine_tick,
    uevr::on_pre_slate_draw_window_render_thread,
    uevr::on_post_slate_draw_window_render_thread,
    uevr::on_pre_calculate_stereo_view_offset,
    uevr::on_post_calculate_stereo_view_offset,
    uevr::on_pre_viewport_client_draw,
    uevr::on_post_viewport_client_draw
};

UEVR_SDKData g_sdk_data {
    &g_sdk_functions,
    &g_sdk_callbacks
};

namespace uevr {
namespace vr {
bool is_runtime_ready() {
    return ::VR::get()->get_runtime()->ready();
}

bool is_openvr() {
    return ::VR::get()->get_runtime()->is_openvr();
}

bool is_openxr() {
    return ::VR::get()->get_runtime()->is_openxr();
}

bool is_hmd_active() {
    return ::VR::get()->is_hmd_active();
}

void get_standing_origin(UEVR_Vector3f* out_origin) {
    auto origin = ::VR::get()->get_standing_origin();
    out_origin->x = origin.x;
    out_origin->y = origin.y;
    out_origin->z = origin.z;
}

void get_rotation_offset(UEVR_Quaternionf* out_rotation) {
    auto rotation = ::VR::get()->get_rotation_offset();
    out_rotation->x = rotation.x;
    out_rotation->y = rotation.y;
    out_rotation->z = rotation.z;
    out_rotation->w = rotation.w;
}

void set_standing_origin(const UEVR_Vector3f* origin) {
    ::VR::get()->set_standing_origin({origin->x, origin->y, origin->z, 1.0f});
}

void set_rotation_offset(const UEVR_Quaternionf* rotation) {
    ::VR::get()->set_rotation_offset({rotation->w, rotation->x, rotation->y, rotation->z});
}

UEVR_TrackedDeviceIndex get_hmd_index() {
    return VR::get()->get_hmd_index();
}

UEVR_TrackedDeviceIndex get_left_controller_index() {
    return VR::get()->get_left_controller_index();
}

UEVR_TrackedDeviceIndex get_right_controller_index() {
    return VR::get()->get_right_controller_index();
}

void get_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_grip_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_grip_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_aim_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_aim_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_grip_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_grip_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_aim_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_aim_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_eye_offset(UEVR_Eye eye, UEVR_Vector3f* out_offset) {
    const auto out = ::VR::get()->get_eye_offset((VRRuntime::Eye)eye);

    out_offset->x = out.x;
    out_offset->y = out.y;
    out_offset->z = out.z;
}

void get_ue_projection_matrix(UEVR_Eye eye, UEVR_Matrix4x4f* out_projection) {
    const auto& projection = ::VR::get()->get_runtime()->projections[eye];
    memcpy(out_projection, &projection, sizeof(UEVR_Matrix4x4f));
}

UEVR_InputSourceHandle get_left_joystick_source() {
    return (UEVR_InputSourceHandle)::VR::get()->get_left_joystick();
}

UEVR_InputSourceHandle get_right_joystick_source() {
    return (UEVR_InputSourceHandle)::VR::get()->get_right_joystick();
}

UEVR_ActionHandle get_action_handle(const char* action_path) {
    return (UEVR_ActionHandle)::VR::get()->get_action_handle(action_path);
}

bool is_action_active(UEVR_ActionHandle action_handle, UEVR_InputSourceHandle source) {
    return ::VR::get()->is_action_active((::vr::VRActionHandle_t)action_handle, (::vr::VRInputValueHandle_t)source);
}

bool is_action_active_any_joystick(UEVR_ActionHandle action_handle) {
    return ::VR::get()->is_action_active_any_joystick((::vr::VRActionHandle_t)action_handle);
}

void get_joystick_axis(UEVR_InputSourceHandle source, UEVR_Vector2f* out_axis) {
    const auto axis = ::VR::get()->get_joystick_axis((::vr::VRInputValueHandle_t)source);

    out_axis->x = axis.x;
    out_axis->y = axis.y;
}

void trigger_haptic_vibration(float seconds_from_now, float duration, float frequency, float amplitude, UEVR_InputSourceHandle source) {
    ::VR::get()->trigger_haptic_vibration(seconds_from_now, duration, frequency, amplitude, (::vr::VRInputValueHandle_t)source);
}

bool is_using_controllers() {
    return ::VR::get()->is_using_controllers();
}

bool is_decoupled_pitch_enabled() {
    return ::VR::get()->is_decoupled_pitch_enabled();
}

unsigned int get_movement_orientation() {
    return ::VR::get()->get_movement_orientation();
}

unsigned int get_lowest_xinput_index() {
    return VR::get()->get_lowest_xinput_index();
}

void recenter_view() {
    VR::get()->recenter_view();
}
}
}

UEVR_VRData g_vr_data {
    uevr::vr::is_runtime_ready,
    uevr::vr::is_openvr,
    uevr::vr::is_openxr,
    uevr::vr::is_hmd_active,
    uevr::vr::get_standing_origin,
    uevr::vr::get_rotation_offset,
    uevr::vr::set_standing_origin,
    uevr::vr::set_rotation_offset,
    uevr::vr::get_hmd_index,
    uevr::vr::get_left_controller_index,
    uevr::vr::get_right_controller_index,
    uevr::vr::get_pose,
    uevr::vr::get_transform,
    uevr::vr::get_grip_pose,
    uevr::vr::get_aim_pose,
    uevr::vr::get_grip_transform,
    uevr::vr::get_aim_transform,
    uevr::vr::get_eye_offset,
    uevr::vr::get_ue_projection_matrix,
    uevr::vr::get_left_joystick_source,
    uevr::vr::get_right_joystick_source,
    uevr::vr::get_action_handle,
    uevr::vr::is_action_active,
    uevr::vr::is_action_active_any_joystick,
    uevr::vr::get_joystick_axis,
    uevr::vr::trigger_haptic_vibration,
    uevr::vr::is_using_controllers,
    uevr::vr::is_decoupled_pitch_enabled,
    uevr::vr::get_movement_orientation,
    uevr::vr::get_lowest_xinput_index,
    uevr::vr::recenter_view,
};


/*
DECLARE_UEVR_HANDLE(UEVR_IVRSystem);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperone);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperoneSetup);
DECLARE_UEVR_HANDLE(UEVR_IVRCompositor);
DECLARE_UEVR_HANDLE(UEVR_IVROverlay);
DECLARE_UEVR_HANDLE(UEVR_IVROverlayView);
DECLARE_UEVR_HANDLE(UEVR_IVRHeadsetView);
DECLARE_UEVR_HANDLE(UEVR_IVRScreenshots);
DECLARE_UEVR_HANDLE(UEVR_IVRRenderModels);
DECLARE_UEVR_HANDLE(UEVR_IVRApplications);
DECLARE_UEVR_HANDLE(UEVR_IVRSettings);
DECLARE_UEVR_HANDLE(UEVR_IVRResources);
DECLARE_UEVR_HANDLE(UEVR_IVRExtendedDisplay);
DECLARE_UEVR_HANDLE(UEVR_IVRTrackedCamera);
DECLARE_UEVR_HANDLE(UEVR_IVRDriverManager);
DECLARE_UEVR_HANDLE(UEVR_IVRInput);
DECLARE_UEVR_HANDLE(UEVR_IVRIOBuffer);
DECLARE_UEVR_HANDLE(UEVR_IVRSpatialAnchors);
DECLARE_UEVR_HANDLE(UEVR_IVRNotifications);
DECLARE_UEVR_HANDLE(UEVR_IVRDebug);
*/

namespace uevr {
namespace openvr{
UEVR_IVRSystem get_vr_system() {
    return (UEVR_IVRSystem)VR::get()->get_openvr_runtime()->hmd;
}

UEVR_IVRChaperone get_vr_chaperone() {
    return (UEVR_IVRChaperone)::vr::VRChaperone();
}

UEVR_IVRChaperoneSetup get_vr_chaperone_setup() {
    return (UEVR_IVRChaperoneSetup)::vr::VRChaperoneSetup();
}

UEVR_IVRCompositor get_vr_compositor() {
    return (UEVR_IVRCompositor)::vr::VRCompositor();
}

UEVR_IVROverlay get_vr_overlay() {
    return (UEVR_IVROverlay)::vr::VROverlay();
}

UEVR_IVROverlayView get_vr_overlay_view() {
    return (UEVR_IVROverlayView)::vr::VROverlayView();
}

UEVR_IVRHeadsetView get_vr_headset_view() {
    return (UEVR_IVRHeadsetView)::vr::VRHeadsetView();
}

UEVR_IVRScreenshots get_vr_screenshots() {
    return (UEVR_IVRScreenshots)::vr::VRScreenshots();
}

UEVR_IVRRenderModels get_vr_render_models() {
    return (UEVR_IVRRenderModels)::vr::VRRenderModels();
}

UEVR_IVRApplications get_vr_applications() {
    return (UEVR_IVRApplications)::vr::VRApplications();
}

UEVR_IVRSettings get_vr_settings() {
    return (UEVR_IVRSettings)::vr::VRSettings();
}

UEVR_IVRResources get_vr_resources() {
    return (UEVR_IVRResources)::vr::VRResources();
}

UEVR_IVRExtendedDisplay get_vr_extended_display() {
    return (UEVR_IVRExtendedDisplay)::vr::VRExtendedDisplay();
}

UEVR_IVRTrackedCamera get_vr_tracked_camera() {
    return (UEVR_IVRTrackedCamera)::vr::VRTrackedCamera();
}

UEVR_IVRDriverManager get_vr_driver_manager() {
    return (UEVR_IVRDriverManager)::vr::VRDriverManager();
}

UEVR_IVRInput get_vr_input() {
    return (UEVR_IVRInput)::vr::VRInput();
}

UEVR_IVRIOBuffer get_vr_io_buffer() {
    return (UEVR_IVRIOBuffer)::vr::VRIOBuffer();
}

UEVR_IVRSpatialAnchors get_vr_spatial_anchors() {
    return (UEVR_IVRSpatialAnchors)::vr::VRSpatialAnchors();
}

UEVR_IVRNotifications get_vr_notifications() {
    return (UEVR_IVRNotifications)::vr::VRNotifications();
}

UEVR_IVRDebug get_vr_debug() {
    return (UEVR_IVRDebug)::vr::VRDebug();
}
}
}

UEVR_OpenVRData g_openvr_data {
    uevr::openvr::get_vr_system,
    uevr::openvr::get_vr_chaperone,
    uevr::openvr::get_vr_chaperone_setup,
    uevr::openvr::get_vr_compositor,
    uevr::openvr::get_vr_overlay,
    uevr::openvr::get_vr_overlay_view,
    uevr::openvr::get_vr_headset_view,
    uevr::openvr::get_vr_screenshots,
    uevr::openvr::get_vr_render_models,
    uevr::openvr::get_vr_applications,
    uevr::openvr::get_vr_settings,
    uevr::openvr::get_vr_resources,
    uevr::openvr::get_vr_extended_display,
    uevr::openvr::get_vr_tracked_camera,
    uevr::openvr::get_vr_driver_manager,
    uevr::openvr::get_vr_input,
    uevr::openvr::get_vr_io_buffer,
    uevr::openvr::get_vr_spatial_anchors,
    uevr::openvr::get_vr_notifications,
    uevr::openvr::get_vr_debug
};

namespace uevr {
namespace openxr {
UEVR_XrInstance get_xr_instance() {
    return (UEVR_XrInstance)VR::get()->get_openxr_runtime()->instance;
}

UEVR_XrSession get_xr_session() {
    return (UEVR_XrSession)VR::get()->get_openxr_runtime()->session;
}

UEVR_XrSpace get_stage_space() {
    return (UEVR_XrSpace)VR::get()->get_openxr_runtime()->stage_space;
}

UEVR_XrSpace get_view_space() {
    return (UEVR_XrSpace)VR::get()->get_openxr_runtime()->view_space;
}
}
}

UEVR_OpenXRData g_openxr_data {
    uevr::openxr::get_xr_instance,
    uevr::openxr::get_xr_session,
    uevr::openxr::get_stage_space,
    uevr::openxr::get_view_space
};

extern "C" __declspec(dllexport) UEVR_PluginInitializeParam g_plugin_initialize_param{
    nullptr, 
    &g_plugin_version, 
    &g_plugin_functions, 
    &g_plugin_callbacks,
    &uevr::g_renderer_data,
    &g_vr_data,
    &g_openvr_data,
    &g_openxr_data,
    &g_sdk_data
};

void verify_sdk_pointers() {
    auto verify = [](auto& g) {
        spdlog::info("Verifying...");

        for (auto i = 0; i < sizeof(g) / sizeof(void*); ++i) {
            if (((void**)&g)[i] == nullptr) {
                spdlog::error("SDK pointer is null at index {}", i);
            }
        }
    };

    spdlog::info("Verifying SDK pointers...");

    verify(g_sdk_data);
}

std::shared_ptr<PluginLoader>& PluginLoader::get() {
    static auto instance = std::make_shared<PluginLoader>();
    return instance;
}

void PluginLoader::early_init() try {
    namespace fs = std::filesystem;

    std::scoped_lock _{m_mux};
    std::string module_path{};

    module_path.resize(1024, 0);
    module_path.resize(GetModuleFileName(nullptr, module_path.data(), module_path.size()));
    spdlog::info("[PluginLoader] Module path {}", module_path);

    const auto plugin_path = Framework::get_persistent_dir() / "plugins";

    spdlog::info("[PluginLoader] Creating directories {}", plugin_path.string());

    if (!fs::create_directories(plugin_path) && !fs::exists(plugin_path)) {
        spdlog::error("[PluginLoader] Failed to create directory for plugins: {}", plugin_path.string());
    } else {
        spdlog::info("[PluginLoader] Created directory for plugins: {}", plugin_path.string());
    }

    spdlog::info("[PluginLoader] Loading plugins...");

    // Load all dlls in the plugins directory.
    for (auto&& entry : fs::directory_iterator{plugin_path}) {
        auto&& path = entry.path();

        if (path.has_extension() && path.extension() == ".dll") {
            auto module = LoadLibrary(path.string().c_str());

            if (module == nullptr) {
                spdlog::error("[PluginLoader] Failed to load {}", path.string());
                m_plugin_load_errors.emplace(path.stem().string(), "Failed to load");
                continue;
            }

            spdlog::info("[PluginLoader] Loaded {}", path.string());
            m_plugins.emplace(path.stem().string(), module);
        }
    }
} catch(const std::exception& e) {
    spdlog::error("[PluginLoader] Exception during early init {}", e.what());
} catch(...) {
    spdlog::error("[PluginLoader] Unknown exception during early init");
}

std::optional<std::string> PluginLoader::on_initialize_d3d_thread() {
    std::scoped_lock _{m_mux};

    // Call UEVR_plugin_required_version on any dlls that export it.
    g_plugin_initialize_param.uevr_module = g_framework->get_framework_module();
    uevr::g_renderer_data.renderer_type = (int)g_framework->get_renderer_type();
    
    if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D11) {
        auto& d3d11 = g_framework->get_d3d11_hook();

        uevr::g_renderer_data.device = d3d11->get_device();
        uevr::g_renderer_data.swapchain = d3d11->get_swap_chain();
    } else if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D12) {
        auto& d3d12 = g_framework->get_d3d12_hook();

        uevr::g_renderer_data.device = d3d12->get_device();
        uevr::g_renderer_data.swapchain = d3d12->get_swap_chain();
        uevr::g_renderer_data.command_queue = d3d12->get_command_queue();
    } else {
        spdlog::error("[PluginLoader] Unsupported renderer type {}", uevr::g_renderer_data.renderer_type);
        return "PluginLoader: Unsupported renderer type detected";
    }

    verify_sdk_pointers();

    for (auto it = m_plugins.begin(); it != m_plugins.end();) {
        auto name = it->first;
        auto mod = it->second;
        auto required_version_fn = (UEVR_PluginRequiredVersionFn)GetProcAddress(mod, "uevr_plugin_required_version");

        if (required_version_fn == nullptr) {
            spdlog::info("[PluginLoader] {} has no uevr_plugin_required_version function, skipping...", name);

            ++it;
            continue;
        }

        UEVR_PluginVersion required_version{};

        try {
            required_version_fn(&required_version);
        } catch(...) {
            spdlog::error("[PluginLoader] {} has an exception in uevr_plugin_required_version, skipping...", name);
            m_plugin_load_errors.emplace(name, "Exception occurred in uevr_plugin_required_version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        spdlog::info(
            "[PluginLoader] {} requires version {}.{}.{}", name, required_version.major, required_version.minor, required_version.patch);

        if (required_version.major != g_plugin_version.major) {
            spdlog::error("[PluginLoader] Plugin {} requires a different major version", name);
            m_plugin_load_errors.emplace(name, "Requires a different major version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        if (required_version.minor > g_plugin_version.minor) {
            spdlog::error("[PluginLoader] Plugin {} requires a newer minor version", name);
            m_plugin_load_errors.emplace(name, "Requires a newer minor version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        if (required_version.patch > g_plugin_version.patch) {
            spdlog::warn("[PluginLoader] Plugin {} desires a newer patch version", name);
            m_plugin_load_warnings.emplace(name, "Desires a newer patch version");
        }

        ++it;
    }

    // Call UEVR_plugin_initialize on any dlls that export it.
    for (auto it = m_plugins.begin(); it != m_plugins.end();) {
        auto name = it->first;
        auto mod = it->second;
        auto init_fn = (UEVR_PluginInitializeFn)GetProcAddress(mod, "uevr_plugin_initialize");

        if (init_fn == nullptr) {
            ++it;
            continue;
        }

        spdlog::info("[PluginLoader] Initializing {}...", name);
        try {
            if (!init_fn(&g_plugin_initialize_param)) {
                spdlog::error("[PluginLoader] Failed to initialize {}", name);
                m_plugin_load_errors.emplace(name, "Failed to initialize");
                FreeLibrary(mod);
                it = m_plugins.erase(it);
                continue;
            }
        } catch(...) {
            spdlog::error("[PluginLoader] {} has an exception in uevr_plugin_initialize, skipping...", name);
            m_plugin_load_errors.emplace(name, "Exception occurred in uevr_plugin_initialize");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        ++it;
    }

    return std::nullopt;
}

void PluginLoader::attempt_unload_plugins() {
    std::unique_lock _{m_api_cb_mtx};

    for (auto& callbacks : m_plugin_callback_lists) {
        callbacks->clear();
    }

    for (auto& pair : m_plugins) {
        FreeLibrary(pair.second);
    }

    m_plugins.clear();
}

void PluginLoader::reload_plugins() {
    early_init();
    on_initialize_d3d_thread();
}

void PluginLoader::on_draw_ui() {
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);

    if (ImGui::CollapsingHeader(get_name().data())) {
        std::scoped_lock _{m_mux};

        if (ImGui::Button("Attempt Unload Plugins")) {
            attempt_unload_plugins();
        }

        if (ImGui::Button("Reload Plugins")) {
            attempt_unload_plugins();
            reload_plugins();
        }

        if (!m_plugins.empty()) {
            ImGui::Text("Loaded plugins:");

            for (auto&& [name, _] : m_plugins) {
                ImGui::Text(name.c_str());
            }
        } else {
            ImGui::Text("No plugins loaded.");
        }

        if (!m_plugin_load_errors.empty()) {
            ImGui::Spacing();
            ImGui::Text("Errors:");
            for (auto&& [name, error] : m_plugin_load_errors) {
                ImGui::Text("%s - %s", name.c_str(), error.c_str());
            }
        }

        if (!m_plugin_load_warnings.empty()) {
            ImGui::Spacing();
            ImGui::Text("Warnings:");
            for (auto&& [name, warning] : m_plugin_load_warnings) {
                ImGui::Text("%s - %s", name.c_str(), warning.c_str());
            }
        }
    }
}

void PluginLoader::on_present() {
    std::shared_lock _{m_api_cb_mtx};

    uevr::g_renderer_data.renderer_type = (int)g_framework->get_renderer_type();
    
    if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D11) {
        auto& d3d11 = g_framework->get_d3d11_hook();

        uevr::g_renderer_data.device = d3d11->get_device();
        uevr::g_renderer_data.swapchain = d3d11->get_swap_chain();
    } else if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D12) {
        auto& d3d12 = g_framework->get_d3d12_hook();

        uevr::g_renderer_data.device = d3d12->get_device();
        uevr::g_renderer_data.swapchain = d3d12->get_swap_chain();
        uevr::g_renderer_data.command_queue = d3d12->get_command_queue();
    }

    for (auto&& cb : m_on_present_cbs) {
        try {
            cb();
        } catch(...) {
            spdlog::error("[PluginLoader] Exception occurred in on_present callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_device_reset() {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_device_reset_cbs) {
        try {
            cb();
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_device_reset callback; one of the plugins has an error.");
        }
    }
}

bool PluginLoader::on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_message_cbs) {
        try {
            if (!cb(hwnd, msg, wparam, lparam)) {
                return false;
            }
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_message callback; one of the plugins has an error.");
            continue;
        }
    }

    return true;
}

void PluginLoader::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_xinput_get_state_cbs) {
        try {
            cb(retval, user_index, (void*)state);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_xinput_get_state callback; one of the plugins has an error.");
            continue;
        }
    }
}

void PluginLoader::on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_xinput_set_state_cbs) {
        try {
            cb(retval, user_index, (void*)vibration);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_xinput_set_state callback; one of the plugins has an error.");
            continue;
        }
    }
}


void PluginLoader::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_engine_tick_cbs) {
        try {
            cb((UEVR_UGameEngineHandle)engine, delta);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_engine_tick callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_engine_tick(sdk::UGameEngine* engine, float delta) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_engine_tick_cbs) {
        try {
            cb((UEVR_UGameEngineHandle)engine, delta);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_engine_tick callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_slate_draw_window_render_thread_cbs) {
        try {
            cb((UEVR_FSlateRHIRendererHandle)renderer, (UEVR_FViewportInfoHandle)viewport_info);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_slate_draw_window callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_slate_draw_window_render_thread_cbs) {
        try {
            cb((UEVR_FSlateRHIRendererHandle)renderer, (UEVR_FViewportInfoHandle)viewport_info);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_slate_draw_window callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                       const float world_to_meters, Vector3f* view_location, bool is_double)
{
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_calculate_stereo_view_offset_cbs) {
        try {
            cb( (UEVR_StereoRenderingDeviceHandle)stereo_device, view_index, world_to_meters, 
                (UEVR_Vector3f*)view_location, (UEVR_Rotatorf*)view_rotation, is_double);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_calculate_stereo_view_offset callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                        const float world_to_meters, Vector3f* view_location, bool is_double)
{
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_calculate_stereo_view_offset_cbs) {
        try {
            cb( (UEVR_StereoRenderingDeviceHandle)stereo_device, view_index, world_to_meters, 
                (UEVR_Vector3f*)view_location, (UEVR_Rotatorf*)view_rotation, is_double);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_calculate_stereo_view_offset callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_viewport_client_draw_cbs) {
        try {
            cb((UEVR_UGameViewportClientHandle)viewport_client, (UEVR_FViewportHandle)viewport, (UEVR_FCanvasHandle)canvas);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_viewport_client_draw callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_viewport_client_draw_cbs) {
        try {
            cb((UEVR_UGameViewportClientHandle)viewport_client, (UEVR_FViewportHandle)viewport, (UEVR_FCanvasHandle)canvas);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_viewport_client_draw callback; one of the plugins has an error.");
        }
    }
}

bool PluginLoader::add_on_present(UEVR_OnPresentCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_present_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_device_reset(UEVR_OnDeviceResetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_device_reset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_message(UEVR_OnMessageCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_message_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_xinput_get_state(UEVR_OnXInputGetStateCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_xinput_get_state_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_xinput_set_state(UEVR_OnXInputSetStateCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_xinput_set_state_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_engine_tick(UEVR_Engine_TickCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_engine_tick_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_engine_tick(UEVR_Engine_TickCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_engine_tick_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_slate_draw_window_render_thread_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_slate_draw_window_render_thread_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_calculate_stereo_view_offset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_calculate_stereo_view_offset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_viewport_client_draw_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_viewport_client_draw_cbs.push_back(cb);
    return true;
}