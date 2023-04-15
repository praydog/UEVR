#pragma once

#include <cstdint>
#include <optional>

namespace detail {
class IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    virtual bool implemented() const  { return false; }

    // 4.27 only
    virtual std::optional<size_t> LateLatchingEnabled_index() const { return std::nullopt; }

    virtual std::optional<size_t> GetVersionString_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetXRSystemFlags_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesSupportPositionalTracking_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesSupportLateUpdate_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesSupportLateProjectionUpdate_index() const { return std::nullopt; }
    virtual std::optional<size_t> HasValidTrackingPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> EnumerateTrackedDevices_index() const { return std::nullopt; }
    virtual std::optional<size_t> CountTrackedDevices_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsTracking_index() const { return std::nullopt; }
    virtual std::optional<size_t> RefreshPoses_index() const { return std::nullopt; }
    virtual std::optional<size_t> RebaseObjectOrientationAndPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetCurrentPose_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetRelativeEyePose_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingSensorProperties_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackedDeviceType_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackedDevicePropertySerialNumber_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetTrackingOrigin_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingOrigin_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingToWorldTransform_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetWorldToMetersScale_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetFloorToEyeTrackingTransform_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdateTrackingToWorldTransform_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetAudioListenerOffset_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetOrientationAndPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetBaseRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBaseRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetBaseOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBaseOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetBasePosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBasePosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> CalibrateExternalTrackingSource_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdateExternalTrackingPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetXRCamera_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetStereoRenderingDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetXRInput_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetLoadingScreen_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHeadTrackingAllowed_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHeadTrackingAllowedForWorld_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHeadTrackingEnforced_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetHeadTrackingEnforced_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnBeginPlay_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnEndPlay_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnStartGameFrame_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnEndGameFrame_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnBeginRendering_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnBeginRendering_GameThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDData_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetMotionControllerData_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetCurrentInteractionProfile_index() const { return std::nullopt; }
    virtual std::optional<size_t> ConfigureGestures_index() const { return std::nullopt; }
    virtual std::optional<size_t> ConnectRemoteXRDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> DisconnectRemoteXRDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPlayAreaBounds_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingOriginTransform_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPlayAreaRect_index() const { return std::nullopt; }
};

class IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    virtual bool implemented() const { return false; }


    virtual std::optional<size_t> UseImplicitHMDPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetUseImplicitHMDPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> ApplyHMDRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdatePlayerCamera_index() const { return std::nullopt; }
    virtual std::optional<size_t> OverrideFOV_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetupLateUpdate_index() const { return std::nullopt; }
    virtual std::optional<size_t> CalculateStereoCameraOffset_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const { return std::nullopt; }
};
}

namespace ue5_1 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> GetXRSystemFlags_index() const override { return 2; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 3; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 4; }
    std::optional<size_t> DoesSupportLateProjectionUpdate_index() const override { return 5; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 6; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 7; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 8; }
    std::optional<size_t> IsTracking_index() const override { return 9; }
    std::optional<size_t> RefreshPoses_index() const override { return 10; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 11; }
    std::optional<size_t> GetCurrentPose_index() const override { return 12; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 13; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 14; }
    std::optional<size_t> GetTrackedDeviceType_index() const override { return 15; }
    std::optional<size_t> GetTrackedDevicePropertySerialNumber_index() const override { return 16; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 17; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 18; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 19; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 20; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 21; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 22; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 23; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> ResetOrientation_index() const override { return 25; }
    std::optional<size_t> ResetPosition_index() const override { return 26; }
    std::optional<size_t> SetBaseRotation_index() const override { return 27; }
    std::optional<size_t> GetBaseRotation_index() const override { return 28; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 29; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 30; }
    std::optional<size_t> SetBasePosition_index() const override { return 31; }
    std::optional<size_t> GetBasePosition_index() const override { return 32; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 33; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 34; }
    std::optional<size_t> GetXRCamera_index() const override { return 35; }
    std::optional<size_t> GetHMDDevice_index() const override { return 36; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 37; }
    std::optional<size_t> GetXRInput_index() const override { return 38; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 39; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 40; }
    std::optional<size_t> IsHeadTrackingAllowedForWorld_index() const override { return 41; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 42; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 43; }
    std::optional<size_t> OnBeginPlay_index() const override { return 44; }
    std::optional<size_t> OnEndPlay_index() const override { return 45; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 46; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 47; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 48; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 49; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 50; }
    std::optional<size_t> GetHMDData_index() const override { return 51; }
    std::optional<size_t> GetMotionControllerData_index() const override { return 52; }
    std::optional<size_t> GetCurrentInteractionProfile_index() const override { return 53; }
    std::optional<size_t> ConfigureGestures_index() const override { return 54; }
    std::optional<size_t> ConnectRemoteXRDevice_index() const override { return 55; }
    std::optional<size_t> DisconnectRemoteXRDevice_index() const override { return 56; }
    std::optional<size_t> GetPlayAreaBounds_index() const override { return 57; }
    std::optional<size_t> GetTrackingOriginTransform_index() const override { return 58; }
    std::optional<size_t> GetPlayAreaRect_index() const override { return 59; }
};
}

