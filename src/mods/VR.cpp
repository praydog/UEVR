#include <fstream>

#include <windows.h>
#include <dbt.h>

#include <imgui.h>
#include <utility/Module.hpp>
#include <utility/Registry.hpp>

#include <sdk/CVar.hpp>

#include "vr/GameThreadWorker.hpp"
#include "Framework.hpp"

#include "VR.hpp"

class ScopeGuard {
public:
    ScopeGuard() = delete;
    ScopeGuard(std::function<void()> on_exit)
        : m_on_exit{on_exit}
    {
    }

    ~ScopeGuard() {
        if (m_on_exit) {
            m_on_exit();
        }
    }

private:
    std::function<void()> m_on_exit;
};

std::shared_ptr<VR>& VR::get() {
    static std::shared_ptr<VR> instance = std::make_shared<VR>();
    return instance;
}

// Called when the mod is initialized
std::optional<std::string> VR::on_initialize() try {
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
        m_openxr->error = 
R"(OpenVR loaded first.
If you want to use OpenXR, remove the openvr_api.dll from your game folder, 
and place the openxr_loader.dll in the same folder.)";
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
    spdlog::info("Attempting to load OpenVR");

    m_openvr = std::make_shared<runtimes::OpenVR>();
    m_openvr->loaded = false;

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

    detect_controllers();

    return std::nullopt;
}

std::optional<std::string> VR::initialize_openxr() {
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

        XrInstanceCreateInfo instance_create_info{XR_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.next = nullptr;
        instance_create_info.enabledExtensionCount = (uint32_t)extensions.size();
        instance_create_info.enabledExtensionNames = extensions.data();

        strcpy(instance_create_info.applicationInfo.applicationName, "UE4VR");
        instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        
        result = xrCreateInstance(&instance_create_info, &m_openxr->instance);

        // we can't convert the result to a string here
        // because the function requires the instance to be valid
        if (result != XR_SUCCESS) {
            m_openxr->error = "Could not create openxr instance: " + std::to_string((int32_t)result);
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

    spdlog::info("[VR] OpenXR system Name: {}", system_properties.systemName);
    spdlog::info("[VR] OpenXR system Vendor: {}", system_properties.vendorId);
    spdlog::info("[VR] OpenXR system max width: {}", system_properties.graphicsProperties.maxSwapchainImageWidth);
    spdlog::info("[VR] OpenXR system max height: {}", system_properties.graphicsProperties.maxSwapchainImageHeight);
    spdlog::info("[VR] OpenXR system supports {} layers", system_properties.graphicsProperties.maxLayerCount);
    spdlog::info("[VR] OpenXR system orientation: {}", system_properties.trackingProperties.orientationTracking);
    spdlog::info("[VR] OpenXR system position: {}", system_properties.trackingProperties.positionTracking);

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
    // This depends on the config being loaded.
    if (!m_init_finished) {
        return std::nullopt;
    }

    spdlog::info("[VR] Creating OpenXR swapchain");

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
    if (!m_runtime->ready() || !is_using_controllers()) {
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

    for (auto& it : m_action_handles) {
        if (is_action_active(it.second, m_left_joystick) || is_action_active(it.second, m_right_joystick)) {
            return true;
        }
    }

    return false;
}

bool VR::on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message != DBT_DEVICEARRIVAL && message != WM_DEVICECHANGE && !m_spoofed_gamepad_connection) {
        SendMessage(wnd, WM_DEVICECHANGE, 0, 0); // this seems to actually be the one that works
        SendMessage(wnd, DBT_DEVICEARRIVAL, 0, 0); // didn't seem to do anything? leaving it here just in case
    }

    return true;
}

void VR::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
    if (user_index != 0) {
        return;
    }

    if (!m_spoofed_gamepad_connection) {
        spdlog::info("[VR] Successfully spoofed gamepad connection");
    }

    m_last_xinput_update = std::chrono::steady_clock::now();
    m_spoofed_gamepad_connection = true;

    if (!is_using_controllers()) {
        return;
    }

    *retval = ERROR_SUCCESS;

    const auto a_button_action = get_action_handle("/actions/default/in/AButton");
    const auto is_right_a_button_down = is_action_active(a_button_action, m_right_joystick);
    const auto is_left_a_button_down = is_action_active(a_button_action, m_left_joystick);

    if (is_right_a_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    }

    if (is_left_a_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    }

    const auto b_button_action = get_action_handle("/actions/default/in/BButton");
    const auto is_right_b_button_down = is_action_active(b_button_action, m_right_joystick);
    const auto is_left_b_button_down = is_action_active(b_button_action, m_left_joystick);

    if (is_right_b_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    }

    if (is_left_b_button_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    }

    const auto joystick_click_action = get_action_handle("/actions/default/in/JoystickClick");
    const auto is_left_joystick_click_down = is_action_active(joystick_click_action, m_left_joystick);
    const auto is_right_joystick_click_down = is_action_active(joystick_click_action, m_right_joystick);

    if (is_left_joystick_click_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    }

    if (is_right_joystick_click_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    }

    const auto trigger_action = get_action_handle("/actions/default/in/Trigger");
    const auto is_left_trigger_down = is_action_active(trigger_action, m_left_joystick);
    const auto is_right_trigger_down = is_action_active(trigger_action, m_right_joystick);

    if (is_left_trigger_down) {
        state->Gamepad.bLeftTrigger = 255;
    }

    if (is_right_trigger_down) {
        state->Gamepad.bRightTrigger = 255;
    }

    const auto grip_action = get_action_handle("/actions/default/in/Grip");
    const auto is_right_grip_down = is_action_active(grip_action, m_right_joystick);
    const auto is_left_grip_down = is_action_active(grip_action, m_left_joystick);

    if (is_right_grip_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    }

    if (is_left_grip_down) {
        state->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    }

    const auto left_joystick_axis = get_joystick_axis(m_left_joystick);
    const auto right_joystick_axis = get_joystick_axis(m_right_joystick);

    state->Gamepad.sThumbLX = (int16_t)(left_joystick_axis.x * 32767.0f);
    state->Gamepad.sThumbLY = (int16_t)(left_joystick_axis.y * 32767.0f);

    state->Gamepad.sThumbRX = (int16_t)(right_joystick_axis.x * 32767.0f);
    state->Gamepad.sThumbRY = (int16_t)(right_joystick_axis.y * 32767.0f);
}

