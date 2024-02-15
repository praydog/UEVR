#pragma once

#define NOMINMAX

#include <memory>
#include <string>

#include <sdk/Math.hpp>

#include "vr/runtimes/OpenVR.hpp"
#include "vr/runtimes/OpenXR.hpp"

#include "vr/D3D11Component.hpp"
#include "vr/D3D12Component.hpp"
#include "vr/OverlayComponent.hpp"

#include "vr/FFakeStereoRenderingHook.hpp"
#include "vr/RenderTargetPoolHook.hpp"
#include "vr/CVarManager.hpp"

#include "Mod.hpp"

#undef max
#include <tracy/Tracy.hpp>

class VR : public Mod {
public:
    enum RenderingMethod {
        NATIVE_STEREO = 0,
        SYNCHRONIZED = 1,
        ALTERNATING = 2,
    };

    enum SyncedSequentialMethod {
        SKIP_TICK = 0,
        SKIP_DRAW = 1,
    };

    enum AimMethod : int32_t {
        GAME,
        HEAD,
        RIGHT_CONTROLLER,
        LEFT_CONTROLLER,
        TWO_HANDED_RIGHT,
        TWO_HANDED_LEFT,
    };

    enum DPadMethod : int32_t {
        RIGHT_TOUCH,
        LEFT_TOUCH,
        LEFT_JOYSTICK,
        RIGHT_JOYSTICK,
        GESTURE_HEAD,
        GESTURE_HEAD_RIGHT,
    };

    enum HORIZONTAL_PROJECTION_OVERRIDE : int32_t {
        HORIZONTAL_DEFAULT,
        HORIZONTAL_SYMMETRIC,
        HORIZONTAL_MIRROR
    };

    enum VERTICAL_PROJECTION_OVERRIDE : int32_t {
        VERTICAL_DEFAULT,
        VERTICAL_SYMMETRIC,
        VERTICAL_MATCHED
    };

    static const inline std::string s_action_pose = "/actions/default/in/Pose";
    static const inline std::string s_action_grip_pose = "/actions/default/in/GripPose";
    static const inline std::string s_action_trigger = "/actions/default/in/Trigger";
    static const inline std::string s_action_grip = "/actions/default/in/Grip";
    static const inline std::string s_action_joystick = "/actions/default/in/Joystick";
    static const inline std::string s_action_joystick_click = "/actions/default/in/JoystickClick";

    static const inline std::string s_action_a_button_left = "/actions/default/in/AButtonLeft";
    static const inline std::string s_action_b_button_left = "/actions/default/in/BButtonLeft";
    static const inline std::string s_action_a_button_touch_left = "/actions/default/in/AButtonTouchLeft";
    static const inline std::string s_action_b_button_touch_left = "/actions/default/in/BButtonTouchLeft";

    static const inline std::string s_action_a_button_right = "/actions/default/in/AButtonRight";
    static const inline std::string s_action_b_button_right = "/actions/default/in/BButtonRight";
    static const inline std::string s_action_a_button_touch_right = "/actions/default/in/AButtonTouchRight";
    static const inline std::string s_action_b_button_touch_right = "/actions/default/in/BButtonTouchRight";

    static const inline std::string s_action_dpad_up = "/actions/default/in/DPad_Up";
    static const inline std::string s_action_dpad_right = "/actions/default/in/DPad_Right";
    static const inline std::string s_action_dpad_down = "/actions/default/in/DPad_Down";
    static const inline std::string s_action_dpad_left = "/actions/default/in/DPad_Left";
    static const inline std::string s_action_system_button = "/actions/default/in/SystemButton";
    static const inline std::string s_action_thumbrest_touch_left = "/actions/default/in/ThumbrestTouchLeft";
    static const inline std::string s_action_thumbrest_touch_right = "/actions/default/in/ThumbrestTouchRight";

public:
    static std::shared_ptr<VR>& get();

    std::string_view get_name() const override { return "VR"; }

    std::optional<std::string> clean_initialize();
    std::optional<std::string> on_initialize_d3d_thread() {
        return clean_initialize();
    }

    std::vector<SidebarEntryInfo> get_sidebar_entries() override {
        return {
            {"Runtime", false},
            {"Unreal", false},
            {"Input", false},
            {"Camera", false},
            {"Keybinds", false},
            {"Console/CVars", true},
            {"Compatibility", true},
            {"Debug", true},
        };
    }

    // texture bounds to tell OpenVR which parts of the submitted texture to render (default - use the whole texture).
    // Will be modified to accommodate forced symmetrical eye projection
    vr::VRTextureBounds_t m_right_bounds{0.0f, 0.0f, 1.0f, 1.0f};
    vr::VRTextureBounds_t m_left_bounds{0.0f, 0.0f, 1.0f, 1.0f};

    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;
    
    void on_draw_ui() override;
    void on_draw_sidebar_entry(std::string_view name) override;
    void on_pre_imgui_frame() override;

    void handle_keybinds();
    void on_frame() override;

    void on_present() override;
    void on_post_present() override;

    void on_device_reset() override {
        get_runtime()->on_device_reset();

        if (m_fake_stereo_hook != nullptr) {
            m_fake_stereo_hook->on_device_reset();
        }

        if (m_is_d3d12) {
            m_d3d12.on_reset(this);
        } else {
            m_d3d11.on_reset(this);
        }
    }

    bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) override;
    void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) override;
    void on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) override;
    void update_imgui_state_from_xinput_state(XINPUT_STATE& state, bool is_vr_controller);

    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                             const float world_to_meters, Vector3f* view_location, bool is_double) override;
    void on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) override;

    void update_hmd_state(bool from_view_extensions = false, uint32_t frame_count = 0);
    void update_action_states();
    void update_dpad_gestures();

    void reinitialize_renderer() {
        if (m_is_d3d12) {
            m_d3d12.force_reset();
        } else {
            m_d3d11.force_reset();
        }
    }


    Vector4f get_position(uint32_t index, bool grip = true)  const;
    Vector4f get_velocity(uint32_t index)  const;
    Vector4f get_angular_velocity(uint32_t index)  const;
    Matrix4x4f get_hmd_rotation(uint32_t frame_count) const;
    Matrix4x4f get_hmd_transform(uint32_t frame_count) const;
    Matrix4x4f get_rotation(uint32_t index, bool grip = true)  const;
    Matrix4x4f get_transform(uint32_t index, bool grip = true) const;
    vr::HmdMatrix34_t get_raw_transform(uint32_t index) const;

    Vector4f get_grip_position(uint32_t index) const {
        return get_position(index, true);
    }

    Vector4f get_aim_position(uint32_t index) const {
        return get_position(index, false);
    }

    Matrix4x4f get_grip_rotation(uint32_t index) const {
        return get_rotation(index, true);
    }

    Matrix4x4f get_aim_rotation(uint32_t index) const {
        return get_rotation(index, false);
    }

    Matrix4x4f get_grip_transform(uint32_t hand_index) const;
    Matrix4x4f get_aim_transform(uint32_t hand_index) const;

    Vector4f get_eye_offset(VRRuntime::Eye eye) const;
    Vector4f get_current_offset();
    
    Matrix4x4f get_eye_transform(uint32_t index);
    Matrix4x4f get_current_eye_transform(bool flip = false);
    Matrix4x4f get_projection_matrix(VRRuntime::Eye eye, bool flip = false);
    Matrix4x4f get_current_projection_matrix(bool flip = false);

    bool is_action_active(vr::VRActionHandle_t action, vr::VRInputValueHandle_t source = vr::k_ulInvalidInputValueHandle) const;

    bool is_action_active_any_joystick(vr::VRActionHandle_t action) const {
        if (is_action_active(action, m_left_joystick)) {
            return true;
        }

        if (is_action_active(action, m_right_joystick)) {
            return true;
        }

        return false;
    }
    Vector2f get_joystick_axis(vr::VRInputValueHandle_t handle) const;

    vr::VRActionHandle_t get_action_handle(std::string_view action_path) {
        if (auto it = m_action_handles.find(action_path.data()); it != m_action_handles.end()) {
            return it->second;
        }

        return vr::k_ulInvalidActionHandle;
    }

    Vector2f get_left_stick_axis() const;
    Vector2f get_right_stick_axis() const;

    void trigger_haptic_vibration(float seconds_from_now, float duration, float frequency, float amplitude, vr::VRInputValueHandle_t source = vr::k_ulInvalidInputValueHandle);
    
    float get_standing_height();
    Vector4f get_standing_origin();
    void set_standing_origin(const Vector4f& origin);

    glm::quat get_rotation_offset();
    void set_rotation_offset(const glm::quat& offset);
    void recenter_view();


    template<typename T = VRRuntime>
    T* get_runtime() const {
        return (T*)m_runtime.get();
    }

    runtimes::OpenXR* get_openxr_runtime() const {
        return m_openxr.get();
    }

    runtimes::OpenVR* get_openvr_runtime() const {
        return m_openvr.get();
    }

    bool is_hmd_active() const {
        if (m_disable_vr) {
            return false;
        }

        auto runtime = get_runtime();

        if (runtime == nullptr) {
            return false;
        }

        return runtime->ready() || (m_stereo_emulation_mode && runtime->loaded);
    }

    auto get_hmd() const {
        return m_openvr->hmd;
    }

    auto& get_openvr_poses() const {
        return m_openvr->render_poses;
    }

    auto& get_overlay_component() {
        return m_overlay_component;
    }

    uint32_t get_hmd_width() const;
    uint32_t get_hmd_height() const;

    const auto& get_eyes() const {
        return get_runtime()->eyes;
    }

    auto get_frame_count() const {
        return m_frame_count;
    }

    auto& get_controllers() const {
        return m_controllers;
    }

    bool is_using_controllers() const {
        return m_controller_test_mode || (m_controllers_allowed &&
        is_hmd_active() && !m_controllers.empty() && (std::chrono::steady_clock::now() - m_last_controller_update) <= std::chrono::seconds((int32_t)m_motion_controls_inactivity_timer->value()));
    }

    bool is_using_controllers_within(std::chrono::seconds seconds) const {
        return is_hmd_active() && !m_controllers.empty() && (std::chrono::steady_clock::now() - m_last_controller_update) <= seconds;
    }

    int get_hmd_index() const {
        return 0;
    }

    int get_left_controller_index() const {
        if (m_runtime->is_openxr()) {
            return 1;
        } else if (m_runtime->is_openvr()) {
            return !m_controllers.empty() ? m_controllers[0] : -1;
        }

        return -1;
    }

    int get_right_controller_index() const {
        if (m_runtime->is_openxr()) {
            return 2;
        } else if (m_runtime->is_openvr()) {
            return !m_controllers.empty() ? m_controllers[1] : -1;
        }

        return -1;
    }

    auto get_left_joystick() const {
        if (!m_swap_controllers->value()) {
            return m_left_joystick;
        }

        return m_right_joystick;
    }

    auto get_right_joystick() const {
        if (!m_swap_controllers->value()) {
            return m_right_joystick;
        }

        return m_left_joystick;
    }

    bool is_gui_enabled() const {
        return m_enable_gui->value();
    }

    auto get_camera_forward_offset() const {
        return m_camera_forward_offset->value();
    }

    auto get_camera_right_offset() const {
        return m_camera_right_offset->value();
    }

    auto get_camera_up_offset() const {
        return m_camera_up_offset->value();
    }

    auto get_world_scale() const {
        return m_world_scale->value();
    }

    auto is_stereo_emulation_enabled() const {
        return m_stereo_emulation_mode;
    }

    void reset_present_event() {
        ResetEvent(m_present_finished_event);
    }

    void wait_for_present() {
        if (!m_wait_for_present) {
            return;
        }

        if (m_frame_count <= m_game_frame_count) {
            //return;
        }

        if (WaitForSingleObject(m_present_finished_event, 11) == WAIT_TIMEOUT) {
            //timed_out = true;
        }

        m_game_frame_count = m_frame_count;
        //ResetEvent(m_present_finished_event);
    }

    auto& get_vr_mutex() {
        return m_openvr_mtx;
    }

    bool is_using_afr() const {
        return m_rendering_method->value() == RenderingMethod::ALTERNATING || 
               m_rendering_method->value() == RenderingMethod::SYNCHRONIZED ||
               m_extreme_compat_mode->value() == true;
    }

    bool is_using_synchronized_afr() const {
        return m_rendering_method->value() == RenderingMethod::SYNCHRONIZED ||
               (m_extreme_compat_mode->value() && m_rendering_method->value() == RenderingMethod::NATIVE_STEREO);
    }

    SyncedSequentialMethod get_synced_sequential_method() const {
        return (SyncedSequentialMethod)m_synced_afr_method->value();
    }

    uint32_t get_lowest_xinput_index() const {
        return m_lowest_xinput_user_index;
    }

    auto& get_render_target_pool_hook() {
        return m_render_target_pool_hook;
    }

    void set_world_to_meters(float value) {
        m_world_to_meters = value;
    }

    float get_world_to_meters() const {
        return m_world_to_meters * m_world_scale->value();
    }

    float get_depth_scale() const {
        return m_depth_scale->value();
    }

    bool is_depth_enabled() const {
        return m_enable_depth->value();
    }

    bool is_decoupled_pitch_enabled() const {
        return m_decoupled_pitch->value();
    }

    bool is_decoupled_pitch_ui_adjust_enabled() const {
        return m_decoupled_pitch_ui_adjust->value();
    }

    void set_decoupled_pitch(bool value) {
        m_decoupled_pitch->value() = value;
    }

    void set_aim_allowed(bool value) {
        m_aim_temp_disabled = !value;
    }

    AimMethod get_aim_method() const {
        if (m_aim_temp_disabled) {
            return AimMethod::GAME;
        }

        return (AimMethod)m_aim_method->value();
    }

    AimMethod get_movement_orientation() const {
        return (AimMethod)m_movement_orientation->value();
    }

    float get_aim_speed() const {
        return m_aim_speed->value();
    }
    
    bool is_aim_multiplayer_support_enabled() const {
        return m_aim_multiplayer_support->value();
    }

    bool is_aim_pawn_control_rotation_enabled() const {
        return m_aim_use_pawn_control_rotation->value();
    }

    bool is_aim_modify_player_control_rotation_enabled() const {
        return m_aim_modify_player_control_rotation->value();
    }

    bool is_aim_interpolation_enabled() const {
        return m_aim_interp->value();
    }
    
    bool is_any_aim_method_active() const {
        return m_aim_method->value() > AimMethod::GAME && !m_aim_temp_disabled;
    }

    bool is_headlocked_aim_enabled() const {
        return m_aim_method->value() == AimMethod::HEAD && !m_aim_temp_disabled;
    }

    bool is_controller_aim_enabled() const {
        const auto value = m_aim_method->value();
        return !m_aim_temp_disabled && (value == AimMethod::LEFT_CONTROLLER || value == AimMethod::RIGHT_CONTROLLER || value == AimMethod::TWO_HANDED_LEFT || value == AimMethod::TWO_HANDED_RIGHT);
    }

    bool is_controller_movement_enabled() const {
        const auto value = m_movement_orientation->value();
        return value == AimMethod::LEFT_CONTROLLER || value == AimMethod::RIGHT_CONTROLLER || value == AimMethod::TWO_HANDED_LEFT || value == AimMethod::TWO_HANDED_RIGHT;
    }

    bool wants_blueprint_load() const {
        return m_load_blueprint_code->value();
    }

    bool is_splitscreen_compatibility_enabled() const {
        return m_splitscreen_compatibility_mode->value();
    }

    uint32_t get_requested_splitscreen_index() const {
        return m_splitscreen_view_index->value();
    }

    bool is_sceneview_compatibility_enabled() const {
        return m_sceneview_compatibility_mode->value();
    }

    bool is_ahud_compatibility_enabled() const {
        return m_compatibility_ahud->value();
    }

    bool is_ghosting_fix_enabled() const {
        return m_ghosting_fix->value();
    }

    auto& get_fake_stereo_hook() {
        return m_fake_stereo_hook;
    }

    void set_pre_flattened_rotation(const glm::quat& rot) {
        std::unique_lock _{m_decoupled_pitch_data.mtx};
        m_decoupled_pitch_data.pre_flattened_rotation = rot;
    }

    auto get_pre_flattened_rotation() const {
        std::shared_lock _{m_decoupled_pitch_data.mtx};
        return m_decoupled_pitch_data.pre_flattened_rotation;
    }

    bool is_using_2d_screen() const {
        return m_2d_screen_mode->value();
    }

    bool is_roomscale_enabled() const {
        return m_roomscale_movement->value() && !m_aim_temp_disabled;
    }

    bool is_dpad_shifting_enabled() const {
        return m_dpad_shifting->value();
    }

    DPadMethod get_dpad_method() const {
        return (DPadMethod)m_dpad_shifting_method->value();
    }

    bool is_snapturn_enabled() const {
        return m_snapturn->value();
    }

    float get_snapturn_js_deadzone() const {
        return m_snapturn_joystick_deadzone->value();
    }

    int get_snapturn_angle() const {
        return m_snapturn_angle->value();
    }

    float get_controller_pitch_offset() const {
        return m_controller_pitch_offset->value();
    }

    bool should_skip_post_init_properties() const {
        return m_compatibility_skip_pip->value();
    }
    
    bool should_skip_uobjectarray_init() const {
        return m_compatibility_skip_uobjectarray_init->value();
    }

    bool is_extreme_compatibility_mode_enabled() const {
        return m_extreme_compat_mode->value();
    }

    auto get_horiztonal_projection_override() const {
        return m_horizontal_projection_override->value();
    }

    auto get_vertical_projection_override() const {
        return m_vertical_projection_override->value();
    }

    bool should_grow_rectangle_for_projection_cropping() const {
        return m_grow_rectangle_for_projection_cropping->value();
    }

