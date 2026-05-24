#define NOMINMAX

#include <fstream>

#include <windows.h>
#include <dbt.h>

#include <imgui.h>
#include <utility/Module.hpp>
#include <utility/Registry.hpp>
#include <utility/ScopeGuard.hpp>

#include <sdk/Globals.hpp>
#include <sdk/CVar.hpp>
#include <sdk/threading/GameThreadWorker.hpp>
#include <sdk/UGameplayStatics.hpp>
#include <sdk/APlayerController.hpp>

#include <tracy/Tracy.hpp>

#include "Framework.hpp"
#include "frameworkConfig.hpp"

#include "utility/Logging.hpp"

#include "VR.hpp"

std::shared_ptr<VR>& VR::get() {
    //static std::shared_ptr<VR> instance = std::make_shared<VR>();
    return g_framework->vr();
}

// Called when the mod is initialized
std::optional<std::string> VR::clean_initialize() try {
    ZoneScopedN(__FUNCTION__);

    auto openvr_error = initialize_openvr();

    if (openvr_error || !m_openvr->loaded) {
        if (m_openvr->error) {
            spdlog::info("OpenVR failed to load: {}", *m_openvr->error);
        }

        m_openvr->is_hmd_active = false;
        m_openvr->was_hmd_active = false;
        m_openvr->needs_pose_update = false;

        // Attempt to load OpenXR instead
        auto openxr_error = initialize_openxr();

        if (openxr_error || !m_openxr->loaded) {
            m_openxr->needs_pose_update = false;
        }
    } else {
        m_openxr->error = "OpenVR loaded first.";
    }

    if (!get_runtime()->loaded) {
        // this is okay. we're not going to fail the whole thing entirely
        // so we're just going to return OK, but
        // when the VR mod draws its menu, it'll say "VR is not available"
        return Mod::on_initialize();
    }

    // Check whether the user has Hardware accelerated GPU scheduling enabled
    const auto hw_schedule_value = utility::get_registry_dword(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        "HwSchMode");

    if (hw_schedule_value) {
        m_has_hw_scheduling = *hw_schedule_value == 2;
    }

    m_init_finished = true;

    // all OK
    return Mod::on_initialize();
} catch(...) {
    spdlog::error("Exception occurred in VR::on_initialize()");

    m_runtime->error = "Exception occurred in VR::on_initialize()";
    m_openxr->dll_missing = false;
    m_openvr->dll_missing = false;
    m_openxr->error = "Exception occurred in VR::on_initialize()";
    m_openvr->error = "Exception occurred in VR::on_initialize()";
    m_openvr->loaded = false;
    m_openvr->is_hmd_active = false;
    m_openxr->loaded = false;
    m_init_finished = false;

    return Mod::on_initialize();
}

std::optional<std::string> VR::initialize_openvr() {
    ZoneScopedN(__FUNCTION__);

    spdlog::info("Attempting to load OpenVR");

    m_openvr = std::make_shared<runtimes::OpenVR>();
    m_openvr->loaded = false;

    const auto wants_openxr = m_requested_runtime_name->value() == "openxr_loader.dll";

    SPDLOG_INFO("[VR] Requested runtime: {}", m_requested_runtime_name->value());

    if (wants_openxr && GetModuleHandleW(L"openxr_loader.dll") != nullptr) {
        // pre-injected
        m_openvr->dll_missing = true;
        m_openvr->error = "OpenXR already loaded";
        return Mod::on_initialize();
    }

    if (GetModuleHandleW(L"openvr_api.dll") == nullptr) {
        // pre-injected
        if (GetModuleHandleW(L"openxr_loader.dll") != nullptr) {
            m_openvr->dll_missing = true;
            m_openvr->error = "OpenXR already loaded";
            return Mod::on_initialize();
        }


        if (utility::load_module_from_current_directory(L"openvr_api.dll") == nullptr) {
            spdlog::info("[VR] Could not load openvr_api.dll");

            m_openvr->dll_missing = true;
            m_openvr->error = "Could not load openvr_api.dll";
            return Mod::on_initialize();
        }
    }

    if (g_framework->is_dx12()) {
        m_d3d12.on_reset(this);
    } else {
        m_d3d11.on_reset(this);
    }

    m_openvr->needs_pose_update = true;
    m_openvr->got_first_poses = false;
    m_openvr->is_hmd_active = true;
    m_openvr->was_hmd_active = true;

    spdlog::info("Attempting to call vr::VR_Init");

    auto error = vr::VRInitError_None;
	m_openvr->hmd = vr::VR_Init(&error, vr::VRApplication_Scene);

    // check if error
    if (error != vr::VRInitError_None) {
        m_openvr->error = "VR_Init failed: " + std::string{vr::VR_GetVRInitErrorAsEnglishDescription(error)};
        return Mod::on_initialize();
    }

    if (m_openvr->hmd == nullptr) {
        m_openvr->error = "VR_Init failed: HMD is null";
        return Mod::on_initialize();
    }

    // get render target size
    m_openvr->update_render_target_size();

    if (vr::VRCompositor() == nullptr) {
        m_openvr->error = "VRCompositor failed to initialize.";
        return Mod::on_initialize();
    }

    auto input_error = initialize_openvr_input();

    if (input_error) {
        m_openvr->error = *input_error;
        return Mod::on_initialize();
    }

    auto overlay_error = m_overlay_component.on_initialize_openvr();

    if (overlay_error) {
        m_openvr->error = *overlay_error;
        return Mod::on_initialize();
    }
    
    m_openvr->loaded = true;
    m_openvr->error = std::nullopt;
    m_runtime = m_openvr;

    return Mod::on_initialize();
}

