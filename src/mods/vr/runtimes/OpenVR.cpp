#include "../../VR.hpp"

#include "OpenVR.hpp"

namespace runtimes {
VRRuntime::Error OpenVR::synchronize_frame() {
    if (this->got_first_poses && !this->is_hmd_active) {
        return VRRuntime::Error::SUCCESS;
    }

    //std::unique_lock _{ this->pose_mtx };
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
    auto ret = vr::VRCompositor()->WaitGetPoses(this->real_render_poses.data(), vr::k_unMaxTrackedDeviceCount, this->real_game_poses.data(), vr::k_unMaxTrackedDeviceCount);

    if (ret == vr::VRCompositorError_None) {
        this->got_first_sync = true;
    }

    return (VRRuntime::Error)ret;
}

VRRuntime::Error OpenVR::update_poses() {
    if (!this->ready()) {
        return VRRuntime::Error::SUCCESS;
    }

    std::unique_lock _{ this->pose_mtx };

    //memcpy(this->render_poses.data(), this->real_render_poses.data(), sizeof(this->render_poses));

    // get the exact poses RIGHT NOW without using WaitGetPoses, we will pass these to VRTextureWithPose_t
    this->hmd->GetDeviceToAbsoluteTrackingPose(vr::ETrackingUniverseOrigin::TrackingUniverseStanding, vr::VRCompositor()->GetFrameTimeRemaining(), this->render_poses.data(), 1);
    this->pose_queue.push_back(this->render_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);

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

vr::HmdMatrix34_t OpenVR::get_pose_for_submit() {
    std::unique_lock _{ this->pose_mtx };

    if (this->pose_queue.size() > 2) {
        this->pose_queue.clear();
    }

    const auto last_pose = this->pose_queue.empty() ? this->real_render_poses[0].mDeviceToAbsoluteTracking : this->pose_queue.front();
    if (!this->pose_queue.empty()) {
        this->pose_queue.pop_front();
    }

    return last_pose;
}
}

