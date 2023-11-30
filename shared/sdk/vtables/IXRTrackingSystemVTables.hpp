// Generated with the help of Narknon and UE4SS using PDBs
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

class IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    virtual bool implemented() const { return false; }

    virtual std::optional<size_t> __vecDelDtor_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDeviceName_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHMDConnected_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHMDEnabled_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDWornState_index() const { return std::nullopt; }
    virtual std::optional<size_t> EnableHMD_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDDeviceType_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDMonitorInfo_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetFieldOfView_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesSupportPositionalTracking_index() const { return std::nullopt; }
    virtual std::optional<size_t> HasValidTrackingPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPositionalTrackingCameraProperties_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetNumOfTrackingSensors_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingSensorProperties_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetInterpupillaryDistance_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetInterpupillaryDistance_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetCurrentOrientationAndPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> RebaseObjectOrientationAndPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetAudioListenerOffset_index() const { return std::nullopt; }
    virtual std::optional<size_t> GatherViewExtensions_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetSpectatorScreenController_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetSpectatorScreenController_2_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetViewExtension_index() const { return std::nullopt; }
    virtual std::optional<size_t> ApplyHmdRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdatePlayerCamera_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDistortionScalingFactor_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetLensCenterOffset_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDistortionWarpValues_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsChromaAbCorrectionEnabled_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetChromaAbCorrectionValues_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsPositionalTrackingEnabled_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHeadTrackingAllowed_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetOrientationAndPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> ResetPosition_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetBaseRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBaseRotation_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetBaseOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBaseOrientation_index() const { return std::nullopt; }
    virtual std::optional<size_t> HasHiddenAreaMesh_index() const { return std::nullopt; }
    virtual std::optional<size_t> HasVisibleAreaMesh_index() const { return std::nullopt; }
    virtual std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> DrawDistortionMesh_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdateScreenSettings_index() const { return std::nullopt; }
    virtual std::optional<size_t> UpdatePostProcessSettings_index() const { return std::nullopt; }
    virtual std::optional<size_t> DrawDebug_index() const { return std::nullopt; }
    virtual std::optional<size_t> HandleInputKey_index() const { return std::nullopt; }
    virtual std::optional<size_t> HandleInputTouch_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnBeginPlay_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnEndPlay_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnStartGameFrame_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnEndGameFrame_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDistortionTextureLeft_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDistortionTextureRight_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTextureOffsetLeft_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTextureOffsetRight_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTextureScaleLeft_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTextureScaleRight_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetRedDistortionParameters_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetGreenDistortionParameters_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetBlueDistortionParameters_index() const { return std::nullopt; }
    virtual std::optional<size_t> NeedsUpscalePostProcessPass_index() const { return std::nullopt; }
    virtual std::optional<size_t> RecordAnalytics_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetVersionString_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetTrackingOrigin_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetTrackingOrigin_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesAppUseVRFocus_index() const { return std::nullopt; }
    virtual std::optional<size_t> DoesAppHaveVRFocus_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetupLateUpdate_index() const { return std::nullopt; }
    virtual std::optional<size_t> ApplyLateUpdate_index() const { return std::nullopt; }

    virtual std::optional<size_t> DrawHiddenAreaMesh_index() const { return std::nullopt; }
    virtual std::optional<size_t> DrawVisibleAreaMesh_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDName_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetClippingPlanes_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetEyeRenderParams_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetHMDDistortionEnabled_index() const { return std::nullopt; }
    virtual std::optional<size_t> BeginRendering_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> BeginRendering_GameThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsSpectatorScreenActive_index() const { return std::nullopt; }
    virtual std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPixelDenity_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetPixelDensity_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetIdealRenderTargetSize_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const { return std::nullopt; }

    virtual std::optional<size_t> IsRenderingPaused_index() const { return std::nullopt; }

    // starting <= 4.15
    virtual std::optional<size_t> Exec_index() const { return std::nullopt; }
    virtual std::optional<size_t> EnablePositionalTracking_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsInLowPersistenceMode_index() const { return std::nullopt; }
    virtual std::optional<size_t> EnableLowPersistenceMode_index() const { return std::nullopt; }
    virtual std::optional<size_t> SetPositionScale3D_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetPositionScale3D_index() const { return std::nullopt; }

    // <= 4.11
    virtual std::optional<size_t> IsFullscreenAllowed_index() const { return std::nullopt; }
    virtual std::optional<size_t> PushPreFullScreenRect_index() const { return std::nullopt; }
    virtual std::optional<size_t> PopPreFullScreenRect_index() const { return std::nullopt; }
    virtual std::optional<size_t> OnScreenModeChange_index() const { return std::nullopt; }
};

class IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    virtual bool implemented() const { return false; }

    virtual std::optional<size_t> __vecDelDtor_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetModuleKeyName_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetModuleAliases_index() const { return std::nullopt; }
    virtual std::optional<size_t> PreInit_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsHMDConnected_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetGraphicsAdapterLuid_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetAudioInputDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetAudioOutputDevice_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetDeviceSystemName_index() const { return std::nullopt; }
    virtual std::optional<size_t> CreateTrackingSystem_index() const { return std::nullopt; }
    virtual std::optional<size_t> GetVulkanExtensions_index() const { return std::nullopt; }
    virtual std::optional<size_t> IsStandaloneStereoOnlyDevice_index() const { return std::nullopt; }

    virtual std::optional<size_t> GetGraphicsAdapter_index() const { return std::nullopt; }
    virtual std::optional<size_t> CreateHeadMountedDisplay_index() const { return std::nullopt; }

    // 4.13
    virtual std::optional<size_t> GetModulePriorityKeyName_index() const { return std::nullopt; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> GetDeviceSystemName_index() const override { return 15; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 16; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 17; }
    std::optional<size_t> IsStandaloneStereoOnlyDevice_index() const override { return 18; }
};
}

namespace ue5_1 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDName_index() const override { return 12; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 13; }
    std::optional<size_t> GetFieldOfView_index() const override { return 14; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 15; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 19; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 20; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 21; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 23; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 24; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 25; }
    std::optional<size_t> GetPixelDenity_index() const override { return 26; }
    std::optional<size_t> SetPixelDensity_index() const override { return 27; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 29; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 30; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 31; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 32; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 33; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 34; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 35; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 36; }
    std::optional<size_t> DrawHiddenAreaMesh_index() const override { return 37; }
    std::optional<size_t> DrawVisibleAreaMesh_index() const override { return 38; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 39; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 41; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 43; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 45; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 46; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 48; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 49; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 50; }
    std::optional<size_t> RecordAnalytics_index() const override { return 51; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 52; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 53; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 54; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> GetDeviceSystemName_index() const override { return 15; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 16; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 17; }
    std::optional<size_t> IsStandaloneStereoOnlyDevice_index() const override { return 18; }
};
}

namespace ue5_00 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDName_index() const override { return 12; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 13; }
    std::optional<size_t> GetFieldOfView_index() const override { return 14; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 15; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 19; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 20; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 21; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 23; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 24; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 25; }
    std::optional<size_t> GetPixelDenity_index() const override { return 26; }
    std::optional<size_t> SetPixelDensity_index() const override { return 27; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 29; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 30; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 31; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 32; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 33; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 34; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 35; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 36; }
    std::optional<size_t> DrawHiddenAreaMesh_index() const override { return 37; }
    std::optional<size_t> DrawVisibleAreaMesh_index() const override { return 38; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 39; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 41; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 43; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 45; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 46; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 48; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 49; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 50; }
    std::optional<size_t> RecordAnalytics_index() const override { return 51; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 52; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 53; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 54; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
    std::optional<size_t> IsStandaloneStereoOnlyDevice_index() const override { return 17; }
};
}