std::optional<std::string> VR::initialize_openvr_input() {
    ZoneScopedN(__FUNCTION__);

    const auto module_directory = Framework::get_persistent_dir();

    // write default actions and bindings with the static strings we have
    for (auto& it : m_binding_files) {
        spdlog::info("Writing default binding file {}", it.first);

        std::ofstream file{ module_directory / it.first };
        file << it.second;
    }

    const auto actions_path = module_directory / "actions.json";
    auto input_error = vr::VRInput()->SetActionManifestPath(actions_path.string().c_str());

    if (input_error != vr::VRInputError_None) {
        return "VRInput failed to set action manifest path: " + std::to_string((uint32_t)input_error);
    }

    // get action set
    auto action_set_error = vr::VRInput()->GetActionSetHandle("/actions/default", &m_action_set);

    if (action_set_error != vr::VRInputError_None) {
        return "VRInput failed to get action set: " + std::to_string((uint32_t)action_set_error);
    }

    if (m_action_set == vr::k_ulInvalidActionSetHandle) {
        return "VRInput failed to get action set handle.";
    }

    for (auto& it : m_action_handles) {
        auto error = vr::VRInput()->GetActionHandle(it.first.c_str(), &it.second.get());

        if (error != vr::VRInputError_None) {
            return "VRInput failed to get action handle: (" + it.first + "): " + std::to_string((uint32_t)error);
        }

        if (it.second == vr::k_ulInvalidActionHandle) {
            return "VRInput failed to get action handle: (" + it.first + ")";
        }
    }

    m_active_action_set.ulActionSet = m_action_set;
    m_active_action_set.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
    m_active_action_set.nPriority = 0;

    m_openvr->pose_action = m_action_pose;
    m_openvr->grip_pose_action = m_action_grip_pose;

    detect_controllers();

    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr() {
    ZoneScopedN(__FUNCTION__);

    m_openxr.reset();
    m_openxr = std::make_shared<runtimes::OpenXR>();

    spdlog::info("[VR] Initializing OpenXR");

    if (GetModuleHandleW(L"openxr_loader.dll") == nullptr) {
        if (utility::load_module_from_current_directory(L"openxr_loader.dll") == nullptr) {
            spdlog::info("[VR] Could not load openxr_loader.dll");

            m_openxr->loaded = false;
            m_openxr->error = "Could not load openxr_loader.dll";

            return std::nullopt;
        }
    }

    if (g_framework->is_dx12()) {
        m_d3d12.on_reset(this);
    } else {
        m_d3d11.on_reset(this);
    }

    m_openxr->needs_pose_update = true;
    m_openxr->got_first_poses = false;

    // Step 1: Create an instance
    spdlog::info("[VR] Creating OpenXR instance");

    XrResult result{XR_SUCCESS};

    // We may just be restarting OpenXR, so try to find an existing instance first
    if (m_openxr->instance == XR_NULL_HANDLE) {
        std::vector<const char*> extensions{};

        if (g_framework->is_dx12()) {
            extensions.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
        } else {
            extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
        }

        // Enumerate available extensions and enable depth extension if available
        uint32_t extension_count{};
        result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extension_count, nullptr);

        std::vector<XrExtensionProperties> extension_properties(extension_count, {XR_TYPE_EXTENSION_PROPERTIES});

        if (!XR_FAILED(result)) try {
            result = xrEnumerateInstanceExtensionProperties(nullptr, extension_count, &extension_count, extension_properties.data());

            if (!XR_FAILED(result)) {
                for (const auto& extension_property : extension_properties) {
                    spdlog::info("[VR] Found OpenXR extension: {}", extension_property.extensionName);
                }

                const std::unordered_set<std::string> wanted_extensions {
                    XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
                    XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME
                    // To be seen if we need more!
                };

                for (const auto& extension_property : extension_properties) {
                    if (wanted_extensions.contains(extension_property.extensionName)) {
                        spdlog::info("[VR] Enabling {} extension", extension_property.extensionName);
                        m_openxr->enabled_extensions.insert(extension_property.extensionName);
                        extensions.push_back(extension_property.extensionName);
                    }
                }
            }
        } catch(...) {
            spdlog::error("[VR] Unknown error while enumerating OpenXR extensions");
        }

        XrInstanceCreateInfo instance_create_info{XR_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.next = nullptr;
        instance_create_info.enabledExtensionCount = (uint32_t)extensions.size();
        instance_create_info.enabledExtensionNames = extensions.data();

        std::string application_name{"UEVR"};

        // Append the current executable name to the application base name
        {
            const auto exe = utility::get_executable();
            const auto full_path = utility::get_module_pathw(exe);

            if (full_path) {
                const auto fs_path = std::filesystem::path(*full_path);
                const auto filename = fs_path.stem().string();

                application_name += "_" + filename;

                // Trim the name to 127 characters
                if (application_name.length() >= XR_MAX_APPLICATION_NAME_SIZE) {
                    application_name = application_name.substr(0, XR_MAX_APPLICATION_NAME_SIZE - 1);
                }
            }
        }

        spdlog::info("[VR] Application name: {}", application_name);

        strcpy(instance_create_info.applicationInfo.applicationName, application_name.c_str());
        instance_create_info.applicationInfo.applicationName[XR_MAX_APPLICATION_NAME_SIZE - 1] = '\0';
        instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        
        result = xrCreateInstance(&instance_create_info, &m_openxr->instance);

        // we can't convert the result to a string here
        // because the function requires the instance to be valid
        if (result != XR_SUCCESS) {
            m_openxr->error = "Could not create openxr instance: " + std::to_string((int32_t)result);
            if (result == XR_ERROR_LIMIT_REACHED) {
                m_openxr->error = "Could not create openxr instance: XR_ERROR_LIMIT_REACHED\n"
                    "Ensure that the OpenXR plugin has been renamed or deleted from the game's binaries folder.";
            }
            spdlog::error("[VR] {}", m_openxr->error.value());

            return std::nullopt;
        }
    } else {
        spdlog::info("[VR] Found existing openxr instance");
    }
    
    // Step 2: Create a system
    spdlog::info("[VR] Creating OpenXR system");

    // We may just be restarting OpenXR, so try to find an existing system first
    if (m_openxr->system == XR_NULL_SYSTEM_ID) {
        XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
        system_info.formFactor = m_openxr->form_factor;

        result = xrGetSystem(m_openxr->instance, &system_info, &m_openxr->system);

        if (result != XR_SUCCESS) {
            m_openxr->error = "Could not create openxr system: " + m_openxr->get_result_string(result);
            spdlog::error("[VR] {}", m_openxr->error.value());

            return std::nullopt;
        }
    } else {
        spdlog::info("[VR] Found existing openxr system");
    }

    // Step 3: Create a session
    spdlog::info("[VR] Initializing graphics info");

    XrSessionCreateInfo session_create_info{XR_TYPE_SESSION_CREATE_INFO};

    if (g_framework->is_dx12()) {
        m_d3d12.openxr().initialize(session_create_info);
    } else {
        m_d3d11.openxr().initialize(session_create_info);
    }

    spdlog::info("[VR] Creating OpenXR session");
    session_create_info.systemId = m_openxr->system;
    result = xrCreateSession(m_openxr->instance, &session_create_info, &m_openxr->session);

    if (result != XR_SUCCESS) {
        m_openxr->error = "Could not create openxr session: " + m_openxr->get_result_string(result);
        spdlog::error("[VR] {}", m_openxr->error.value());

        return std::nullopt;
    }

    // Step 4: Create a space
    spdlog::info("[VR] Creating OpenXR space");

    // We may just be restarting OpenXR, so try to find an existing space first

    if (m_openxr->stage_space == XR_NULL_HANDLE) {
        XrReferenceSpaceCreateInfo space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space_create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        space_create_info.poseInReferenceSpace = {};
        space_create_info.poseInReferenceSpace.orientation.w = 1.0f;

        result = xrCreateReferenceSpace(m_openxr->session, &space_create_info, &m_openxr->stage_space);

        if (result != XR_SUCCESS) {
            m_openxr->error = "Could not create openxr stage space: " + m_openxr->get_result_string(result);
            spdlog::error("[VR] {}", m_openxr->error.value());

            return std::nullopt;
        }
    }

    if (m_openxr->view_space == XR_NULL_HANDLE) {
        XrReferenceSpaceCreateInfo space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space_create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        space_create_info.poseInReferenceSpace = {};
        space_create_info.poseInReferenceSpace.orientation.w = 1.0f;

        result = xrCreateReferenceSpace(m_openxr->session, &space_create_info, &m_openxr->view_space);

        if (result != XR_SUCCESS) {
            m_openxr->error = "Could not create openxr view space: " + m_openxr->get_result_string(result);
            spdlog::error("[VR] {}", m_openxr->error.value());

            return std::nullopt;
        }
    }

    // Step 5: Get the system properties
    spdlog::info("[VR] Getting OpenXR system properties");

    XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
    result = xrGetSystemProperties(m_openxr->instance, m_openxr->system, &system_properties);

    if (result != XR_SUCCESS) {
        m_openxr->error = "Could not get system properties: " + m_openxr->get_result_string(result);
        spdlog::error("[VR] {}", m_openxr->error.value());

        return std::nullopt;
    }

    m_openxr->on_system_properties_acquired(system_properties);

    // Step 6: Get the view configuration properties
    m_openxr->update_render_target_size();

    // Step 7: Create a view
    if (!m_openxr->view_configs.empty()){
        m_openxr->views.resize(m_openxr->view_configs.size(), {XR_TYPE_VIEW});
        m_openxr->stage_views.resize(m_openxr->view_configs.size(), {XR_TYPE_VIEW});
    }

    if (m_openxr->view_configs.empty()) {
        m_openxr->error = "No view configurations found";
        spdlog::error("[VR] {}", m_openxr->error.value());

        return std::nullopt;
    }

    m_openxr->loaded = true;
    m_runtime = m_openxr;

    if (auto err = initialize_openxr_input()) {
        m_openxr->error = err.value();
        m_openxr->loaded = false;
        spdlog::error("[VR] {}", m_openxr->error.value());

        return std::nullopt;
    }

    detect_controllers();

    if (m_init_finished) {
        // This is usually done in on_config_load
        // but the runtime can be reinitialized, so we do it here instead
        initialize_openxr_swapchains();
    }

    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr_input() {
    ZoneScopedN(__FUNCTION__);

    if (auto err = m_openxr->initialize_actions(VR::actions_json)) {
        m_openxr->error = err.value();
        spdlog::error("[VR] {}", m_openxr->error.value());

        return std::nullopt;
    }
    
    for (auto& it : m_action_handles) {
        auto openxr_action_name = m_openxr->translate_openvr_action_name(it.first);

        if (m_openxr->action_set.action_map.contains(openxr_action_name)) {
            it.second.get() = (decltype(it.second)::type)m_openxr->action_set.action_map[openxr_action_name];
            spdlog::info("[VR] Successfully mapped action {} to {}", it.first, openxr_action_name);
        }
    }

    m_left_joystick = (decltype(m_left_joystick))VRRuntime::Hand::LEFT;
    m_right_joystick = (decltype(m_right_joystick))VRRuntime::Hand::RIGHT;

    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr_swapchains() {
    ZoneScopedN(__FUNCTION__);

    // This depends on the config being loaded.
    if (!m_init_finished) {
        return std::nullopt;
    }

    spdlog::info("[VR] Creating OpenXR swapchain");

    const auto supported_swapchain_formats = m_openxr->get_supported_swapchain_formats();

    // Log
    for (auto f : supported_swapchain_formats) {
        spdlog::info("[VR] Supported swapchain format: {}", (uint32_t)f);
    }

    if (g_framework->is_dx12()) {
        auto err = m_d3d12.openxr().create_swapchains();

        if (err) {
            m_openxr->error = err.value();
            m_openxr->loaded = false;
            spdlog::error("[VR] {}", m_openxr->error.value());

            return m_openxr->error;
        }
    } else {
        auto err = m_d3d11.openxr().create_swapchains();

        if (err) {
            m_openxr->error = err.value();
            m_openxr->loaded = false;
            spdlog::error("[VR] {}", m_openxr->error.value());
            return m_openxr->error;
        }
    }

    return std::nullopt;
}

bool VR::detect_controllers() {
    ZoneScopedN(__FUNCTION__);

    // already detected
    if (!m_controllers.empty()) {
        return true;
    }

    if (get_runtime()->is_openvr()) {
        auto left_joystick_origin_error = vr::EVRInputError::VRInputError_None;
        auto right_joystick_origin_error = vr::EVRInputError::VRInputError_None;

        vr::InputOriginInfo_t left_joystick_origin_info{};
        vr::InputOriginInfo_t right_joystick_origin_info{};

        // Get input origin info for the joysticks
        // get the source input device handles for the joysticks
        auto left_joystick_error = vr::VRInput()->GetInputSourceHandle("/user/hand/left", &m_left_joystick);

        if (left_joystick_error != vr::VRInputError_None) {
            return false;
        }

        auto right_joystick_error = vr::VRInput()->GetInputSourceHandle("/user/hand/right", &m_right_joystick);

        if (right_joystick_error != vr::VRInputError_None) {
            return false;
        }

        m_openvr->left_controller_handle = m_left_joystick;
        m_openvr->right_controller_handle = m_right_joystick;

        left_joystick_origin_info = {};
        right_joystick_origin_info = {};

        left_joystick_origin_error = vr::VRInput()->GetOriginTrackedDeviceInfo(m_left_joystick, &left_joystick_origin_info, sizeof(left_joystick_origin_info));
        right_joystick_origin_error = vr::VRInput()->GetOriginTrackedDeviceInfo(m_right_joystick, &right_joystick_origin_info, sizeof(right_joystick_origin_info));
        if (left_joystick_origin_error != vr::EVRInputError::VRInputError_None || right_joystick_origin_error != vr::EVRInputError::VRInputError_None) {
            return false;
        }

        // Instead of manually going through the devices,
        // We do this. The order of the devices isn't always guaranteed to be
        // Left, and then right. Using the input state handles will always
        // Get us the correct device indices.
        m_controllers.push_back(left_joystick_origin_info.trackedDeviceIndex);
        m_controllers.push_back(right_joystick_origin_info.trackedDeviceIndex);
        m_controllers_set.insert(left_joystick_origin_info.trackedDeviceIndex);
        m_controllers_set.insert(right_joystick_origin_info.trackedDeviceIndex);

        spdlog::info("Left Hand: {}", left_joystick_origin_info.trackedDeviceIndex);
        spdlog::info("Right Hand: {}", right_joystick_origin_info.trackedDeviceIndex);

        m_openvr->left_controller_index = left_joystick_origin_info.trackedDeviceIndex;
        m_openvr->right_controller_index = right_joystick_origin_info.trackedDeviceIndex;
    } else if (get_runtime()->is_openxr()) {
        // ezpz
        m_controllers.push_back(1);
        m_controllers.push_back(2);
        m_controllers_set.insert(1);
        m_controllers_set.insert(2);

        spdlog::info("Left Hand: {}", 1);
        spdlog::info("Right Hand: {}", 2);
    }


    return true;
}

bool VR::is_any_action_down() {
    ZoneScopedN(__FUNCTION__);

    if (!m_runtime->ready()) {
        return false;
    }

    const auto left_axis = get_left_stick_axis();
    const auto right_axis = get_right_stick_axis();

    if (glm::length(left_axis) >= m_joystick_deadzone->value()) {
        return true;
    }

    if (glm::length(right_axis) >= m_joystick_deadzone->value()) {
        return true;
    }

    const auto left_joystick = get_left_joystick();
    const auto right_joystick = get_right_joystick();

    for (auto& it : m_action_handles) {
        // These are too easy to trigger
        if (it.second == m_action_thumbrest_touch_left || it.second == m_action_thumbrest_touch_right) {
            continue;
        }

        if (it.second == m_action_a_button_touch_left || it.second == m_action_a_button_touch_right) {
            continue;
        }

        if (it.second == m_action_b_button_touch_left || it.second == m_action_b_button_touch_right) {
            continue;
        }

        if (is_action_active(it.second, left_joystick) || is_action_active(it.second, right_joystick)) {
            return true;
        }
    }

    return false;
}

bool VR::on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    ZoneScopedN(__FUNCTION__);

    if (message == WM_DEVICECHANGE && !m_spoofed_gamepad_connection) {
        spdlog::info("[VR] Received WM_DEVICECHANGE");
        m_last_xinput_spoof_sent = std::chrono::steady_clock::now();
    }

    return true;
}

void VR::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
    ZoneScopedN(__FUNCTION__);

    if (std::chrono::steady_clock::now() - m_last_engine_tick > std::chrono::seconds(1)) {
        SPDLOG_INFO_EVERY_N_SEC(1, "[VR] XInputGetState called, but engine tick hasn't been called in over a second. Is the game loading?");
        update_action_states();
    }

    if (*retval == ERROR_SUCCESS) {
        // Once here for normal gamepads, and once for the spoofed gamepad at the end
        update_imgui_state_from_xinput_state(*state, false);
        gamepad_snapturn(*state);
    }

    const auto now = std::chrono::steady_clock::now();

    if (now - m_last_xinput_update > std::chrono::seconds(2)) {
        m_lowest_xinput_user_index = user_index;
    }

    if (user_index < m_lowest_xinput_user_index) {
        m_lowest_xinput_user_index = user_index;
        spdlog::info("[VR] Changed lowest XInput user index to {}", user_index);
    }

    if (user_index != m_lowest_xinput_user_index) {
        if (!m_spoofed_gamepad_connection && is_using_controllers()) {
            spdlog::info("[VR] XInputGetState called, but user index is {}", user_index);
        }

        return;
    }

    if (!m_spoofed_gamepad_connection) {
        spdlog::info("[VR] Successfully spoofed gamepad connection @ {}", user_index);
    }
    
    m_last_xinput_update = now;
    m_spoofed_gamepad_connection = true;

    auto runtime = get_runtime();

    auto do_pause_select = [&]() {
        if (!runtime->ready()) {
            return;
        }

        if (runtime->handle_pause) {
            // Spoof the start button being pressed
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_START;
            *retval = ERROR_SUCCESS;
            runtime->handle_pause = false;
            runtime->handle_select_button = false;
        }

        if (runtime->handle_select_button) {
            // Spoof the back button being pressed
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
            *retval = ERROR_SUCCESS;
            runtime->handle_select_button = false;
            runtime->handle_pause = false;
        }
    };

    do_pause_select();

    if (is_using_controllers_within(std::chrono::minutes(5))) {
        *retval = ERROR_SUCCESS;
    }

    if (!is_using_controllers()) {
        return;
    }

    // Clear button state for VR controllers
    if (is_using_controllers_within(std::chrono::seconds(5))) {
        state->Gamepad.wButtons = 0;
        state->Gamepad.bLeftTrigger = 0;
        state->Gamepad.bRightTrigger = 0;
        state->Gamepad.sThumbLX = 0;
        state->Gamepad.sThumbLY = 0;
        state->Gamepad.sThumbRX = 0;
        state->Gamepad.sThumbRY = 0;
    }

    const auto left_joystick = get_left_joystick();
    const auto right_joystick = get_right_joystick();
    const auto wants_swap = m_swap_controllers->value();

    runtime->handle_pause_select(is_action_active_any_joystick(m_action_system_button));
    do_pause_select();

    const auto& a_button_left = !wants_swap ? m_action_a_button_left : m_action_a_button_right;
    const auto& a_button_right = !wants_swap ? m_action_a_button_right : m_action_a_button_left;

    const auto is_right_a_button_down = is_action_active_any_joystick(a_button_right);
    const auto is_left_a_button_down = is_action_active_any_joystick(a_button_left);

    if (is_right_a_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    }

    if (is_left_a_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    }

    const auto& b_button_left = !wants_swap ? m_action_b_button_left : m_action_b_button_right;
    const auto& b_button_right = !wants_swap ? m_action_b_button_right : m_action_b_button_left;

    const auto is_right_b_button_down = is_action_active_any_joystick(b_button_right);
    const auto is_left_b_button_down = is_action_active_any_joystick(b_button_left);

    if (is_right_b_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    }

    if (is_left_b_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    }

    const auto is_left_joystick_click_down = is_action_active(m_action_joystick_click, left_joystick);
    const auto is_right_joystick_click_down = is_action_active(m_action_joystick_click, right_joystick);

    if (is_left_joystick_click_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    }

    if (is_right_joystick_click_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    }

    const auto is_left_trigger_down = is_action_active(m_action_trigger, left_joystick);
    const auto is_right_trigger_down = is_action_active(m_action_trigger, right_joystick);

    if (is_left_trigger_down) {
        state->Gamepad.bLeftTrigger = 255;
    }

    if (is_right_trigger_down) {
        state->Gamepad.bRightTrigger = 255;
    }

    const auto is_right_grip_down = is_action_active(m_action_grip, right_joystick);
    const auto is_left_grip_down = is_action_active(m_action_grip, left_joystick);

    if (is_right_grip_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    }

    if (is_left_grip_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    }

    const auto is_dpad_up_down = is_action_active_any_joystick(m_action_dpad_up);

    if (is_dpad_up_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
    }

    const auto is_dpad_right_down = is_action_active_any_joystick(m_action_dpad_right);

    if (is_dpad_right_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    }

    const auto is_dpad_down_down = is_action_active_any_joystick(m_action_dpad_down);

    if (is_dpad_down_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
    }

    const auto is_dpad_left_down = is_action_active_any_joystick(m_action_dpad_left);

    if (is_dpad_left_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }

    const auto left_joystick_axis = get_joystick_axis(left_joystick);
    const auto right_joystick_axis = get_joystick_axis(right_joystick);

    const auto true_left_joystick_axis = get_joystick_axis(m_left_joystick);
    const auto true_right_joystick_axis = get_joystick_axis(m_right_joystick);

    state->Gamepad.sThumbLX = (int16_t)std::clamp<float>(((float)state->Gamepad.sThumbLX + left_joystick_axis.x * 32767.0f), -32767.0f, 32767.0f);
    state->Gamepad.sThumbLY = (int16_t)std::clamp<float>(((float)state->Gamepad.sThumbLY + left_joystick_axis.y * 32767.0f), -32767.0f, 32767.0f);

    state->Gamepad.sThumbRX = (int16_t)std::clamp<float>(((float)state->Gamepad.sThumbRX + right_joystick_axis.x * 32767.0f), -32767.0f, 32767.0f);
    state->Gamepad.sThumbRY = (int16_t)std::clamp<float>(((float)state->Gamepad.sThumbRY + right_joystick_axis.y * 32767.0f), -32767.0f, 32767.0f);

    bool already_dpad_shifted{false};
    bool true_left_joystick_as_dpad{false}; 
    bool true_right_joystick_as_dpad{false}; 

    if (m_dpad_gesture_state.direction != DPadGestureState::Direction::NONE) {
        already_dpad_shifted = true;

        if ((m_dpad_gesture_state.direction & DPadGestureState::Direction::UP) != 0) {
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        }

        if ((m_dpad_gesture_state.direction & DPadGestureState::Direction::RIGHT) != 0) {
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        }

        if ((m_dpad_gesture_state.direction & DPadGestureState::Direction::DOWN) != 0) {
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        }

        if ((m_dpad_gesture_state.direction & DPadGestureState::Direction::LEFT) != 0) {
            state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
        }

        DPadMethod dpad_method = get_dpad_method();
        if (dpad_method == DPadMethod::GESTURE_HEAD){
            true_left_joystick_as_dpad = true;
        }
        else if (dpad_method == DPadMethod::GESTURE_HEAD_RIGHT) {
            true_right_joystick_as_dpad = true;
        }


        std::scoped_lock _{m_dpad_gesture_state.mtx};
        m_dpad_gesture_state.direction = DPadGestureState::Direction::NONE;
    }

    // Touching the thumbrest allows us to use the thumbstick as a dpad.  Additional options are for controllers without capacitives/games that rely solely on DPad
    if (!already_dpad_shifted && m_dpad_shifting->value()) {
        bool button_touch_inactive{true};
        bool thumbrest_check{false};

        DPadMethod dpad_method = get_dpad_method();
        if (dpad_method == DPadMethod::RIGHT_TOUCH) {
            thumbrest_check = is_action_active_any_joystick(m_action_thumbrest_touch_right);
            button_touch_inactive = !is_action_active_any_joystick(m_action_a_button_touch_right) && !is_action_active_any_joystick(m_action_b_button_touch_right);
        }
        if (dpad_method == DPadMethod::LEFT_TOUCH) {
            thumbrest_check = is_action_active_any_joystick(m_action_thumbrest_touch_left);
            button_touch_inactive = !is_action_active_any_joystick(m_action_a_button_touch_left) && !is_action_active_any_joystick(m_action_b_button_touch_left);
        }

        // Toggling UEVR menu using L3 + R3 has higher priority 
        const auto dpad_active = (is_right_joystick_click_down &&  (dpad_method == DPadMethod::RIGHT_JOYSTICK_CLICK) && (! is_left_joystick_click_down)) 
        || (is_left_joystick_click_down &&  (dpad_method == DPadMethod::LEFT_JOYSTICK_CLICK) && (! is_right_joystick_click_down)) 
        || (button_touch_inactive && thumbrest_check) || dpad_method == DPadMethod::LEFT_JOYSTICK || dpad_method == DPadMethod::RIGHT_JOYSTICK;

        if (dpad_active) {
            float ty{0.0f};
            float tx{0.0f};
            //SHORT ThumbY{0};
            //SHORT ThumbX{0};
            // If someone is accidentally touching both thumbrests while also moving a joystick, this will default to left joystick.
            if (dpad_method == DPadMethod::RIGHT_TOUCH || dpad_method == DPadMethod::LEFT_JOYSTICK || dpad_method == DPadMethod::RIGHT_JOYSTICK_CLICK) {
                //ThumbY = state->Gamepad.sThumbLY;
                //ThumbX = state->Gamepad.sThumbLX;
                ty = true_left_joystick_axis.y;
                tx = true_left_joystick_axis.x;
                true_left_joystick_as_dpad = true;
            }
            else if (dpad_method == DPadMethod::LEFT_TOUCH || dpad_method == DPadMethod::RIGHT_JOYSTICK || dpad_method == DPadMethod::LEFT_JOYSTICK_CLICK) {
                //ThumbY = state->Gamepad.sThumbRY;
                //ThumbX = state->Gamepad.sThumbRX;
                ty = true_right_joystick_axis.y;
                tx = true_right_joystick_axis.x;
                true_right_joystick_as_dpad = true;
            }
            
            if (ty >= 0.5f) {
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
            }

            if (ty <= -0.5f) {
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
            }

            if (tx >= 0.5f) {
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
            }

            if (tx <= -0.5f) {
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
            }

            if(dpad_method == DPadMethod::RIGHT_JOYSTICK_CLICK)
            {
                state->Gamepad.wButtons &= ~XINPUT_GAMEPAD_RIGHT_THUMB;
            }
            else if(dpad_method == DPadMethod::LEFT_JOYSTICK_CLICK) 
            {
                state->Gamepad.wButtons &= ~XINPUT_GAMEPAD_LEFT_THUMB;
            }

        }
    }

    // Zero out the thumbstick values
    if (true_left_joystick_as_dpad) {
        if (!wants_swap) {
            state->Gamepad.sThumbLY = 0;
            state->Gamepad.sThumbLX = 0;
        } else {
            state->Gamepad.sThumbRY = 0;
            state->Gamepad.sThumbRX = 0;
        }
    }
    else if (true_right_joystick_as_dpad) {
        if (!wants_swap) {
            state->Gamepad.sThumbRY = 0;
            state->Gamepad.sThumbRX = 0;
        } else {
            state->Gamepad.sThumbLY = 0;
            state->Gamepad.sThumbLX = 0;
        }
    }



    // Determine if snapturn should be run on frame
    if (m_snapturn->value()) {
        DPadMethod dpad_method = get_dpad_method();
        const auto snapturn_deadzone = get_snapturn_js_deadzone();
        float stick_axis{};

        if (!m_was_snapturn_run_on_input) {
            if (dpad_method == RIGHT_JOYSTICK) {
                stick_axis = true_left_joystick_axis.x;
                if (glm::abs(stick_axis) >= snapturn_deadzone) {
                    if (stick_axis < 0) {
                        m_snapturn_left = true;
                    }
                    m_snapturn_on_frame = true;
                    m_was_snapturn_run_on_input = true;
                }
            }
            else {
                stick_axis = right_joystick_axis.x;
                const auto& thumbrest_touch_left = !wants_swap ? m_action_thumbrest_touch_left : m_action_thumbrest_touch_right;
                const auto& stick_as_dpad = (!wants_swap) ? true_right_joystick_as_dpad : true_left_joystick_as_dpad;
                if (glm::abs(stick_axis) >= snapturn_deadzone){
                    if(!stick_as_dpad) {
                        if (stick_axis < 0) {
                            m_snapturn_left = true;
                        }
                        m_snapturn_on_frame = true;
                    }
                    // Requiring the joystick returning to its natrual position at least once before another snapturn,
                    // even if no snapturn is actually run
                    m_was_snapturn_run_on_input = true; 
                }
            }
        }
        else {
            if (dpad_method == RIGHT_JOYSTICK) {
                if (glm::abs(true_left_joystick_axis.x) < snapturn_deadzone) {
                    m_was_snapturn_run_on_input = false;
                } else {
                    state->Gamepad.sThumbLY = 0;
                    state->Gamepad.sThumbLX = 0;
                }
            }
            else {
                if (glm::abs(right_joystick_axis.x) < snapturn_deadzone) {
                    m_was_snapturn_run_on_input = false;
                } else {
                    state->Gamepad.sThumbRY = 0;
                    state->Gamepad.sThumbRX = 0;
                }
            }
        }
    }
    
    // Do it again after all the VR buttons have been spoofed
    update_imgui_state_from_xinput_state(*state, true);
}

void VR::on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {
    ZoneScopedN(__FUNCTION__);

    if (user_index != m_lowest_xinput_user_index) {
        return;
    }

    if (!is_using_controllers()) {
        return;
    }

    const auto left_amplitude = ((float)vibration->wLeftMotorSpeed / 65535.0f) * 5.0f;
    const auto right_amplitude = ((float)vibration->wRightMotorSpeed / 65535.0f) * 5.0f;

    if (left_amplitude > 0.0f) {
        trigger_haptic_vibration(0.0f, 0.1f, 1.0f, left_amplitude, get_left_joystick());
    }

    if (right_amplitude > 0.0f) {
        trigger_haptic_vibration(0.0f, 0.1f, 1.0f, right_amplitude, get_right_joystick());
    }
}

// Allows imgui navigation to work with the controllers
void VR::update_imgui_state_from_xinput_state(XINPUT_STATE& state, bool is_vr_controller) {
    ZoneScopedN(__FUNCTION__);

    bool is_using_this_controller = true;

    const auto is_using_vr_controller_recently = is_using_controllers_within(std::chrono::seconds(1));
    const auto is_gamepad = !is_vr_controller;

    if (is_vr_controller && !is_using_vr_controller_recently) {
        is_using_this_controller = false;
    } else if (is_gamepad && is_using_vr_controller_recently) { // dont allow gamepad navigation if using vr controllers
        is_using_this_controller = false;
    }

    // L3 + R3 to open the menu
    if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0 && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0) {
        if (!FrameworkConfig::get()->is_enable_l3_r3_toggle()) {
            return;
        }

        bool should_open = true;

        const auto now = std::chrono::steady_clock::now();

        if (FrameworkConfig::get()->is_l3_r3_long_press() && !g_framework->is_drawing_ui()) {
            if (!m_xinput_context.menu_longpress_begin_held) {
                m_xinput_context.menu_longpress_begin = now;
            }

            m_xinput_context.menu_longpress_begin_held = true;
            should_open = (now - m_xinput_context.menu_longpress_begin) >= std::chrono::seconds(1);
        } else {
            m_xinput_context.menu_longpress_begin_held = false;
        }

        if (should_open && now - m_last_xinput_l3_r3_menu_open >= std::chrono::seconds(1)) {
            m_last_xinput_l3_r3_menu_open = std::chrono::steady_clock::now();
            g_framework->set_draw_ui(!g_framework->is_drawing_ui());

            state.Gamepad.wButtons &= ~(XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB); // so input doesn't go through to the game
        }
    } else if (is_using_this_controller) {
        m_xinput_context.headlocked_begin_held = false;
        m_xinput_context.menu_longpress_begin_held = false;
    }

    // We need to adjust the stick values based on the selected movement orientation value if the user wants to do this
    // It will either need to be adjusted by the HMD rotation or one of the controllers.
    if (is_using_this_controller && m_movement_orientation->value() != VR::AimMethod::GAME && m_movement_orientation->value() != m_aim_method->value()) {
        const auto left_stick_og = glm::vec2((float)state.Gamepad.sThumbLX, (float)state.Gamepad.sThumbLY );
        const auto left_stick_magnitude = glm::clamp(glm::length(left_stick_og), -32767.0f, 32767.0f);
        const auto left_stick = glm::normalize(left_stick_og);
        const auto left_stick_angle = glm::atan2(left_stick.y, left_stick.x);

        if (this->is_controller_movement_enabled() && is_vr_controller) {
            const auto controller_index = this->get_movement_orientation() == VR::AimMethod::LEFT_CONTROLLER ? get_left_controller_index() : get_right_controller_index();
            const auto controller_rotation = utility::math::flatten(m_rotation_offset * glm::quat{get_rotation(controller_index)});
            const auto controller_forward = controller_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            const auto controller_angle = glm::atan2(controller_forward.x, controller_forward.z);

            // Normalize angles to [0, 2π]
            const auto normalized_left_stick_angle = left_stick_angle < 0 ? left_stick_angle + 2 * glm::pi<float>() : left_stick_angle;
            const auto normalized_controller_angle = controller_angle < 0 ? controller_angle + 2 * glm::pi<float>() : controller_angle;

            // Add the angles together
            const auto new_left_stick_angle = utility::math::fix_angle(normalized_left_stick_angle + normalized_controller_angle);
            const auto new_left_stick = glm::vec2(glm::cos(new_left_stick_angle), glm::sin(new_left_stick_angle)) * left_stick_magnitude;

            state.Gamepad.sThumbLX = (int16_t)new_left_stick.x;
            state.Gamepad.sThumbLY = (int16_t)new_left_stick.y;
        } else { // Fallback to head aim
            // Rotate the left stick by the HMD rotation
            const auto hmd_rotation = utility::math::flatten(m_rotation_offset * glm::quat{get_rotation(0)});
            const auto hmd_forward = hmd_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            const auto hmd_angle = glm::atan2(hmd_forward.x, hmd_forward.z);

            // Normalize angles to [0, 2π]
            const auto normalized_left_stick_angle = left_stick_angle < 0 ? left_stick_angle + 2 * glm::pi<float>() : left_stick_angle;
            const auto normalized_hmd_angle = hmd_angle < 0 ? hmd_angle + 2 * glm::pi<float>() : hmd_angle;

            // Add the angles together
            const auto new_left_stick_angle = utility::math::fix_angle(normalized_left_stick_angle + normalized_hmd_angle);
            const auto new_left_stick = glm::vec2{glm::cos(new_left_stick_angle), glm::sin(new_left_stick_angle)} * left_stick_magnitude;

            state.Gamepad.sThumbLX = (int16_t)new_left_stick.x;
            state.Gamepad.sThumbLY = (int16_t)new_left_stick.y;
        }
    }

    if (!g_framework->is_drawing_ui()) {
        m_rt_modifier.draw = false;
        return;
    }

    if (!is_using_this_controller) {
        return;
    }

    // Gamepad navigation when the menu is open
    m_xinput_context.enqueue(is_vr_controller, state, [this](const XINPUT_STATE& state, bool is_vr_controller){
        static auto last_time = std::chrono::high_resolution_clock::now();

        const auto delta = std::chrono::duration<float>((std::chrono::high_resolution_clock::now() - last_time)).count();
        last_time = std::chrono::high_resolution_clock::now();

        auto& io = ImGui::GetIO();
        auto& gamepad = state.Gamepad;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        // Headlocked aim toggle
        if (!FrameworkConfig::get()->is_l3_r3_long_press()) {
            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0 && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0) {
                if (!m_xinput_context.headlocked_begin_held) {
                    m_xinput_context.headlocked_begin = std::chrono::steady_clock::now();
                    m_xinput_context.headlocked_begin_held = true;
                }
            } else {
                m_xinput_context.headlocked_begin_held = false;
            }
        }

        // Now that we're drawing the UI, check for special button combos the user can use as shortcuts
        // like recenter view, set standing origin, camera offset modification, etc.
        m_rt_modifier.draw = gamepad.bRightTrigger >= 128;

        if (!m_rt_modifier.draw) {
            m_rt_modifier.page = 0;
            m_rt_modifier.was_moving_left = false;
            m_rt_modifier.was_moving_right = false;
        }

        // If user holding down RT with menu open...
        if (m_rt_modifier.draw) {
            // Camera offset modification
            const auto right_ratio = (float)gamepad.sThumbLX / 32767.0f;
            const auto forward_ratio = (float)gamepad.sThumbLY / 32767.0f;
            const auto up_ratio = (float)gamepad.sThumbRY / 32767.0f;

            if (right_ratio <= -0.25f || right_ratio >= 0.25f) {
                const auto right_offset = right_ratio * delta * 150.0f;
                m_camera_right_offset->value() += right_offset;
            }

            if (forward_ratio <= -0.25f || forward_ratio >= 0.25f) {
                const auto forward_offset = forward_ratio * delta * 150.0f;
                m_camera_forward_offset->value() += forward_offset;
            }

            if (up_ratio <= -0.25f || up_ratio >= 0.25f) {
                const auto up_offset = up_ratio * delta * 150.0f;
                m_camera_up_offset->value() += up_offset;
            }

            if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                if (!m_rt_modifier.was_moving_left) {
                    if (m_rt_modifier.page > 0) {
                        m_rt_modifier.page--;
                    } else {
                        m_rt_modifier.page = m_rt_modifier.num_pages - 1;
                    }

                    m_rt_modifier.was_moving_left = true;
                }
            } else {
                m_rt_modifier.was_moving_left = false;
            }

            if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                if (!m_rt_modifier.was_moving_right) {
                    if (m_rt_modifier.page < m_rt_modifier.num_pages - 1) {
                        m_rt_modifier.page++;
                    } else {
                        m_rt_modifier.page = 0;
                    }

                    m_rt_modifier.was_moving_right = true;
                }
            } else {
                m_rt_modifier.was_moving_right = false;
            }

            // Reset camera offset
            switch (m_rt_modifier.page) {
            case 2:
                if (gamepad.wButtons & XINPUT_GAMEPAD_B) {
                    save_camera(2);
                }

                // Recenter
                if (gamepad.wButtons & XINPUT_GAMEPAD_Y) {
                    save_camera(1);
                }

                // Reset standing origin
                if (gamepad.wButtons & XINPUT_GAMEPAD_X) {
                    save_camera(0);
                }
                break;
            
            case 1:
                if (gamepad.wButtons & XINPUT_GAMEPAD_B) {
                    load_camera(2);
                }

                // Recenter
                if (gamepad.wButtons & XINPUT_GAMEPAD_Y) {
                    load_camera(1);
                }

                // Reset standing origin
                if (gamepad.wButtons & XINPUT_GAMEPAD_X) {
                    load_camera(0);
                }

                break; 
            case 0:
            default:
                if (gamepad.wButtons & XINPUT_GAMEPAD_B) {
                    m_camera_right_offset->value() = 0.0f;
                    m_camera_forward_offset->value() = 0.0f;
                    m_camera_up_offset->value() = 0.0f;
                }

                // Recenter
                if (gamepad.wButtons & XINPUT_GAMEPAD_Y) {
                    this->recenter_view();
                }

                // Reset standing origin
                if (gamepad.wButtons & XINPUT_GAMEPAD_X) {
                    this->set_standing_origin(this->get_position(0));
                }
                
                break;
            }

            // ignore everything else
            return;
        }

        // From imgui_impl_win32.cpp
        #define IM_SATURATE(V)                      (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
        #define MAP_BUTTON(KEY_NO, BUTTON_ENUM)     { io.AddKeyEvent(KEY_NO, (gamepad.wButtons & BUTTON_ENUM) != 0); }
        #define MAP_ANALOG(KEY_NO, VALUE, V0, V1)   { float vn = (float)(VALUE - V0) / (float)(V1 - V0); io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); }

        MAP_BUTTON(ImGuiKey_GamepadStart,           XINPUT_GAMEPAD_START);
        MAP_BUTTON(ImGuiKey_GamepadBack,            XINPUT_GAMEPAD_BACK);
        MAP_BUTTON(ImGuiKey_GamepadFaceLeft,        XINPUT_GAMEPAD_X);
        MAP_BUTTON(ImGuiKey_GamepadFaceRight,       XINPUT_GAMEPAD_B);
        MAP_BUTTON(ImGuiKey_GamepadFaceUp,          XINPUT_GAMEPAD_Y);
        MAP_BUTTON(ImGuiKey_GamepadFaceDown,        XINPUT_GAMEPAD_A);
        MAP_BUTTON(ImGuiKey_GamepadDpadLeft,        XINPUT_GAMEPAD_DPAD_LEFT);
        MAP_BUTTON(ImGuiKey_GamepadDpadRight,       XINPUT_GAMEPAD_DPAD_RIGHT);
        MAP_BUTTON(ImGuiKey_GamepadDpadUp,          XINPUT_GAMEPAD_DPAD_UP);
        MAP_BUTTON(ImGuiKey_GamepadDpadDown,        XINPUT_GAMEPAD_DPAD_DOWN);
        MAP_ANALOG(ImGuiKey_GamepadL2,              gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
        MAP_ANALOG(ImGuiKey_GamepadR2,              gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
        MAP_BUTTON(ImGuiKey_GamepadL3,              XINPUT_GAMEPAD_LEFT_THUMB);
        MAP_BUTTON(ImGuiKey_GamepadR3,              XINPUT_GAMEPAD_RIGHT_THUMB);

        if (!is_vr_controller) {
            MAP_ANALOG(ImGuiKey_GamepadLStickLeft,      gamepad.sThumbLX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
            MAP_ANALOG(ImGuiKey_GamepadLStickRight,     gamepad.sThumbLX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
            MAP_ANALOG(ImGuiKey_GamepadLStickUp,        gamepad.sThumbLY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
            MAP_ANALOG(ImGuiKey_GamepadLStickDown,      gamepad.sThumbLY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
            MAP_BUTTON(ImGuiKey_GamepadL1,              XINPUT_GAMEPAD_LEFT_SHOULDER);
            MAP_BUTTON(ImGuiKey_GamepadR1,              XINPUT_GAMEPAD_RIGHT_SHOULDER);
        } else {
            // Map it to the dpad
            const auto left_stick_left = gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
            if (m_xinput_context.vr.left_stick_left.was_pressed(left_stick_left)) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, true);
            } else if (!left_stick_left) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, false);
            }

            const auto left_stick_right = gamepad.sThumbLX > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
            if (m_xinput_context.vr.left_stick_right.was_pressed(left_stick_right)) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadRight, true);
            } else if (!left_stick_right) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadRight, false);
            }

            const auto left_stick_up = gamepad.sThumbLY > +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
            if (m_xinput_context.vr.left_stick_up.was_pressed(left_stick_up)) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadUp, true);
            } else if (!left_stick_up) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadUp, false);
            }

            const auto left_stick_down = gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
            if (m_xinput_context.vr.left_stick_down.was_pressed(left_stick_down)) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadDown, true);
            } else if (!left_stick_down) {
                io.AddKeyEvent(ImGuiKey_GamepadDpadDown, false);
            }
        }

        MAP_ANALOG(ImGuiKey_GamepadRStickLeft,      gamepad.sThumbRX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
        MAP_ANALOG(ImGuiKey_GamepadRStickRight,     gamepad.sThumbRX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
        MAP_ANALOG(ImGuiKey_GamepadRStickUp,        gamepad.sThumbRY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
        MAP_ANALOG(ImGuiKey_GamepadRStickDown,      gamepad.sThumbRY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    });

    // Zero out the state so we don't send input to the game.
    ZeroMemory(&state.Gamepad, sizeof(XINPUT_GAMEPAD));
}

void VR::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    ZoneScopedN(__FUNCTION__);

    m_cvar_manager->on_pre_engine_tick(engine, delta);
    m_last_engine_tick = std::chrono::steady_clock::now();

    if (!get_runtime()->loaded || !is_hmd_active()) {
        return;
    }

    SPDLOG_INFO_ONCE("VR: Pre-engine tick");

    m_render_target_pool_hook->on_pre_engine_tick(engine, delta);

    update_statistics_overlay(engine);

    // Dont update action states on AFR frames
    // TODO: fix this for actual AFR, but we dont really care about pure AFR since synced beats it most of the time
    if (m_fake_stereo_hook != nullptr && !m_fake_stereo_hook->is_ignoring_next_viewport_draw()) {
        update_action_states();
    }
}