void VR::on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {
    if (m_left_joystick == 0 || m_right_joystick == 0) {
        return;
    }

    const auto left_amplitude = ((float)vibration->wLeftMotorSpeed / 65535.0f) * 5.0f;
    const auto right_amplitude = ((float)vibration->wRightMotorSpeed / 65535.0f) * 5.0f;

    trigger_haptic_vibration(0.0f, 0.1f, 1.0f, left_amplitude, m_left_joystick);
    trigger_haptic_vibration(0.0f, 0.1f, 1.0f, right_amplitude, m_right_joystick);
}

void VR::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (!get_runtime()->loaded || !is_hmd_active()) {
        return;
    }

    //update_hmd_state();
    update_action_states();
}

void VR::update_hmd_state(bool from_view_extensions, uint32_t frame_count) {
    std::scoped_lock _{m_openvr_mtx};

    auto runtime = get_runtime();
    
    /*if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::EARLY) {
        if (runtime->is_openxr()) {
            if (g_framework->get_renderer_type() == Framework::RendererType::D3D11) {
                if (!runtime->got_first_sync || runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                    return;
                }  
            } else if (runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                return;
            }

            m_openxr->begin_frame();
        } else {
            if (runtime->synchronize_frame() != VRRuntime::Error::SUCCESS) {
                return;
            }
        }
    }*/
    
    runtime->update_poses(from_view_extensions, frame_count);

    // Update the poses used for the game
    // If we used the data directly from the WaitGetPoses call, we would have to lock a different mutex and wait a long time
    // This is because the WaitGetPoses call is blocking, and we don't want to block any game logic
    {
        std::unique_lock _{ runtime->pose_mtx };

        if (runtime->wants_reset_origin && runtime->ready()) {
            set_rotation_offset(glm::identity<glm::quat>());
            m_standing_origin = get_position_unsafe(vr::k_unTrackedDeviceIndex_Hmd);

            runtime->wants_reset_origin = false;
        }
    }

    runtime->update_matrices(m_nearz, m_farz);

    // On first run, set the standing origin to the headset position
    if (!runtime->got_first_poses) {
        m_standing_origin = get_position(vr::k_unTrackedDeviceIndex_Hmd);
    }

    if (!runtime->got_first_poses && runtime->is_openvr()) {
        //std::unique_lock _{m_openvr->pose_mtx};
        //m_openvr->pose_queue.clear();
    }

    runtime->got_first_poses = true;
}