namespace ue5_1 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue5_00 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> GetXRSystemFlags_index() const override { return 2; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 3; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 4; }
    std::optional<size_t> DoesSupportLateProjectionUpdate_index() const override { return 5; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 6; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 7; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 8; }
    std::optional<size_t> IsTracking_index() const override { return 9; }
    std::optional<size_t> RefreshPoses_index() const override { return 10; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 11; }
    std::optional<size_t> GetCurrentPose_index() const override { return 12; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 13; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 14; }
    std::optional<size_t> GetTrackedDeviceType_index() const override { return 15; }
    std::optional<size_t> GetTrackedDevicePropertySerialNumber_index() const override { return 16; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 17; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 18; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 19; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 20; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 21; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 22; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 23; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> ResetOrientation_index() const override { return 25; }
    std::optional<size_t> ResetPosition_index() const override { return 26; }
    std::optional<size_t> SetBaseRotation_index() const override { return 27; }
    std::optional<size_t> GetBaseRotation_index() const override { return 28; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 29; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 30; }
    std::optional<size_t> SetBasePosition_index() const override { return 31; }
    std::optional<size_t> GetBasePosition_index() const override { return 32; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 33; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 34; }
    std::optional<size_t> GetXRCamera_index() const override { return 35; }
    std::optional<size_t> GetHMDDevice_index() const override { return 36; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 37; }
    std::optional<size_t> GetXRInput_index() const override { return 38; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 39; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 40; }
    std::optional<size_t> IsHeadTrackingAllowedForWorld_index() const override { return 41; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 42; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 43; }
    std::optional<size_t> OnBeginPlay_index() const override { return 44; }
    std::optional<size_t> OnEndPlay_index() const override { return 45; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 46; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 47; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 48; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 49; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 50; }
    std::optional<size_t> GetHMDData_index() const override { return 51; }
    std::optional<size_t> GetMotionControllerData_index() const override { return 52; }
    std::optional<size_t> ConfigureGestures_index() const override { return 53; }
    std::optional<size_t> ConnectRemoteXRDevice_index() const override { return 54; }
    std::optional<size_t> DisconnectRemoteXRDevice_index() const override { return 55; }
    std::optional<size_t> GetPlayAreaBounds_index() const override { return 56; }
};
}

namespace ue5_00 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_27 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> GetXRSystemFlags_index() const override { return 2; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 3; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 4; }
    std::optional<size_t> LateLatchingEnabled_index() const override { return 5; }
    std::optional<size_t> DoesSupportLateProjectionUpdate_index() const override { return 6; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 7; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 8; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 9; }
    std::optional<size_t> IsTracking_index() const override { return 10; }
    std::optional<size_t> RefreshPoses_index() const override { return 11; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 12; }
    std::optional<size_t> GetCurrentPose_index() const override { return 13; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 14; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 15; }
    std::optional<size_t> GetTrackedDeviceType_index() const override { return 16; }
    std::optional<size_t> GetTrackedDevicePropertySerialNumber_index() const override { return 17; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 18; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 19; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 20; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 21; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 22; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 23; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 24; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 25; }
    std::optional<size_t> ResetOrientation_index() const override { return 26; }
    std::optional<size_t> ResetPosition_index() const override { return 27; }
    std::optional<size_t> SetBaseRotation_index() const override { return 28; }
    std::optional<size_t> GetBaseRotation_index() const override { return 29; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 30; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 31; }
    std::optional<size_t> SetBasePosition_index() const override { return 32; }
    std::optional<size_t> GetBasePosition_index() const override { return 33; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 34; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 35; }
    std::optional<size_t> GetXRCamera_index() const override { return 36; }
    std::optional<size_t> GetHMDDevice_index() const override { return 37; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 38; }
    std::optional<size_t> GetXRInput_index() const override { return 39; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 40; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 41; }
    std::optional<size_t> IsHeadTrackingAllowedForWorld_index() const override { return 42; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 43; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 44; }
    std::optional<size_t> OnBeginPlay_index() const override { return 45; }
    std::optional<size_t> OnEndPlay_index() const override { return 46; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 47; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 48; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 49; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 50; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 51; }
    std::optional<size_t> GetHMDData_index() const override { return 52; }
    std::optional<size_t> GetMotionControllerData_index() const override { return 53; }
    std::optional<size_t> ConfigureGestures_index() const override { return 54; }
    std::optional<size_t> ConnectRemoteXRDevice_index() const override { return 55; }
    std::optional<size_t> DisconnectRemoteXRDevice_index() const override { return 56; }
    std::optional<size_t> GetPlayAreaBounds_index() const override { return 57; }
};
}

