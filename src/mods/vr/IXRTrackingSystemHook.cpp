#include <unordered_map>

#include <bdshemu.h>
#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>
#include <utility/Emulation.hpp>
#include <utility/Patch.hpp>

#include "utility/Logging.hpp"

#include <sdk/Utility.hpp>
#include <sdk/UObjectArray.hpp>
#include <sdk/UClass.hpp>
#include <sdk/FProperty.hpp>
#include <sdk/ScriptVector.hpp>
#include <sdk/UGameplayStatics.hpp>
#include <sdk/APlayerController.hpp>
#include <sdk/APlayerCameraManager.hpp>

#include "../VR.hpp"

#include <sdk/vtables/IXRTrackingSystemVTables.hpp>
#include <sdk/structures/FXRMotionControllerData.hpp>
#include <sdk/structures/FXRHMDData.hpp>
#include <sdk/UHeadMountedDisplayFunctionLibrary.hpp>
#include <sdk/UFunction.hpp>

#include "IXRTrackingSystemHook.hpp"

detail::IXRTrackingSystemVT& get_tracking_system_vtable(std::optional<std::string> version_override = std::nullopt) {
    const auto str_version = version_override.has_value() ? version_override.value() : utility::narrow(sdk::search_for_version(utility::get_executable()).value_or(L"0.00"));
    auto version = sdk::get_file_version_info();

    if (str_version != "0.00") {
        SPDLOG_INFO("Found version {} from executable", str_version);
        version.dwFileVersionMS = 0;
    } else {
        SPDLOG_INFO("Found version {}.{} from executable (disk version)", HIWORD(version.dwFileVersionMS), LOWORD(version.dwFileVersionMS));
    }

    // TODO: actually dump 5.4
    if (version.dwFileVersionMS == 0x50004 || str_version.starts_with("5.4")) {
        return ue5_3::IXRTrackingSystemVT::get();
    }

    // >= 5.3
    if (version.dwFileVersionMS == 0x50003 || str_version.starts_with("5.3")) {
        return ue5_3::IXRTrackingSystemVT::get();
    }

    // TODO: actually dump 5.2
    // >= 5.2
    if (version.dwFileVersionMS == 0x50002 || str_version.starts_with("5.2")) {
        return ue5_1::IXRTrackingSystemVT::get();
    }

    // >= 5.1
    if (version.dwFileVersionMS == 0x50001 || str_version.starts_with("5.1")) {
        return ue5_1::IXRTrackingSystemVT::get();
    }

    // >= 5.0
    if (version.dwFileVersionMS == 0x50000 || str_version.starts_with("5.0")) {
        return ue5_00::IXRTrackingSystemVT::get();
    }

    // 4.27
    if (version.dwFileVersionMS == 0x4001B || str_version.starts_with("4.27")) {
        return ue4_27::IXRTrackingSystemVT::get();
    }

    // 4.26
    if (version.dwFileVersionMS == 0x4001A || str_version.starts_with("4.26")) {
        return ue4_26::IXRTrackingSystemVT::get();
    }
    
    // 4.25
    if (version.dwFileVersionMS == 0x40019 || str_version.starts_with("4.25")) {
        return ue4_25::IXRTrackingSystemVT::get();
    }

    // 4.24
    if (version.dwFileVersionMS == 0x40018 || str_version.starts_with("4.24")) {
        return ue4_24::IXRTrackingSystemVT::get();
    }

    // 4.23
    if (version.dwFileVersionMS == 0x40017 || str_version.starts_with("4.23")) {
        return ue4_23::IXRTrackingSystemVT::get();
    }

    // 4.22
    if (version.dwFileVersionMS == 0x40016 || str_version.starts_with("4.22")) {
        return ue4_22::IXRTrackingSystemVT::get();
    }

    // 4.21
    if (version.dwFileVersionMS == 0x40015 || str_version.starts_with("4.21")) {
        return ue4_21::IXRTrackingSystemVT::get();
    }

    // 4.20
    if (version.dwFileVersionMS == 0x40014 || str_version.starts_with("4.20")) {
        return ue4_20::IXRTrackingSystemVT::get();
    }

    // 4.19
    if (version.dwFileVersionMS == 0x40013 || str_version.starts_with("4.19")) {
        return ue4_19::IXRTrackingSystemVT::get();
    }

    // 4.18
    if (version.dwFileVersionMS == 0x40012 || str_version.starts_with("4.18")) {
        return ue4_18::IXRTrackingSystemVT::get();
    }

    // versions lower than 4.18 do not have IXRTrackingSystem
    return detail::IXRTrackingSystemVT::get();
}


detail::IXRCameraVT& get_camera_vtable(std::optional<std::string> version_override = std::nullopt) {
    const auto str_version = version_override.has_value() ? version_override.value() : utility::narrow(sdk::search_for_version(utility::get_executable()).value_or(L"0.00"));
    auto version = sdk::get_file_version_info();

    if (str_version != "0.00") {
        version.dwFileVersionMS = 0;
    }

    // TODO: actually dump 5.4
    if (version.dwFileVersionMS == 0x50004 || str_version.starts_with("5.4")) {
        return ue5_3::IXRCameraVT::get();
    }

    // TODO: actually dump 5.2
    if (version.dwFileVersionMS == 0x50003 || str_version.starts_with("5.3")) {
        return ue5_3::IXRCameraVT::get();
    }

    // >= 5.2
    if (version.dwFileVersionMS == 0x50002 || str_version.starts_with("5.2")) {
        return ue5_1::IXRCameraVT::get();
    }

    // >= 5.1
    if (version.dwFileVersionMS == 0x50001 || str_version.starts_with("5.1")) {
        return ue5_1::IXRCameraVT::get();
    }

    // >= 5.0
    if (version.dwFileVersionMS == 0x50000 || str_version.starts_with("5.0")) {
        return ue5_00::IXRCameraVT::get();
    }

    // 4.27
    if (version.dwFileVersionMS == 0x4001B || str_version.starts_with("4.27")) {
        return ue4_27::IXRCameraVT::get();
    }

    // 4.26
    if (version.dwFileVersionMS == 0x4001A || str_version.starts_with("4.26")) {
        return ue4_26::IXRCameraVT::get();
    }

    // 4.25
    if (version.dwFileVersionMS == 0x40019 || str_version.starts_with("4.25")) {
        return ue4_25::IXRCameraVT::get();
    }

    // 4.24
    if (version.dwFileVersionMS == 0x40018 || str_version.starts_with("4.24")) {
        return ue4_24::IXRCameraVT::get();
    }

    // 4.23
    if (version.dwFileVersionMS == 0x40017 || str_version.starts_with("4.23")) {
        return ue4_23::IXRCameraVT::get();
    }

    // 4.22
    if (version.dwFileVersionMS == 0x40016 || str_version.starts_with("4.22")) {
        return ue4_22::IXRCameraVT::get();
    }

    // 4.21
    if (version.dwFileVersionMS == 0x40015 || str_version.starts_with("4.21")) {
        return ue4_21::IXRCameraVT::get();
    }

    // 4.20
    if (version.dwFileVersionMS == 0x40014 || str_version.starts_with("4.20")) {
        return ue4_20::IXRCameraVT::get();
    }

    // 4.19
    if (version.dwFileVersionMS == 0x40013 || str_version.starts_with("4.19")) {
        return ue4_19::IXRCameraVT::get();
    }

    // 4.18
    if (version.dwFileVersionMS == 0x40012 || str_version.starts_with("4.18")) {
        return ue4_18::IXRCameraVT::get();
    }

    // Versions lower than 4.18 do not have IXRCamera
    return detail::IXRCameraVT::get();
}