void VR::on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                             const float world_to_meters, Vector3f* view_location, bool is_double)
{
    if (!is_hmd_active()) {
        m_camera_freeze.position_wants_freeze = false;
        m_camera_freeze.rotation_wants_freeze = false;
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    const auto delta = std::chrono::duration<float, std::chrono::seconds::period>(now - m_last_lerp_update).count();

    Rotator<double>* view_rotation_double = (Rotator<double>*)view_rotation;
    Vector3d* view_location_double = (Vector3d*)view_location;

    glm::vec3 target_rotation = is_double ? glm::vec3{*(glm::vec<3, double>*)view_rotation_double} : *(glm::vec<3, float>*)view_rotation;

    const auto should_lerp_pitch = m_lerp_camera_pitch->value();
    const auto should_lerp_yaw = m_lerp_camera_yaw->value();
    const auto should_lerp_roll = m_lerp_camera_roll->value();

    auto lerp_angle = [](auto a, auto b, auto t) {
        const auto diff = b - a;
        if constexpr (std::is_same_v<decltype(a), double>) {
            if (diff > 180.0) {
                b -= 360.0;
            } else if (diff < -180.0) {
                b += 360.0;
            }
        } else {
            if (diff > 180.0f) {
                b -= 360.0f;
            } else if (diff < -180.0f) {
                b += 360.0f;
            }
        }

        return glm::lerp(a, b, t);
    };

    const auto lerp_t = m_lerp_camera_speed->value() * delta;

    if (should_lerp_pitch) {
        if (is_double) {
            view_rotation_double->pitch = lerp_angle((double)m_camera_lerp.last_rotation.x, (double)target_rotation.x, (double)lerp_t);
        } else {
            view_rotation->pitch = lerp_angle(m_camera_lerp.last_rotation.x, target_rotation.x, lerp_t);
        }
    }

    if (should_lerp_yaw) {
        if (is_double) {
            view_rotation_double->yaw = lerp_angle((double)m_camera_lerp.last_rotation.y, (double)target_rotation.y, (double)lerp_t);
        } else {
            view_rotation->yaw = lerp_angle(m_camera_lerp.last_rotation.y, target_rotation.y, lerp_t);
        }
    }

    if (should_lerp_roll) {
        if (is_double) {
            view_rotation_double->roll = lerp_angle((double)m_camera_lerp.last_rotation.z, (double)target_rotation.z, (double)lerp_t);
        } else {
            view_rotation->roll = lerp_angle(m_camera_lerp.last_rotation.z, target_rotation.z, lerp_t);
        }
    }

    if (is_double) {
        m_camera_lerp.last_rotation = glm::vec3{ (float)view_rotation_double->pitch, (float)view_rotation_double->yaw, (float)view_rotation_double->roll };
    } else {
        m_camera_lerp.last_rotation = glm::vec3{ view_rotation->pitch, view_rotation->yaw, view_rotation->roll };
    }

    m_last_lerp_update = std::chrono::high_resolution_clock::now();

    if (m_camera_freeze.position_wants_freeze) {
        if (is_double) {
            m_camera_freeze.position = glm::vec3{ (float)view_location_double->x, (float)view_location_double->y, (float)view_location_double->z };
        } else {
            m_camera_freeze.position = glm::vec3{ view_location->x, view_location->y, view_location->z };
        }

        m_camera_freeze.position_wants_freeze = false;
        m_camera_freeze.position_frozen = true;
    }

    if (m_camera_freeze.rotation_wants_freeze) {
        if (is_double) {
            m_camera_freeze.rotation = glm::vec3{ (float)view_rotation_double->pitch, (float)view_rotation_double->yaw, (float)view_rotation_double->roll };
        } else {
            m_camera_freeze.rotation = glm::vec3{ view_rotation->pitch, view_rotation->yaw, view_rotation->roll };
        }

        m_camera_freeze.rotation_wants_freeze = false;
        m_camera_freeze.rotation_frozen = true;
    }

    if (m_camera_freeze.position_frozen) {
        if (is_double) {
            view_location_double->x = m_camera_freeze.position.x;
            view_location_double->y = m_camera_freeze.position.y;
            view_location_double->z = m_camera_freeze.position.z;
        } else {
            view_location->x = m_camera_freeze.position.x;
            view_location->y = m_camera_freeze.position.y;
            view_location->z = m_camera_freeze.position.z;
        }
    }

    if (m_camera_freeze.rotation_frozen) {
        if (is_double) {
            view_rotation_double->pitch = m_camera_freeze.rotation.x;
            view_rotation_double->yaw = m_camera_freeze.rotation.y;
            view_rotation_double->roll = m_camera_freeze.rotation.z;
        } else {
            view_rotation->pitch = m_camera_freeze.rotation.x;
            view_rotation->yaw = m_camera_freeze.rotation.y;
            view_rotation->roll = m_camera_freeze.rotation.z;
        }
    }
}

void VR::on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas){
    ZoneScopedN(__FUNCTION__);

    if (m_custom_z_near_enabled->value()) {
        SPDLOG_INFO_ONCE("Attempting to set custom z near");
        sdk::globals::get_near_clipping_plane() = m_custom_z_near->value();
    }
}