namespace ue4_27 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDName_index() const override { return 12; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 13; }
    std::optional<size_t> GetFieldOfView_index() const override { return 14; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 15; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 19; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 20; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 21; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 23; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 24; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 25; }
    std::optional<size_t> GetPixelDenity_index() const override { return 26; }
    std::optional<size_t> SetPixelDensity_index() const override { return 27; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 29; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 30; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 31; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 32; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 33; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 34; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 35; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 36; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 38; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 39; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 41; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 43; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 45; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 46; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 48; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 49; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 50; }
    std::optional<size_t> RecordAnalytics_index() const override { return 51; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 52; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 53; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 54; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_26 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 24; }
    std::optional<size_t> GetPixelDenity_index() const override { return 25; }
    std::optional<size_t> SetPixelDensity_index() const override { return 26; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 34; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 35; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 38; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 45; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 48; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 49; }
    std::optional<size_t> RecordAnalytics_index() const override { return 50; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 51; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 52; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 53; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_25 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 24; }
    std::optional<size_t> GetPixelDenity_index() const override { return 25; }
    std::optional<size_t> SetPixelDensity_index() const override { return 26; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 34; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 35; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 38; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 45; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 48; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 49; }
    std::optional<size_t> RecordAnalytics_index() const override { return 50; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 51; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 52; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 53; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_24 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> CreateHMDPostProcessPass_RenderThread_index() const override { return 24; }
    std::optional<size_t> GetPixelDenity_index() const override { return 25; }
    std::optional<size_t> SetPixelDensity_index() const override { return 26; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 34; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 35; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 38; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 45; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 48; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 49; }
    std::optional<size_t> RecordAnalytics_index() const override { return 50; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 51; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 52; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 53; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_23 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> GetPixelDenity_index() const override { return 24; }
    std::optional<size_t> SetPixelDensity_index() const override { return 25; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 26; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 28; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 29; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 30; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 31; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 32; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 33; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 34; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 35; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 38; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 40; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 42; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 44; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 45; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 47; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 48; }
    std::optional<size_t> RecordAnalytics_index() const override { return 49; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 50; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 51; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 52; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_22 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> GetPixelDenity_index() const override { return 24; }
    std::optional<size_t> SetPixelDensity_index() const override { return 25; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 26; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 28; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 29; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 30; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 31; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 32; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 33; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 34; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 35; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 38; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 45; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 48; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 49; }
    std::optional<size_t> RecordAnalytics_index() const override { return 50; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 51; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 52; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 53; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_21 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> IsSpectatorScreenActive_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> GetPixelDenity_index() const override { return 24; }
    std::optional<size_t> SetPixelDensity_index() const override { return 25; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 26; }
    std::optional<size_t> GetIdealDebugCanvasRenderTargetSize_index() const override { return 27; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 28; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 29; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 30; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 31; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 32; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 33; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 34; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 35; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 36; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 37; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 38; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 39; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 40; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 41; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 43; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 44; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 45; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 46; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 47; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 48; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 49; }
    std::optional<size_t> RecordAnalytics_index() const override { return 50; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 51; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 52; }
    std::optional<size_t> IsRenderingPaused_index() const override { return 53; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_20 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 22; }
    std::optional<size_t> GetPixelDenity_index() const override { return 23; }
    std::optional<size_t> SetPixelDensity_index() const override { return 24; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 25; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 26; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 27; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 28; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 29; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 30; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 31; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 32; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 33; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 34; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 35; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 36; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 37; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 38; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 39; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 40; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 41; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 43; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 44; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 45; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 46; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 47; }
    std::optional<size_t> RecordAnalytics_index() const override { return 48; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 49; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 50; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_19 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 14; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 15; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 16; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 18; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 19; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 20; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 22; }
    std::optional<size_t> GetPixelDenity_index() const override { return 23; }
    std::optional<size_t> SetPixelDensity_index() const override { return 24; }
    std::optional<size_t> GetIdealRenderTargetSize_index() const override { return 25; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 26; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 27; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 28; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 29; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 30; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 31; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 32; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 33; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 34; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 35; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 36; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 37; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 38; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 39; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 40; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 41; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 42; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 43; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 44; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 45; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 46; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 47; }
    std::optional<size_t> RecordAnalytics_index() const override { return 48; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 49; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 50; }
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
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapterLuid_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateTrackingSystem_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_18 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> GetHMDWornState_index() const override { return 10; }
    std::optional<size_t> EnableHMD_index() const override { return 11; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 12; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 13; }
    std::optional<size_t> GetFieldOfView_index() const override { return 14; }
    std::optional<size_t> SetClippingPlanes_index() const override { return 15; }
    std::optional<size_t> GetEyeRenderParams_RenderThread_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetHMDDistortionEnabled_index() const override { return 19; }
    std::optional<size_t> BeginRendering_RenderThread_index() const override { return 20; }
    std::optional<size_t> BeginRendering_GameThread_index() const override { return 21; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 22; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 23; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 24; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 25; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 26; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 27; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 28; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 29; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 30; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 31; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 32; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 33; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 34; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 35; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 36; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 37; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 38; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 39; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 40; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 41; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 42; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 43; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 44; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 45; }
    std::optional<size_t> RecordAnalytics_index() const override { return 46; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 47; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 48; }
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

namespace ue4_17 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> GetModuleAliases_index() const override { return 9; }
    std::optional<size_t> PreInit_index() const override { return 10; }
    std::optional<size_t> IsHMDConnected_index() const override { return 11; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 12; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 13; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 14; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 15; }
    std::optional<size_t> GetVulkanExtensions_index() const override { return 16; }
};
}

namespace ue4_17 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetDeviceName_index() const override { return 8; }
    std::optional<size_t> IsHMDConnected_index() const override { return 9; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 10; }
    std::optional<size_t> GetHMDWornState_index() const override { return 11; }
    std::optional<size_t> EnableHMD_index() const override { return 12; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 13; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 14; }
    std::optional<size_t> GetFieldOfView_index() const override { return 15; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 16; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 17; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 18; }
    std::optional<size_t> GetNumOfTrackingSensors_index() const override { return 19; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 20; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 21; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 22; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 25; }
    std::optional<size_t> GatherViewExtensions_index() const override { return 26; }
    std::optional<size_t> GetSpectatorScreenController_index() const override { return 27; }
    std::optional<size_t> GetSpectatorScreenController_2_index() const override { return 28; }
    std::optional<size_t> GetViewExtension_index() const override { return 29; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 30; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 31; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 32; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 33; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 34; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 35; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 36; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 37; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 38; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 39; }
    std::optional<size_t> ResetOrientation_index() const override { return 40; }
    std::optional<size_t> ResetPosition_index() const override { return 41; }
    std::optional<size_t> SetBaseRotation_index() const override { return 42; }
    std::optional<size_t> GetBaseRotation_index() const override { return 43; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 44; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 46; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 47; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 48; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 49; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 50; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 51; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 52; }
    std::optional<size_t> DrawDebug_index() const override { return 53; }
    std::optional<size_t> HandleInputKey_index() const override { return 54; }
    std::optional<size_t> HandleInputTouch_index() const override { return 55; }
    std::optional<size_t> OnBeginPlay_index() const override { return 56; }
    std::optional<size_t> OnEndPlay_index() const override { return 57; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 58; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 59; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 60; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 61; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 62; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 63; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 64; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 65; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 66; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 67; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 68; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 69; }
    std::optional<size_t> RecordAnalytics_index() const override { return 70; }
    std::optional<size_t> GetVersionString_index() const override { return 71; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 72; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 73; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 74; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 75; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 76; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 77; }
};
}