detail::IHeadMountedDisplayVT& get_hmd_vtable(std::optional<std::string> version_override = std::nullopt) {
    const auto str_version = version_override.has_value() ? version_override.value() : utility::narrow(sdk::search_for_version(utility::get_executable()).value_or(L"0.00"));
    auto version = sdk::get_file_version_info();

    if (str_version != "0.00") {
        version.dwFileVersionMS = 0;
    }

    // TODO: actually dump 5.4
    if (version.dwFileVersionMS == 0x50004 || str_version.starts_with("5.4")) {
        return ue5_3::IHeadMountedDisplayVT::get();
    }

    // 5.3
    if (version.dwFileVersionMS == 0x50003 || str_version.starts_with("5.3")) {
        return ue5_3::IHeadMountedDisplayVT::get();
    }

    // TODO: actually dump 5.2
    // >= 5.2
    if (version.dwFileVersionMS == 0x50002 || str_version.starts_with("5.2")) {
        return ue5_1::IHeadMountedDisplayVT::get();
    }

    // >= 5.1
    if (version.dwFileVersionMS == 0x50001 || str_version.starts_with("5.1")) {
        return ue5_1::IHeadMountedDisplayVT::get();
    }

    // >= 5.0
    if (version.dwFileVersionMS == 0x50000 || str_version.starts_with("5.0")) {
        return ue5_00::IHeadMountedDisplayVT::get();
    }

    // 4.27
    if (version.dwFileVersionMS == 0x4001B || str_version.starts_with("4.27")) {
        return ue4_27::IHeadMountedDisplayVT::get();
    }

    // 4.26
    if (version.dwFileVersionMS == 0x4001A || str_version.starts_with("4.26")) {
        return ue4_26::IHeadMountedDisplayVT::get();
    }

    // 4.25
    if (version.dwFileVersionMS == 0x40019 || str_version.starts_with("4.25")) {
        return ue4_25::IHeadMountedDisplayVT::get();
    }

    // 4.24
    if (version.dwFileVersionMS == 0x40018 || str_version.starts_with("4.24")) {
        return ue4_24::IHeadMountedDisplayVT::get();
    }

    // 4.23
    if (version.dwFileVersionMS == 0x40017 || str_version.starts_with("4.23")) {
        return ue4_23::IHeadMountedDisplayVT::get();
    }

    // 4.22
    if (version.dwFileVersionMS == 0x40016 || str_version.starts_with("4.22")) {
        return ue4_22::IHeadMountedDisplayVT::get();
    }

    // 4.21
    if (version.dwFileVersionMS == 0x40015 || str_version.starts_with("4.21")) {
        return ue4_21::IHeadMountedDisplayVT::get();
    }

    // 4.20
    if (version.dwFileVersionMS == 0x40014 || str_version.starts_with("4.20")) {
        return ue4_20::IHeadMountedDisplayVT::get();
    }

    // 4.19
    if (version.dwFileVersionMS == 0x40013 || str_version.starts_with("4.19")) {
        return ue4_19::IHeadMountedDisplayVT::get();
    }

    // 4.18
    if (version.dwFileVersionMS == 0x40012 || str_version.starts_with("4.18")) {
        return ue4_18::IHeadMountedDisplayVT::get();
    }

    // 4.17
    if (version.dwFileVersionMS == 0x40011 || str_version.starts_with("4.17")) {
        return ue4_17::IHeadMountedDisplayVT::get();
    }

    // 4.16
    if (version.dwFileVersionMS == 0x40010 || str_version.starts_with("4.16")) {
        return ue4_16::IHeadMountedDisplayVT::get();
    }

    // 4.15
    if (version.dwFileVersionMS == 0x4000F || str_version.starts_with("4.15")) {
        return ue4_15::IHeadMountedDisplayVT::get();
    }

    // 4.14
    if (version.dwFileVersionMS == 0x4000E || str_version.starts_with("4.14")) {
        return ue4_14::IHeadMountedDisplayVT::get();
    }

    // 4.13
    if (version.dwFileVersionMS == 0x4000D || str_version.starts_with("4.13")) {
        return ue4_13::IHeadMountedDisplayVT::get();
    }

    // 4.12
    if (version.dwFileVersionMS == 0x4000C || str_version.starts_with("4.12")) {
        return ue4_12::IHeadMountedDisplayVT::get();
    }

    // 4.11
    if (version.dwFileVersionMS == 0x4000B || str_version.starts_with("4.11")) {
        return ue4_11::IHeadMountedDisplayVT::get();
    }

    // 4.10
    if (version.dwFileVersionMS == 0x4000A || str_version.starts_with("4.10")) {
        return ue4_10::IHeadMountedDisplayVT::get();
    }

    return detail::IHeadMountedDisplayVT::get();
}

IXRTrackingSystemHook* g_hook = nullptr;

namespace detail {
struct FunctionInfo {
    size_t functions_within{0};
    bool calls_xr_camera{false};
    bool calls_update_player_camera{false};
    bool calls_apply_hmd_rotation{false};
    bool process_view_rotation_analysis_failed{false};
};

std::mutex return_address_to_functions_mutex{};
std::unordered_map<uintptr_t, uintptr_t> return_address_to_functions{};
std::unordered_map<uintptr_t, FunctionInfo> functions{};
std::atomic<uint32_t> total_times_funcs_called{0};
}

IXRTrackingSystemHook::IXRTrackingSystemHook(FFakeStereoRenderingHook* stereo_hook, size_t offset_in_engine) 
    : m_stereo_hook{stereo_hook},
    m_offset_in_engine{offset_in_engine}
{
    SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook");

    g_hook = this;

    pre_initialize();
}