private:
    Vector4f get_position_unsafe(uint32_t index) const;
    Vector4f get_velocity_unsafe(uint32_t index) const;
    Vector4f get_angular_velocity_unsafe(uint32_t index) const;

private:
    std::optional<std::string> initialize_openvr();
    std::optional<std::string> initialize_openvr_input();
    std::optional<std::string> initialize_openxr();
    std::optional<std::string> initialize_openxr_input();
    std::optional<std::string> initialize_openxr_swapchains();

    bool detect_controllers();
    bool is_any_action_down();

    std::optional<std::string> reinitialize_openvr() {
        spdlog::info("Reinitializing OpenVR");
        std::scoped_lock _{m_openvr_mtx};

        m_runtime.reset();
        m_runtime = std::make_shared<VRRuntime>();
        m_openvr.reset();

        // Reinitialize openvr input, hopefully this fixes the issue
        m_controllers.clear();
        m_controllers_set.clear();

        auto e = initialize_openvr();

        if (e) {
            spdlog::error("Failed to reinitialize OpenVR: {}", *e);
        }

        return e;
    }

    std::optional<std::string> reinitialize_openxr() {
        spdlog::info("Reinitializing OpenXR");
        std::scoped_lock _{m_openvr_mtx};

        if (m_is_d3d12) {
            m_d3d12.openxr().destroy_swapchains();
        } else {
            m_d3d11.openxr().destroy_swapchains();
        }

        m_openxr.reset();
        m_runtime.reset();
        m_runtime = std::make_shared<VRRuntime>();
        
        m_controllers.clear();
        m_controllers_set.clear();

        auto e = initialize_openxr();

        if (e) {
            spdlog::error("Failed to reinitialize OpenXR: {}", *e);
        }

        return e;
    }

    float m_nearz{ 0.1f };
    float m_farz{ 3000.0f };
    float m_world_to_meters{1.0f}; // Placeholder, it gets set later in a hook

    std::unique_ptr<FFakeStereoRenderingHook> m_fake_stereo_hook{ std::make_unique<FFakeStereoRenderingHook>() };
    std::unique_ptr<RenderTargetPoolHook> m_render_target_pool_hook{ std::make_unique<RenderTargetPoolHook>() };
    std::unique_ptr<CVarManager> m_cvar_manager{ std::make_unique<CVarManager>() };

    std::shared_ptr<VRRuntime> m_runtime{std::make_shared<VRRuntime>()}; // will point to the real runtime if it exists
    std::shared_ptr<runtimes::OpenVR> m_openvr{std::make_shared<runtimes::OpenVR>()};
    std::shared_ptr<runtimes::OpenXR> m_openxr{std::make_shared<runtimes::OpenXR>()};

    mutable TracyLockable(std::recursive_mutex, m_openvr_mtx);
    mutable TracyLockable(std::recursive_mutex, m_reinitialize_mtx);
    mutable TracyLockable(std::recursive_mutex, m_actions_mtx);
    mutable std::shared_mutex m_rotation_mtx{};

    std::vector<int32_t> m_controllers{};
    std::unordered_set<int32_t> m_controllers_set{};

    glm::vec3 m_overlay_rotation{-1.550f, 0.0f, -1.330f};
    glm::vec4 m_overlay_position{0.0f, 0.06f, -0.07f, 1.0f};
    
    Vector4f m_standing_origin{ 0.0f, 1.5f, 0.0f, 0.0f };
    glm::quat m_rotation_offset{ glm::identity<glm::quat>() };

    HANDLE m_present_finished_event{CreateEvent(nullptr, TRUE, FALSE, nullptr)};

    Vector4f m_raw_projections[2]{};

    vrmod::D3D11Component m_d3d11{};
    vrmod::D3D12Component m_d3d12{};
    vrmod::OverlayComponent m_overlay_component;
    bool m_disable_overlay{false};

    // Action set handles
    vr::VRActionSetHandle_t m_action_set{};
    vr::VRActiveActionSet_t m_active_action_set{};

    // Action handles
    vr::VRActionHandle_t m_action_pose{ };
    vr::VRActionHandle_t m_action_trigger{ };
    vr::VRActionHandle_t m_action_grip{ };
    vr::VRActionHandle_t m_action_grip_pose{ };
    vr::VRActionHandle_t m_action_joystick{};
    vr::VRActionHandle_t m_action_joystick_click{};

    vr::VRActionHandle_t m_action_a_button_right{};
    vr::VRActionHandle_t m_action_a_button_touch_right{};
    vr::VRActionHandle_t m_action_b_button_right{};
    vr::VRActionHandle_t m_action_b_button_touch_right{};

    vr::VRActionHandle_t m_action_a_button_left{};
    vr::VRActionHandle_t m_action_a_button_touch_left{};
    vr::VRActionHandle_t m_action_b_button_left{};
    vr::VRActionHandle_t m_action_b_button_touch_left{};

    vr::VRActionHandle_t m_action_dpad_up{};
    vr::VRActionHandle_t m_action_dpad_right{};
    vr::VRActionHandle_t m_action_dpad_down{};
    vr::VRActionHandle_t m_action_dpad_left{};

    vr::VRActionHandle_t m_action_system_button{};
    vr::VRActionHandle_t m_action_haptic{};
    vr::VRActionHandle_t m_action_thumbrest_touch_left{};
    vr::VRActionHandle_t m_action_thumbrest_touch_right{};

    std::unordered_map<std::string, std::reference_wrapper<vr::VRActionHandle_t>> m_action_handles {
        { s_action_pose, m_action_pose },
        { s_action_grip_pose, m_action_grip_pose },
        { s_action_trigger, m_action_trigger },
        { s_action_grip, m_action_grip },
        { s_action_joystick, m_action_joystick },
        { s_action_joystick_click, m_action_joystick_click },

        { s_action_a_button_left, m_action_a_button_left },
        { s_action_b_button_left, m_action_b_button_left },
        { s_action_a_button_touch_left, m_action_a_button_touch_left },
        { s_action_b_button_touch_left, m_action_b_button_touch_left },

        { s_action_a_button_right, m_action_a_button_right },
        { s_action_b_button_right, m_action_b_button_right },
        { s_action_a_button_touch_right, m_action_a_button_touch_right },
        { s_action_b_button_touch_right, m_action_b_button_touch_right },

        { s_action_dpad_up, m_action_dpad_up },
        { s_action_dpad_right, m_action_dpad_right },
        { s_action_dpad_down, m_action_dpad_down },
        { s_action_dpad_left, m_action_dpad_left },

        { s_action_system_button, m_action_system_button },
        { s_action_thumbrest_touch_left, m_action_thumbrest_touch_left },
        { s_action_thumbrest_touch_right, m_action_thumbrest_touch_right },

        // Out
        { "/actions/default/out/Haptic", m_action_haptic },
    };

    // Input sources
    vr::VRInputValueHandle_t m_left_joystick{};
    vr::VRInputValueHandle_t m_right_joystick{};

    std::chrono::steady_clock::time_point m_last_controller_update{};
    std::chrono::steady_clock::time_point m_last_xinput_update{};
    std::chrono::steady_clock::time_point m_last_xinput_spoof_sent{};
    std::chrono::steady_clock::time_point m_last_xinput_l3_r3_menu_open{};
    std::chrono::steady_clock::time_point m_last_interaction_display{};
    std::chrono::steady_clock::time_point m_last_engine_tick{};

    uint32_t m_lowest_xinput_user_index{};

    std::chrono::nanoseconds m_last_input_delay{};
    std::chrono::nanoseconds m_avg_input_delay{};

    static const inline std::vector<std::string> s_rendering_method_names {
        "Native Stereo",
        "Synchronized Sequential",
        "Alternating/AFR",
    };

    static const inline std::vector<std::string> s_synced_afr_method_names {
        "Skip Tick",
        "Skip Draw",
    };

    static const inline std::vector<std::string> s_aim_method_names {
        "Game",
        "Head/HMD",
        "Right Controller",
        "Left Controller",
        "Two Handed (Right)",
        "Two Handed (Left)",
    };

    static const inline std::vector<std::string> s_dpad_method_names {
        "Right Thumbrest + Left Joystick",
        "Left Thumbrest + Right Joystick",
        "Left Joystick (Disables Standard Joystick Input)",
        "Right Joystick (Disables Standard Joystick Input)",
        "Gesture (Head) + Left Joystick",
        "Gesture (Head) + Right Joystick",
    };

    static const inline std::vector<std::string> s_horizontal_projection_override_names{
        "Raw / default",
        "Symmetrical",
        "Mirrored",
    };

    static const inline std::vector<std::string> s_vertical_projection_override_names{
        "Raw / default",
        "Symmetrical",
        "Matched",
    };

    const ModCombo::Ptr m_rendering_method{ ModCombo::create(generate_name("RenderingMethod"), s_rendering_method_names) };
    const ModCombo::Ptr m_synced_afr_method{ ModCombo::create(generate_name("SyncedSequentialMethod"), s_synced_afr_method_names, 1) };
    const ModToggle::Ptr m_extreme_compat_mode{ ModToggle::create(generate_name("ExtremeCompatibilityMode"), false, true) };
    const ModToggle::Ptr m_uncap_framerate{ ModToggle::create(generate_name("UncapFramerate"), true) };
    const ModToggle::Ptr m_disable_blur_widgets{ ModToggle::create(generate_name("DisableBlurWidgets"), true) };
    const ModToggle::Ptr m_disable_hdr_compositing{ ModToggle::create(generate_name("DisableHDRCompositing"), true, true) };
    const ModToggle::Ptr m_disable_hzbocclusion{ ModToggle::create(generate_name("DisableHZBOcclusion"), true, true) };
    const ModToggle::Ptr m_disable_instance_culling{ ModToggle::create(generate_name("DisableInstanceCulling"), true, true) };
    const ModToggle::Ptr m_desktop_fix{ ModToggle::create(generate_name("DesktopRecordingFix_V2"), true) };
    const ModToggle::Ptr m_enable_gui{ ModToggle::create(generate_name("EnableGUI"), true) };
    const ModToggle::Ptr m_enable_depth{ ModToggle::create(generate_name("EnableDepth"), false) };
    const ModToggle::Ptr m_decoupled_pitch{ ModToggle::create(generate_name("DecoupledPitch"), false) };
    const ModToggle::Ptr m_decoupled_pitch_ui_adjust{ ModToggle::create(generate_name("DecoupledPitchUIAdjust"), true) };
    const ModToggle::Ptr m_load_blueprint_code{ ModToggle::create(generate_name("LoadBlueprintCode"), false, true) };
    const ModToggle::Ptr m_2d_screen_mode{ ModToggle::create(generate_name("2DScreenMode"), false) };
    const ModToggle::Ptr m_roomscale_movement{ ModToggle::create(generate_name("RoomscaleMovement"), false) };
    const ModToggle::Ptr m_swap_controllers{ ModToggle::create(generate_name("SwapControllerInputs"), false) };
    const ModCombo::Ptr m_horizontal_projection_override{ModCombo::create(generate_name("HorizontalProjectionOverride"), s_horizontal_projection_override_names)};
    const ModCombo::Ptr m_vertical_projection_override{ModCombo::create(generate_name("VerticalProjectionOverride"), s_vertical_projection_override_names)};
    const ModToggle::Ptr m_grow_rectangle_for_projection_cropping{ModToggle::create(generate_name("GrowRectangleForProjectionCropping"), false)};

    // Snap turn settings and globals
    void gamepad_snapturn(XINPUT_STATE& state);
    void process_snapturn();
    
    const ModToggle::Ptr m_snapturn{ ModToggle::create(generate_name("SnapTurn"), false) };
    const ModSlider::Ptr m_snapturn_joystick_deadzone{ ModSlider::create(generate_name("SnapturnJoystickDeadzone"), 0.01f, 0.99f, 0.2f) };
    const ModInt32::Ptr m_snapturn_angle{ ModSliderInt32::create(generate_name("SnapturnTurnAngle"), 1, 359, 45) };
    bool m_snapturn_on_frame{false};
    bool m_snapturn_left{false};
    bool m_was_snapturn_run_on_input{false};

    const ModSlider::Ptr m_controller_pitch_offset{ ModSlider::create(generate_name("ControllerPitchOffset"), -90.0f, 90.0f, 0.0f) };

    // Aim method and movement orientation are not the same thing, but they can both have the same options
    const ModCombo::Ptr m_aim_method{ ModCombo::create(generate_name("AimMethod"), s_aim_method_names, AimMethod::GAME) };
    const ModCombo::Ptr m_movement_orientation{ ModCombo::create(generate_name("MovementOrientation"), s_aim_method_names, AimMethod::GAME) };
    AimMethod m_previous_aim_method{ AimMethod::GAME };
    const ModToggle::Ptr m_aim_use_pawn_control_rotation{ ModToggle::create(generate_name("AimUsePawnControlRotation"), false) };
    const ModToggle::Ptr m_aim_modify_player_control_rotation{ ModToggle::create(generate_name("AimModifyPlayerControlRotation"), false) };
    const ModToggle::Ptr m_aim_multiplayer_support{ ModToggle::create(generate_name("AimMPSupport"), false) };
    const ModToggle::Ptr m_aim_interp{ ModToggle::create(generate_name("AimInterp"), true, true) };
    const ModSlider::Ptr m_aim_speed{ ModSlider::create(generate_name("AimSpeed"), 0.01f, 25.0f, 15.0f) };
    const ModToggle::Ptr m_dpad_shifting{ ModToggle::create(generate_name("DPadShifting"), true) };
    const ModCombo::Ptr m_dpad_shifting_method{ ModCombo::create(generate_name("DPadShiftingMethod"), s_dpad_method_names, DPadMethod::RIGHT_TOUCH) };
    
    struct DPadGestureState {
        std::recursive_mutex mtx{};
        enum Direction : uint8_t {
            NONE,
            UP = 1 << 0,
            RIGHT = 1 << 1,
            DOWN = 1 << 2,
            LEFT = 1 << 3,
        };
        uint8_t direction{NONE};
    } m_dpad_gesture_state{};

    //const ModToggle::Ptr m_headlocked_aim{ ModToggle::create(generate_name("HeadLockedAim"), false) };
    //const ModToggle::Ptr m_headlocked_aim_controller_based{ ModToggle::create(generate_name("HeadLockedAimControllerBased"), false) };
    const ModSlider::Ptr m_motion_controls_inactivity_timer{ ModSlider::create(generate_name("MotionControlsInactivityTimer"), 30.0f, 100.0f, 30.0f) };
    const ModSlider::Ptr m_joystick_deadzone{ ModSlider::create(generate_name("JoystickDeadzone"), 0.01f, 0.9f, 0.2f) };
    const ModSlider::Ptr m_camera_forward_offset{ ModSlider::create(generate_name("CameraForwardOffset"), -4000.0f, 4000.0f, 0.0f) };
    const ModSlider::Ptr m_camera_right_offset{ ModSlider::create(generate_name("CameraRightOffset"), -4000.0f, 4000.0f, 0.0f) };
    const ModSlider::Ptr m_camera_up_offset{ ModSlider::create(generate_name("CameraUpOffset"), -4000.0f, 4000.0f, 0.0f) };
    const ModSlider::Ptr m_camera_fov_distance_multiplier{ ModSlider::create(generate_name("CameraFOVDistanceMultiplier"), 0.00f, 1000.0f, 0.0f) };
    const ModSlider::Ptr m_world_scale{ ModSlider::create(generate_name("WorldScale"), 0.01f, 10.0f, 1.0f) };
    const ModSlider::Ptr m_depth_scale{ ModSlider::create(generate_name("DepthScale"), 0.01f, 1.0f, 1.0f) };

    const ModToggle::Ptr m_ghosting_fix{ ModToggle::create(generate_name("GhostingFix"), false) };

    const ModSlider::Ptr m_custom_z_near{ ModSlider::create(generate_name("CustomZNear"), 0.001f, 100.0f, 0.01f, true) };
    const ModToggle::Ptr m_custom_z_near_enabled{ ModToggle::create(generate_name("EnableCustomZNear"), false, true) };

    const ModToggle::Ptr m_splitscreen_compatibility_mode{ ModToggle::create(generate_name("Compatibility_SplitScreen"), false, true) };
    const ModInt32::Ptr m_splitscreen_view_index{ ModInt32::create(generate_name("SplitscreenViewIndex"), 0, true) };

    const ModToggle::Ptr m_sceneview_compatibility_mode{ ModToggle::create(generate_name("Compatibility_SceneView"), false, true) };

    const ModToggle::Ptr m_compatibility_skip_pip{ ModToggle::create(generate_name("Compatibility_SkipPostInitProperties"), false, true) };
    const ModToggle::Ptr m_compatibility_skip_uobjectarray_init{ ModToggle::create(generate_name("Compatibility_SkipUObjectArrayInit"), false, true) };

    const ModToggle::Ptr m_compatibility_ahud{ ModToggle::create(generate_name("Compatibility_AHUD"), false, true) };

    // Keybinds
    const ModKey::Ptr m_keybind_recenter{ ModKey::create(generate_name("RecenterViewKey")) };
    const ModKey::Ptr m_keybind_set_standing_origin{ ModKey::create(generate_name("ResetStandingOriginKey")) };

    const ModKey::Ptr m_keybind_load_camera_0{ ModKey::create(generate_name("LoadCamera0Key")) };
    const ModKey::Ptr m_keybind_load_camera_1{ ModKey::create(generate_name("LoadCamera1Key")) };
    const ModKey::Ptr m_keybind_load_camera_2{ ModKey::create(generate_name("LoadCamera2Key")) };

    const ModKey::Ptr m_keybind_toggle_2d_screen{ ModKey::create(generate_name("Toggle2DScreenKey")) };
    const ModKey::Ptr m_keybind_disable_vr{ ModKey::create(generate_name("DisableVRKey")) };
    bool m_disable_vr{false}; // definitely should not be persistent

    const ModKey::Ptr m_keybind_toggle_gui{ ModKey::create(generate_name("ToggleSlateGUIKey")) };
    
    const ModString::Ptr m_requested_runtime_name{ ModString::create("Frontend_RequestedRuntime", "unset") };

    struct DecoupledPitchData {
        mutable std::shared_mutex mtx{};
        glm::quat pre_flattened_rotation{};
    } m_decoupled_pitch_data{};

    struct CameraFreeze {
        glm::vec3 position{};
        glm::vec3 rotation{}; // euler
        bool position_frozen{false};
        bool rotation_frozen{false};

        bool position_wants_freeze{false};
        bool rotation_wants_freeze{false};
    } m_camera_freeze{};

    struct CameraData {
        glm::vec3 offset{};
        float world_scale{1.0f};
        bool decoupled_pitch{false};
        bool decoupled_pitch_ui_adjust{true};
    };
    std::array<CameraData, 3> m_camera_datas{};
    void save_cameras();
    void load_cameras();
    void load_camera(int index);
    void save_camera(int index);

    bool m_stereo_emulation_mode{false}; // not a good config option, just for debugging
    bool m_wait_for_present{true};
    bool m_controllers_allowed{true};
    bool m_controller_test_mode{false};

    ValueList m_options{
        *m_rendering_method,
        *m_synced_afr_method,
        *m_extreme_compat_mode,
        *m_uncap_framerate,
        *m_disable_hdr_compositing,
        *m_disable_hzbocclusion,
        *m_disable_instance_culling,
        *m_desktop_fix,
        *m_enable_gui,
        *m_enable_depth,
        *m_decoupled_pitch,
        *m_decoupled_pitch_ui_adjust,
        *m_load_blueprint_code,
        *m_2d_screen_mode,
        *m_roomscale_movement,
        *m_swap_controllers,
        *m_horizontal_projection_override,
        *m_vertical_projection_override,
        *m_grow_rectangle_for_projection_cropping,
        *m_snapturn,
        *m_snapturn_joystick_deadzone,
        *m_snapturn_angle,
        *m_controller_pitch_offset,
        *m_aim_method,
        *m_movement_orientation,
        *m_aim_use_pawn_control_rotation,
        *m_aim_modify_player_control_rotation,
        *m_aim_multiplayer_support,
        *m_aim_speed,
        *m_aim_interp,
        *m_dpad_shifting,
        *m_dpad_shifting_method,
        *m_motion_controls_inactivity_timer,
        *m_joystick_deadzone,
        *m_camera_forward_offset,
        *m_camera_right_offset,
        *m_camera_up_offset,
        *m_world_scale,
        *m_depth_scale,
        *m_custom_z_near,
        *m_custom_z_near_enabled,
        *m_ghosting_fix,
        *m_splitscreen_compatibility_mode,
        *m_splitscreen_view_index,
        *m_compatibility_skip_pip,
        *m_compatibility_skip_uobjectarray_init,
        *m_compatibility_ahud,
        *m_sceneview_compatibility_mode,
        *m_keybind_recenter,
        *m_keybind_set_standing_origin,
        *m_keybind_load_camera_0,
        *m_keybind_load_camera_1,
        *m_keybind_load_camera_2,
        *m_keybind_toggle_2d_screen,
        *m_keybind_disable_vr,
        *m_keybind_toggle_gui,
        *m_requested_runtime_name,
    };
    

    int m_game_frame_count{};
    int m_frame_count{};
    int m_render_frame_count{};
    int m_last_frame_count{-1};
    int m_left_eye_frame_count{0};
    int m_right_eye_frame_count{0};

    bool m_submitted{false};

    // == 1 or == 0
    uint8_t m_left_eye_interval{0};
    uint8_t m_right_eye_interval{1};

    bool m_first_config_load{true};
    bool m_first_submit{true};
    bool m_is_d3d12{false};
    bool m_backbuffer_inconsistency{false};
    bool m_init_finished{false};
    bool m_has_hw_scheduling{false}; // hardware accelerated GPU scheduling
    bool m_spoofed_gamepad_connection{false};
    bool m_aim_temp_disabled{false};

    struct {
        bool draw{false};
        bool was_moving_left{false};
        bool was_moving_right{false};
        uint8_t page{0};
        uint8_t num_pages{3};
    } m_rt_modifier{};

    bool m_disable_projection_matrix_override{ false };
    bool m_disable_view_matrix_override{false};
    bool m_disable_backbuffer_size_override{false};

    struct XInputContext {
        struct PadContext {
            using Func = std::function<void(const XINPUT_STATE&, bool is_vr_controller)>;
            std::optional<Func> update{};
            XINPUT_STATE state{};
        };

        PadContext gamepad{};
        PadContext vr_controller{};
        
        TracyLockable(std::recursive_mutex, mtx);

        struct VRState {
            class StickState {
            public:
                bool was_pressed(bool current_state) {
                    if (!current_state) {
                        is_pressed = false;
                        return false;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    if (is_pressed && now - initial_press > std::chrono::milliseconds(500)) {
                        return true;
                    }

                    if (!is_pressed) {
                        initial_press = now;
                        is_pressed = true;
                        return true;
                    }

                    return false;
                } 
            
            private:
                std::chrono::steady_clock::time_point initial_press{};
                bool is_pressed{false};
            };

            StickState left_stick_up{};
            StickState left_stick_down{};
            StickState left_stick_left{};
            StickState left_stick_right{};
        } vr;

        void enqueue(bool is_vr_controller, const XINPUT_STATE& in_state, PadContext::Func func) {
            ZoneScopedN(__FUNCTION__);

            std::scoped_lock _{mtx};
            if (is_vr_controller) {
                vr_controller.update = func;
                vr_controller.state = in_state;
            } else {
                gamepad.update = func;
                gamepad.state = in_state;
            }
        }

        void update() {
            ZoneScopedN(__FUNCTION__);

            std::scoped_lock _{mtx};

            if (vr_controller.update) {
                (*vr_controller.update)(vr_controller.state, true);
                vr_controller.update.reset();
            }

            if (gamepad.update) {
                (*gamepad.update)(gamepad.state, false);
                gamepad.update.reset();
            }
        }

        bool headlocked_begin_held{false};
        bool menu_longpress_begin_held{false};
        std::chrono::steady_clock::time_point headlocked_begin{};
        std::chrono::steady_clock::time_point menu_longpress_begin{};
    } m_xinput_context{};

    static std::string actions_json;
    static std::string binding_rift_json;
    static std::string bindings_oculus_touch_json;
    static std::string binding_vive;
    static std::string bindings_vive_controller;
    static std::string bindings_knuckles;

    const std::unordered_map<std::string, std::string> m_binding_files {
        { "actions.json", actions_json },
        { "binding_rift.json", binding_rift_json },
        { "bindings_oculus_touch.json", bindings_oculus_touch_json },
        { "binding_vive.json", binding_vive },
        { "bindings_vive_controller.json", bindings_vive_controller },
        { "bindings_knuckles.json", bindings_knuckles }
    };

    friend class vrmod::D3D11Component;
    friend class vrmod::D3D12Component;
    friend class vrmod::OverlayComponent;
    friend class FFakeStereoRenderingHook;
};