namespace ue4_16 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> IsHMDConnected_index() const override { return 10; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 11; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 12; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 13; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 14; }
};
}

namespace ue4_16 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetDeviceName_index() const override { return 8; }
    std::optional<size_t> IsHMDConnected_index() const override { return 9; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 10; }
    std::optional<size_t> GetHMDWornState_index() const override { return 11; }
    std::optional<size_t> EnableHMD_index() const override { return 12; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 13; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 14; }
    std::optional<size_t> GetFieldOfView_index() const override { return 15; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 16; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 17; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 18; }
    std::optional<size_t> GetNumOfTrackingSensors_index() const override { return 19; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 20; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 21; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 22; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 25; }
    std::optional<size_t> GatherViewExtensions_index() const override { return 26; }
    std::optional<size_t> GetViewExtension_index() const override { return 27; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 28; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 29; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 30; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 31; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 32; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 33; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 34; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 36; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 37; }
    std::optional<size_t> ResetOrientation_index() const override { return 38; }
    std::optional<size_t> ResetPosition_index() const override { return 39; }
    std::optional<size_t> SetBaseRotation_index() const override { return 40; }
    std::optional<size_t> GetBaseRotation_index() const override { return 41; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 42; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 43; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 44; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 45; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 46; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 47; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 48; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 49; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 50; }
    std::optional<size_t> DrawDebug_index() const override { return 51; }
    std::optional<size_t> HandleInputKey_index() const override { return 52; }
    std::optional<size_t> HandleInputTouch_index() const override { return 53; }
    std::optional<size_t> OnBeginPlay_index() const override { return 54; }
    std::optional<size_t> OnEndPlay_index() const override { return 55; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 56; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 57; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 58; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 59; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 60; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 61; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 62; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 63; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 64; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 65; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 66; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 67; }
    std::optional<size_t> RecordAnalytics_index() const override { return 68; }
    std::optional<size_t> GetVersionString_index() const override { return 69; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 70; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 71; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 72; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 73; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 74; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 75; }
};
}