void IXRTrackingSystemHook::pre_initialize() {
    SPDLOG_INFO("IXRTrackingSystemHook::pre_initialize");

    for (auto i = 0; i < m_xrtracking_vtable.size(); ++i) {
        m_xrtracking_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    for (auto i = 0; i < m_camera_vtable.size(); ++i) {
        m_camera_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    for (auto i = 0; i < m_hmd_vtable.size(); ++i) {
        m_hmd_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    for (auto i = 0; i < m_stereo_rendering_vtable.size(); ++i) {
        m_stereo_rendering_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    for (auto i = 0; i < m_view_extension_vtable.size(); ++i) {
        m_view_extension_vtable[i] = (uintptr_t)+[](void*) {
            return nullptr;
        };
    }

    // GetSystemName
    m_xrtracking_vtable[0] = (uintptr_t)+[](void* this_ptr, sdk::FName* out) -> sdk::FName* {
        static sdk::FName fake_name{};
        return &fake_name;
    };

    const auto version = sdk::get_file_version_info();
    const auto str_version = utility::narrow(sdk::search_for_version(utility::get_executable()).value_or(L"0.00"));
    
    try {
        const auto first_half = std::stoi(str_version.substr(0, str_version.find('.')));
        const auto second_half = std::stoi(str_version.substr(str_version.find('.') + 1, str_version.size() - 1));

        if (first_half == 4 && second_half == 26) {
            m_is_4_26 = true;
        }

        if (first_half == 4 && second_half <= 25) {
            SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: version <= 4.25");
            m_is_leq_4_25 = true;
        }

        if (first_half == 4 && second_half <= 17) {
            SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: version <= 4.17");
            m_is_leq_4_17 = true;
        }
    } catch(...) {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: failed to convert second half of version string to number");
    }

    if (!m_is_leq_4_25 && version.dwFileVersionMS >= 0x40000 && version.dwFileVersionMS <= 0x40019) {
        SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: version <= 4.25");
        m_is_leq_4_25 = true;
    }

    if (!m_is_leq_4_17 && version.dwFileVersionMS >= 0x40000 && version.dwFileVersionMS <= 0x40011) {
        SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: version <= 4.17");
        m_is_leq_4_17 = true;
    }
}

void IXRTrackingSystemHook::on_draw_ui() {
}

void IXRTrackingSystemHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    auto& vr = VR::get();

    if (!m_initialized && (vr->is_any_aim_method_active() || vr->wants_blueprint_load())) {
        if (!m_initialized) {
            initialize();
        }
    }

    if (vr->is_any_aim_method_active()) {
        auto& data = m_process_view_rotation_data;

        // This can happen if player logic stops running (e.g. player has died or entered a loading screen)
        // so we dont want the UI off in nowhere land
        if (data.was_called && std::chrono::high_resolution_clock::now() - data.last_update >= std::chrono::seconds(2)) {
            data.was_called = false;
            vr->recenter_view();
            vr->set_pre_flattened_rotation(glm::identity<glm::quat>());

            SPDLOG_INFO("IXRTrackingSystemHook: Recentering view because of timeout");
        }
    }
}

void IXRTrackingSystemHook::initialize() {
    m_initialized = true;

    SPDLOG_INFO("IXRTrackingSystemHook::initialize");

    if (sdk::UGameEngine::get() == nullptr) {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: UGameEngine not found");
        return;
    }

    if (m_offset_in_engine == 0) {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: m_offset_in_engine not set");
        return;
    }

    const auto& camera_vt = get_camera_vtable(m_overridden_version);
    const auto& trackvt = get_tracking_system_vtable(m_overridden_version);
    const auto& hmdvt = get_hmd_vtable(m_overridden_version);

    if (trackvt.implemented()) {
        if (trackvt.GetXRCamera_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetXRCamera_index().value()] = (uintptr_t)&get_xr_camera;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_xr_camera_index not implemented");
        }

        if (trackvt.IsHeadTrackingAllowed_index().has_value()) {
            m_xrtracking_vtable[trackvt.IsHeadTrackingAllowed_index().value()] = (uintptr_t)&is_head_tracking_allowed;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_head_tracking_allowed_index not implemented");
        }

        if (trackvt.IsHeadTrackingAllowedForWorld_index().has_value()) {
            m_xrtracking_vtable[trackvt.IsHeadTrackingAllowedForWorld_index().value()] = (uintptr_t)&is_head_tracking_allowed_for_world;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_head_tracking_allowed_for_world_index not implemented");
        }

        if (trackvt.GetMotionControllerData_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetMotionControllerData_index().value()] = (uintptr_t)&get_motion_controller_data;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_motion_controller_data_index not implemented");
        }

        if (trackvt.GetHMDData_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetHMDData_index().value()] = (uintptr_t)&get_hmd_data;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_hmd_data_index not implemented");
        }

        if (trackvt.GetCurrentPose_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetCurrentPose_index().value()] = (uintptr_t)&get_current_pose;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_current_pose_index not implemented");
        }

        if (trackvt.GetXRSystemFlags_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetXRSystemFlags_index().value()] = (uintptr_t)&get_xr_system_flags;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_xr_system_flags_index not implemented");
        }

        // Doesn't cause a crash, but must be implemented to fix audio bugs
        if (trackvt.GetAudioListenerOffset_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetAudioListenerOffset_index().value()] = (uintptr_t)&get_audio_listener_offset;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_audio_listener_offset_index not implemented");
        }

        // Some games calls this for some reason so it needs to be implemented so we dont crash
        if (trackvt.GetBaseOrientation_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetBaseOrientation_index().value()] = (uintptr_t)&get_base_orientation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_base_orientation_index not implemented");
        }

        if (trackvt.GetBasePosition_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetBasePosition_index().value()] = (uintptr_t)&get_base_position;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_base_position_index not implemented");
        }

        if (trackvt.GetBaseRotation_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetBaseRotation_index().value()] = (uintptr_t)&get_base_rotation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_base_rotation_index not implemented");
        }

        if (trackvt.ResetOrientation_index().has_value()) {
            m_xrtracking_vtable[trackvt.ResetOrientation_index().value()] = (uintptr_t)&reset_orientation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_orientation_index not implemented");
        }

        if (trackvt.ResetPosition_index().has_value()) {
            m_xrtracking_vtable[trackvt.ResetPosition_index().value()] = (uintptr_t)&reset_position;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_position_index not implemented");
        }

        if (trackvt.ResetOrientationAndPosition_index().has_value()) {
            m_xrtracking_vtable[trackvt.ResetOrientationAndPosition_index().value()] = (uintptr_t)&reset_orientation_and_position;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_orientation_and_position_index not implemented");
        }

        if (m_is_4_26) {
            if (trackvt.GetStereoRenderingDevice_index().has_value()) {
                m_xrtracking_vtable[trackvt.GetStereoRenderingDevice_index().value()] = (uintptr_t)&get_stereo_rendering_device;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_stereo_rendering_device_index not implemented");
            }
        } else {
            if (trackvt.GetStereoRenderingDevice_index().has_value()) {
                m_xrtracking_vtable[trackvt.GetStereoRenderingDevice_index().value()] = (uintptr_t)+[](void* a1, void* a2, void* a3) -> void* {
                    SPDLOG_INFO_ONCE("GetStereoRenderingDevice called");

                    return nullptr;
                };
            }
        }

        if (hmdvt.implemented() && trackvt.GetHMDDevice_index().has_value()) {
            m_xrtracking_vtable[trackvt.GetHMDDevice_index().value()] = (uintptr_t)+[]() -> void* {
                SPDLOG_INFO_ONCE("GetHMDDevice called");

                return &g_hook->m_hmd_device;
            };

            if (hmdvt.IsHMDConnected_index().has_value()) {
                m_hmd_vtable[hmdvt.IsHMDConnected_index().value()] = (uintptr_t)&is_hmd_connected;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_hmd_connected_index not implemented");
            }

            if (hmdvt.GetIdealDebugCanvasRenderTargetSize_index().has_value()) {
                // This one is a bit tricky. In very rare cases this index can be off by one. We need to make the hook
                // verify that the return address is within UGameViewportClient::Draw. We will not just hook this function, but one index ahead as well.
                m_hmd_vtable[hmdvt.GetIdealDebugCanvasRenderTargetSize_index().value()] = (uintptr_t)&get_ideal_debug_canvas_render_target_size;
                m_hmd_vtable[hmdvt.GetIdealDebugCanvasRenderTargetSize_index().value() + 1] = (uintptr_t)&get_ideal_debug_canvas_render_target_size;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_ideal_debug_canvas_render_target_size_index not implemented");
            }

            if (hmdvt.ResetOrientation_index().has_value()) {
                m_hmd_vtable[hmdvt.ResetPosition_index().value()] = (uintptr_t)&reset_orientation;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_orientation_index not implemented");
            }

            if (hmdvt.ResetPosition_index().has_value()) {
                m_hmd_vtable[hmdvt.ResetPosition_index().value()] = (uintptr_t)&reset_position;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_position_index not implemented");
            }

            if (hmdvt.ResetOrientationAndPosition_index().has_value()) {
                m_hmd_vtable[hmdvt.ResetOrientationAndPosition_index().value()] = (uintptr_t)&reset_orientation_and_position;
            } else {
                SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: reset_orientation_and_position_index not implemented");
            }

            m_hmd_device.vtable = m_hmd_vtable.data();
            m_hmd_device.stereo_rendering_vtable = m_stereo_rendering_vtable.data();
        }
    } else if (hmdvt.implemented()) {
        SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook: IXRTrackingSystemVT not implemented, using IHeadMountedDisplayVT");

        if (hmdvt.IsHeadTrackingAllowed_index().has_value()) {
            m_hmd_vtable[hmdvt.IsHeadTrackingAllowed_index().value()] = (uintptr_t)&is_head_tracking_allowed;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_head_tracking_allowed_index not implemented");
        }

        if (hmdvt.ApplyHmdRotation_index().has_value()) {
            m_hmd_vtable[hmdvt.ApplyHmdRotation_index().value()] = (uintptr_t)&apply_hmd_rotation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: apply_hmd_rotation_index not implemented");
        }

        if (hmdvt.IsHMDConnected_index().has_value()) {
            m_hmd_vtable[hmdvt.IsHMDConnected_index().value()] = (uintptr_t)&is_hmd_connected;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: is_hmd_connected_index not implemented");
        }

        if (hmdvt.GetAudioListenerOffset_index().has_value()) {
            m_hmd_vtable[hmdvt.GetAudioListenerOffset_index().value()] = (uintptr_t)&get_audio_listener_offset;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_audio_listener_offset_index not implemented");
        }

        if (hmdvt.UpdatePlayerCamera_index().has_value()) {
            m_hmd_vtable[hmdvt.UpdatePlayerCamera_index().value()] = (uintptr_t)&update_player_camera;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: update_player_camera_index not implemented");
        }

        if (hmdvt.GetViewExtension_index().has_value()) {
            m_hmd_vtable[hmdvt.GetViewExtension_index().value()] = (uintptr_t)&get_view_extension;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: get_view_extension_index not implemented");
        }
    } else {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: IXRTrackingSystemVT and IXRHeadMountedDisplayVT not implemented");
    }

    if (camera_vt.implemented()) {
        if (camera_vt.ApplyHMDRotation_index().has_value()) {
            m_camera_vtable[camera_vt.ApplyHMDRotation_index().value()] = (uintptr_t)&apply_hmd_rotation;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: apply_hmd_rotation_index not implemented");
        }

        if (camera_vt.UpdatePlayerCamera_index().has_value()) {
            m_camera_vtable[camera_vt.UpdatePlayerCamera_index().value()] = (uintptr_t)&update_player_camera;
        } else {
            SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: update_player_camera_index not implemented");
        }
    } else {
        SPDLOG_ERROR("IXRTrackingSystemHook::IXRTrackingSystemHook: IXRCameraVT not implemented");
    }

    m_view_extension.vtable = m_view_extension_vtable.data();
    m_view_extension_shared.obj = &m_view_extension;
    m_view_extension_shared.ref_controller = nullptr;

    if (camera_vt.implemented()) {
        m_xr_camera.vtable = m_camera_vtable.data();
        m_xr_camera_shared.obj = &m_xr_camera;
    }

    if (trackvt.implemented()) { // >= 4.18
        auto tracking_system = (SharedPtr*)((uintptr_t)sdk::UGameEngine::get() + m_offset_in_engine);

        m_xr_tracking_system.vtable = m_xrtracking_vtable.data();
        tracking_system->obj = &m_xr_tracking_system;
        tracking_system->ref_controller = nullptr;
        //tracking_system->ref_controller = m_ref_controller.get();
    } else if (hmdvt.implemented()) { // <= 4.17
        auto hmd_device = (SharedPtr*)((uintptr_t)sdk::UGameEngine::get() + m_offset_in_engine);

        m_hmd_device.vtable = m_hmd_vtable.data();
        m_hmd_device.stereo_rendering_vtable = m_stereo_rendering_vtable.data();
        hmd_device->obj = &m_hmd_device;
        hmd_device->ref_controller = nullptr;
    }

    SPDLOG_INFO("IXRTrackingSystemHook::IXRTrackingSystemHook done");
}

IXRTrackingSystemHook::SharedPtr* IXRTrackingSystemHook::get_stereo_rendering_device(sdk::IXRTrackingSystem* This, IXRTrackingSystemHook::SharedPtr* out, void* a3) {
    // So we are not actually using this for anything right now, we are just using it to switch our vtables to 4.27 if needed.
    // Not actually aware of any instance where we would legitimately need to return a valid stereo device.
    if (!g_hook->m_is_4_26) {
        return nullptr;
    }

    SPDLOG_INFO_ONCE("IXRTrackingSystemHook::get_stereo_rendering_device");

    static std::mutex mtx{};
    static std::unordered_set<uintptr_t> invalid_return_addresses{};

    const auto return_address = (uintptr_t)_ReturnAddress(); 
    
    std::scoped_lock _{mtx};

    if (invalid_return_addresses.contains(return_address)) {
        return nullptr;
    }

    try {
        invalid_return_addresses.insert(return_address);

        // Setup the emulator. Set RAX to magic value. If RAX changes at any point, abort.
        const auto module_within = utility::get_module_within(return_address);

        if (!module_within) {
            SPDLOG_ERROR("IXRTrackingSystemHook::get_stereo_rendering_device: failed to get module within");

            return nullptr;
        }

        static auto check_ix = [](const INSTRUX& ix) {
            for (auto i = 0; i < ix.OperandsCount; ++i) {
                const auto& op = ix.Operands[i];

                if (op.Type != ND_OPERAND_TYPE::ND_OP_MEM) {
                    continue;
                }

                if (op.Info.Memory.HasBase && op.Info.Memory.Base == NDR_RAX) {
                    SPDLOG_INFO("Found dereference of RAX");
                    return true;
                }
            }

            return false;
        };

        const auto retdecode = utility::decode_one((uint8_t*)return_address);

        if (!retdecode) {
            SPDLOG_ERROR("IXRTrackingSystemHook::get_stereo_rendering_device: failed to decode instruction at 0x{:x}", return_address);
            return nullptr;
        }

        bool should_switch_to_4_27 = check_ix(*retdecode);

        if (!should_switch_to_4_27) {
            utility::ShemuContext base_context{*module_within};

            base_context.ctx->Registers.RegRip = return_address;
            base_context.ctx->Registers.RegRax = 0xdeadbeef;
            base_context.ctx->MemThreshold = 10;

            utility::emulate(*module_within, return_address, 10, base_context, [&should_switch_to_4_27](const utility::ShemuContextExtended& ctx) -> utility::ExhaustionResult {
                if (should_switch_to_4_27) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (check_ix(ctx.ctx->ctx->Instruction) || check_ix(ctx.next.ix)) {
                    should_switch_to_4_27 = true;
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.next.ix.BranchInfo.IsBranch) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.ctx->ctx->Registers.RegRax != 0xdeadbeef) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.next.writes_to_memory) {
                    return utility::ExhaustionResult::STEP_OVER;
                }

                return utility::ExhaustionResult::CONTINUE;
            });
        }

        if (should_switch_to_4_27) {
            SPDLOG_INFO("IXRTrackingSystemHook::get_stereo_rendering_device: detected necessary switch to 4.27");

            // pre_initialize clears out all of our already set vtables to their default values
            g_hook->pre_initialize();
            g_hook->m_is_4_26 = false;
            g_hook->m_overridden_version = "4.27";
            // now set up the vtables again
            g_hook->initialize();

            // This is the real function that was supposed to be called.
            if (out != nullptr) {
                *out = g_hook->m_xr_camera_shared;
            }
            
            return out;
        }
    } catch(...) {
        SPDLOG_ERROR("IXRTrackingSystemHook::get_stereo_rendering_device: exception");
        invalid_return_addresses.insert(return_address);
    }

    return nullptr;
}

