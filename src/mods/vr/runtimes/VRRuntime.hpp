#pragma once

#include <cstdint>
#include <string_view>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <array>

#include <spdlog/spdlog.h>
#include <sdk/Math.hpp>
#include <utility/Config.hpp>

struct VRRuntime {
    enum class Error : int64_t {
        UNSPECIFIED = -1,
        SUCCESS = 0,
        // rest of the error codes will be from the specific VR runtime
    };

    enum class Type : uint8_t {
        NONE,
        OPENXR,
        OPENVR,
    };

    enum class Eye : uint8_t {
        LEFT,
        RIGHT,
    };

    enum Hand : uint8_t {
        LEFT,
        RIGHT,
    };

    virtual ~VRRuntime() {};

    virtual std::string_view name() const {
        return "NONE";
    }

    virtual bool ready() const {
        return this->loaded;
    }

    virtual Type type() const { 
        return Type::NONE;
    }

    virtual void destroy() {
        this->loaded = false;
    }

    virtual Error synchronize_frame(std::optional<uint32_t> frame_count = std::nullopt) {
        return Error::SUCCESS;
    }

    virtual Error fix_frame() {
        if (!this->frame_synced) {
            synchronize_frame();
        }

        return Error::SUCCESS;
    }

    virtual Error update_poses(bool from_view_extensions = false, uint32_t frame_count = 0) {
        return Error::SUCCESS;
    }

    virtual Error update_render_target_size() {
        return Error::SUCCESS;
    }

    virtual Error consume_events(std::function<void(void*)> callback) {
        return Error::SUCCESS;
    }

    virtual uint32_t get_width() const {
        return 0;
    }

    virtual uint32_t get_height() const {
        return 0;
    }

    virtual Error update_matrices(float nearz, float farz) {
        return Error::SUCCESS;
    }

    virtual Error update_input() {
        return Error::SUCCESS;
    }

    virtual void enqueue_render_poses(uint32_t frame_count) {

    }

    virtual bool is_depth_allowed() const {
        return false;
    }

    virtual bool is_cylinder_layer_allowed() const {
        return false;
    }

    virtual void on_config_load(const utility::Config& cfg, bool set_defaults) {}
    virtual void on_config_save(utility::Config& cfg) {}
    virtual void on_draw_ui() {}
    virtual void on_device_reset() {}

    virtual void on_pre_render_game_thread(uint32_t frame_count) {};
    virtual void on_pre_render_render_thread(uint32_t frame_count) {};
    virtual void on_pre_render_rhi_thread(uint32_t frame_count) {};

    bool is_openxr() const {
        return this->type() == Type::OPENXR;
    }

    bool is_openvr() const {
        return this->type() == Type::OPENVR;
    }

    void handle_pause_select(bool systembutton_pressed) {
        const auto now = std::chrono::steady_clock::now();

        if (systembutton_pressed && !this->was_pause_button_pressed) {
            this->last_pause_press = now;
        }

        if (systembutton_pressed && this->was_pause_button_pressed) {
            if (now - this->last_pause_press > std::chrono::milliseconds(500)) {
                this->handle_select_button = true;
                this->handle_pause = false;
                this->last_select_press = now;
            }
        }

        if (this->was_pause_button_pressed && !systembutton_pressed && (now - this->last_select_press > std::chrono::milliseconds(500))) {
            this->handle_pause = true;
            this->handle_select_button = false;
        }

        this->was_pause_button_pressed = systembutton_pressed;
    }

    bool loaded{false};
    bool wants_reinitialize{false};
    bool dll_missing{false};

    // in the case of OpenVR we always need at least one initial WaitGetPoses before the game will render
    // even if we don't have anything to submit yet, otherwise the compositor
    // will return VRCompositorError_DoNotHaveFocus
    bool needs_pose_update{true};
    bool got_first_poses{false};
    bool got_first_valid_poses{false};
    bool got_first_sync{false};
    bool frame_synced{false};
    bool handle_pause{false};
    bool handle_select_button{false}; // long press on pause button
    bool was_pause_button_pressed{false};
    bool wants_reset_origin{true};

