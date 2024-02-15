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
        this->got_first_valid_poses = true;
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

    const auto& vr = VR::get();
    const auto pitch_offset = vr->get_controller_pitch_offset();

    auto correct_rotation = [pitch_offset](const glm::mat4& mat) {
        if (pitch_offset == 0.0f) {
            return mat;
        }

        const auto rot = glm::extractMatrixRotation(mat);
        const auto rot_corrected = glm::rotate(rot, glm::radians(pitch_offset), glm::vec3{ 1.0f, 0.0f, 0.0f });

        auto new_mat = rot_corrected;
        new_mat[3] = mat[3];

        return new_mat;
    };

    memcpy(this->render_poses.data(), this->real_render_poses.data(), sizeof(this->render_poses));
    
    // Update grip and aim poses independently of render poses
    if (this->pose_action != vr::k_ulInvalidActionHandle && this->grip_pose_action != vr::k_ulInvalidActionHandle && 
        this->left_controller_handle != vr::k_ulInvalidInputValueHandle && this->right_controller_handle != vr::k_ulInvalidInputValueHandle &&
        this->left_controller_index != vr::k_unTrackedDeviceIndexInvalid && this->right_controller_index != vr::k_unTrackedDeviceIndexInvalid) 
    {
        vr::InputPoseActionData_t left_aim_pose_data{};
        vr::InputPoseActionData_t right_aim_pose_data{};
        vr::InputPoseActionData_t left_grip_pose_data{};
        vr::InputPoseActionData_t right_grip_pose_data{};

        const auto res1_aim = vr::VRInput()->GetPoseActionDataForNextFrame(this->pose_action, vr::TrackingUniverseStanding, &left_aim_pose_data, sizeof(left_aim_pose_data), this->left_controller_handle);
        const auto res2_aim = vr::VRInput()->GetPoseActionDataForNextFrame(this->pose_action, vr::TrackingUniverseStanding, &right_aim_pose_data, sizeof(right_aim_pose_data), this->right_controller_handle);
        const auto res1_grip = vr::VRInput()->GetPoseActionDataForNextFrame(this->grip_pose_action, vr::TrackingUniverseStanding, &left_grip_pose_data, sizeof(left_grip_pose_data), this->left_controller_handle);
        const auto res2_grip = vr::VRInput()->GetPoseActionDataForNextFrame(this->grip_pose_action, vr::TrackingUniverseStanding, &right_grip_pose_data, sizeof(right_grip_pose_data), this->right_controller_handle);


        if (this->left_controller_index < this->render_poses.size()) {
            if (res1_aim == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&left_aim_pose_data.pose.mDeviceToAbsoluteTracking };
                this->aim_matrices[VRRuntime::Hand::LEFT] = correct_rotation(glm::rowMajor4(matrix));
            } else {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->left_controller_index].mDeviceToAbsoluteTracking.m };
                this->aim_matrices[VRRuntime::Hand::LEFT] = correct_rotation(glm::rowMajor4(matrix));
            }

            if (res1_grip == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&left_grip_pose_data.pose.mDeviceToAbsoluteTracking };
                this->grip_matrices[VRRuntime::Hand::LEFT] = correct_rotation(glm::rowMajor4(matrix));
            } else {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->left_controller_index].mDeviceToAbsoluteTracking.m };
                this->grip_matrices[VRRuntime::Hand::LEFT] = correct_rotation(glm::rowMajor4(matrix));
            }
        }

        if (this->right_controller_index < this->render_poses.size()) {
            if (res2_aim == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&right_aim_pose_data.pose.mDeviceToAbsoluteTracking };
                this->aim_matrices[VRRuntime::Hand::RIGHT] = correct_rotation(glm::rowMajor4(matrix));
            } else {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->right_controller_index].mDeviceToAbsoluteTracking.m };
                this->aim_matrices[VRRuntime::Hand::RIGHT] = correct_rotation(glm::rowMajor4(matrix));
            }

            if (res2_grip == vr::VRInputError_None) {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&right_grip_pose_data.pose.mDeviceToAbsoluteTracking };
                this->grip_matrices[VRRuntime::Hand::RIGHT] = correct_rotation(glm::rowMajor4(matrix));
            } else {
                const auto matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->right_controller_index].mDeviceToAbsoluteTracking.m };
                this->grip_matrices[VRRuntime::Hand::RIGHT] = correct_rotation(glm::rowMajor4(matrix));
            }
        }
    } else if (left_controller_index < this->render_poses.size() && right_controller_index < this->render_poses.size()) {
        const auto left_matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->left_controller_index].mDeviceToAbsoluteTracking.m };
        const auto right_matrix = Matrix4x4f{ *(Matrix3x4f*)&this->render_poses[this->right_controller_index].mDeviceToAbsoluteTracking.m };

        this->grip_matrices[VRRuntime::Hand::LEFT] = correct_rotation(glm::rowMajor4(left_matrix));
        this->grip_matrices[VRRuntime::Hand::RIGHT] = correct_rotation(glm::rowMajor4(right_matrix));
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