void IXRTrackingSystemHook::manual_update_control_rotation() {
    const auto world = sdk::UEngine::get()->get_world();

    if (world == nullptr) {
        return;
    }

    const auto controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

    if (controller == nullptr) {
        return;
    }

    const auto pawn = controller->get_acknowledged_pawn();

    if (pawn == nullptr) {
        return;
    }

    auto control_rotation = controller->get_control_rotation();
    
    Rotator<double> ue5_rotation {
        (double)control_rotation.x,
        (double)control_rotation.y,
        (double)control_rotation.z
    };

    const auto is_ue5 = g_hook->m_stereo_hook->has_double_precision();

    Rotator<float>* chosen_rotation = is_ue5 ? (Rotator<float>*)&ue5_rotation : (Rotator<float>*)&control_rotation;

    update_view_rotation(controller, chosen_rotation);
    
    // Conv back to vec3<float>
    glm::vec3 final_rotation{};

    if (is_ue5) {
        final_rotation = glm::vec3{
            (float)ue5_rotation.pitch,
            (float)ue5_rotation.yaw,
            (float)ue5_rotation.roll
        };
    } else {
        final_rotation = glm::vec3{
            control_rotation.x,
            control_rotation.y,
            control_rotation.z
        };
    }

    controller->set_control_rotation(final_rotation);
}

bool IXRTrackingSystemHook::analyze_head_tracking_allowed(uintptr_t return_address) {
    ++detail::total_times_funcs_called;

    std::scoped_lock _{detail::return_address_to_functions_mutex};

    auto it = detail::return_address_to_functions.find(return_address);

    if (it == detail::return_address_to_functions.end()) {
        const auto vfunc = utility::find_virtual_function_start(return_address);

        if (vfunc) {
            detail::return_address_to_functions[return_address] = *vfunc;
        } else {
            detail::return_address_to_functions[return_address] = 0;
        }

        it = detail::return_address_to_functions.find(return_address);
    }

    if (it->second == 0) {
        return false;
    }

    auto& func = detail::functions[it->second];

    if (!g_hook->m_process_view_rotation_hook && detail::total_times_funcs_called >= 100 && 
        !func.calls_xr_camera && !func.calls_update_player_camera && !func.calls_apply_hmd_rotation && !func.process_view_rotation_analysis_failed)
    {
        g_hook->m_attempted_hook_view_rotation = true;

        SPDLOG_INFO("Possibly found ProcessViewRotation: 0x{:x}", it->second);

        const auto module = utility::get_module_within(it->second);

        if (!module) {
            SPDLOG_ERROR("Failed to get module for ProcessViewRotation");
            return false;
        }

        const auto func_ptr = utility::scan_ptr(*module, it->second);

        if (!func_ptr) {
            SPDLOG_ERROR("Failed to find ProcessViewRotation");
            return false;
        }

        // We do not definitively hook ProcessViewRotation here, we point it towards a function analyzer
        // that checks whether the arguments match up with what we expect. If they do, we hook it.
        m_addr_of_process_view_rotation_ptr = *func_ptr;
        //g_hook->m_process_view_rotation_hook = std::make_unique<PointerHook>((void**)*func_ptr, (void*)&process_view_rotation_analyzer);
        g_hook->m_process_view_rotation_hook = safetyhook::create_inline((void*)it->second, (void*)&process_view_rotation_analyzer);

        if (!g_hook->m_process_view_rotation_hook) {
            SPDLOG_ERROR("Failed to hook ProcessViewRotation");
            func.process_view_rotation_analysis_failed = true;
            g_hook->m_attempted_hook_view_rotation = false;
            return true;
        }

        SPDLOG_INFO("Hooked ProcessViewRotation");
    }

    return true;
}

void* IXRTrackingSystemHook::get_orientation_and_position_native(void* rcx, void* rdx, void* r8, void* r9) {
    SPDLOG_INFO_ONCE("GetOrientationAndPosition native function {:x}", (uintptr_t)_ReturnAddress());

    const auto og = g_hook->m_native_get_oap_hook->get_original<decltype(&get_orientation_and_position_native)>();

    g_hook->m_within_get_oap_native = true;

    const auto result = og(rcx, rdx, r8, r9);

    g_hook->m_within_get_oap_native = false;

    return result;
}

void* IXRTrackingSystemHook::is_head_mounted_display_enabled_native(void* rcx, void* rdx, void* r8, void* r9) {
    SPDLOG_INFO_ONCE("IsHeadMountedDisplayEnabled native function {:x}", (uintptr_t)_ReturnAddress());

    const auto og = g_hook->m_native_is_hmd_enabled_hook->get_original<decltype(&is_head_mounted_display_enabled_native)>();

    g_hook->m_within_is_hmd_enabled_native = true;

    const auto result = og(rcx, rdx, r8, r9);

    g_hook->m_within_is_hmd_enabled_native = false;

    return result;
}

bool IXRTrackingSystemHook::is_head_tracking_allowed(sdk::IXRTrackingSystem*) {
    SPDLOG_INFO_ONCE("is_head_tracking_allowed {:x}", (uintptr_t)_ReturnAddress());

    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return false;
    }

    if (g_hook->m_is_leq_4_17) {
        return true;
    }

    static bool attempted_check = false;

    if (vr->wants_blueprint_load() && !attempted_check) try {
        attempted_check = true;
        const auto uobjectarray = sdk::FUObjectArray::get();

        if (uobjectarray == nullptr) {
            SPDLOG_ERROR("Failed to find FUObjectArray");
            return false;
        }

        const auto hmd_lib = sdk::UHeadMountedDisplayFunctionLibrary::static_class();

        if (hmd_lib == nullptr) {
            SPDLOG_ERROR("Failed to find UHeadMountedDisplayFunctionLibrary");
            return false;
        }

        if (sdk::UFunction::get_native_function_offset() == 0) {
            SPDLOG_ERROR("UFunction::get_native_function_offset is 0");
            return false;
        }

        // GetOrientationAndPosition hook
        auto get_orientation_and_position_fn = hmd_lib->find_function(L"GetOrientationAndPosition");

        if (get_orientation_and_position_fn != nullptr) {
            auto& native_fn_get_oap = get_orientation_and_position_fn->get_native_function();

            if (native_fn_get_oap != nullptr && !IsBadReadPtr(native_fn_get_oap, sizeof(void*))) {
                g_hook->m_native_get_oap_hook = std::make_unique<PointerHook>((void**)&native_fn_get_oap, get_orientation_and_position_native);
                SPDLOG_INFO("Hooked GetOrientationAndPosition native function");
            } else {
                SPDLOG_ERROR("Failed to hook GetOrientationAndPosition native function");
            }
        } else {
            SPDLOG_ERROR("Failed to find GetOrientationAndPosition native function");
        }

        // IsHeadMountedDisplayEnabled hook
        auto is_head_mounted_display_enabled_fn = hmd_lib->find_function(L"IsHeadMountedDisplayEnabled");

        if (is_head_mounted_display_enabled_fn != nullptr) {
            auto& native_fn_is_hmd_enabled = is_head_mounted_display_enabled_fn->get_native_function();

            if (native_fn_is_hmd_enabled != nullptr && !IsBadReadPtr(native_fn_is_hmd_enabled, sizeof(void*))) {
                g_hook->m_native_is_hmd_enabled_hook = std::make_unique<PointerHook>((void**)&native_fn_is_hmd_enabled, is_head_mounted_display_enabled_native);
                SPDLOG_INFO("Hooked IsHeadMountedDisplayEnabled native function");
            } else {
                SPDLOG_ERROR("Failed to hook IsHeadMountedDisplayEnabled native function");
            }
        } else {
            SPDLOG_ERROR("Failed to find IsHeadMountedDisplayEnabled native function");
        }
    } catch(...) {
        SPDLOG_ERROR("Failed to hook native functions due to exception");
    }

    if (!vr->is_any_aim_method_active()) {
        // Only allow this to return true if BP functions are the ones calling it
        // Like GetOrientationAndPosition
        return g_hook->is_within_valid_head_tracking_allowed_code();
    }

    if (!g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();

        // <= 4.25 doesn't have IsHeadTrackingAllowedForWorld
        if (g_hook->m_is_leq_4_25) {
            g_hook->analyze_head_tracking_allowed(return_address);
        }
    }
    

    return !g_hook->m_process_view_rotation_hook || g_hook->is_within_valid_head_tracking_allowed_code();
}