void VR::update_hmd_state(bool from_view_extensions, uint32_t frame_count) {
    ZoneScopedN(__FUNCTION__);

    std::scoped_lock _{m_reinitialize_mtx};

    auto runtime = get_runtime();
    if (m_uncap_framerate->value()) {
        sdk::set_cvar_data_float(L"Engine", L"t.MaxFPS", 500.0f);
    }

    // Allows games running in HDR mode to not have a black UI overlay
    if (m_disable_hdr_compositing->value()) {
        sdk::set_cvar_data_int(L"SlateRHIRenderer", L"r.HDR.UI.CompositeMode", 0);
    }

    if (m_disable_blur_widgets->value()) {
        if (auto val = sdk::get_cvar_int(L"Slate", L"Slate.AllowBackgroundBlurWidgets"); val && *val != 0) {
            sdk::set_cvar_int(L"Slate", L"Slate.AllowBackgroundBlurWidgets", 0);
        }
    }

    if (!is_using_afr()) {
        const auto is_hzbo_frozen_by_cvm = m_cvar_manager != nullptr && m_cvar_manager->is_hzbo_frozen_and_enabled();

        // Forcefully disable r.HZBOcclusion, it doesn't work with native stereo mode (sometimes)
        // Except when the user sets it to 1 with the CVar Manager, we need to respect that
        if (m_disable_hzbocclusion->value() && !is_hzbo_frozen_by_cvm) {
            const auto r_hzb_occlusion_value = sdk::get_cvar_int(L"Renderer", L"r.HZBOcclusion");

            // Only set it once, otherwise we'll be spamming a Set call every frame
            if (r_hzb_occlusion_value && *r_hzb_occlusion_value != 0) {
                sdk::set_cvar_int(L"Renderer", L"r.HZBOcclusion", 0);
            }
        }

        if (m_disable_instance_culling->value()) {
            const auto r_instance_culling_value = sdk::get_cvar_int(L"Renderer", L"r.InstanceCulling.OcclusionCull");

            if (r_instance_culling_value && *r_instance_culling_value != 0) {
                sdk::set_cvar_int(L"Renderer", L"r.InstanceCulling.OcclusionCull", 0);
            }
        }
    }

    if (frame_count != 0 && is_using_afr() && frame_count % 2 == 0) {
        if (runtime->is_openxr()) {
            std::scoped_lock __{ m_openxr->sync_assignment_mtx };

            const auto last_frame = (frame_count - 1) % runtimes::OpenXR::QUEUE_SIZE;
            const auto now_frame = frame_count % runtimes::OpenXR::QUEUE_SIZE;
            m_openxr->pipeline_states[now_frame] = m_openxr->pipeline_states[last_frame];
            m_openxr->pipeline_states[now_frame].frame_count = now_frame;
        } else {
            const auto last_frame = (frame_count - 1) % m_openvr->pose_queue.size();
            const auto now_frame = frame_count % m_openvr->pose_queue.size();
            m_openvr->pose_queue[now_frame] = m_openvr->pose_queue[last_frame];
        }

        // Forcefully disable motion blur because it freaks out with AFR
        sdk::set_cvar_data_int(L"Engine", L"r.DefaultFeature.MotionBlur", 0);
        return;
    }
    
    runtime->update_poses(from_view_extensions, frame_count);

    // Update the poses used for the game
    // If we used the data directly from the WaitGetPoses call, we would have to lock a different mutex and wait a long time
    // This is because the WaitGetPoses call is blocking, and we don't want to block any game logic
    if (runtime->wants_reset_origin && runtime->ready() && runtime->got_first_valid_poses) {
        std::unique_lock _{ runtime->pose_mtx };
        set_rotation_offset(glm::identity<glm::quat>());
        m_standing_origin = get_position_unsafe(vr::k_unTrackedDeviceIndex_Hmd);

        runtime->wants_reset_origin = false;
    }

    runtime->update_matrices(m_nearz, m_farz);

    runtime->got_first_poses = true;
}