namespace ue4_27 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_26 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> GetXRSystemFlags_index() const override { return 2; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 3; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 4; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 5; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 6; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 7; }
    std::optional<size_t> IsTracking_index() const override { return 8; }
    std::optional<size_t> RefreshPoses_index() const override { return 9; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 10; }
    std::optional<size_t> GetCurrentPose_index() const override { return 11; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 12; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 13; }
    std::optional<size_t> GetTrackedDeviceType_index() const override { return 14; }
    std::optional<size_t> GetTrackedDevicePropertySerialNumber_index() const override { return 15; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 16; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 17; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 18; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 19; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 20; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 21; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 22; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> ResetOrientation_index() const override { return 24; }
    std::optional<size_t> ResetPosition_index() const override { return 25; }
    std::optional<size_t> SetBaseRotation_index() const override { return 26; }
    std::optional<size_t> GetBaseRotation_index() const override { return 27; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 28; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 29; }
    std::optional<size_t> SetBasePosition_index() const override { return 30; }
    std::optional<size_t> GetBasePosition_index() const override { return 31; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 32; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 33; }
    std::optional<size_t> GetXRCamera_index() const override { return 34; }
    std::optional<size_t> GetHMDDevice_index() const override { return 35; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 36; }
    std::optional<size_t> GetXRInput_index() const override { return 37; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 38; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 39; }
    std::optional<size_t> IsHeadTrackingAllowedForWorld_index() const override { return 40; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 41; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 42; }
    std::optional<size_t> OnBeginPlay_index() const override { return 43; }
    std::optional<size_t> OnEndPlay_index() const override { return 44; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 45; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 46; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 47; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 48; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 49; }
    std::optional<size_t> GetHMDData_index() const override { return 50; }
    std::optional<size_t> GetMotionControllerData_index() const override { return 51; }
    std::optional<size_t> ConfigureGestures_index() const override { return 52; }
    std::optional<size_t> ConnectRemoteXRDevice_index() const override { return 53; }
    std::optional<size_t> DisconnectRemoteXRDevice_index() const override { return 54; }
};
}

namespace ue4_26 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_25 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 16; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 17; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 18; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 19; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> ResetOrientation_index() const override { return 21; }
    std::optional<size_t> ResetPosition_index() const override { return 22; }
    std::optional<size_t> SetBaseRotation_index() const override { return 23; }
    std::optional<size_t> GetBaseRotation_index() const override { return 24; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 26; }
    std::optional<size_t> SetBasePosition_index() const override { return 27; }
    std::optional<size_t> GetBasePosition_index() const override { return 28; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 29; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 30; }
    std::optional<size_t> GetXRCamera_index() const override { return 31; }
    std::optional<size_t> GetHMDDevice_index() const override { return 32; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 33; }
    std::optional<size_t> GetXRInput_index() const override { return 34; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 36; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 37; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 38; }
    std::optional<size_t> OnBeginPlay_index() const override { return 39; }
    std::optional<size_t> OnEndPlay_index() const override { return 40; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 41; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 42; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 43; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 44; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 45; }
};
}