bool IXRTrackingSystemHook::is_head_tracking_allowed_for_world(sdk::IXRTrackingSystem*, void*) {
    SPDLOG_INFO_ONCE("is_head_tracking_allowed_for_world {:x}", (uintptr_t)_ReturnAddress());

    auto& vr = VR::get();

    if (!vr->is_hmd_active() || !vr->is_any_aim_method_active()) {
        return false;
    }

    if (!g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        g_hook->analyze_head_tracking_allowed(return_address);

        return true;
    }

    /*(if (!VR::get()->is_hmd_active()) {
        return false;
    }

    return true;*/

    return false;
}

IXRTrackingSystemHook::SharedPtr* IXRTrackingSystemHook::get_xr_camera(sdk::IXRTrackingSystem*, IXRTrackingSystemHook::SharedPtr* out, size_t device_id) {
    SPDLOG_INFO_ONCE("get_xr_camera {:x}", (uintptr_t)_ReturnAddress());

    if (out != nullptr) {
        *out = g_hook->m_xr_camera_shared;
    }

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_xr_camera = true;
        }
    }

    return out;
}

void IXRTrackingSystemHook::get_motion_controller_data(sdk::IXRTrackingSystem*, void* world, uint32_t hand, void* motion_controller_data) {
    SPDLOG_INFO_ONCE("get_motion_controller_data {:x}", (uintptr_t)_ReturnAddress());

    const auto e_hand = (ue::EControllerHand)hand;
    const auto vr = VR::get();
    const auto world_scale = vr->get_world_to_meters();

    auto rotation_offset = vr->get_rotation_offset();

    if (vr->is_decoupled_pitch_enabled()) {
        const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
        const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);
        rotation_offset = glm::normalize(pre_flat_pitch * vr->get_rotation_offset());
    }

    static const auto mc_data_struct = sdk::FUObjectArray::get() != nullptr ? (sdk::UScriptStruct*)sdk::find_uobject(L"ScriptStruct /Script/HeadMountedDisplay.XRMotionControllerData") : nullptr;

    const auto aim_transform = e_hand == ue::EControllerHand::Left ? vr->get_aim_transform(vr->get_left_controller_index()) : vr->get_aim_transform(vr->get_right_controller_index());
    const auto grip_transform = e_hand == ue::EControllerHand::Left ? vr->get_grip_transform(vr->get_left_controller_index()) : vr->get_grip_transform(vr->get_right_controller_index());

    const auto aim_position = rotation_offset * glm::vec3{aim_transform[3] - vr->get_standing_origin()};
    const auto aim_rotation = glm::normalize(rotation_offset * glm::quat{aim_transform});
    const auto grip_position = rotation_offset * glm::vec3{grip_transform[3] - vr->get_standing_origin()};
    const auto grip_rotation = glm::normalize(rotation_offset * glm::quat{grip_transform});

    const auto final_aim_position = utility::math::glm_to_ue4(aim_position * world_scale);
    const auto final_aim_rotation = utility::math::glm_to_ue4(aim_rotation);
    const auto final_grip_position = utility::math::glm_to_ue4(grip_position * world_scale);
    const auto final_grip_rotation = utility::math::glm_to_ue4(grip_rotation);

    if (mc_data_struct == nullptr) {
        const auto data = (ue4_27::FXRMotionControllerData*)motion_controller_data;
        data->bValid = true;
        data->GripRotation = { final_grip_rotation.x, final_grip_rotation.y, final_grip_rotation.z, final_grip_rotation.w };
        data->GripPosition = final_grip_position;
        data->AimRotation = { final_aim_rotation.x, final_aim_rotation.y, final_aim_rotation.z, final_aim_rotation.w };
        data->AimPosition = final_aim_position;
    } else {
        const auto bValid_prop = mc_data_struct->find_property(L"bValid");
        const auto GripRotation_prop = mc_data_struct->find_property(L"GripRotation");
        const auto GripPosition_prop = mc_data_struct->find_property(L"GripPosition");
        const auto AimRotation_prop = mc_data_struct->find_property(L"AimRotation");
        const auto AimPosition_prop = mc_data_struct->find_property(L"AimPosition");
        const auto is_ue5 = g_hook->m_stereo_hook->has_double_precision();

        if (bValid_prop != nullptr) {
            *bValid_prop->get_data<bool>(motion_controller_data) = true;
        }

        if (GripRotation_prop != nullptr) {
            if (is_ue5) {
                *GripRotation_prop->get_data<glm::vec<4, double>>(motion_controller_data) = { final_grip_rotation.x, final_grip_rotation.y, final_grip_rotation.z, final_grip_rotation.w };
            } else {
                *GripRotation_prop->get_data<glm::vec<4, float>>(motion_controller_data) = { final_grip_rotation.x, final_grip_rotation.y, final_grip_rotation.z, final_grip_rotation.w };
            }
        }

        if (GripPosition_prop != nullptr) {
            if (is_ue5) {
                *GripPosition_prop->get_data<glm::vec<3, double>>(motion_controller_data) = final_grip_position;
            } else {
                *GripPosition_prop->get_data<glm::vec<3, float>>(motion_controller_data) = final_grip_position;
            }
        }

        if (AimRotation_prop != nullptr) {
            if (is_ue5) {
                *AimRotation_prop->get_data<glm::vec<4, double>>(motion_controller_data) = { final_aim_rotation.x, final_aim_rotation.y, final_aim_rotation.z, final_aim_rotation.w };
            } else {
                *AimRotation_prop->get_data<glm::vec<4, float>>(motion_controller_data) = { final_aim_rotation.x, final_aim_rotation.y, final_aim_rotation.z, final_aim_rotation.w };
            }
        }

        if (AimPosition_prop != nullptr) {
            if (is_ue5) {
                *AimPosition_prop->get_data<glm::vec<3, double>>(motion_controller_data) = final_aim_position;
            } else {
                *AimPosition_prop->get_data<glm::vec<3, float>>(motion_controller_data) = final_aim_position;
            }
        }
    }
}

void IXRTrackingSystemHook::get_hmd_data(sdk::IXRTrackingSystem*, void* world, void* hmd_data) {
    SPDLOG_INFO_ONCE("get_hmd_data {:x}", (uintptr_t)_ReturnAddress());

    const auto& vr = VR::get();
    const auto world_scale = vr->get_world_to_meters();

    auto rotation_offset = vr->get_rotation_offset();

    if (vr->is_decoupled_pitch_enabled()) {
        const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
        const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);
        rotation_offset = glm::normalize(pre_flat_pitch * vr->get_rotation_offset());
    }

    static const auto hmd_data_struct = sdk::FUObjectArray::get() != nullptr ? (sdk::UScriptStruct*)sdk::find_uobject(L"ScriptStruct /Script/HeadMountedDisplay.XRHMDData") : nullptr;

    const auto position = rotation_offset * glm::vec3{vr->get_position(vr->get_hmd_index()) - vr->get_standing_origin()};
    const auto rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(vr->get_hmd_index())});

    if (hmd_data_struct == nullptr) {
        const auto data = (ue4_27::FXRHMDData*)hmd_data;
        data->Position = utility::math::glm_to_ue4(position * world_scale);

        const auto q = utility::math::glm_to_ue4(rotation);
        data->Rotation = { q.x, q.y, q.z, q.w };
    } else {
        const auto Position_prop = hmd_data_struct->find_property(L"Position");
        const auto Rotation_prop = hmd_data_struct->find_property(L"Rotation");
        const auto is_ue5 = g_hook->m_stereo_hook->has_double_precision();

        if (Position_prop != nullptr) {
            if (is_ue5) {
                *Position_prop->get_data<glm::vec<3, double>>(hmd_data) = utility::math::glm_to_ue4(position * world_scale);
            } else {
                *Position_prop->get_data<glm::vec<3, float>>(hmd_data) = utility::math::glm_to_ue4(position * world_scale);
            }
        }

        if (Rotation_prop != nullptr) {
            if (is_ue5) {
                const auto q = utility::math::glm_to_ue4(rotation);
                *Rotation_prop->get_data<glm::vec<4, double>>(hmd_data) = { q.x, q.y, q.z, q.w };
            } else {
                const auto q = utility::math::glm_to_ue4(rotation);
                *Rotation_prop->get_data<glm::vec<4, float>>(hmd_data) = { q.x, q.y, q.z, q.w };
            }
        }
    }
}

