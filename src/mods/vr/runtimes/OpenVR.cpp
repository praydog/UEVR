#include "../../VR.hpp"

#include "OpenVR.hpp"

namespace runtimes {
VRRuntime::Error OpenVR::synchronize_frame(std::optional<uint32_t> frame_count) {
    if (this->got_first_poses && !this->is_hmd_active) {
        return VRRuntime::Error::SUCCESS;
    }

    if (this->frame_synced) {
        return VRRuntime::Error::SUCCESS;
    }

    std::unique_lock _{ this->pose_mtx };
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
    auto ret = vr::VRCompositor()->WaitGetPoses(this->real_render_poses.data(), vr::k_unMaxTrackedDeviceCount, this->real_game_poses.data(), vr::k_unMaxTrackedDeviceCount);

    if (ret == vr::VRCompositorError_None) {
        this->got_first_sync = true;
        this->frame_synced = true;
    }

    return (VRRuntime::Error)ret;
}

VRRuntime::Error OpenVR::update_poses(bool from_view_extensions, uint32_t frame_count) {
    if (!this->ready()) {
        return VRRuntime::Error::SUCCESS;
    }

    std::unique_lock _{ this->pose_mtx };

    memcpy(this->render_poses.data(), this->real_render_poses.data(), sizeof(this->render_poses));
    
    if (this->pose_action != 0 && this->left_controller_handle != vr::k_ulInvalidInputValueHandle && this->right_controller_handle != vr::k_ulInvalidInputValueHandle &&
        this->left_controller_index != vr::k_unTrackedDeviceIndexInvalid && this->right_controller_index != vr::k_unTrackedDeviceIndexInvalid) 
    {
        vr::InputPoseActionData_t left_pose_data{};
        vr::InputPoseActionData_t right_pose_data{};

        const auto res1 = vr::VRInput()->GetPoseActionDataForNextFrame(this->pose_action, vr::TrackingUniverseStanding, &left_pose_data, sizeof(left_pose_data), this->left_controller_handle);
        const auto res2 = vr::VRInput()->GetPoseActionDataForNextFrame(this->pose_action, vr::TrackingUniverseStanding, &right_pose_data, sizeof(right_pose_data), this->right_controller_handle);

        if (this->left_controller_index < this->render_poses.size()) {
            if (res1 == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&left_pose_data.pose.mDeviceToAbsoluteTracking };
                this->aim_matrices[VRRuntime::Hand::LEFT] = glm::rowMajor4(matrix);
            }

            const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->left_controller_index].mDeviceToAbsoluteTracking.m };
            this->grip_matrices[VRRuntime::Hand::LEFT] = glm::rowMajor4(matrix);
        }

        if (this->right_controller_index < this->render_poses.size()) {
            if (res2 == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&right_pose_data.pose.mDeviceToAbsoluteTracking };
                this->aim_matrices[VRRuntime::Hand::RIGHT] = glm::rowMajor4(matrix);
            }

            const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->right_controller_index].mDeviceToAbsoluteTracking.m };
            this->grip_matrices[VRRuntime::Hand::RIGHT] = glm::rowMajor4(matrix);
        }
    }

    bool should_enqueue = false;
    if (frame_count == 0) {
        frame_count = ++this->internal_frame_count;
        should_enqueue = true;
    } else {
        this->internal_frame_count = frame_count;
    }

    const auto& hmd_pose = this->render_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
    this->pose_queue[frame_count % this->pose_queue.size()] = hmd_pose;
    
    if (should_enqueue) {
        enqueue_render_poses_unsafe(frame_count); // because we've already locked the mutexes.
    }

    this->needs_pose_update = false;
    return VRRuntime::Error::SUCCESS;
}

VRRuntime::Error OpenVR::update_render_target_size() {
    this->hmd->GetRecommendedRenderTargetSize(&this->w, &this->h);

    return VRRuntime::Error::SUCCESS;
}

uint32_t OpenVR::get_width() const {
    return this->w;
}

uint32_t OpenVR::get_height() const {
    return this->h;
}