    std::chrono::steady_clock::time_point last_pause_press{};
    std::chrono::steady_clock::time_point last_select_press{};

    std::optional<std::string> error{};

    std::array<Matrix4x4f, 2> projections{};
    std::array<Matrix4x4f, 2> eyes{};
    std::array<Matrix4x4f, 2> aim_matrices{};
    std::array<Matrix4x4f, 2> grip_matrices{};

    mutable std::shared_mutex projections_mtx{};
    mutable std::shared_mutex eyes_mtx{};
    mutable std::shared_mutex pose_mtx{};

    Vector4f raw_projections[2]{};

    uint32_t internal_frame_count{};
    uint32_t internal_render_frame_count{};
    bool has_render_frame_count{false};

    // view bounds proportions - left xmin, xmax, ymin, ymax then right xmin, xmax, ymin, ymax
    // used to crop the rendered eye textures to account for projection adjustments
    float view_bounds[2][4] = {0, 1, 0, 1, 0, 1, 0, 1};

    // The world-locked aperture rectangle spatial mode looks through (mirrors
    // OverlayComponent::UIPlaneTransform, which can't be included here without a cycle).
    struct SpatialAperture {
        glm::vec3 center{};
        glm::vec3 right{};
        glm::vec3 up{};
        glm::vec3 normal{}; // toward the player
        float half_width{};
        float half_height{};
    };

    // Off-axis (fishtank) projection for one eye through the aperture, in the GAME's
    // runtime-agnostic convention; shared by OpenVR and OpenXR so the math cannot drift.
    // NB: the vertical skew must be (sum_tb * -inv_tb) -- flipping it inverts vertical parallax
    // (a swim that tracks head pitch). Also resets view_bounds[eye] to the full texture.
    void set_spatial_projection(uint32_t eye, const glm::vec3& eye_pos, const SpatialAperture& ap, float nearz) {
        // Eye offset in aperture axes; the render camera faces the aperture normal (frozen in
        // calculate_stereo_view_offset), so this is a standard off-axis frustum.
        const glm::vec3 e = eye_pos - ap.center;
        const float ex = glm::dot(e, ap.right);
        const float ey = glm::dot(e, ap.up);
        float d = glm::dot(e, ap.normal); // distance from the aperture plane (eye on the +N / player side)
        if (d < 0.05f) {
            d = 0.05f;
        }

        const float left   = (-ap.half_width  - ex) / d;
        const float right  = ( ap.half_width  - ex) / d;
        const float top    = ( ap.half_height - ey) / d;
        const float bottom = (-ap.half_height - ey) / d;

        const float sum_rl = right + left;
        const float sum_tb = top + bottom;
        const float inv_rl = 1.0f / (right - left);
        const float inv_tb = 1.0f / (top - bottom);

        this->projections[eye] = Matrix4x4f {
            (2.0f * inv_rl), 0.0f, 0.0f, 0.0f,
            0.0f, (2.0f * inv_tb), 0.0f, 0.0f,
            (sum_rl * -inv_rl), (sum_tb * -inv_tb), 0.0f, 1.0f,
            0.0f, 0.0f, nearz, 0.0f
        };

        this->view_bounds[eye][0] = 0.0f;
        this->view_bounds[eye][1] = 1.0f;
        this->view_bounds[eye][2] = 0.0f;
        this->view_bounds[eye][3] = 1.0f;
    }

    float last_eye_matrix_nearz = 0.01f;
    bool should_update_eye_matrices{true};
    bool should_recalculate_eye_projections{false};
    bool is_modifying_eye_texture_scale{false};

    // factor to scale the recommended eye texture size where we're cropping due to projection overrides, but
    // want to retain the final eye texture resolution
    float eye_width_adjustment{1};
    float eye_height_adjustment{1};
};