void VR::update_action_states() {
    ZoneScopedN(__FUNCTION__);

    std::scoped_lock _{m_actions_mtx};

    auto runtime = get_runtime();

    if (runtime == nullptr || runtime->wants_reinitialize) {
        return;
    }

    static bool once = true;

    if (once) {
        spdlog::info("VR: Updating action states");
        once = false;
    }


    if (runtime->is_openvr()) {
        const auto start_time = std::chrono::high_resolution_clock::now();

        auto error = vr::VRInput()->UpdateActionState(&m_active_action_set, sizeof(m_active_action_set), 1);

        if (error != vr::VRInputError_None) {
            spdlog::error("VRInput failed to update action state: {}", (uint32_t)error);
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto time_delta = end_time - start_time;

        m_last_input_delay = time_delta;
        m_avg_input_delay = (m_avg_input_delay + time_delta) / 2;

        if ((end_time - start_time) >= std::chrono::milliseconds(30)) {
            spdlog::warn("VRInput update action state took too long: {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

            //reinitialize_openvr();
            runtime->wants_reinitialize = true;
        }   
    } else {
        get_runtime()->update_input();
    }

    bool actively_using_controller = false;

    if (is_any_action_down()) {
        m_last_controller_update = std::chrono::steady_clock::now();
        actively_using_controller = true;
    }

    const auto last_xinput_update_is_late = std::chrono::steady_clock::now() - m_last_xinput_update >= std::chrono::seconds(2);
    const auto should_be_spoofing = (actively_using_controller || get_runtime()->handle_pause);

    if (m_spoofed_gamepad_connection && last_xinput_update_is_late && should_be_spoofing) {
        m_spoofed_gamepad_connection = false;
    }

    if (!m_spoofed_gamepad_connection && last_xinput_update_is_late && should_be_spoofing) {
        spdlog::info("[VR] Attempting to spoof gamepad connection");
        g_framework->post_message(WM_DEVICECHANGE, 0, 0);
        g_framework->activate_window();

        m_last_xinput_spoof_sent = std::chrono::steady_clock::now();
    }

    /*if (m_recenter_view_key->is_key_down_once()) {
        recenter_view();
    }

    if (m_set_standing_key->is_key_down_once()) {
        set_standing_origin(get_position(0));
    }*/

    static bool once2 = true;

    if (once2) {
        spdlog::info("VR: Updated action states");
        once2 = false;
    }

    update_dpad_gestures();
}

void VR::update_dpad_gestures() {
    if (!is_hmd_active()) {
        return;
    }

    const auto dpad_method = get_dpad_method();
    if (dpad_method != DPadMethod::GESTURE_HEAD && dpad_method != DPadMethod::GESTURE_HEAD_RIGHT) {
        return;
    }

    const auto wanted_index = dpad_method == DPadMethod::GESTURE_HEAD ? get_left_controller_index() : get_right_controller_index();

    const auto controller_pos = glm::vec3{get_position(wanted_index)};
    const auto hmd_transform = get_hmd_transform(m_frame_count);

    // Check if controller is near HMD
    const auto dist = glm::length(controller_pos - glm::vec3{hmd_transform[3]});

    if (dist > 0.2f) {
        return;
    }

    const auto dir_to_left = glm::normalize(controller_pos - glm::vec3{hmd_transform[3]});
    const auto hmd_dir = glm::quat{glm::extractMatrixRotation(hmd_transform)} * glm::vec3{0.0f, 0.0f, 1.0f};

    const auto angle = glm::acos(glm::dot(dir_to_left, hmd_dir));

    constexpr float threshold = glm::radians(120.0f);

    if (angle > threshold) {
        return;
    }

    // Make sure the angle is to the left/right of the HMD
    if (dpad_method == DPadMethod::GESTURE_HEAD_RIGHT) {
        if (glm::cross(dir_to_left, hmd_dir).y > 0.0f) {
            return;
        }
    } else if (glm::cross(dir_to_left, hmd_dir).y < 0.0f) {
        return;
    }

    // Send a vibration pulse to the controller
    const auto chosen_joystick = dpad_method == DPadMethod::GESTURE_HEAD ? m_left_joystick : m_right_joystick;
    trigger_haptic_vibration(0.0f, 0.1f, 1.0f, 5.0f, chosen_joystick);

    std::scoped_lock _{m_dpad_gesture_state.mtx};

    const auto left_joystick_axis = get_joystick_axis(chosen_joystick);

    if (left_joystick_axis.x < -0.5f) {
        m_dpad_gesture_state.direction |= DPadGestureState::Direction::LEFT;
    } else if (left_joystick_axis.x > 0.5f) {
        m_dpad_gesture_state.direction |= DPadGestureState::Direction::RIGHT;
    } 
    
    if (left_joystick_axis.y < -0.5f) {
        m_dpad_gesture_state.direction |= DPadGestureState::Direction::DOWN;
    } else if (left_joystick_axis.y > 0.5f) {
        m_dpad_gesture_state.direction |= DPadGestureState::Direction::UP;
    }
}

void VR::on_config_load(const utility::Config& cfg, bool set_defaults) {
    ZoneScopedN(__FUNCTION__);

    for (IModValue& option : m_options) {
        option.config_load(cfg, set_defaults);
    }

    if (get_runtime() != nullptr && get_runtime()->loaded) {
        get_runtime()->on_config_load(cfg, set_defaults);

        // Run the rest of OpenXR initialization code here that depends on config values
        if (m_first_config_load) {
            m_first_config_load = false; // because the frontend can request config reloads

            if (get_runtime()->is_openxr()) {
                spdlog::info("[VR] Finishing up OpenXR initialization");
                initialize_openxr_swapchains();
            }
        }
    }

    if (m_fake_stereo_hook != nullptr) {
        m_fake_stereo_hook->on_config_load(cfg, set_defaults);
    }

    m_overlay_component.on_config_load(cfg, set_defaults);

    if (m_cvar_manager != nullptr) {
        m_cvar_manager->on_config_load(cfg, set_defaults);   
    }

    // Load camera offsets
    load_cameras();
}

void VR::on_config_save(utility::Config& cfg) {
    ZoneScopedN(__FUNCTION__);

    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }

    if (m_fake_stereo_hook != nullptr) {
        m_fake_stereo_hook->on_config_save(cfg);
    }

    if (get_runtime()->loaded) {
        get_runtime()->on_config_save(cfg);
    }

    m_overlay_component.on_config_save(cfg);

    // Save camera offsets
    save_cameras();
}

void VR::load_cameras() try {
    ZoneScopedN(__FUNCTION__);

    const auto cameras_txt = Framework::get_persistent_dir("cameras.txt");

    if (std::filesystem::exists(cameras_txt)) {
        spdlog::info("[VR] Loading camera offsets from {}", cameras_txt.string());

        utility::Config cfg{cameras_txt.string()};

        for (auto i = 0; i < m_camera_datas.size(); i++) {
            auto& data = m_camera_datas[i];

            if (auto offs = cfg.get<float>(std::format("camera_right_offset{}", i))) {
                data.offset.x = *offs;
            }

            if (auto offs = cfg.get<float>(std::format("camera_up_offset{}", i))) {
                data.offset.y = *offs;
            }

            if (auto offs = cfg.get<float>(std::format("camera_forward_offset{}", i))) {
                data.offset.z = *offs;
            }

            if (auto scale = cfg.get<float>(std::format("world_scale{}", i))) {
                data.world_scale = *scale;
            }

            if (auto decoupled_pitch = cfg.get<bool>(std::format("decoupled_pitch{}", i))) {
                data.decoupled_pitch = *decoupled_pitch;
            }

            if (auto decoupled_pitch_ui_adjust = cfg.get<bool>(std::format("decoupled_pitch_ui_adjust{}", i))) {
                data.decoupled_pitch_ui_adjust = *decoupled_pitch_ui_adjust;
            }
        }
    }
} catch(...) {
    spdlog::error("[VR] Failed to load camera offsets");
}

void VR::load_camera(int index) {
    ZoneScopedN(__FUNCTION__);

    if (index < 0 || index >= m_camera_datas.size()) {
        return;
    }

    const auto& data = m_camera_datas[index];

    m_camera_right_offset->value() = data.offset.x;
    m_camera_up_offset->value() = data.offset.y;
    m_camera_forward_offset->value() = data.offset.z;
    m_world_scale->value() = data.world_scale;
    m_decoupled_pitch->value() = data.decoupled_pitch;
    m_decoupled_pitch_ui_adjust->value() = data.decoupled_pitch_ui_adjust;
}

void VR::save_camera(int index) {
    ZoneScopedN(__FUNCTION__);

    if (index < 0 || index >= m_camera_datas.size()) {
        return;
    }

    auto& data = m_camera_datas[index];

    data.offset = {
        m_camera_right_offset->value(),
        m_camera_up_offset->value(),
        m_camera_forward_offset->value()
    };

    data.world_scale = m_world_scale->value();
    data.decoupled_pitch = m_decoupled_pitch->value();
    data.decoupled_pitch_ui_adjust = m_decoupled_pitch_ui_adjust->value();

    save_cameras();
}

void VR::save_cameras() try {
    ZoneScopedN(__FUNCTION__);

    const auto cameras_txt = Framework::get_persistent_dir("cameras.txt");

    spdlog::info("[VR] Saving camera offsets to {}", cameras_txt.string());

    utility::Config cfg{cameras_txt.string()};

    for (auto i = 0; i < m_camera_datas.size(); i++) {
        const auto& data = m_camera_datas[i];
        cfg.set<float>(std::format("camera_right_offset{}", i), data.offset.x);
        cfg.set<float>(std::format("camera_up_offset{}", i), data.offset.y);
        cfg.set<float>(std::format("camera_forward_offset{}", i), data.offset.z);
        cfg.set<float>(std::format("world_scale{}", i), m_camera_datas[i].world_scale);
        cfg.set<bool>(std::format("decoupled_pitch{}", i), m_camera_datas[i].decoupled_pitch);
        cfg.set<bool>(std::format("decoupled_pitch_ui_adjust{}", i), m_camera_datas[i].decoupled_pitch_ui_adjust);
    }

    cfg.save(cameras_txt.string());
} catch(...) {
    spdlog::error("[VR] Failed to save camera offsets");
}


void VR::on_pre_imgui_frame() {
    ZoneScopedN(__FUNCTION__);

    m_xinput_context.update();

    if (!get_runtime()->ready()) {
        return;
    }

    if (!m_disable_overlay) {
        m_overlay_component.on_pre_imgui_frame();
    }
}

void VR::handle_keybinds() {
    ZoneScopedN(__FUNCTION__);

    if (m_keybind_recenter->is_key_down_once()) {
        recenter_view();
    }

    if (m_keybind_recenter_horizon->is_key_down_once()) {
        recenter_horizon();
    }

    if (m_keybind_load_camera_0->is_key_down_once()) {
        load_camera(0);
    }

    if (m_keybind_load_camera_1->is_key_down_once()) {
        load_camera(1);
    }

    if (m_keybind_load_camera_2->is_key_down_once()) {
        load_camera(2);
    }

    if (m_keybind_set_standing_origin->is_key_down_once()) {
        m_standing_origin = get_position(0);
    }

    if (m_keybind_toggle_2d_screen->is_key_down_once()) {
        m_2d_screen_mode->toggle();
    }

    if (m_keybind_disable_vr->is_key_down_once()) {
        m_disable_vr = !m_disable_vr; // definitely should not be persistent
    }

    // The Slate UI
    if (m_keybind_toggle_gui->is_key_down_once()) {
        m_enable_gui->toggle();
    }
}

void VR::on_frame() {
    ZoneScopedN(__FUNCTION__);

    m_cvar_manager->on_frame();
    handle_keybinds();

    if (!get_runtime()->ready()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto is_allowed_draw_window = now - m_last_xinput_update < std::chrono::seconds(2);

    if (!is_allowed_draw_window) {
        m_rt_modifier.draw = false;
    }

    if (is_allowed_draw_window && m_xinput_context.headlocked_begin_held && !FrameworkConfig::get()->is_l3_r3_long_press()) {
        const auto rt_size = g_framework->get_rt_size();

        ImGui::Begin("AimMethod Notification", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);

        ImGui::Text("Continue holding down L3 + R3 to toggle aim method");

        if (std::chrono::steady_clock::now() - m_xinput_context.headlocked_begin >= std::chrono::seconds(1)) {
            if (m_aim_method->value() == VR::AimMethod::GAME) {
                m_aim_method->value() = m_previous_aim_method;
            } else {
                m_aim_method->value() = VR::AimMethod::GAME; // turns it off
            }

            m_xinput_context.headlocked_begin_held = false;
        } else {
            if (m_aim_method->value() != VR::AimMethod::GAME) {
                m_previous_aim_method = (VR::AimMethod)m_aim_method->value();
            } else if (m_previous_aim_method == VR::AimMethod::GAME) {
                m_previous_aim_method = VR::AimMethod::HEAD; // so it will at least be something
            }
        }

        const auto window_size = ImGui::GetWindowSize();

        const auto centered_x = (rt_size.x / 2) - (window_size.x / 2);
        const auto centered_y = (rt_size.y / 2) - (window_size.y / 2);
        ImGui::SetWindowPos(ImVec2(centered_x, centered_y), ImGuiCond_Always);

        ImGui::End();
    }

    if (m_rt_modifier.draw) {
        const auto rt_size = g_framework->get_rt_size();

        ImGui::Begin("RT Modifier Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + Left Stick: Camera left/right/forward/back");
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + Right Stick: Camera up/down");
        
        ImGui::Text("Page: %d", m_rt_modifier.page + 1);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "DPad Left: Previous page | DPad Right: Next page");

        switch (m_rt_modifier.page) {
        case 2:
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + B: Save Camera 2");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + Y: Save Camera 1");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + X: Save Camera 0");
            break;

        case 1:
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + B: Load Camera 2");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + Y: Load Camera 1");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + X: Load Camera 0");
            break;

        case 0:
        default:
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + B: Reset camera offset");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + Y: Recenter view");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "RT + X: Reset standing origin");
            m_rt_modifier.page = 0;
            break;
        }

        const auto window_size = ImGui::GetWindowSize();

        const auto centered_x = (rt_size.x / 2) - (window_size.x / 2);
        const auto centered_y = (rt_size.y / 2) - (window_size.y / 2);
        ImGui::SetWindowPos(ImVec2(centered_x, centered_y), ImGuiCond_Always);
        ImGui::End();
    }
}