VRRuntime::Error OpenVR::consume_events(std::function<void(void*)> callback) {
    // Process OpenVR events
    vr::VREvent_t event{};
    while (this->hmd->PollNextEvent(&event, sizeof(event))) {
        if (callback) {
            callback(&event);
        }

        switch ((vr::EVREventType)event.eventType) {
            // Detect whether video settings changed
            case vr::VREvent_SteamVRSectionSettingChanged: {
                spdlog::info("VR: VREvent_SteamVRSectionSettingChanged");
                update_render_target_size();
            } break;

            // Detect whether SteamVR reset the standing/seated pose
            case vr::VREvent_SeatedZeroPoseReset: [[fallthrough]];
            case vr::VREvent_StandingZeroPoseReset: {
                spdlog::info("VR: VREvent_SeatedZeroPoseReset");
                this->wants_reset_origin = true;
            } break;

            case vr::VREvent_DashboardActivated: {
                this->handle_pause = true;
            } break;

            default:
                spdlog::info("VR: Unknown event: {}", (uint32_t)event.eventType);
                break;
        }
    }

    return VRRuntime::Error::SUCCESS;
}

VRRuntime::Error OpenVR::update_matrices(float nearz, float farz){
    std::unique_lock __{ this->eyes_mtx };
    const auto local_left = this->hmd->GetEyeToHeadTransform(vr::Eye_Left);
    const auto local_right = this->hmd->GetEyeToHeadTransform(vr::Eye_Right);

    this->eyes[vr::Eye_Left] = glm::rowMajor4(Matrix4x4f{ *(Matrix3x4f*)&local_left } );
    this->eyes[vr::Eye_Right] = glm::rowMajor4(Matrix4x4f{ *(Matrix3x4f*)&local_right } );

    //auto pleft = this->hmd->GetProjectionMatrix(vr::Eye_Left, nearz, farz);
    //auto pright = this->hmd->GetProjectionMatrix(vr::Eye_Right, nearz, farz);

    //this->projections[vr::Eye_Left] = glm::rowMajor4(Matrix4x4f{ *(Matrix4x4f*)&pleft } );
    //this->projections[vr::Eye_Right] = glm::rowMajor4(Matrix4x4f{ *(Matrix4x4f*)&pright } );

    this->hmd->GetProjectionRaw(vr::Eye_Left, &this->raw_projections[vr::Eye_Left][0], &this->raw_projections[vr::Eye_Left][1], &this->raw_projections[vr::Eye_Left][2], &this->raw_projections[vr::Eye_Left][3]);
    this->hmd->GetProjectionRaw(vr::Eye_Right, &this->raw_projections[vr::Eye_Right][0], &this->raw_projections[vr::Eye_Right][1], &this->raw_projections[vr::Eye_Right][2], &this->raw_projections[vr::Eye_Right][3]);

    auto get_mat = [&](vr::EVREye eye) {
        const auto left =   this->raw_projections[eye][0] * -1.0f;
        const auto right =  this->raw_projections[eye][1] * -1.0f;
        const auto top =    this->raw_projections[eye][2] * -1.0f;
        const auto bottom = this->raw_projections[eye][3] * -1.0f;
        float sum_rl = (left + right);
        float sum_tb = (top + bottom);
        float inv_rl = (1.0f / (left - right));
        float inv_tb = (1.0f / (top - bottom));

        return Matrix4x4f {
            (2.0f * inv_rl), 0.0f, 0.0f, 0.0f,
            0.0f, (2.0f * inv_tb), 0.0f, 0.0f,
            (sum_rl * inv_rl), (sum_tb * inv_tb), 0.0f, 1.0f,
            0.0f, 0.0f, nearz, 0.0f
        };
    };

    this->projections[vr::Eye_Left] = get_mat(vr::Eye_Left);
    this->projections[vr::Eye_Right] = get_mat(vr::Eye_Right);

    return VRRuntime::Error::SUCCESS;
}

void OpenVR::destroy() {
    if (this->loaded) {
        vr::VR_Shutdown();
    }
}

void  OpenVR::enqueue_render_poses(uint32_t frame_count) {
    std::unique_lock _{ this->pose_mtx };
    enqueue_render_poses_unsafe(frame_count);
}

void OpenVR::enqueue_render_poses_unsafe(uint32_t frame_count) {
    this->internal_render_frame_count = frame_count;
    this->has_render_frame_count = true;
}

vr::HmdMatrix34_t OpenVR::get_pose_for_submit() {
    std::unique_lock _{ this->pose_mtx };

    const auto frame_count = has_render_frame_count ? internal_render_frame_count : internal_frame_count;
    const auto out = get_hmd_pose(frame_count);

    this->has_render_frame_count = false;

    return out;
}
}