void IXRTrackingSystemHook::get_current_pose(sdk::IXRTrackingSystem*, int32_t device_id, Quat<float>* out_rot, glm::vec3* out_pos) {
    SPDLOG_INFO_ONCE("get_current_pose {:x}", (uintptr_t)_ReturnAddress());

    const auto& vr = VR::get();
    const auto world_scale = vr->get_world_to_meters();

    auto rotation_offset = vr->get_rotation_offset();

    if (vr->is_decoupled_pitch_enabled()) {
        const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
        const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);
        rotation_offset = glm::normalize(pre_flat_pitch * vr->get_rotation_offset());
    }

    switch (device_id) {
    // Todo: motion controllers? Don't know how BP can pass through a valid device id
    case 0: 
    default: {
        const auto position = rotation_offset * glm::vec3{vr->get_position(vr->get_hmd_index()) - vr->get_standing_origin()};
        const auto rotation = glm::normalize(rotation_offset * glm::quat{vr->get_rotation(vr->get_hmd_index())});

        const auto is_ue5 = g_hook->m_stereo_hook->has_double_precision();

        if (!is_ue5) {
            *out_pos = utility::math::glm_to_ue4(position * world_scale);

            const auto q = utility::math::glm_to_ue4(rotation);
            *out_rot = { q.x, q.y, q.z, q.w };
        } else {
            *(glm::vec<3, double>*)out_pos = utility::math::glm_to_ue4(position * world_scale);

            const auto q = utility::math::glm_to_ue4(rotation);
            *(glm::vec<4, double>*)out_rot = { q.x, q.y, q.z, q.w };
        }

        break;
    }
    }
}

enum ECustomSystemFlags : int32_t {
    SYSTEMFLAG_NONE = 0,
    SYSTEMFLAG_HMD_ACTIVE = 1 << 0,
    SYSTEMFLAG_DECOUPLED_PITCH = 1 << 1,
    SYSTEMFLAG_OPENXR = 1 << 2,
    SYSTEMFLAG_OPENVR = 1 << 3,
    SYSTEMFLAG_MOTION_CONTROLLERS_ACTIVE = 1 << 4,
    SYSTEMFLAG_LEFT_THUMBREST_ACTIVE = 1 << 5,
    SYSTEMFLAG_RIGHT_THUMBREST_ACTIVE = 1 << 6,
    SYSTEMFLAG_GAME_AIMING_MODE = 1 << 7,
    SYSTEMFLAG_HEAD_AIMING_MODE = 1 << 8,
    SYSTEMFLAG_LEFT_CONTROLLER_AIMING_MODE = 1 << 9,
    SYSTEMFLAG_RIGHT_CONTROLLER_AIMING_MODE = 1 << 10,
    SYSTEMFLAG_TWO_HANDED_LEFT_AIMING_MODE = 1 << 11,
    SYSTEMFLAG_TWO_HANDED_RIGHT_AIMING_MODE = 1 << 12,
};

int32_t IXRTrackingSystemHook::get_xr_system_flags(sdk::IXRTrackingSystem* system) {
    SPDLOG_INFO_ONCE("get_xr_system_flags {:x}", (uintptr_t)_ReturnAddress());

    const auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return ECustomSystemFlags::SYSTEMFLAG_NONE;
    }

    int32_t out = ECustomSystemFlags::SYSTEMFLAG_HMD_ACTIVE;

    if (vr->is_decoupled_pitch_enabled()) {
        out |= ECustomSystemFlags::SYSTEMFLAG_DECOUPLED_PITCH;
    }

    if (vr->is_using_controllers()) {
        out |= ECustomSystemFlags::SYSTEMFLAG_MOTION_CONTROLLERS_ACTIVE;
    }

    const auto runtime = vr->get_runtime();

    if (runtime->is_openvr()) {
        out |= ECustomSystemFlags::SYSTEMFLAG_OPENVR;
    } else if (runtime->is_openxr()) {
        out |= ECustomSystemFlags::SYSTEMFLAG_OPENXR;
    }

    const auto left_thumbrest_handle = vr->get_action_handle(VR::s_action_thumbrest_touch_left);
    const auto right_thumbrest_handle = vr->get_action_handle(VR::s_action_thumbrest_touch_right);

    if (vr->is_action_active_any_joystick(left_thumbrest_handle)) {
        out |= ECustomSystemFlags::SYSTEMFLAG_LEFT_THUMBREST_ACTIVE;
    }

    if (vr->is_action_active_any_joystick(right_thumbrest_handle)) {
        out |= ECustomSystemFlags::SYSTEMFLAG_RIGHT_THUMBREST_ACTIVE;
    }

    if (!vr->is_any_aim_method_active()) {
        out |= ECustomSystemFlags::SYSTEMFLAG_GAME_AIMING_MODE;
    } else {
        const auto aim_method = vr->get_aim_method();

        if (aim_method == VR::AimMethod::HEAD) {
            out |= ECustomSystemFlags::SYSTEMFLAG_HEAD_AIMING_MODE;
        } else if (aim_method == VR::AimMethod::LEFT_CONTROLLER) {
            out |= ECustomSystemFlags::SYSTEMFLAG_LEFT_CONTROLLER_AIMING_MODE;
        } else if (aim_method == VR::AimMethod::RIGHT_CONTROLLER) {
            out |= ECustomSystemFlags::SYSTEMFLAG_RIGHT_CONTROLLER_AIMING_MODE;
        } else if (aim_method == VR::AimMethod::TWO_HANDED_LEFT) {
            out |= ECustomSystemFlags::SYSTEMFLAG_TWO_HANDED_LEFT_AIMING_MODE;
        } else if (aim_method == VR::AimMethod::TWO_HANDED_RIGHT) {
            out |= ECustomSystemFlags::SYSTEMFLAG_TWO_HANDED_RIGHT_AIMING_MODE;
        }
    }

    return out;
}

void* IXRTrackingSystemHook::get_audio_listener_offset(sdk::IXRTrackingSystem*, void* a2, void* a3) {
    SPDLOG_INFO_ONCE("get_audio_listener_offset {:x}", (uintptr_t)_ReturnAddress());

    static bool is_a2_stack = !IsBadReadPtr(a2, sizeof(void*));

    if (is_a2_stack) {
        float* foffset = (float*)a2;
        double* doffset = (double*)a2;

        if (g_hook->m_stereo_hook->has_double_precision()) {
            doffset[0] = 0.0;
            doffset[1] = 0.0;
            doffset[2] = 0.0;
        } else {
            foffset[0] = 0.0f;
            foffset[1] = 0.0f;
            foffset[2] = 0.0f;
        }

        return a2;
    }

    float* foffset = (float*)a3;
    double* doffset = (double*)a3;

    if (g_hook->m_stereo_hook->has_double_precision()) {
        doffset[0] = 0.0;
        doffset[1] = 0.0;
        doffset[2] = 0.0;
    } else {
        foffset[0] = 0.0f;
        foffset[1] = 0.0f;
        foffset[2] = 0.0f;
    }

    return a3;
}

// Returns quaternion
void* IXRTrackingSystemHook::get_base_orientation(sdk::IXRTrackingSystem*, void* a2) {
    SPDLOG_INFO_ONCE("get_base_orientation {:x}", (uintptr_t)_ReturnAddress());

    if (g_hook->m_stereo_hook->has_double_precision()) {
        Quat<double>* out = (Quat<double>*)a2;

        out->x = 0.0;
        out->y = 0.0;
        out->z = 0.0;
        out->w = 1.0;
    } else {
        Quat<float>* out = (Quat<float>*)a2;

        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
        out->w = 1.0f;
    }

    return a2;
}

// Returns vec3
void* IXRTrackingSystemHook::get_base_position(sdk::IXRTrackingSystem*, void* a2) {
    SPDLOG_INFO_ONCE("get_base_position {:x}", (uintptr_t)_ReturnAddress());

    if (g_hook->m_stereo_hook->has_double_precision()) {
        double* out = (double*)a2;

        out[0] = 0.0;
        out[1] = 0.0;
        out[2] = 0.0;
    } else {
        float* out = (float*)a2;

        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }

    return a2;
}

// Returns Rotator
void* IXRTrackingSystemHook::get_base_rotation(sdk::IXRTrackingSystem*, void* a2) {
    SPDLOG_INFO_ONCE("get_base_rotation {:x}", (uintptr_t)_ReturnAddress());

    if (g_hook->m_stereo_hook->has_double_precision()) {
        Rotator<double>* out = (Rotator<double>*)a2;

        out->pitch = 0.0;
        out->yaw = 0.0;
        out->roll = 0.0;
    } else {
        Rotator<float>* out = (Rotator<float>*)a2;

        out->pitch = 0.0f;
        out->yaw = 0.0f;
        out->roll = 0.0f;
    }

    return a2;
}

void* IXRTrackingSystemHook::reset_orientation_and_position(sdk::IXRTrackingSystem*, float yaw) {
    SPDLOG_INFO_ONCE("reset_orientation_and_position {:x}", (uintptr_t)_ReturnAddress());

    auto& vr = VR::get();
    vr->set_standing_origin(vr->get_position(vr->get_hmd_index()));
    vr->recenter_view();

    return nullptr;
}

void* IXRTrackingSystemHook::reset_orientation(sdk::IXRTrackingSystem*, float yaw) {
    SPDLOG_INFO_ONCE("reset_orientation {:x}", (uintptr_t)_ReturnAddress());

    auto& vr = VR::get();
    vr->recenter_view();

    return nullptr;
}

void* IXRTrackingSystemHook::reset_position(sdk::IXRTrackingSystem*) {
    SPDLOG_INFO_ONCE("reset_position {:x}", (uintptr_t)_ReturnAddress());

    auto& vr = VR::get();
    vr->set_standing_origin(vr->get_position(vr->get_hmd_index()));

    return nullptr;
}

bool IXRTrackingSystemHook::is_hmd_connected(sdk::IHeadMountedDisplay*) {
    SPDLOG_INFO_ONCE("is_hmd_connected {:x}", (uintptr_t)_ReturnAddress());

    return VR::get()->is_hmd_active();
}