namespace ue4_15 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> IsHMDConnected_index() const override { return 10; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 11; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 12; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 13; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 14; }
};
}

namespace ue4_15 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetDeviceName_index() const override { return 8; }
    std::optional<size_t> IsHMDConnected_index() const override { return 9; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 10; }
    std::optional<size_t> GetHMDWornState_index() const override { return 11; }
    std::optional<size_t> EnableHMD_index() const override { return 12; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 13; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 14; }
    std::optional<size_t> GetFieldOfView_index() const override { return 15; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 16; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 17; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 18; }
    std::optional<size_t> GetNumOfTrackingSensors_index() const override { return 19; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 20; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 21; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 22; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 25; }
    std::optional<size_t> GetViewExtension_index() const override { return 26; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 27; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> Exec_index() const override { return 34; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 35; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 36; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 37; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 38; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 39; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 40; }
    std::optional<size_t> ResetOrientation_index() const override { return 41; }
    std::optional<size_t> ResetPosition_index() const override { return 42; }
    std::optional<size_t> SetBaseRotation_index() const override { return 43; }
    std::optional<size_t> GetBaseRotation_index() const override { return 44; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 46; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 47; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 48; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 49; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 50; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 51; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 52; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 53; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 54; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 55; }
    std::optional<size_t> DrawDebug_index() const override { return 56; }
    std::optional<size_t> HandleInputKey_index() const override { return 57; }
    std::optional<size_t> HandleInputTouch_index() const override { return 58; }
    std::optional<size_t> OnBeginPlay_index() const override { return 59; }
    std::optional<size_t> OnEndPlay_index() const override { return 60; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 61; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 62; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 63; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 64; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 65; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 66; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 67; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 68; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 69; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 70; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 71; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 72; }
    std::optional<size_t> RecordAnalytics_index() const override { return 73; }
    std::optional<size_t> GetVersionString_index() const override { return 74; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 75; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 76; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 77; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 78; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 79; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 80; }
};
}

namespace ue4_14 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModuleKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> IsHMDConnected_index() const override { return 10; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 11; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 12; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 13; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 14; }
};
}

namespace ue4_14 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetDeviceName_index() const override { return 8; }
    std::optional<size_t> IsHMDConnected_index() const override { return 9; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 10; }
    std::optional<size_t> GetHMDWornState_index() const override { return 11; }
    std::optional<size_t> EnableHMD_index() const override { return 12; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 13; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 14; }
    std::optional<size_t> GetFieldOfView_index() const override { return 15; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 16; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 17; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 18; }
    std::optional<size_t> GetNumOfTrackingSensors_index() const override { return 19; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 20; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 21; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 22; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 25; }
    std::optional<size_t> GetViewExtension_index() const override { return 26; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 27; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> Exec_index() const override { return 34; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 35; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 36; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 37; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 38; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 39; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 40; }
    std::optional<size_t> ResetOrientation_index() const override { return 41; }
    std::optional<size_t> ResetPosition_index() const override { return 42; }
    std::optional<size_t> SetBaseRotation_index() const override { return 43; }
    std::optional<size_t> GetBaseRotation_index() const override { return 44; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 46; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 47; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 48; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 49; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 50; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 51; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 52; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 53; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 54; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 55; }
    std::optional<size_t> DrawDebug_index() const override { return 56; }
    std::optional<size_t> HandleInputKey_index() const override { return 57; }
    std::optional<size_t> HandleInputTouch_index() const override { return 58; }
    std::optional<size_t> OnBeginPlay_index() const override { return 59; }
    std::optional<size_t> OnEndPlay_index() const override { return 60; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 61; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 62; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 63; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 64; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 65; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 66; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 67; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 68; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 69; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 70; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 71; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 72; }
    std::optional<size_t> RecordAnalytics_index() const override { return 73; }
    std::optional<size_t> GetVersionString_index() const override { return 74; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 75; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 76; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 77; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 78; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 79; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 80; }
};
}

