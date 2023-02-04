#pragma once

#include <deque>
#include <openvr.h>

#include "VRRuntime.hpp"

namespace runtimes {
struct OpenVR final : public VRRuntime {
    OpenVR() {
        this->custom_stage = SynchronizeStage::VERY_LATE;
    }

    virtual ~OpenVR() {
        this->destroy();
    }

    std::string_view name() const override {
        return "OpenVR";
    }

    VRRuntime::Type type() const override { 
        return VRRuntime::Type::OPENVR;
    }

    bool ready() const override {
        return VRRuntime::ready() && this->is_hmd_active && this->got_first_poses;
    }

    VRRuntime::Error synchronize_frame() override;
    VRRuntime::Error update_poses(bool from_view_extensions = false, uint32_t frame_count = 0) override;
    VRRuntime::Error update_render_target_size() override;

    uint32_t get_width() const override;
    uint32_t get_height() const override;

    VRRuntime::Error consume_events(std::function<void(void*)> callback) override;
    VRRuntime::Error update_matrices(float nearz, float farz) override;

    void destroy() override;

    void enqueue_render_poses(uint32_t frame_count) override;
    void enqueue_render_poses_unsafe(uint32_t frame_count);

    vr::HmdMatrix34_t get_hmd_pose(uint32_t frame_count) const {
        return this->pose_queue[frame_count % this->pose_queue.size()];
    }

    vr::HmdMatrix34_t get_current_hmd_pose() const {
        return this->pose_queue[internal_frame_count % this->pose_queue.size()];
    }

    vr::HmdMatrix34_t get_pose_for_submit();

    void on_device_reset() override {
        std::unique_lock _{ this->pose_mtx };
    }

    bool is_hmd_active{false};
    bool was_hmd_active{true};

    uint32_t w{0};
    uint32_t h{0};

    vr::IVRSystem* hmd{nullptr};

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> real_render_poses;
    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> real_game_poses;

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> render_poses;
    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> game_poses;

    std::chrono::system_clock::time_point last_hmd_active_time{};

    std::array<vr::HmdMatrix34_t, 3> pose_queue{};
};
}