int32_t* IXRTrackingSystemHook::get_ideal_debug_canvas_render_target_size(sdk::IHeadMountedDisplay*, int32_t* out) {
    const auto return_address = (uintptr_t)_ReturnAddress();
    SPDLOG_INFO_ONCE("get_ideal_debug_canvas_render_target_size {:x}", return_address);

    if (out != nullptr) {
        // Verify first that this function dereferences RAX at some point afterwards
        static std::mutex mtx{};
        static std::unordered_set<uintptr_t> valid_return_addresses{};
        static std::unordered_set<uintptr_t> invalid_return_addresses{};

        std::scoped_lock _{mtx};

        if (invalid_return_addresses.contains(return_address)) {
            return nullptr;
        }

        if (valid_return_addresses.contains(return_address)) {     
            // This is what the engine does... I guess? I don't see any VR plugins implementing this function
            // look into it later to see if it's even useful
            out[0] = 1024;
            out[1] = 1024;

            return out;
        }

        // Setup the emulator. Set RAX to magic value. If RAX changes at any point, abort.
        const auto module_within = utility::get_module_within(return_address);

        if (!module_within) {
            SPDLOG_ERROR("get_ideal_debug_canvas_render_target_size: invalid module within");
            invalid_return_addresses.insert(return_address);

            return nullptr;
        }

        static auto check_ix = [](const INSTRUX& ix) {
            for (auto i = 0; i < ix.OperandsCount; ++i) {
                const auto& op = ix.Operands[i];

                if (op.Type != ND_OPERAND_TYPE::ND_OP_MEM) {
                    continue;
                }

                if (op.Info.Memory.HasBase && op.Info.Memory.Base == NDR_RAX) {
                    SPDLOG_INFO("Found dereference of RAX");
                    return true;
                }
            }

            return false;
        };

        const auto retdecode = utility::decode_one((uint8_t*)return_address);

        if (!retdecode) {
            SPDLOG_ERROR("get_ideal_debug_canvas_render_target_size: invalid retdecode");
            invalid_return_addresses.insert(return_address);

            return nullptr;
        }

        bool is_valid = check_ix(*retdecode);

        if (!is_valid) {
            utility::ShemuContext base_context{*module_within};

            base_context.ctx->Registers.RegRip = return_address;
            base_context.ctx->Registers.RegRax = 0xdeadbeef;
            base_context.ctx->MemThreshold = 10;

            utility::emulate(*module_within, return_address, 10, base_context, [&is_valid](const utility::ShemuContextExtended& ctx) -> utility::ExhaustionResult {
                if (check_ix(ctx.ctx->ctx->Instruction) || check_ix(ctx.next.ix)) {
                    is_valid = true;
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.next.ix.BranchInfo.IsBranch) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.ctx->ctx->Registers.RegRax != 0xdeadbeef) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (ctx.next.writes_to_memory) {
                    return utility::ExhaustionResult::STEP_OVER;
                }

                return utility::ExhaustionResult::CONTINUE;
            });
        }

        if (!is_valid) {
            SPDLOG_ERROR("get_ideal_debug_canvas_render_target_size: invalid emulation result");
            invalid_return_addresses.insert(return_address);
            return nullptr;
        }

        valid_return_addresses.insert(return_address);

        // This is what the engine does... I guess? I don't see any VR plugins implementing this function
        // look into it later to see if it's even useful
        out[0] = 1024;
        out[1] = 1024;

        return out;
    }

    return nullptr;
}

IXRTrackingSystemHook::SharedPtr* IXRTrackingSystemHook::get_view_extension(sdk::IHeadMountedDisplay*, SharedPtr* out) {
    SPDLOG_INFO_ONCE("get_view_extension {:x}", (uintptr_t)_ReturnAddress());

    if (out != nullptr) {
        *out = g_hook->m_view_extension_shared;
    }

    return out;
}

void IXRTrackingSystemHook::apply_hmd_rotation(sdk::IXRCamera*, sdk::APlayerController* player_controller, Rotator<float>* rot) {
    SPDLOG_INFO_ONCE("apply_hmd_rotation {:x}", (uintptr_t)_ReturnAddress());

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        const auto return_address = (uintptr_t)_ReturnAddress();
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_apply_hmd_rotation = true;
        }

        g_hook->update_view_rotation(player_controller, rot);
    }

    /*if (g_hook->m_stereo_hook == nullptr) {
        return;
    }

    if (!VR::get()->is_hmd_active()) {
        return;
    }

    if (g_hook->m_stereo_hook->has_double_precision()) {
        *(Rotator<double>*)rot = g_hook->m_stereo_hook->m_last_rotation_double;
    } else {
        *rot = g_hook->m_stereo_hook->m_last_rotation;
    }

    VR::get()->recenter_view();*/
}

bool IXRTrackingSystemHook::update_player_camera(sdk::IXRCamera*, Quat<float>* rel_rot, glm::vec3* rel_pos) {
    SPDLOG_INFO_ONCE("update_player_camera {:x}", (uintptr_t)_ReturnAddress());

    if (VR::get()->is_hmd_active() && !g_hook->m_process_view_rotation_hook && !g_hook->m_attempted_hook_view_rotation) {
        ++detail::total_times_funcs_called;

        std::scoped_lock _{detail::return_address_to_functions_mutex};

        const auto return_address = (uintptr_t)_ReturnAddress();
        auto it = detail::return_address_to_functions.find(return_address);

        if (it == detail::return_address_to_functions.end()) {
            const auto vfunc = utility::find_virtual_function_start(return_address);

            if (vfunc) {
                detail::return_address_to_functions[return_address] = *vfunc;
            } else {
                detail::return_address_to_functions[return_address] = 0;
            }

            it = detail::return_address_to_functions.find(return_address);
        }

        if (it->second != 0) {
            auto& func = detail::functions[it->second];
            func.calls_update_player_camera = true;
        }
    }

    if (!g_hook->m_relative_transform_corrected) {
        g_hook->m_relative_transform_corrected = true;

        const auto return_address = (uintptr_t)_ReturnAddress();
        const auto module_within = utility::get_module_within(return_address);

        if (module_within) {
            // We need to emulate from the return address and find the first call
            // We need to nop out this call because it modifies the relative transform
            // causing the player to turn into a midget in some games
            // and also in some games causes strange rotation issues
            utility::ShemuContext ctx{*module_within};

            ctx.ctx->Registers.RegRip = return_address;
            ctx.ctx->Registers.RegRax = 1;
            ctx.ctx->MemThreshold = 100;

            utility::emulate(*module_within, return_address, 100, ctx, [](const utility::ShemuContextExtended& ctx) -> utility::ExhaustionResult {
                if (ctx.next.writes_to_memory) {
                    spdlog::info("Skipping memory write at {:x}", ctx.ctx->ctx->Registers.RegRip);
                    return utility::ExhaustionResult::STEP_OVER;
                }
    
                if (std::string_view{ctx.next.ix.Mnemonic}.starts_with("CALL")) {
                    SPDLOG_INFO("Creating nop patch at {:x}", ctx.ctx->ctx->Registers.RegRip);
                    static auto patch = Patch::create_nop(ctx.ctx->ctx->Registers.RegRip, ctx.next.ix.Length);
                    return utility::ExhaustionResult::BREAK;
                }

                return utility::ExhaustionResult::CONTINUE;
            });
        } else {
            SPDLOG_WARN("Could not find module within {:x}", return_address);
        }
    }

    if (g_hook->m_stereo_hook->has_double_precision()) {
        *(Quat<double>*)rel_rot = { 0.0, 0.0, 0.0, 1.0};
        double* rel_pos_d = (double*)rel_pos;
        rel_pos_d[0] = 0.0;
        rel_pos_d[1] = 0.0;
        rel_pos_d[2] = 0.0;
    } else {
        *rel_rot = { 0.0f, 0.0f, 0.0f, 1.0f };
        *rel_pos = { 0.0f, 0.0f, 0.0f };
    }
    return true;
}

void* IXRTrackingSystemHook::process_view_rotation_analyzer(void* a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6) {
    SPDLOG_INFO_ONCE("process_view_rotation_analyzer {:x}", (uintptr_t)_ReturnAddress());

    std::scoped_lock _{detail::return_address_to_functions_mutex};

    auto call_orig = [&]() {
        return g_hook->m_process_view_rotation_hook.call<void*>(a1, a2, a3, a4, a5, a6);
    };

    const auto result = call_orig();

    // Not the exact stack pointer but at least some memory pointing to the stack
    // We need this to check whether the arguments reside on the stack.
    // If they don't, then this is not the correct function and we need to analyze the next one.
    const auto stack_pointer = (intptr_t)_AddressOfReturnAddress();

    // Check if a3 and a4 are on the stack, and are not equal to eachother
    if (std::abs((intptr_t)a3 - stack_pointer) > 0x1000 || std::abs((intptr_t)a4 - stack_pointer) > 0x1000 || a3 == a4) {
        SPDLOG_ERROR("Function we hooked for ProcessViewRotation is not the correct one. Analyzing next function.");

        const auto function_addr = g_hook->m_process_view_rotation_hook.target_address();
        detail::functions[function_addr].process_view_rotation_analysis_failed = true;

        g_hook->m_process_view_rotation_hook.reset();
        g_hook->m_attempted_hook_view_rotation = false;
    } else {
        SPDLOG_INFO("Found correct function for ProcessViewRotation. Hooking it.");
        const auto target = g_hook->m_process_view_rotation_hook.target();
        g_hook->m_process_view_rotation_hook.reset();
        //g_hook->m_process_view_rotation_hook = std::make_unique<PointerHook>((void**)g_hook->m_addr_of_process_view_rotation_ptr, &IXRTrackingSystemHook::process_view_rotation);
        g_hook->m_process_view_rotation_hook = safetyhook::create_inline(target, &IXRTrackingSystemHook::process_view_rotation);
    }

    return result;
}