namespace ue4_13 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModulePriorityKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> IsHMDConnected_index() const override { return 10; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 11; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 12; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 13; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 14; }
};
}

namespace ue4_13 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetDeviceName_index() const override { return 8; }
    std::optional<size_t> IsHMDConnected_index() const override { return 9; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 10; }
    std::optional<size_t> GetHMDWornState_index() const override { return 11; }
    std::optional<size_t> EnableHMD_index() const override { return 12; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 13; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 14; }
    std::optional<size_t> GetFieldOfView_index() const override { return 15; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 16; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 17; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 18; }
    std::optional<size_t> GetNumOfTrackingSensors_index() const override { return 19; }
    std::optional<size_t> GetTrackingSensorProperties_index() const override { return 20; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 21; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 22; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 23; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 24; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 25; }
    std::optional<size_t> GetViewExtension_index() const override { return 26; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 27; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 28; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 29; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 30; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 31; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 32; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 33; }
    std::optional<size_t> Exec_index() const override { return 34; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 35; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 36; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 37; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 38; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 39; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 40; }
    std::optional<size_t> ResetOrientation_index() const override { return 41; }
    std::optional<size_t> ResetPosition_index() const override { return 42; }
    std::optional<size_t> SetBaseRotation_index() const override { return 43; }
    std::optional<size_t> GetBaseRotation_index() const override { return 44; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 46; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 47; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 48; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 49; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 50; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 51; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 52; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 53; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 54; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 55; }
    std::optional<size_t> DrawDebug_index() const override { return 56; }
    std::optional<size_t> HandleInputKey_index() const override { return 57; }
    std::optional<size_t> HandleInputTouch_index() const override { return 58; }
    std::optional<size_t> OnBeginPlay_index() const override { return 59; }
    std::optional<size_t> OnEndPlay_index() const override { return 60; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 61; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 62; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 63; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 64; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 65; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 66; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 67; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 68; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 69; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 70; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 71; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 72; }
    std::optional<size_t> RecordAnalytics_index() const override { return 73; }
    std::optional<size_t> GetVersionString_index() const override { return 74; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 75; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 76; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 77; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 78; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 79; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 80; }
};
}

namespace ue4_12 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModulePriorityKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> IsHMDConnected_index() const override { return 10; }
    std::optional<size_t> GetGraphicsAdapter_index() const override { return 11; }
    std::optional<size_t> GetAudioInputDevice_index() const override { return 12; }
    std::optional<size_t> GetAudioOutputDevice_index() const override { return 13; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 14; }
};
}

namespace ue4_12 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> EnableHMD_index() const override { return 10; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 14; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 15; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 19; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> GetAudioListenerOffset_index() const override { return 21; }
    std::optional<size_t> GetViewExtension_index() const override { return 22; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 23; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 24; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 25; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 26; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 27; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 28; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 29; }
    std::optional<size_t> Exec_index() const override { return 30; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 31; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 32; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 33; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 34; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 35; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 36; }
    std::optional<size_t> ResetOrientation_index() const override { return 37; }
    std::optional<size_t> ResetPosition_index() const override { return 38; }
    std::optional<size_t> SetBaseRotation_index() const override { return 39; }
    std::optional<size_t> GetBaseRotation_index() const override { return 40; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 41; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 42; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 43; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 44; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 45; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 46; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 47; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 48; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 49; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 50; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 51; }
    std::optional<size_t> DrawDebug_index() const override { return 52; }
    std::optional<size_t> HandleInputKey_index() const override { return 53; }
    std::optional<size_t> HandleInputTouch_index() const override { return 54; }
    std::optional<size_t> OnBeginPlay_index() const override { return 55; }
    std::optional<size_t> OnEndPlay_index() const override { return 56; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 57; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 58; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 59; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 60; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 61; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 62; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 63; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 64; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 65; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 66; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 67; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 68; }
    std::optional<size_t> RecordAnalytics_index() const override { return 69; }
    std::optional<size_t> GetVersionString_index() const override { return 70; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 71; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 72; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 73; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 74; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 75; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 76; }
};
}