void VR::on_present() {
    ZoneScopedN(__FUNCTION__);

    m_present_thread_id = GetCurrentThreadId();

    utility::ScopeGuard _guard {[&]() {
        if (!is_using_afr() || (m_render_frame_count + 1) % 2 == m_left_eye_interval) {
            SetEvent(m_present_finished_event);
        }

        m_last_frame_count = m_render_frame_count;
    }};

    m_frame_count = get_runtime()->internal_render_frame_count;

    if (!is_using_afr() || m_render_frame_count % 2 == m_left_eye_interval) {
        ResetEvent(m_present_finished_event);
    }

    auto runtime = get_runtime();

    if (!runtime->loaded) {
        m_fake_stereo_hook->on_frame(); // Just let all the hooks engage, whatever.
        return;
    }

    runtime->consume_events(nullptr);

    m_fake_stereo_hook->on_frame();

    auto openvr = get_runtime<runtimes::OpenVR>();

    if (runtime->is_openvr()) {
        if (openvr->got_first_poses) {
            const auto hmd_activity = openvr->hmd->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd);
            auto hmd_active = hmd_activity == vr::k_EDeviceActivityLevel_UserInteraction || hmd_activity == vr::k_EDeviceActivityLevel_UserInteraction_Timeout;

            if (hmd_active) {
                openvr->last_hmd_active_time = std::chrono::system_clock::now();
            }

            const auto now = std::chrono::system_clock::now();

            if (now - openvr->last_hmd_active_time <= std::chrono::seconds(5)) {
                hmd_active = true;
            }

            openvr->is_hmd_active = hmd_active;

            // upon headset re-entry, reinitialize OpenVR
            if (openvr->is_hmd_active && !openvr->was_hmd_active) {
                openvr->wants_reinitialize = true;
            }

            openvr->was_hmd_active = openvr->is_hmd_active;

            if (!is_hmd_active()) {
                return;
            }
        } else {
            openvr->is_hmd_active = true; // We need to force out an initial WaitGetPoses call
            openvr->was_hmd_active = true;
        }
    }

    // attempt to fix crash when reinitializing openvr
    std::scoped_lock _{m_openvr_mtx};
    m_submitted = false;

    const auto renderer = g_framework->get_renderer_type();
    vr::EVRCompositorError e = vr::EVRCompositorError::VRCompositorError_None;

    const auto is_left_eye_frame = is_using_afr() ? (m_render_frame_count % 2 == m_left_eye_interval) : true;

    if (is_left_eye_frame && get_synchronize_stage() == VR::SynchronizeStage::LATE) {
        const auto had_sync = runtime->got_first_sync;
        runtime->synchronize_frame();

        if (!runtime->got_first_poses || !had_sync) {
            update_hmd_state();
        }
    }

    if (renderer == Framework::RendererType::D3D11) {
        // if we don't do this then D3D11 OpenXR freezes for some reason.
        if (!runtime->got_first_sync) {
            SPDLOG_INFO_EVERY_N_SEC(1, "Attempting to sync!");
            if (get_synchronize_stage() == VR::SynchronizeStage::LATE) {
                runtime->synchronize_frame();
            }

            update_hmd_state();
        }

        m_is_d3d12 = false;
        e = m_d3d11.on_frame(this);
    } else if (renderer == Framework::RendererType::D3D12) {
        m_is_d3d12 = true;
        e = m_d3d12.on_frame(this);
    }

    // force a waitgetposes call to fix this...
    if (e == vr::EVRCompositorError::VRCompositorError_AlreadySubmitted && runtime->is_openvr()) {
        openvr->got_first_poses = false;
        openvr->needs_pose_update = true;
    }

    if (m_submitted) {
        if (m_submitted) {
            if (!m_disable_overlay) {
                m_overlay_component.on_post_compositor_submit();
            }

            if (runtime->is_openvr()) {
                //vr::VRCompositor()->SetExplicitTimingMode(vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
                //vr::VRCompositor()->PostPresentHandoff();
            }
        }

        //runtime->needs_pose_update = true;
        m_submitted = false;

        // On the first ever submit, we need to activate the window and set the mouse to the center
        // so the user doesn't have to click on the window to get input.
        if (m_first_submit) {
            m_first_submit = false;

            // for some reason this doesn't work if called directly from here
            // so we have to do it in a separate thread
            std::thread worker([]() {
                g_framework->activate_window();
                g_framework->set_mouse_to_center();
                spdlog::info("Finished first submit from worker thread!");
            });
            worker.detach();
        }
    }
}

void VR::on_post_present() {
    FrameMarkNamed("Present");
    ZoneScopedN(__FUNCTION__);

    const auto is_same_frame = m_render_frame_count > 0 && m_render_frame_count == m_frame_count;

    m_render_frame_count = m_frame_count;

    auto runtime = get_runtime();

    if (!get_runtime()->loaded) {
        return;
    }

    std::scoped_lock _{m_openvr_mtx};

    if (!m_is_d3d12) {
        m_d3d11.on_post_present(this);
    } else {
        m_d3d12.on_post_present(this);
    }

    detect_controllers();

    const auto is_left_eye_frame = is_using_afr() ? (is_same_frame || (m_render_frame_count % 2 == m_left_eye_interval)) : true;

    if (is_left_eye_frame) {
        if (get_synchronize_stage() == VR::SynchronizeStage::VERY_LATE || !runtime->got_first_sync) {
            const auto had_sync = runtime->got_first_sync;
            runtime->synchronize_frame();

            if (!runtime->got_first_poses || !had_sync) {
                update_hmd_state();
            }
        }

        if (runtime->is_openxr() && runtime->ready() && get_synchronize_stage() > VR::SynchronizeStage::EARLY) {
            if (!m_openxr->frame_began) {
                m_openxr->begin_frame();
            }
        }
    }

    if (runtime->wants_reinitialize) {
        std::scoped_lock _{m_reinitialize_mtx};

        if (runtime->is_openvr()) {
            m_openvr->wants_reinitialize = false;
            reinitialize_openvr();
        } else if (runtime->is_openxr()) {
            m_openxr->wants_reinitialize = false;
            reinitialize_openxr();
        }
    }
}

uint32_t VR::get_hmd_width() const {
    if (m_2d_screen_mode->value()) {
        if (get_runtime()->is_openxr()) {
            return g_framework->get_rt_size().x * m_openxr->resolution_scale->value();
        }

        return g_framework->get_rt_size().x;
    }

    if (m_extreme_compat_mode->value()) {
        return g_framework->get_rt_size().x;
    }

    return std::max<uint32_t>(get_runtime()->get_width(), 128);
}

uint32_t VR::get_hmd_height() const {
    if (m_2d_screen_mode->value()) {
        if (get_runtime()->is_openxr()) {
            return g_framework->get_rt_size().y * m_openxr->resolution_scale->value();
        }

        return g_framework->get_rt_size().y;
    }

    if (m_extreme_compat_mode->value()) {
        return g_framework->get_rt_size().y;
    }

    return std::max<uint32_t>(get_runtime()->get_height(), 128);
}