// TODO: this is called 6 times per tick, which seems a bit generous
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
    // SPDLOG_INFO("Original left  {}, {}, {}, {}", this->raw_projections[0][0], this->raw_projections[0][1],this->raw_projections[0][2], this->raw_projections[0][3]);
    // SPDLOG_INFO("Original right {}, {}, {}, {}", this->raw_projections[1][0], this->raw_projections[1][1],this->raw_projections[1][2], this->raw_projections[1][3]);

    auto& vr = VR::get();
    auto get_mat = [&](vr::EVREye eye) {
        const auto tan_half_fov = new float[4];

        if (vr->get_horiztonal_projection_override() == VR::HORIZONTAL_PROJECTION_OVERRIDE::HORIZONTAL_SYMMETRIC) {
            // TODO: don't need to repeat this calculation for each eye?
            tan_half_fov[0] = std::max(std::max(-this->raw_projections[0][0], this->raw_projections[0][1]),
                                       std::max(-this->raw_projections[1][0], this->raw_projections[1][1]));
            tan_half_fov[1] = -tan_half_fov[0];
        } else if (vr->get_horiztonal_projection_override() == VR::HORIZONTAL_PROJECTION_OVERRIDE::HORIZONTAL_MIRROR) {
            float max_outer = std::max(-this->raw_projections[0][0], this->raw_projections[1][1]);
            float max_inner = std::max(this->raw_projections[0][1], -this->raw_projections[1][0]);
            tan_half_fov[0] = eye == 0 ? max_outer : max_inner;
            tan_half_fov[1] = eye == 0 ? -max_inner : -max_outer;
        } else {
            tan_half_fov[0] = -this->raw_projections[eye][0];
            tan_half_fov[1] = -this->raw_projections[eye][1];
        }

        if (vr->get_vertical_projection_override() == VR::VERTICAL_PROJECTION_OVERRIDE::VERTICAL_SYMMETRIC) {
            // TODO: don't need to repeat this calculation for each eye?
            tan_half_fov[2] = std::max(std::max(-this->raw_projections[0][2], this->raw_projections[0][3]),
                                       std::max(-this->raw_projections[1][2], this->raw_projections[1][3]));
            tan_half_fov[3] = -tan_half_fov[2];
        } else if (vr->get_vertical_projection_override() == VR::VERTICAL_PROJECTION_OVERRIDE::VERTICAL_MATCHED) {

            float max_top = std::max(-this->raw_projections[0][2], -this->raw_projections[1][2]);
            float max_bottom = std::max(this->raw_projections[0][3], this->raw_projections[1][3]);
            tan_half_fov[2] = max_top;
            tan_half_fov[3] = -max_bottom;
        } else {
            tan_half_fov[2] = -this->raw_projections[eye][2];
            tan_half_fov[3] = -this->raw_projections[eye][3];
        }
        m_view_bounds[eye][0] = 0.5f + 0.5f * this->raw_projections[eye][0] / tan_half_fov[0];
        m_view_bounds[eye][1] = 0.5f - 0.5f * this->raw_projections[eye][1] / tan_half_fov[1];
        m_view_bounds[eye][2] = 0.5f + 0.5f * this->raw_projections[eye][2] / tan_half_fov[2];
        m_view_bounds[eye][3] = 0.5f - 0.5f * this->raw_projections[eye][3] / tan_half_fov[3];
        const auto left =   tan_half_fov[0];
        const auto right =  tan_half_fov[1];
        const auto top =    tan_half_fov[2];
        const auto bottom = tan_half_fov[3];

        // signs : at this point we expect right [1] and bottom [3] to be negative
        // SPDLOG_INFO("derived for eye {} {}, {}, {}, {}", eye, left, right, top, bottom);
        // SPDLOG_INFO("derived bounds eye {} {}, {}, {}, {}", eye, m_view_bounds[eye][0], m_view_bounds[eye][1], m_view_bounds[eye][2], m_view_bounds[eye][3]);
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