namespace ue4_11 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModulePriorityKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 10; }
};
}

namespace ue4_11 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> EnableHMD_index() const override { return 10; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 14; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 15; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 19; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> GetViewExtension_index() const override { return 21; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 22; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 23; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 24; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 25; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 26; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 27; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 28; }
    std::optional<size_t> Exec_index() const override { return 29; }
    std::optional<size_t> IsFullscreenAllowed_index() const override { return 30; }
    std::optional<size_t> PushPreFullScreenRect_index() const override { return 31; }
    std::optional<size_t> PopPreFullScreenRect_index() const override { return 32; }
    std::optional<size_t> OnScreenModeChange_index() const override { return 33; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 34; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 36; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 37; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 38; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 39; }
    std::optional<size_t> ResetOrientation_index() const override { return 40; }
    std::optional<size_t> ResetPosition_index() const override { return 41; }
    std::optional<size_t> SetBaseRotation_index() const override { return 42; }
    std::optional<size_t> GetBaseRotation_index() const override { return 43; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 44; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 46; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 47; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 48; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 49; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 50; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 51; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 52; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 53; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 54; }
    std::optional<size_t> DrawDebug_index() const override { return 55; }
    std::optional<size_t> HandleInputKey_index() const override { return 56; }
    std::optional<size_t> OnBeginPlay_index() const override { return 57; }
    std::optional<size_t> OnEndPlay_index() const override { return 58; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 59; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 60; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 61; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 62; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 63; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 64; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 65; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 66; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 67; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 68; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 69; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 70; }
    std::optional<size_t> RecordAnalytics_index() const override { return 71; }
    std::optional<size_t> GetVersionString_index() const override { return 72; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 73; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 74; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 75; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 76; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 77; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 78; }
};
}

namespace ue4_10 {
class IHeadMountedDisplayModuleVT : public detail::IHeadMountedDisplayModuleVT {
public:
    static IHeadMountedDisplayModuleVT& get() {
        static IHeadMountedDisplayModuleVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> GetModulePriorityKeyName_index() const override { return 8; }
    std::optional<size_t> PreInit_index() const override { return 9; }
    std::optional<size_t> CreateHeadMountedDisplay_index() const override { return 10; }
};
}

namespace ue4_10 {
class IHeadMountedDisplayVT : public detail::IHeadMountedDisplayVT {
public:
    static IHeadMountedDisplayVT& get() {
        static IHeadMountedDisplayVT instance;
        return instance;
    }

    bool implemented() const override { return true; }