namespace ue4_25 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_24 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 16; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 17; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 18; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 19; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> ResetOrientation_index() const override { return 21; }
    std::optional<size_t> ResetPosition_index() const override { return 22; }
    std::optional<size_t> SetBaseRotation_index() const override { return 23; }
    std::optional<size_t> GetBaseRotation_index() const override { return 24; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 26; }
    std::optional<size_t> SetBasePosition_index() const override { return 27; }
    std::optional<size_t> GetBasePosition_index() const override { return 28; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 29; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 30; }
    std::optional<size_t> GetXRCamera_index() const override { return 31; }
    std::optional<size_t> GetHMDDevice_index() const override { return 32; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 33; }
    std::optional<size_t> GetXRInput_index() const override { return 34; }
    std::optional<size_t> GetLoadingScreen_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 36; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 37; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 38; }
    std::optional<size_t> OnBeginPlay_index() const override { return 39; }
    std::optional<size_t> OnEndPlay_index() const override { return 40; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 41; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 42; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 43; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 44; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 45; }
};
}

namespace ue4_24 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_23 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 16; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 17; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 18; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 19; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> ResetOrientation_index() const override { return 21; }
    std::optional<size_t> ResetPosition_index() const override { return 22; }
    std::optional<size_t> SetBaseRotation_index() const override { return 23; }
    std::optional<size_t> GetBaseRotation_index() const override { return 24; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 26; }
    std::optional<size_t> SetBasePosition_index() const override { return 27; }
    std::optional<size_t> GetBasePosition_index() const override { return 28; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 29; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 30; }
    std::optional<size_t> GetXRCamera_index() const override { return 31; }
    std::optional<size_t> GetHMDDevice_index() const override { return 32; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 33; }
    std::optional<size_t> GetXRInput_index() const override { return 34; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 36; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 37; }
    std::optional<size_t> OnBeginPlay_index() const override { return 38; }
    std::optional<size_t> OnEndPlay_index() const override { return 39; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 40; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 41; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 42; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 43; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 44; }
};
}

namespace ue4_23 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_22 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetWorldToMetersScale_index() const override { return 16; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 17; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 18; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 19; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> ResetOrientation_index() const override { return 21; }
    std::optional<size_t> ResetPosition_index() const override { return 22; }
    std::optional<size_t> SetBaseRotation_index() const override { return 23; }
    std::optional<size_t> GetBaseRotation_index() const override { return 24; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 26; }
    std::optional<size_t> SetBasePosition_index() const override { return 27; }
    std::optional<size_t> GetBasePosition_index() const override { return 28; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 29; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 30; }
    std::optional<size_t> GetXRCamera_index() const override { return 31; }
    std::optional<size_t> GetHMDDevice_index() const override { return 32; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 33; }
    std::optional<size_t> GetXRInput_index() const override { return 34; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingEnforced_index() const override { return 36; }
    std::optional<size_t> SetHeadTrackingEnforced_index() const override { return 37; }
    std::optional<size_t> OnBeginPlay_index() const override { return 38; }
    std::optional<size_t> OnEndPlay_index() const override { return 39; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 40; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 41; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 42; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 43; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 44; }
};
}

namespace ue4_22 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
    std::optional<size_t> GetPassthroughCameraUVs_RenderThread_index() const override { return 9; }
};
}

namespace ue4_21 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 16; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 17; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 18; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 19; }
    std::optional<size_t> ResetOrientation_index() const override { return 20; }
    std::optional<size_t> ResetPosition_index() const override { return 21; }
    std::optional<size_t> SetBaseRotation_index() const override { return 22; }
    std::optional<size_t> GetBaseRotation_index() const override { return 23; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 24; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> SetBasePosition_index() const override { return 26; }
    std::optional<size_t> GetBasePosition_index() const override { return 27; }
    std::optional<size_t> CalibrateExternalTrackingSource_index() const override { return 28; }
    std::optional<size_t> UpdateExternalTrackingPosition_index() const override { return 29; }
    std::optional<size_t> GetXRCamera_index() const override { return 30; }
    std::optional<size_t> GetHMDDevice_index() const override { return 31; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 32; }
    std::optional<size_t> GetXRInput_index() const override { return 33; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 34; }
    std::optional<size_t> OnBeginPlay_index() const override { return 35; }
    std::optional<size_t> OnEndPlay_index() const override { return 36; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 37; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 38; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 39; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 40; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 41; }
};
}

namespace ue4_21 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
};
}