void VR::update_action_states() {
    auto runtime = get_runtime();

    if (runtime->wants_reinitialize) {
        return;
    }

    if (m_spoofed_gamepad_connection && std::chrono::steady_clock::now() - m_last_xinput_update >= std::chrono::seconds(2)) {
        m_spoofed_gamepad_connection = false;
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

    const auto deadzone = m_joystick_deadzone->value();
    const auto left_axis_len = glm::length(get_left_stick_axis());
    const auto right_axis_len = glm::length(get_right_stick_axis());

    if (left_axis_len > deadzone || right_axis_len > deadzone || is_any_action_down()) {
        m_last_controller_update = std::chrono::steady_clock::now();
    }

    /*if (m_recenter_view_key->is_key_down_once()) {
        recenter_view();
    }

    if (m_set_standing_key->is_key_down_once()) {
        set_standing_origin(get_position(0));
    }*/
}

void VR::on_config_load(const utility::Config& cfg) {
    for (IModValue& option : m_options) {
        option.config_load(cfg);
    }

    if (m_fake_stereo_hook != nullptr) {
        m_fake_stereo_hook->on_config_load(cfg);
    }

    if (get_runtime()->loaded) {
        get_runtime()->on_config_load(cfg);
    }

    // Run the rest of OpenXR initialization code here that depends on config values
    if (get_runtime()->is_openxr() && get_runtime()->loaded) {
        spdlog::info("[VR] Finishing up OpenXR initialization");
        initialize_openxr_swapchains();
    }

    if (get_runtime()->loaded) {
        m_fake_stereo_hook = std::make_unique<FFakeStereoRenderingHook>();
    }

    m_overlay_component.on_config_load(cfg);
}

void VR::on_config_save(utility::Config& cfg) {
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
}

void VR::on_pre_imgui_frame() {
    if (!get_runtime()->ready()) {
        return;
    }

    if (!m_disable_overlay) {
        m_overlay_component.on_pre_imgui_frame();
    }
}

void VR::on_present() {
    ScopeGuard _guard {[&]() {
        if (!m_use_afr->value() || (m_render_frame_count + 1) % 2 == m_left_eye_interval) {
            SetEvent(m_present_finished_event);
        }

        m_last_frame_count = m_render_frame_count;
    }};

    ++m_frame_count;

    if (!m_use_afr->value() || (m_render_frame_count + 1) % 2 == m_left_eye_interval) {
        ResetEvent(m_present_finished_event);
    }

    auto runtime = get_runtime();

    if (!runtime->loaded) {
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

    if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::LATE) {
        const auto had_sync = runtime->got_first_sync;
        runtime->synchronize_frame();

        if (!runtime->got_first_poses || !had_sync) {
            update_hmd_state();
        }
    }


    if (renderer == Framework::RendererType::D3D11) {
        // if we don't do this then D3D11 OpenXR freezes for some reason.
        if (!runtime->got_first_sync) {
            spdlog::info("Attempting to sync!");
            if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::LATE) {
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

    if (m_submitted || runtime->needs_pose_update) {
        if (m_submitted) {
            if (!m_disable_overlay) {
                m_overlay_component.on_post_compositor_submit();
            }

            if (runtime->is_openvr()) {
                //vr::VRCompositor()->SetExplicitTimingMode(vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
                vr::VRCompositor()->PostPresentHandoff();
            }
        }

        //runtime->needs_pose_update = true;
        m_submitted = false;
    }
}

void VR::on_post_present() {
    m_render_frame_count = m_frame_count;

    auto runtime = get_runtime();

    if (!get_runtime()->loaded) {
        return;
    }

    std::scoped_lock _{m_openvr_mtx};

    detect_controllers();

    if (runtime->get_synchronize_stage() == VRRuntime::SynchronizeStage::VERY_LATE || !runtime->got_first_sync) {
        const auto had_sync = runtime->got_first_sync;
        runtime->synchronize_frame();

        if (!runtime->got_first_poses || !had_sync) {
            update_hmd_state();
        }
    }

    if (runtime->is_openxr() && runtime->ready() && !m_use_afr->value() && runtime->get_synchronize_stage() > VRRuntime::SynchronizeStage::EARLY) {
        if (!m_openxr->frame_began) {
            m_openxr->begin_frame();
        }
    }

    if (runtime->get_synchronize_stage() > VRRuntime::SynchronizeStage::EARLY) {
        //update_hmd_state();
    }

    if (runtime->wants_reinitialize) {
        if (runtime->is_openvr()) {
            m_openvr->wants_reinitialize = false;
            reinitialize_openvr();
        } else if (runtime->is_openxr()) {
            m_openxr->wants_reinitialize = false;
            reinitialize_openxr();
        }
    }
}

void VR::on_draw_ui() {
    // create VR tree entry in menu (imgui)
    if (get_runtime()->loaded) {
        ImGui::SetNextItemOpen(m_has_hw_scheduling, ImGuiCond_::ImGuiCond_FirstUseEver);
    } else {
        if (m_openvr->error && !m_openvr->dll_missing) {
            ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_FirstUseEver);
        } else {
            ImGui::SetNextItemOpen(false, ImGuiCond_::ImGuiCond_FirstUseEver);
        }
    }

    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }

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

    display_error(m_openxr, "openxr_loader.dll");
    display_error(m_openvr, "openvr_api.dll");

    if (!get_runtime()->loaded) {
        ImGui::TextWrapped("No runtime loaded.");
        return;
    }

    ImGui::TextWrapped("Hardware scheduling: %s", m_has_hw_scheduling ? "Enabled" : "Disabled");

    if (m_has_hw_scheduling) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextWrapped("WARNING: Hardware-accelerated GPU scheduling is enabled. This may cause the game to run slower.");
        ImGui::TextWrapped("Go into your Windows Graphics settings and disable \"Hardware-accelerated GPU scheduling\"");
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    ImGui::Text("Unreal Options");

    m_camera_forward_offset->draw("Camera Forward Offset");
    m_camera_right_offset->draw("Camera Right Offset");
    m_camera_up_offset->draw("Camera Up Offset");
    m_world_scale->draw("World Scale");

    m_enable_gui->draw("Enable GUI");

    ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_FirstUseEver);

    if (ImGui::TreeNode("CVars")) {
        // Boolean values
        // r.OneFrameThreadLag
        auto one_frame_thread_lag = sdk::rendering::get_one_frame_thread_lag_cvar()->get<int>();

        if (one_frame_thread_lag != nullptr) try {
            auto value = one_frame_thread_lag->get();

            if (ImGui::Checkbox("One Frame Thread Lag", (bool*)&value)) {
                one_frame_thread_lag->set(value);
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read frame thread lag cvar");
        }

        // r.AllowHDR
        static auto r_allow_hdr_cvar = sdk::find_cvar_data(L"Engine", L"r.AllowHDR");

        if (r_allow_hdr_cvar) try {
            auto r_allow_hdr = r_allow_hdr_cvar->get<int>();

            if (r_allow_hdr != nullptr) {
                auto value = r_allow_hdr_cvar->get<int>();

                if (ImGui::Checkbox("Allow HDR", (bool*)&value)) {
                    r_allow_hdr_cvar->set(value);
                }
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read r.AllowHDR cvar");
        }

        // r.AllowOcclusionQueries
        static auto r_allow_occlusion_queries_cvar = sdk::find_cvar_data(L"Engine", L"r.AllowOcclusionQueries");

        if (r_allow_occlusion_queries_cvar) try {
            auto r_allow_occlusion_queries = r_allow_occlusion_queries_cvar->get<int>();

            if (r_allow_occlusion_queries != nullptr) {
                auto value = r_allow_occlusion_queries->get();

                if (ImGui::Checkbox("Allow Occlusion Queries", (bool*)&value)) {
                    r_allow_occlusion_queries->set(value);
                }
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read r.AllowOcclusionQueries cvar");
        }

        // r.HZBOcclusion
        static auto r_hzbo_cvar = sdk::find_cvar(L"Engine", L"r.HZBOcclusion");

        if (r_hzbo_cvar != nullptr && *r_hzbo_cvar != nullptr) try {
            auto cvar = *r_hzbo_cvar;
            auto value = cvar->GetInt();

            if (ImGui::Checkbox("HZBOcclusion", (bool*)&value)) {
                GameThreadWorker::get().enqueue([cvar, value] {
                    cvar->Set(std::to_wstring(value).c_str());
                });
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read r.HZBOcclusion cvar");
        }

        // r.VolumetricCloud
        static auto r_volumetric_cloud_cvar = sdk::find_cvar_data(L"Engine", L"r.VolumetricCloud");

        if (r_volumetric_cloud_cvar) try {
            auto r_volumetric_cloud = r_volumetric_cloud_cvar->get<int>();

            if (r_volumetric_cloud != nullptr) {
                auto value = r_volumetric_cloud->get();

                if (ImGui::Checkbox("Volumetric Cloud", (bool*)&value)) {
                    r_volumetric_cloud->set(value);
                }
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read r.VolumetricCloud cvar");
        }

        // Sliders (int values)
        // r.AmbientOcclusionLevels
        static auto r_ambient_occlusion_levels_cvar = sdk::find_cvar_data(L"Engine", L"r.AmbientOcclusionLevels");

        if (r_ambient_occlusion_levels_cvar) try {
            auto r_ambient_occlusion_levels = r_ambient_occlusion_levels_cvar->get<int>();

            if (r_ambient_occlusion_levels != nullptr) {
                auto value = r_ambient_occlusion_levels->get();

                if (ImGui::SliderInt("Ambient Occlusion Levels", &value, 0, 4)) {
                    r_ambient_occlusion_levels->set(value);
                }
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read ambient occlusion levels cvar");
        }

        // r.LightCulling.Quality
        static auto r_light_culling_quality_cvar = sdk::find_cvar(L"Engine", L"r.LightCulling.Quality");

        if (r_light_culling_quality_cvar != nullptr && *r_light_culling_quality_cvar != nullptr) try {
            auto cvar = *r_light_culling_quality_cvar;
            auto value = cvar->GetInt();

            if (ImGui::SliderInt("Light Culling Quality", &value, 0, 2)) {
                GameThreadWorker::get().enqueue([cvar, value] {
                    cvar->Set(std::to_wstring(value).c_str());
                });
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read light culling quality cvar");
        }

        static auto r_subsurface_scattering_cvar = sdk::find_cvar_data(L"Engine", L"r.SubsurfaceScattering");

        if (r_subsurface_scattering_cvar) try {
            auto r_subsurface_scattering = r_subsurface_scattering_cvar->get<int>();

            if (r_subsurface_scattering != nullptr) {
                auto value = r_subsurface_scattering->get();

                if (ImGui::SliderInt("Subsurface Scattering", &value, 0, 2)) {
                    r_subsurface_scattering->set(value);
                }
            }
        } catch(...) {
            ImGui::TextWrapped("Failed to read subsurface scattering cvar");
        }

        ImGui::TreePop();
    }

    if (m_fake_stereo_hook != nullptr) {
        m_fake_stereo_hook->on_draw_ui();
    }

    ImGui::Text("Runtime Information");

    ImGui::TextWrapped("VR Runtime: %s", get_runtime()->name().data());
    ImGui::TextWrapped("Render Resolution: %d x %d", get_runtime()->get_width(), get_runtime()->get_height());

    if (get_runtime()->is_openvr()) {
        ImGui::TextWrapped("Resolution can be changed in SteamVR");
    }

    get_runtime()->on_draw_ui();
    
    ImGui::Combo("Sync Mode", (int*)&get_runtime()->custom_stage, "Early\0Late\0Very Late\0");
    ImGui::DragFloat4("Right Bounds", (float*)&m_right_bounds, 0.005f, -2.0f, 2.0f);
    ImGui::DragFloat4("Left Bounds", (float*)&m_left_bounds, 0.005f, -2.0f, 2.0f);

    m_overlay_component.on_draw_ui();

    ImGui::Separator();

    if (ImGui::Button("Set Standing Height")) {
        m_standing_origin.y = get_position(0).y;
    }

    ImGui::SameLine();

    if (ImGui::Button("Set Standing Origin")) {
        m_standing_origin = get_position(0);
    }

    if (ImGui::Button("Recenter View")) {
        recenter_view();
    }

    if (ImGui::Button("Reinitialize Runtime")) {
        get_runtime()->wants_reinitialize = true;
    }

    ImGui::Text("Graphical Options");

    if (ImGui::TreeNode("Desktop Recording Fix")) {
        ImGui::PushID("Desktop");
        m_desktop_fix->draw("Enabled");
        m_desktop_fix_skip_present->draw("Skip Present");
        ImGui::PopID();
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Debug info");
    ImGui::Checkbox("Disable Projection Matrix Override", &m_disable_projection_matrix_override);
    ImGui::Checkbox("Disable View Matrix Override", &m_disable_view_matrix_override);
    ImGui::Checkbox("Disable Backbuffer Size Override", &m_disable_backbuffer_size_override);
    ImGui::Checkbox("Disable VR Overlay", &m_disable_overlay);
    ImGui::Checkbox("Stereo Emulation Mode", &m_stereo_emulation_mode);
    ImGui::Checkbox("Wait for Present", &m_wait_for_present);

    const double min_ = 0.0;
    const double max_ = 25.0;
    ImGui::SliderScalar("Prediction Scale", ImGuiDataType_Double, &m_openxr->prediction_scale, &min_, &max_);

    ImGui::DragFloat4("Raw Left", (float*)&m_raw_projections[0], 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat4("Raw Right", (float*)&m_raw_projections[1], 0.01f, -100.0f, 100.0f);
}

Vector4f VR::get_position(uint32_t index) const {
    if (index >= vr::k_unMaxTrackedDeviceCount) {
        return Vector4f{};
    }

    std::shared_lock _{ get_runtime()->pose_mtx };
    std::shared_lock __{ get_runtime()->eyes_mtx };

    return get_position_unsafe(index);
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
            return Vector4f{ *(Vector3f*)&m_openxr->get_current_view_space_location().pose.position, 1.0f };
        } else if (index > 0) {
            return Vector4f{ *(Vector3f*)&m_openxr->hands[index-1].location.pose.position, 1.0f };
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

        return Vector4f{ *(Vector3f*)&m_openxr->hands[index-1].velocity.linearVelocity, 0.0f };
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
    
        return Vector4f{ *(Vector3f*)&m_openxr->hands[index-1].velocity.angularVelocity, 0.0f };
    }

    return Vector4f{};
}

Matrix4x4f VR::get_rotation(uint32_t index) const {
    if (get_runtime()->is_openvr()) {
        if (index >= vr::k_unMaxTrackedDeviceCount) {
            return glm::identity<Matrix4x4f>();
        }

        std::shared_lock _{ get_runtime()->pose_mtx };

        if (index == vr::k_unTrackedDeviceIndex_Hmd) {
            const auto pose = m_openvr->get_current_hmd_pose();
            const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose };
            return glm::extractMatrixRotation(glm::rowMajor4(matrix));
        }

        auto& pose = get_openvr_poses()[index];
        auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking };
        return glm::extractMatrixRotation(glm::rowMajor4(matrix));
    } else if (get_runtime()->is_openxr()) {
        std::shared_lock _{ get_runtime()->pose_mtx };
        std::shared_lock __{ get_runtime()->eyes_mtx };

        // HMD rotation
        if (index == 0 && !m_openxr->stage_views.empty()) {
            const auto q = runtimes::OpenXR::to_glm(m_openxr->get_current_view_space_location().pose.orientation);
            const auto m = glm::extractMatrixRotation(Matrix4x4f{q});

            return m;
            //return Matrix4x4f{*(glm::quat*)&m_openxr->stage_views[0].pose.orientation};
        } else if (index > 0) {
            if (index == VRRuntime::Hand::LEFT+1) {
                return Matrix4x4f{runtimes::OpenXR::to_glm(m_openxr->hands[VRRuntime::Hand::LEFT].location.pose.orientation)};
            } else if (index == VRRuntime::Hand::RIGHT+1) {
                return Matrix4x4f{runtimes::OpenXR::to_glm(m_openxr->hands[VRRuntime::Hand::RIGHT].location.pose.orientation)};
            }
        }

        return glm::identity<Matrix4x4f>();
    }

    return glm::identity<Matrix4x4f>();
}

Matrix4x4f VR::get_transform(uint32_t index) const {
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

        const auto& pose = get_openvr_poses()[index];
        const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking };
        return glm::rowMajor4(matrix);
    } else if (get_runtime()->is_openxr()) {
        std::shared_lock _{ get_runtime()->pose_mtx };

        // HMD rotation
        if (index == 0 && !m_openxr->stage_views.empty()) {
            const auto& vspl = m_openxr->get_current_view_space_location();
            auto mat = Matrix4x4f{runtimes::OpenXR::to_glm(vspl.pose.orientation)};
            mat[3] = Vector4f{*(Vector3f*)&vspl.pose.position, 1.0f};
            return mat;
        } else if (index > 0) {
            if (index == VRRuntime::Hand::LEFT+1) {
                auto mat = Matrix4x4f{runtimes::OpenXR::to_glm(m_openxr->hands[VRRuntime::Hand::LEFT].location.pose.orientation)};
                mat[3] = Vector4f{*(Vector3f*)&m_openxr->hands[VRRuntime::Hand::LEFT].location.pose.position, 1.0f};
                return mat;
            } else if (index == VRRuntime::Hand::RIGHT+1) {
                auto mat = Matrix4x4f{runtimes::OpenXR::to_glm(m_openxr->hands[VRRuntime::Hand::RIGHT].location.pose.orientation)};
                mat[3] = Vector4f{*(Vector3f*)&m_openxr->hands[VRRuntime::Hand::RIGHT].location.pose.position, 1.0f};
                return mat;
            }
        }
    }

    return glm::identity<Matrix4x4f>();
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
    if (!get_runtime()->loaded) {
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
    if (!get_runtime()->loaded) {
        return Vector2f{};
    }

    if (get_runtime()->is_openvr()) {
        vr::InputAnalogActionData_t data{};
        vr::VRInput()->GetAnalogActionData(m_action_joystick, &data, sizeof(data), handle);

        const auto deadzone = m_joystick_deadzone->value();
        const auto out = Vector2f{ data.x, data.y };

        return glm::length(out) > deadzone ? out : Vector2f{};
    } else if (get_runtime()->is_openxr()) {
        if (handle == (vr::VRInputValueHandle_t)VRRuntime::Hand::LEFT) {
            auto out = m_openxr->get_left_stick_axis();
            return glm::length(out) > m_joystick_deadzone->value() ? out : Vector2f{};
        } else if (handle == (vr::VRInputValueHandle_t)VRRuntime::Hand::RIGHT) {
            auto out = m_openxr->get_right_stick_axis();
            return glm::length(out) > m_joystick_deadzone->value() ? out : Vector2f{};
        }
    }

    return Vector2f{};
}

Vector2f VR::get_left_stick_axis() const {
    return get_joystick_axis(m_left_joystick);
}

Vector2f VR::get_right_stick_axis() const {
    return get_joystick_axis(m_right_joystick);
}

void VR::trigger_haptic_vibration(float seconds_from_now, float duration, float frequency, float amplitude, vr::VRInputValueHandle_t source) {
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
    std::shared_lock _{ get_runtime()->pose_mtx };

    return m_standing_origin.y;
}

Vector4f VR::get_standing_origin() {
    std::shared_lock _{ get_runtime()->pose_mtx };

    return m_standing_origin;
}

void VR::set_standing_origin(const Vector4f& origin) {
    std::unique_lock _{ get_runtime()->pose_mtx };
    
    m_standing_origin = origin;
}

glm::quat VR::get_rotation_offset() {
    std::shared_lock _{ m_rotation_mtx };

    return m_rotation_offset;
}

void VR::set_rotation_offset(const glm::quat& offset) {
    std::unique_lock _{ m_rotation_mtx };

    m_rotation_offset = offset;
}

void VR::recenter_view() {
    const auto new_rotation_offset = glm::normalize(glm::inverse(utility::math::flatten(glm::quat{get_rotation(0)})));

    set_rotation_offset(new_rotation_offset);
}