    std::optional<size_t> __vecDelDtor_index() const override { return 0; }
    std::optional<size_t> IsHMDConnected_index() const override { return 8; }
    std::optional<size_t> IsHMDEnabled_index() const override { return 9; }
    std::optional<size_t> EnableHMD_index() const override { return 10; }
    std::optional<size_t> GetHMDDeviceType_index() const override { return 11; }
    std::optional<size_t> GetHMDMonitorInfo_index() const override { return 12; }
    std::optional<size_t> GetFieldOfView_index() const override { return 13; }
    std::optional<size_t> DoesSupportPositionalTracking_index() const override { return 14; }
    std::optional<size_t> HasValidTrackingPosition_index() const override { return 15; }
    std::optional<size_t> GetPositionalTrackingCameraProperties_index() const override { return 16; }
    std::optional<size_t> SetInterpupillaryDistance_index() const override { return 17; }
    std::optional<size_t> GetInterpupillaryDistance_index() const override { return 18; }
    std::optional<size_t> GetCurrentOrientationAndPosition_index() const override { return 19; }
    std::optional<size_t> RebaseObjectOrientationAndPosition_index() const override { return 20; }
    std::optional<size_t> GetViewExtension_index() const override { return 21; }
    std::optional<size_t> ApplyHmdRotation_index() const override { return 22; }
    std::optional<size_t> UpdatePlayerCamera_index() const override { return 23; }
    std::optional<size_t> GetDistortionScalingFactor_index() const override { return 24; }
    std::optional<size_t> GetLensCenterOffset_index() const override { return 25; }
    std::optional<size_t> GetDistortionWarpValues_index() const override { return 26; }
    std::optional<size_t> IsChromaAbCorrectionEnabled_index() const override { return 27; }
    std::optional<size_t> GetChromaAbCorrectionValues_index() const override { return 28; }
    std::optional<size_t> Exec_index() const override { return 29; }
    std::optional<size_t> IsFullscreenAllowed_index() const override { return 30; }
    std::optional<size_t> PushPreFullScreenRect_index() const override { return 31; }
    std::optional<size_t> PopPreFullScreenRect_index() const override { return 32; }
    std::optional<size_t> OnScreenModeChange_index() const override { return 33; }
    std::optional<size_t> IsPositionalTrackingEnabled_index() const override { return 34; }
    std::optional<size_t> EnablePositionalTracking_index() const override { return 35; }
    std::optional<size_t> IsHeadTrackingAllowed_index() const override { return 36; }
    std::optional<size_t> IsInLowPersistenceMode_index() const override { return 37; }
    std::optional<size_t> EnableLowPersistenceMode_index() const override { return 38; }
    std::optional<size_t> ResetOrientationAndPosition_index() const override { return 39; }
    std::optional<size_t> ResetOrientation_index() const override { return 40; }
    std::optional<size_t> ResetPosition_index() const override { return 41; }
    std::optional<size_t> SetBaseRotation_index() const override { return 42; }
    std::optional<size_t> GetBaseRotation_index() const override { return 43; }
    std::optional<size_t> SetBaseOrientation_index() const override { return 44; }
    std::optional<size_t> GetBaseOrientation_index() const override { return 45; }
    std::optional<size_t> SetPositionScale3D_index() const override { return 46; }
    std::optional<size_t> GetPositionScale3D_index() const override { return 47; }
    std::optional<size_t> HasHiddenAreaMesh_index() const override { return 48; }
    std::optional<size_t> HasVisibleAreaMesh_index() const override { return 49; }
    std::optional<size_t> DrawHiddenAreaMesh_RenderThread_index() const override { return 50; }
    std::optional<size_t> DrawVisibleAreaMesh_RenderThread_index() const override { return 51; }
    std::optional<size_t> DrawDistortionMesh_RenderThread_index() const override { return 52; }
    std::optional<size_t> UpdateScreenSettings_index() const override { return 53; }
    std::optional<size_t> UpdatePostProcessSettings_index() const override { return 54; }
    std::optional<size_t> DrawDebug_index() const override { return 55; }
    std::optional<size_t> HandleInputKey_index() const override { return 56; }
    std::optional<size_t> OnBeginPlay_index() const override { return 57; }
    std::optional<size_t> OnEndPlay_index() const override { return 58; }
    std::optional<size_t> OnStartGameFrame_index() const override { return 59; }
    std::optional<size_t> OnEndGameFrame_index() const override { return 60; }
    std::optional<size_t> GetDistortionTextureLeft_index() const override { return 61; }
    std::optional<size_t> GetDistortionTextureRight_index() const override { return 62; }
    std::optional<size_t> GetTextureOffsetLeft_index() const override { return 63; }
    std::optional<size_t> GetTextureOffsetRight_index() const override { return 64; }
    std::optional<size_t> GetTextureScaleLeft_index() const override { return 65; }
    std::optional<size_t> GetTextureScaleRight_index() const override { return 66; }
    std::optional<size_t> GetRedDistortionParameters_index() const override { return 67; }
    std::optional<size_t> GetGreenDistortionParameters_index() const override { return 68; }
    std::optional<size_t> GetBlueDistortionParameters_index() const override { return 69; }
    std::optional<size_t> NeedsUpscalePostProcessPass_index() const override { return 70; }
    std::optional<size_t> RecordAnalytics_index() const override { return 71; }
    std::optional<size_t> GetVersionString_index() const override { return 72; }
    std::optional<size_t> SetTrackingOrigin_index() const override { return 73; }
    std::optional<size_t> GetTrackingOrigin_index() const override { return 74; }
    std::optional<size_t> DoesAppUseVRFocus_index() const override { return 75; }
    std::optional<size_t> DoesAppHaveVRFocus_index() const override { return 76; }
    std::optional<size_t> SetupLateUpdate_index() const override { return 77; }
    std::optional<size_t> ApplyLateUpdate_index() const override { return 78; }
};
}