void VR::on_draw_sidebar_entry(std::string_view name) {
    const auto hash = utility::hash(name.data());

    // Draw the ui thats always drawn first.
    on_draw_ui();

    /*const auto made_child = ImGui::BeginChild("VRChild", ImVec2(0, 0), true, ImGuiWindowFlags_::ImGuiWindowFlags_NavFlattened);

    utility::ScopeGuard sg([made_child]() {
        if (made_child) {
            ImGui::EndChild();
        }
    });*/

    enum SelectedPage {
        PAGE_RUNTIME,
        PAGE_UNREAL,
        PAGE_INPUT,
        PAGE_CAMERA,
        PAGE_KEYBINDS,
        PAGE_CONSOLE,
        PAGE_COMPATIBILITY,
        PAGE_DEBUG,
    };

    SelectedPage selected_page = PAGE_RUNTIME;

    /*ImGui::BeginTable("VRTable", 2, ImGuiTableFlags_::ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_::ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("LeftPane", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("RightPane", ImGuiTableColumnFlags_WidthStretch);

    // Draw left pane
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); // Set to the first column

        ImGui::BeginGroup();

        auto dcs = [&](const char* label, SelectedPage page_value) -> bool {
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
            utility::ScopeGuard sg3([]() {
                ImGui::PopStyleVar();
            });
            if (ImGui::Selectable(label, selected_page == page_value)) {
                selected_page = page_value;
                return true;
            }
            return false;
        };

        dcs("Runtime", PAGE_RUNTIME);
        dcs("Unreal", PAGE_UNREAL);
        dcs("Input", PAGE_INPUT);
        dcs("Camera", PAGE_CAMERA);
        dcs("Console/CVars", PAGE_CONSOLE);
        dcs("Compatibility", PAGE_COMPATIBILITY);
        dcs("Debug", PAGE_DEBUG);

        ImGui::EndGroup();
    }

    ImGui::TableNextColumn(); // Move to the next column (right)
    ImGui::BeginGroup();*/

    switch (hash) {
    case "Runtime"_fnv:
        selected_page = PAGE_RUNTIME;
        break;
    case "Unreal"_fnv:
        selected_page = PAGE_UNREAL;
        break;
    case "Input"_fnv:
        selected_page = PAGE_INPUT;
        break;
    case "Camera"_fnv:
        selected_page = PAGE_CAMERA;
        break;
    case "Keybinds"_fnv:
        selected_page = PAGE_KEYBINDS;
        break;
    case "Console/CVars"_fnv:
        selected_page = PAGE_CONSOLE;
        break;
    case "Compatibility"_fnv:
        selected_page = PAGE_COMPATIBILITY;
        break;
    case "Debug"_fnv:
        selected_page = PAGE_DEBUG;
        break;
    default:
        ImGui::Text("Unknown page selected");
        break;
    }

    if (selected_page == PAGE_RUNTIME) {
        if (m_has_hw_scheduling) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::TextWrapped("WARNING: Hardware-accelerated GPU scheduling is enabled. This may cause the game to run slower.");
            ImGui::TextWrapped("Go into your Windows Graphics settings and disable \"Hardware-accelerated GPU scheduling\"");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Note: This is only necessary if you are experiencing performance issues.");
        }

        if (GetModuleHandleW(L"nvngx_dlssg.dll") != nullptr) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::TextWrapped("WARNING: DLSS Frame Generation has been detected. Make sure it is disabled within in-game settings.");
            ImGui::PopStyleColor();
        }

        ImGui::Text((std::string{"Runtime Information ("} + get_runtime()->name().data() + ")").c_str());

        m_desktop_fix->draw("Desktop Spectator View");
        ImGui::SameLine();
        m_2d_screen_mode->draw("2D Screen Mode");

        ImGui::TextWrapped("Render Resolution (per-eye): %d x %d", get_runtime()->get_width(), get_runtime()->get_height());
        ImGui::TextWrapped("Total Render Resolution: %d x %d", get_runtime()->get_width() * 2, get_runtime()->get_height());

        if (get_runtime()->is_openvr()) {
            ImGui::TextWrapped("Resolution can be changed in SteamVR");
        }

        get_runtime()->on_draw_ui();

        m_overlay_component.on_draw_ui();

        ImGui::TreePop();
    }

    if (selected_page == PAGE_UNREAL) {
        m_rendering_method->draw("Rendering Method");
        m_synced_afr_method->draw("Synced Sequential Method");

        m_world_scale->draw("World Scale");
        m_depth_scale->draw("Depth Scale");

        m_disable_hzbocclusion->draw("Disable HZBOcclusion");
        m_disable_instance_culling->draw("Disable Instance Culling");
        m_disable_hdr_compositing->draw("Disable HDR Composition");
        m_disable_blur_widgets->draw("Disable Blur Widgets");
        m_uncap_framerate->draw("Uncap Framerate");
        m_enable_gui->draw("Enable GUI");
        m_enable_depth->draw("Enable Depth-based Latency Reduction");
        m_load_blueprint_code->draw("Load Blueprint Code");
        m_ghosting_fix->draw("Ghosting Fix");

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Native Stereo Fix")) {
            m_native_stereo_fix->draw("Enabled");
            m_native_stereo_fix_same_pass->draw("Use Same Stereo Pass");
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Near Clip Plane")) {
            m_custom_z_near_enabled->draw("Enable");

            if (m_custom_z_near_enabled->value()) {
                m_custom_z_near->draw("Value");

                if (m_custom_z_near->value() <= 0.0f) {
                    m_custom_z_near->value() = 0.01f;
                }
            }

            ImGui::TreePop();
        }
    }

    if (selected_page == PAGE_INPUT) {
        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Controller")) {
            m_joystick_deadzone->draw("VR Joystick Deadzone");
            m_controller_pitch_offset->draw("Controller Pitch Offset");

            m_dpad_shifting->draw("DPad Shifting");
            ImGui::SameLine();
            m_swap_controllers->draw("Left-handed Controller Inputs");
            m_dpad_shifting_method->draw("DPad Shifting Method");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Aim Method")) {
            ImGui::TextWrapped("Some games may not work with this enabled.");
            if (m_aim_method->draw("Type")) {
                m_previous_aim_method = (AimMethod)m_aim_method->value();
            }

            m_aim_speed->draw("Speed");
            m_aim_interp->draw("Smoothing");

            m_aim_modify_player_control_rotation->draw("Modify Player Control Rotation");
            ImGui::SameLine();
            m_aim_use_pawn_control_rotation->draw("Use Pawn Control Rotation");

            m_aim_multiplayer_support->draw("Multiplayer Support");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Snap Turn")) {
            m_snapturn->draw("Enabled");
            m_snapturn_angle->draw("Angle");
            m_snapturn_joystick_deadzone->draw("Deadzone");
        
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Movement Orientation")) {
            m_movement_orientation->draw("Type");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Roomscale Movement")) {
            m_roomscale_movement->draw("Enabled");

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, headset movement will affect the movement of the player character.");
            }

            ImGui::SameLine();
            m_roomscale_sweep->draw("Sweep Movement");
            // Draw description of option
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, roomscale movement will use a sweep to prevent the player from moving through walls.\nThis also allows physics objects to interact with the player, like doors.");
            }

            ImGui::TreePop();
        }
    }

    if (selected_page == PAGE_CAMERA) {
        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Camera Freeze")) {
            float camera_offset[] = {m_camera_forward_offset->value(), m_camera_right_offset->value(), m_camera_up_offset->value()};
            if (ImGui::SliderFloat3("Camera Offset", camera_offset, -4000.0f, 4000.0f)) {
                m_camera_forward_offset->value() = camera_offset[0];
                m_camera_right_offset->value() = camera_offset[1];
                m_camera_up_offset->value() = camera_offset[2];
            }

            for (auto i = 0; i < m_camera_datas.size(); ++i) {
                auto& data = m_camera_datas[i];

                if (ImGui::Button(std::format("Save Camera {}", i).data())) {
                    save_camera(i);
                }

                ImGui::SameLine();

                if (ImGui::Button(std::format("Load Camera {}", i).data())) {
                    load_camera(i);
                }
            }

            bool pos_freeze = m_camera_freeze.position_frozen || m_camera_freeze.position_wants_freeze;
            if (ImGui::Checkbox("Freeze Position", &pos_freeze)) {
                if (pos_freeze) {
                    m_camera_freeze.position_wants_freeze = true;
                } else {
                    m_camera_freeze.position_frozen = false;
                }
            }

            ImGui::SameLine();
            bool rot_freeze = m_camera_freeze.rotation_frozen || m_camera_freeze.rotation_wants_freeze;
            if (ImGui::Checkbox("Freeze Rotation", &rot_freeze)) {
                if (rot_freeze) {
                    m_camera_freeze.rotation_wants_freeze = true;
                } else {
                    m_camera_freeze.rotation_frozen = false;
                }
            }

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Camera Lerp")) {
            m_lerp_camera_pitch->draw("Lerp Pitch");
            ImGui::SameLine();
            m_lerp_camera_yaw->draw("Lerp Yaw");
            ImGui::SameLine();
            m_lerp_camera_roll->draw("Lerp Roll");
            m_lerp_camera_speed->draw("Lerp Speed");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Decoupled Pitch")) {
            m_decoupled_pitch->draw("Enabled");
            m_decoupled_pitch_ui_adjust->draw("Auto Adjust UI");

            ImGui::TreePop();
        }
    }

    if (selected_page == PAGE_KEYBINDS) {
        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Playspace Keys")) {
            m_keybind_recenter->draw("Recenter View Key");
            m_keybind_recenter_horizon->draw("Recenter Horizon Key");
            m_keybind_set_standing_origin->draw("Set Standing Origin Key");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Camera Keys")) {
            m_keybind_load_camera_0->draw("Load Camera 0 Key");
            m_keybind_load_camera_1->draw("Load Camera 1 Key");
            m_keybind_load_camera_2->draw("Load Camera 2 Key");

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Overlay/Runtime Keys")) {
            m_keybind_toggle_2d_screen->draw("Toggle 2D Screen Mode Key");
            m_keybind_toggle_gui->draw("Toggle In-Game UI Key");
            m_keybind_disable_vr->draw("Disable VR Key");

            ImGui::TreePop();
        }
    }

    if (selected_page == PAGE_CONSOLE) {
        m_cvar_manager->on_draw_ui();
    }

    if (selected_page == PAGE_COMPATIBILITY) {
        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Compatibility Options")) {
            m_compatibility_ahud->draw("AHUD UI Compatibility");
            m_compatibility_skip_uobjectarray_init->draw("Skip UObjectArray Init");
            m_compatibility_skip_pip->draw("Skip PostInitProperties");
            m_sceneview_compatibility_mode->draw("SceneView Compatibility Mode");
            m_extreme_compat_mode->draw("Extreme Compatibility Mode");

            // changes to any of these options should trigger a regeneration of the eye projection matrices
            const auto horizontal_projection_changed = m_horizontal_projection_override->draw("Horizontal Projection");
            const auto vertical_projection_changed = m_vertical_projection_override->draw("Vertical Projection");
            const auto scale_render = m_grow_rectangle_for_projection_cropping->draw("Scale Render Target");
            const auto scale_render_changed = get_runtime()->is_modifying_eye_texture_scale != scale_render;
            get_runtime()->is_modifying_eye_texture_scale = scale_render;
            get_runtime()->should_recalculate_eye_projections = horizontal_projection_changed || vertical_projection_changed || scale_render_changed;

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Splitscreen Compatibility")) {
            m_splitscreen_compatibility_mode->draw("Enabled");
            m_splitscreen_view_index->draw("Index");
            ImGui::TreePop();
        }
    }
    
    if (selected_page == PAGE_DEBUG) {
        if (m_fake_stereo_hook != nullptr) {
            m_fake_stereo_hook->on_draw_ui();
        }

        //ImGui::Combo("Sync Mode", (int*)&get_runtime()->custom_stage, "Early\0Late\0Very Late\0");
        m_sync_mode->draw("Sync Mode");
        ImGui::DragFloat4("Right Bounds", (float*)&m_right_bounds, 0.005f, -2.0f, 2.0f);
        ImGui::DragFloat4("Left Bounds", (float*)&m_left_bounds, 0.005f, -2.0f, 2.0f);
        ImGui::Checkbox("Disable Projection Matrix Override", &m_disable_projection_matrix_override);
        ImGui::Checkbox("Disable View Matrix Override", &m_disable_view_matrix_override);
        ImGui::Checkbox("Disable Backbuffer Size Override", &m_disable_backbuffer_size_override);
        ImGui::Checkbox("Disable VR Overlay", &m_disable_overlay);
        ImGui::Checkbox("Disable VR Entirely", &m_disable_vr);
        ImGui::Checkbox("Stereo Emulation Mode", &m_stereo_emulation_mode);
        ImGui::Checkbox("Wait for Present", &m_wait_for_present);
        m_controllers_allowed->draw("Controllers allowed");
        ImGui::Checkbox("Controller test mode", &m_controller_test_mode);
        m_show_fps->draw("Show FPS");
        m_show_statistics->draw("Show Engine Statistics");

        const double min_ = 0.0;
        const double max_ = 25.0;
        ImGui::SliderScalar("Prediction Scale", ImGuiDataType_Double, &m_openxr->prediction_scale, &min_, &max_);

        ImGui::DragFloat4("Raw Left", (float*)&m_raw_projections[0], 0.01f, -100.0f, 100.0f);
        ImGui::DragFloat4("Raw Right", (float*)&m_raw_projections[1], 0.01f, -100.0f, 100.0f);

        const auto left_stick_axis = get_left_stick_axis();
        const auto right_stick_axis = get_right_stick_axis();

        ImGui::DragFloat2("Left Stick", (float*)&left_stick_axis, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat2("Right Stick", (float*)&right_stick_axis, 0.01f, -1.0f, 1.0f);

        ImGui::TextWrapped("Hardware scheduling: %s", m_has_hw_scheduling ? "Enabled" : "Disabled");
    }

    ImGui::EndGroup();
    //ImGui::EndTable();
}

void VR::on_draw_ui() {
    ZoneScopedN(__FUNCTION__);

    // create VR tree entry in menu (imgui)
    ImGui::PushID("VR");
    ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
    if (!m_fake_stereo_hook->has_attempted_to_hook_engine() || !m_fake_stereo_hook->has_attempted_to_hook_slate()) {
        std::string adjusted_name = get_name().data();
        adjusted_name += " (Loading...)";

        /*if (!ImGui::CollapsingHeader(adjusted_name.data())) {
            ImGui::PopID();
            return;
        }*/

        ImGui::TextWrapped("Loading...");
    } else {
        /*if (!ImGui::CollapsingHeader(get_name().data())) {
            ImGui::PopID();
            return;
        }*/
    }
    ImGui::PopID();

    auto display_error = [](auto& runtime, std::string dll_name) {
        if (runtime == nullptr || !runtime->error && runtime->loaded) {
            return;
        }

        if (runtime->error && runtime->dll_missing) {
            ImGui::TextWrapped("%s not loaded: %s not found", runtime->name().data(), dll_name.data());
            ImGui::TextWrapped("Please select %s from the loader if you want to use %s", runtime->name().data(), runtime->name().data());
        } else if (runtime->error) {
            ImGui::TextWrapped("%s not loaded: %s", runtime->name().data(), runtime->error->c_str());
        } else {
            ImGui::TextWrapped("%s not loaded: Unknown error", runtime->name().data());
        }

        ImGui::Separator();
    };

    if (!get_runtime()->loaded || get_runtime()->error) {
        display_error(m_openxr, "openxr_loader.dll");
        display_error(m_openvr, "openvr_api.dll");
    }

    if (!get_runtime()->loaded) {
        ImGui::TextWrapped("No runtime loaded.");

        if (ImGui::Button("Attempt to reinitialize")) {
            clean_initialize();
        }

        return;
    }

    if (ImGui::Button("Set Standing Height")) {
        m_standing_origin.y = get_position(0).y;
    }

    ImGui::SameLine();

    if (ImGui::Button("Set Standing Origin")) {
        m_standing_origin = get_position(0);
    }

    ImGui::SameLine();

    if (ImGui::Button("Recenter View")) {
        recenter_view();
    }

    ImGui::SameLine();

    if (ImGui::Button("Recenter Horizon")) {
        recenter_horizon();
    }

    if (ImGui::Button("Reinitialize Runtime")) {
        get_runtime()->wants_reinitialize = true;
    }
}

Vector4f VR::get_position(uint32_t index, bool grip) const {
    return get_transform(index, grip)[3];
}

Vector4f VR::get_velocity(uint32_t index) const {
    if (index >= vr::k_unMaxTrackedDeviceCount) {
        return Vector4f{};
    }

    std::shared_lock _{ get_runtime()->pose_mtx };

    return get_velocity_unsafe(index);
}

Vector4f VR::get_angular_velocity(uint32_t index) const {
    if (index >= vr::k_unMaxTrackedDeviceCount) {
        return Vector4f{};
    }

    std::shared_lock _{ get_runtime()->pose_mtx };

    return get_angular_velocity_unsafe(index);
}

Vector4f VR::get_position_unsafe(uint32_t index) const {
    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return Vector4f{};
        }

        if (index == vr::k_unTrackedDeviceIndex_Hmd) {
            const auto pose = m_openvr->get_current_hmd_pose();
            auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose };
            auto result = glm::rowMajor4(matrix)[3];
            result.w = 1.0f;

            return result;
        }

        if (index == get_left_controller_index()) {
            return m_openvr->grip_matrices[VRRuntime::Hand::LEFT][3];
        }

        if (index == get_right_controller_index()) {
            return m_openvr->grip_matrices[VRRuntime::Hand::RIGHT][3];
        }

        auto& pose = get_openvr_poses()[index];
        auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking };
        auto result = glm::rowMajor4(matrix)[3];
        result.w = 1.0f;

        return result;
    } else if (get_runtime()->is_openxr()) {
        if (index >= 3) {
            return Vector4f{};
        }

        // HMD position
        if (index == 0 && !m_openxr->stage_views.empty()) {
            const auto vspl = m_openxr->get_current_view_space_location();
            return Vector4f{ *(Vector3f*)&vspl.pose.position, 1.0f };
        } else if (index > 0) {
            if (index == get_left_controller_index()) {
                return m_openxr->grip_matrices[VRRuntime::Hand::LEFT][3];
            } else if (index == get_right_controller_index()) {
                return m_openxr->grip_matrices[VRRuntime::Hand::RIGHT][3];
            }
        }

        return Vector4f{};
    } 

    return Vector4f{};
}

Vector4f VR::get_velocity_unsafe(uint32_t index) const {
    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return Vector4f{};
        }

        const auto& pose = get_openvr_poses()[index];
        const auto& velocity = pose.vVelocity;

        return Vector4f{ velocity.v[0], velocity.v[1], velocity.v[2], 0.0f };
    } else if (get_runtime()->is_openxr()) {
        if (index >= 3) {
            return Vector4f{};
        }

        // todo: implement HMD velocity
        if (index == 0) {
            return Vector4f{};
        }

        return Vector4f{ *(Vector3f*)&m_openxr->hands[index-1].grip_velocity.linearVelocity, 0.0f };
    }

    return Vector4f{};
}