namespace ue4_20 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> GetFloorToEyeTrackingTransform_index() const override { return 16; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 17; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 18; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 19; }
    std::optional<size_t> ResetOrientation_index() const override { return 20; }
    std::optional<size_t> ResetPosition_index() const override { return 21; }
    std::optional<size_t> SetBaseRotation_index() const override { return 22; }
    std::optional<size_t> GetBaseRotation_index() const override { return 23; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 24; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 25; }
    std::optional<size_t> GetXRCamera_index() const override { return 26; }
    std::optional<size_t> GetHMDDevice_index() const override { return 27; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 28; }
    std::optional<size_t> GetXRInput_index() const override { return 29; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 30; }
    std::optional<size_t> OnBeginPlay_index() const override { return 31; }
    std::optional<size_t> OnEndPlay_index() const override { return 32; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 33; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 34; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 35; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 36; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 37; }
};
}

namespace ue4_20 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
};
}

namespace ue4_19 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> DoesSupportLateUpdate_index() const override { return 3; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 4; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 5; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 6; }
    std::optional<size_t> IsTracking_index() const override { return 7; }
    std::optional<size_t> RefreshPoses_index() const override { return 8; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 9; }
    std::optional<size_t> GetCurrentPose_index() const override { return 10; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 11; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 12; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 14; }
    std::optional<size_t> GetTrackingToWorldTransform_index() const override { return 15; }
    std::optional<size_t> UpdateTrackingToWorldTransform_index() const override { return 16; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 17; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 18; }
    std::optional<size_t> ResetOrientation_index() const override { return 19; }
    std::optional<size_t> ResetPosition_index() const override { return 20; }
    std::optional<size_t> SetBaseRotation_index() const override { return 21; }
    std::optional<size_t> GetBaseRotation_index() const override { return 22; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 23; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 24; }
    std::optional<size_t> GetXRCamera_index() const override { return 25; }
    std::optional<size_t> GetHMDDevice_index() const override { return 26; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 27; }
    std::optional<size_t> GetXRInput_index() const override { return 28; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 29; }
    std::optional<size_t> OnBeginPlay_index() const override { return 30; }
    std::optional<size_t> OnEndPlay_index() const override { return 31; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 32; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 33; }
    std::optional<size_t> OnBeginRendering_RenderThread_index() const override { return 34; }
    std::optional<size_t> OnBeginRendering_GameThread_index() const override { return 35; }
    std::optional<size_t> OnLateUpdateApplied_RenderThread_index() const override { return 36; }
};
}

namespace ue4_19 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> GetUseImplicitHMDPosition_index() const override { return 3; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 4; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 5; }
    std::optional<size_t> OverrideFOV_index() const override { return 6; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 7; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 8; }
};
}

namespace ue4_18 {
class IXRTrackingSystemVT : public detail::IXRTrackingSystemVT {
public:
    static IXRTrackingSystemVT& get() {
        static IXRTrackingSystemVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> GetVersionString_index() const override { return 1; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 2; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 3; }
    std::optional<size_t> EnumerateTrackedDevices_index() const override { return 4; }
    std::optional<size_t> CountTrackedDevices_index() const override { return 5; }
    std::optional<size_t> IsTracking_index() const override { return 6; }
    std::optional<size_t> RefreshPoses_index() const override { return 7; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 8; }
    std::optional<size_t> GetCurrentPose_index() const override { return 9; }
    std::optional<size_t> GetRelativeEyePose_index() const override { return 10; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 11; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 12; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 13; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 14; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 15; }
    std::optional<size_t> ResetOrientation_index() const override { return 16; }
    std::optional<size_t> ResetPosition_index() const override { return 17; }
    std::optional<size_t> SetBaseRotation_index() const override { return 18; }
    std::optional<size_t> GetBaseRotation_index() const override { return 19; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 20; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 21; }
    std::optional<size_t> GetXRCamera_index() const override { return 22; }
    std::optional<size_t> GetHMDDevice_index() const override { return 23; }
    std::optional<size_t> GetStereoRenderingDevice_index() const override { return 24; }
    std::optional<size_t> GetXRInput_index() const override { return 25; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 26; }
    std::optional<size_t> OnBeginPlay_index() const override { return 27; }
    std::optional<size_t> OnEndPlay_index() const override { return 28; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 29; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 30; }
};
}

namespace ue4_18 {
class IXRCameraVT : public detail::IXRCameraVT {
public:
    static IXRCameraVT& get() {
        static IXRCameraVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> UseImplicitHMDPosition_index() const override { return 2; }
    std::optional<size_t> ApplyHMDRotation_index() const override { return 3; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 4; }
    std::optional<size_t> OverrideFOV_index() const override { return 5; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 6; }
    std::optional<size_t> CalculateStereoCameraOffset_index() const override { return 7; }
};
}

