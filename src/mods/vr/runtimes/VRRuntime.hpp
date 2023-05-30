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

    enum class SynchronizeStage : int32_t { 
        EARLY,
        LATE,
        VERY_LATE
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

    virtual SynchronizeStage get_synchronize_stage() const {
        return this->custom_stage;
    }

    virtual Error update_input() {
        return Error::SUCCESS;
    }

    virtual void enqueue_render_poses(uint32_t frame_count) {

    }

    virtual bool is_depth_allowed() const {
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

    SynchronizeStage custom_stage{SynchronizeStage::EARLY};

    uint32_t internal_frame_count{};
    uint32_t internal_render_frame_count{};
    bool has_render_frame_count{false};
};