Vector4f VR::get_angular_velocity_unsafe(uint32_t index) const {
    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return Vector4f{};
        }

        const auto& pose = get_openvr_poses()[index];
        const auto& angular_velocity = pose.vAngularVelocity;

        return Vector4f{ angular_velocity.v[0], angular_velocity.v[1], angular_velocity.v[2], 0.0f };
    } else if (get_runtime()->is_openxr()) {
        if (index >= 3) {
            return Vector4f{};
        }

        // todo: implement HMD velocity
        if (index == 0) {
            return Vector4f{};
        }
    
        return Vector4f{ *(Vector3f*)&m_openxr->hands[index-1].grip_velocity.angularVelocity, 0.0f };
    }

    return Vector4f{};
}

Matrix4x4f VR::get_hmd_rotation(uint32_t frame_count) const {
    return glm::extractMatrixRotation(get_hmd_transform(frame_count));
}

Matrix4x4f VR::get_hmd_transform(uint32_t frame_count) const {
    ZoneScopedN(__FUNCTION__);

    if (get_runtime()->is_openvr()) {
        std::shared_lock _{ get_runtime()->pose_mtx };

        const auto pose = m_openvr->get_hmd_pose(frame_count);
        const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose };
        return glm::rowMajor4(matrix);
    } else if (get_runtime()->is_openxr()) {
        std::shared_lock __{ get_runtime()->eyes_mtx };

        const auto vspl = m_openxr->get_view_space_location(frame_count);
        auto mat = Matrix4x4f{runtimes::OpenXR::to_glm(vspl.pose.orientation)};
        mat[3] = Vector4f{*(Vector3f*)&vspl.pose.position, 1.0f};

        return mat;
    }

    return glm::identity<Matrix4x4f>();
}

Matrix4x4f VR::get_rotation(uint32_t index, bool grip) const {
    return glm::extractMatrixRotation(get_transform(index, grip));
}

Matrix4x4f VR::get_transform(uint32_t index, bool grip) const {
    ZoneScopedN(__FUNCTION__);

    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return glm::identity<Matrix4x4f>();
        }

        std::shared_lock _{ get_runtime()->pose_mtx };

        if (index == vr::k_unTrackedDeviceIndex_Hmd) {
            const auto pose = m_openvr->get_current_hmd_pose();
            const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose };
            return glm::rowMajor4(matrix);
        }

        if (index == get_left_controller_index()) {
            return grip ? m_openvr->grip_matrices[VRRuntime::Hand::LEFT] : m_openvr->aim_matrices[VRRuntime::Hand::LEFT];
        } else if (index == get_right_controller_index()) {
            return grip ? m_openvr->grip_matrices[VRRuntime::Hand::RIGHT] : m_openvr->aim_matrices[VRRuntime::Hand::RIGHT];
        }

        const auto& pose = get_openvr_poses()[index];
        const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking };
        return glm::rowMajor4(matrix);
    } else if (get_runtime()->is_openxr()) {
        // HMD rotation
        if (index == 0 && !m_openxr->stage_views.empty()) {
            const auto vspl = m_openxr->get_current_view_space_location();
            auto mat = Matrix4x4f{runtimes::OpenXR::to_glm(vspl.pose.orientation)};
            mat[3] = Vector4f{*(Vector3f*)&vspl.pose.position, 1.0f};
            return mat;
        } else if (index > 0) {
            if (index == get_left_controller_index()) {
                return grip ? m_openxr->grip_matrices[VRRuntime::Hand::LEFT] : m_openxr->aim_matrices[VRRuntime::Hand::LEFT];
            } else if (index == get_right_controller_index()) {
                return grip ? m_openxr->grip_matrices[VRRuntime::Hand::RIGHT] : m_openxr->aim_matrices[VRRuntime::Hand::RIGHT];
            }
        }
    }

    return glm::identity<Matrix4x4f>();
}

Matrix4x4f VR::get_grip_transform(uint32_t index) const {
    return get_transform(index);
}

Matrix4x4f VR::get_aim_transform(uint32_t index) const {
    return get_transform(index, false);
}

vr::HmdMatrix34_t VR::get_raw_transform(uint32_t index) const {
    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return vr::HmdMatrix34_t{};
        }

        std::shared_lock _{ get_runtime()->pose_mtx };

        if (index == vr::k_unTrackedDeviceIndex_Hmd) {
            return m_openvr->get_current_hmd_pose();
        }

        auto& pose = get_openvr_poses()[index];
        return pose.mDeviceToAbsoluteTracking;
    } else {
        spdlog::error("VR: get_raw_transform: not implemented for {}", get_runtime()->name());
        return vr::HmdMatrix34_t{};
    }
}

Vector4f VR::get_eye_offset(VRRuntime::Eye eye) const {
    ZoneScopedN(__FUNCTION__);

    if (!is_hmd_active()) {
        return Vector4f{};
    }

    std::shared_lock _{ get_runtime()->eyes_mtx };

    if (eye == VRRuntime::Eye::LEFT) {
        return get_runtime()->eyes[vr::Eye_Left][3];
    }
    
    return get_runtime()->eyes[vr::Eye_Right][3];
}

Vector4f VR::get_current_offset() {
    if (!is_hmd_active()) {
        return Vector4f{};
    }

    std::shared_lock _{ get_runtime()->eyes_mtx };

    if (m_frame_count % 2 == m_left_eye_interval) {
        //return Vector4f{m_eye_distance * -1.0f, 0.0f, 0.0f, 0.0f};
        return get_runtime()->eyes[vr::Eye_Left][3];
    }
    
    return get_runtime()->eyes[vr::Eye_Right][3];
    //return Vector4f{m_eye_distance, 0.0f, 0.0f, 0.0f};
}

Matrix4x4f VR::get_eye_transform(uint32_t index) {
    ZoneScopedN(__FUNCTION__);

    if (!is_hmd_active() || index > 2) {
        return glm::identity<Matrix4x4f>();
    }

    std::shared_lock _{get_runtime()->eyes_mtx};

    return get_runtime()->eyes[index];
}

Matrix4x4f VR::get_current_eye_transform(bool flip) {
    if (!is_hmd_active()) {
        return glm::identity<Matrix4x4f>();
    }

    std::shared_lock _{get_runtime()->eyes_mtx};

    auto mod_count = flip ? m_right_eye_interval : m_left_eye_interval;

    if (m_frame_count % 2 == mod_count) {
        return get_runtime()->eyes[vr::Eye_Left];
    }

    return get_runtime()->eyes[vr::Eye_Right];
}

Matrix4x4f VR::get_projection_matrix(VRRuntime::Eye eye, bool flip) {
    ZoneScopedN(__FUNCTION__);

    if (!is_hmd_active()) {
        return glm::identity<Matrix4x4f>();
    }

    std::shared_lock _{get_runtime()->eyes_mtx};

    if ((eye == VRRuntime::Eye::LEFT && !flip) || (eye == VRRuntime::Eye::RIGHT && flip)) {
        return get_runtime()->projections[(uint32_t)VRRuntime::Eye::LEFT];
    }

    return get_runtime()->projections[(uint32_t)VRRuntime::Eye::RIGHT];
}

Matrix4x4f VR::get_current_projection_matrix(bool flip) {
    if (!is_hmd_active()) {
        return glm::identity<Matrix4x4f>();
    }

    std::shared_lock _{get_runtime()->eyes_mtx};

    auto mod_count = flip ? m_right_eye_interval : m_left_eye_interval;

    if (m_frame_count % 2 == mod_count) {
        return get_runtime()->projections[(uint32_t)VRRuntime::Eye::LEFT];
    }

    return get_runtime()->projections[(uint32_t)VRRuntime::Eye::RIGHT];
}

bool VR::is_action_active(vr::VRActionHandle_t action, vr::VRInputValueHandle_t source) const {
    ZoneScopedN(__FUNCTION__);

    if (!get_runtime()->loaded) {
        return false;
    }

    if (action == vr::k_ulInvalidActionHandle) {
        return false;
    }
    
    bool active = false;

    if (get_runtime()->is_openvr()) {
        vr::InputDigitalActionData_t data{};
        vr::VRInput()->GetDigitalActionData(action, &data, sizeof(data), source);

        active = data.bActive && data.bState;
    } else if (get_runtime()->is_openxr()) {
        active = m_openxr->is_action_active((XrAction)action, (VRRuntime::Hand)source);
    }

    return active;
}

Vector2f VR::get_joystick_axis(vr::VRInputValueHandle_t handle) const {
    ZoneScopedN(__FUNCTION__);

    if (!get_runtime()->loaded) {
        return Vector2f{};
    }

    if (get_runtime()->is_openvr()) {
        vr::InputAnalogActionData_t data{};
        vr::VRInput()->GetAnalogActionData(m_action_joystick, &data, sizeof(data), handle);

        const auto deadzone = m_joystick_deadzone->value();
        auto out = Vector2f{ data.x, data.y };

        //return glm::length(out) > deadzone ? out : Vector2f{};
        if (glm::abs(out.x) < deadzone) {
            out.x = 0.0f;
        }

        if (glm::abs(out.y) < deadzone) {
            out.y = 0.0f;
        }

        return out;
    } else if (get_runtime()->is_openxr()) {
        // Not using get_left/right_joystick here because it flips the controllers
        if (handle == m_left_joystick) {
            auto out = m_openxr->get_left_stick_axis();
            //return glm::length(out) > m_joystick_deadzone->value() ? out : Vector2f{};
            // okay.. instead of that actually clamp x/y to the proper deadzone
            if (glm::abs(out.x) < m_joystick_deadzone->value()) {
                out.x = 0.0f;
            }

            if (glm::abs(out.y) < m_joystick_deadzone->value()) {
                out.y = 0.0f;
            }

            return out;
        } else if (handle == m_right_joystick) {
            auto out = m_openxr->get_right_stick_axis();
            //return glm::length(out) > m_joystick_deadzone->value() ? out : Vector2f{};

            if (glm::abs(out.x) < m_joystick_deadzone->value()) {
                out.x = 0.0f;
            }

            if (glm::abs(out.y) < m_joystick_deadzone->value()) {
                out.y = 0.0f;
            }

            return out;
        }
    }

    return Vector2f{};
}

Vector2f VR::get_left_stick_axis() const {
    return get_joystick_axis(get_left_joystick());
}

Vector2f VR::get_right_stick_axis() const {
    return get_joystick_axis(get_right_joystick());
}

void VR::trigger_haptic_vibration(float seconds_from_now, float duration, float frequency, float amplitude, vr::VRInputValueHandle_t source) {
    ZoneScopedN(__FUNCTION__);

    if (!get_runtime()->loaded || !is_using_controllers()) {
        return;
    }

    if (get_runtime()->is_openvr()) {
        vr::VRInput()->TriggerHapticVibrationAction(m_action_haptic, seconds_from_now, duration, frequency, amplitude, source);
    } else if (get_runtime()->is_openxr()) {
        m_openxr->trigger_haptic_vibration(duration, frequency, amplitude, (VRRuntime::Hand)source);
    }
}

float VR::get_standing_height() {
    ZoneScopedN(__FUNCTION__);

    std::shared_lock _{ get_runtime()->pose_mtx };

    return m_standing_origin.y;
}

Vector4f VR::get_standing_origin() {
    ZoneScopedN(__FUNCTION__);

    std::shared_lock _{ get_runtime()->pose_mtx };

    return m_standing_origin;
}

void VR::set_standing_origin(const Vector4f& origin) {
    ZoneScopedN(__FUNCTION__);

    std::unique_lock _{ get_runtime()->pose_mtx };
    
    m_standing_origin = origin;
}

glm::quat VR::get_rotation_offset() {
    ZoneScopedN(__FUNCTION__);

    std::shared_lock _{ m_rotation_mtx };

    return m_rotation_offset;
}

void VR::set_rotation_offset(const glm::quat& offset) {
    ZoneScopedN(__FUNCTION__);

    std::unique_lock _{ m_rotation_mtx };

    m_rotation_offset = offset;
}

void VR::recenter_view() {
    ZoneScopedN(__FUNCTION__);

    const auto new_rotation_offset = glm::normalize(glm::inverse(utility::math::flatten(glm::quat{get_rotation(0)})));

    set_rotation_offset(new_rotation_offset);
}

void VR::recenter_horizon() {
    ZoneScopedN(__FUNCTION__);

    const auto new_rotation_offset = glm::normalize(glm::inverse(glm::quat{get_rotation(0)}));

    set_rotation_offset(new_rotation_offset);
}

void VR::gamepad_snapturn(XINPUT_STATE& state) {
    if (!m_snapturn->value()) {
        return;
    }

    if (!is_hmd_active()) {
        return;
    }

    const auto stick_axis = (float)state.Gamepad.sThumbRX / (float)std::numeric_limits<SHORT>::max();

    if (!m_was_snapturn_run_on_input) {
        if (glm::abs(stick_axis) > m_snapturn_joystick_deadzone->value()) {
            m_snapturn_left = stick_axis < 0.0f;
            m_snapturn_on_frame = true;
            m_was_snapturn_run_on_input = true;
            state.Gamepad.sThumbRX = 0;
        }
    } else {
        if (glm::abs(stick_axis) < m_snapturn_joystick_deadzone->value()) {
            m_was_snapturn_run_on_input = false;
        } else {
            state.Gamepad.sThumbRX = 0;
        }
    }
}

void VR::process_snapturn() {
    if (!m_snapturn_on_frame) {
        return;
    }

    const auto engine = sdk::UEngine::get();

    if (engine == nullptr) {
        return;
    }

    const auto world = engine->get_world();

    if (world == nullptr) {
        return;
    }

    if (const auto controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0); controller != nullptr) {
        auto controller_rot = controller->get_control_rotation();
        auto turn_degrees = get_snapturn_angle();
        
        if (m_snapturn_left) {
            turn_degrees = -turn_degrees;
            m_snapturn_left = false;
        }

        controller_rot.y += turn_degrees;
        controller->set_control_rotation(controller_rot);
    }
        
    m_snapturn_on_frame = false;
}

void VR::update_statistics_overlay(sdk::UGameEngine* engine) {
    if (engine == nullptr) {
        return;
    }
    
    if (m_show_fps_state != m_show_fps->value()) {
        engine->exec(L"stat fps");
        m_show_fps_state = m_show_fps->value();
    }
    
    if (m_show_statistics_state != m_show_statistics->value()) {
        engine->exec(L"stat unit");
        m_show_statistics_state = m_show_statistics->value();
    }
}