void IXRTrackingSystemHook::process_view_rotation(
    sdk::APlayerCameraManager* pcm, float delta_time, Rotator<float>* rot, Rotator<float>* delta_rot) {
    SPDLOG_INFO_ONCE("process_view_rotation {:x}", (uintptr_t)_ReturnAddress());

    auto call_orig = [&]() {
        g_hook->m_process_view_rotation_hook.call<void>(pcm, delta_time, rot, delta_rot);
    };

    auto& vr = VR::get();

    if (!vr->is_hmd_active() || !vr->is_any_aim_method_active()) {
        call_orig();
        return;
    }

    //g_hook->pre_update_view_rotation(rot);

    call_orig();

    g_hook->update_view_rotation(pcm, rot);
}

void IXRTrackingSystemHook::pre_update_view_rotation(sdk::UObject* reference_obj, Rotator<float>* rot) {
    auto& vr = VR::get();

    if (m_stereo_hook->has_double_precision()) {
        //*(Rotator<double>*)rot = g_hook->m_stereo_hook->m_last_rotation_double;
        *(Rotator<double>*)rot = g_hook->m_last_view_rotation_double;
    } else {
        //*rot = g_hook->m_stereo_hook->m_last_rotation;
        *rot = g_hook->m_last_view_rotation;
    }
}

void IXRTrackingSystemHook::update_view_rotation(sdk::UObject* reference_obj, Rotator<float>* rot) {
    auto& vr = VR::get();

    // Double check that the player controller passed through here is the local player controller
    static bool had_detection_error = false;
    if (!had_detection_error && vr->is_aim_multiplayer_support_enabled()) try {
        if (reference_obj != nullptr && sdk::FUObjectArray::get() != nullptr) {
            const auto reference_obj_c = reference_obj->get_class();

            static const auto player_controller_class = sdk::APlayerController::static_class();
            static const auto player_camera_manager_class = sdk::APlayerCameraManager::static_class();

            sdk::APlayerController* pc = nullptr;

            if (player_controller_class != nullptr && reference_obj_c != nullptr && reference_obj_c->is_a(player_controller_class)) {
                pc = (sdk::APlayerController*)reference_obj;
            } else if (player_camera_manager_class != nullptr && reference_obj_c != nullptr && reference_obj_c->is_a(player_camera_manager_class)) {
                const auto pcm = (sdk::APlayerCameraManager*)reference_obj;
                pc = pcm->get_owning_player_controller();
            }

            if (pc != nullptr && !pc->is_local_player_controller()) {
                return;
            }
        }
    } catch(...) {
        had_detection_error = true;
        SPDLOG_ERROR("[IXRTrackingSystemHook] Error detecting reference object, cannot support multiplayer");
    }

    const auto now = std::chrono::high_resolution_clock::now();
    const auto delta_time = now - m_process_view_rotation_data.last_update;
    const auto delta_float = glm::min(std::chrono::duration_cast<std::chrono::duration<float>>(delta_time).count(), 0.1f);
    m_process_view_rotation_data.last_update = now;
    m_process_view_rotation_data.was_called = true;

    if (!vr->is_hmd_active() || !vr->is_any_aim_method_active()) {
        return;
    }

    vr->set_decoupled_pitch(true);

    const auto has_double = g_hook->m_stereo_hook->has_double_precision();
    auto rot_d = (Rotator<double>*)rot;
    
    const auto view_mat_inverse =
    has_double ?
        glm::yawPitchRoll(
            glm::radians((float)-rot_d->yaw),
            glm::radians((float)rot_d->pitch),
            glm::radians((float)-rot_d->roll))
        : glm::yawPitchRoll(
            glm::radians(-rot->yaw),
            glm::radians(rot->pitch),
            glm::radians(-rot->roll));

    const auto view_quat_inverse = glm::quat {
        view_mat_inverse
    };

    auto vqi_norm = glm::normalize(view_quat_inverse);

    // Decoupled Pitch
    if (vr->is_decoupled_pitch_enabled()) {
        //vr->set_pre_flattened_rotation(vqi_norm);
        vqi_norm = utility::math::flatten(vqi_norm);
    }

    const auto wants_controller = vr->is_controller_aim_enabled() && vr->is_using_controllers();
    const auto rotation_offset = vr->get_rotation_offset();
    const auto og_hmd_pos = vr->get_position(0);
    const auto og_hmd_rot = glm::quat{vr->get_rotation(0)};

    glm::vec3 euler{};

    if (wants_controller) {
        glm::vec3 right_controller_forward{};
        glm::vec3 og_controller_pos{};
        glm::quat og_controller_rot{};

        const auto aim_type = (VR::AimMethod)vr->get_aim_method();

        if (aim_type == VR::AimMethod::RIGHT_CONTROLLER || aim_type == VR::AimMethod::LEFT_CONTROLLER) {
            const auto controller_index = aim_type == VR::AimMethod::RIGHT_CONTROLLER ? vr->get_right_controller_index() : vr->get_left_controller_index();
            og_controller_rot = glm::quat{vr->get_aim_rotation(controller_index)};
            og_controller_pos = glm::vec3{vr->get_aim_position(controller_index)};
            right_controller_forward = og_controller_rot * glm::vec3{0.0f, 0.0f, 1.0f};
        } else if (aim_type == VR::AimMethod::TWO_HANDED_RIGHT) { // two handed modes are for imitating rifle aiming
            const auto right_controller_index = vr->get_right_controller_index();
            const auto left_controller_index = vr->get_left_controller_index();
            const auto pos_delta = glm::normalize(glm::vec3{vr->get_aim_position(left_controller_index) - vr->get_aim_position(right_controller_index)});
            og_controller_rot = utility::math::to_quat(pos_delta);
            og_controller_pos = glm::vec3{vr->get_aim_position(right_controller_index)};
            right_controller_forward = og_controller_rot * glm::vec3{0.0f, 0.0f, -1.0f};
        } else if (aim_type == VR::AimMethod::TWO_HANDED_LEFT) {
            const auto right_controller_index = vr->get_right_controller_index();
            const auto left_controller_index = vr->get_left_controller_index();
            const auto pos_delta = glm::normalize(glm::vec3{vr->get_aim_position(right_controller_index) - vr->get_aim_position(left_controller_index)});
            og_controller_rot = utility::math::to_quat(pos_delta);
            og_controller_pos = glm::vec3{vr->get_aim_position(left_controller_index)};
            right_controller_forward = og_controller_rot * glm::vec3{0.0f, 0.0f, -1.0f};
        }
        // We need to construct a sightline from the standing origin to the direction the controller is facing
        // This is so the camera will be facing a more correct direction
        // rather than the raw controller rotation
        const auto right_controller_end = og_controller_pos + (right_controller_forward * 1000.0f);
        const auto adjusted_forward = glm::normalize(right_controller_end - glm::vec3{vr->get_standing_origin()});
        const auto target_forward = utility::math::to_quat(adjusted_forward);

        glm::quat right_controller_forward_rot{};

        if (vr->is_aim_interpolation_enabled()) {
            // quaternion distance between target_forward and last_aim_rot
            auto spherical_distance = glm::dot(target_forward, m_process_view_rotation_data.last_aim_rot);

            if (spherical_distance < 0.0f) {
                // we do this because we want to rotate the shortest distance
                spherical_distance = -spherical_distance;
            }

            right_controller_forward_rot = glm::slerp(m_process_view_rotation_data.last_aim_rot, target_forward, delta_float * vr->get_aim_speed() * spherical_distance);
        } else {
            right_controller_forward_rot = target_forward;
        }

        const auto wanted_rotation = glm::normalize(rotation_offset * right_controller_forward_rot);
        const auto new_rotation = glm::normalize(vqi_norm * wanted_rotation);
        euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

        vr->set_rotation_offset(glm::inverse(utility::math::flatten(right_controller_forward_rot)));

        m_process_view_rotation_data.last_aim_rot = right_controller_forward_rot;

        /*const auto previous_standing_origin = glm::vec3{vr->get_standing_origin()};
        static auto last_delta = glm::vec3{og_controller_pos};
        static auto last_rot_delta = utility::math::flatten(right_controller_forward_rot);
        const auto current_delta = glm::vec3{og_controller_pos};
        const auto current_rot_delta = utility::math::flatten(right_controller_forward_rot);
        const auto delta_delta = current_delta - last_delta;
        const auto rot_delta_delta = glm::normalize(current_rot_delta * glm::inverse(last_rot_delta));
        const auto new_standing_origin = previous_standing_origin + delta_delta;
        vr->set_standing_origin(glm::vec4{new_standing_origin.x, new_standing_origin.y, new_standing_origin.z, 1.0f});

        last_delta = current_delta;
        last_rot_delta = current_rot_delta;*/
    } else {
        const auto current_hmd_rotation = glm::normalize(rotation_offset * og_hmd_rot);
        const auto new_rotation = glm::normalize(vqi_norm * current_hmd_rotation);
        euler = glm::degrees(utility::math::euler_angles_from_steamvr(new_rotation));

        vr->recenter_view();
    }

    if (has_double) {
        rot_d->pitch = euler.x;
        rot_d->yaw = euler.y;
        rot_d->roll = euler.z;
        g_hook->m_last_view_rotation_double = *rot_d;
    } else {
        rot->pitch = euler.x;
        rot->yaw = euler.y;
        rot->roll = euler.z;
        g_hook->m_last_view_rotation = *rot;
    }
}
