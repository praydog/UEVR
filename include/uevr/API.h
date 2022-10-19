/* C API for UEVR. */
/* Must be ANSI C89 compatible. */
#ifndef UEVR_API_H
#define UEVR_API_H

#ifndef __cplusplus
#include <stdbool.h>
#include <wchar.h>
#endif

#define UEVR_IN
#define UEVR_OUT

#define UEVR_PLUGIN_VERSION_MAJOR 1
#define UEVR_PLUGIN_VERSION_MINOR 0
#define UEVR_PLUGIN_VERSION_PATCH 0

#define UEVR_RENDERER_D3D11 0
#define UEVR_RENDERER_D3D12 1

typedef struct {
    int major;
    int minor;
    int patch;
} UEVR_PluginVersion;

/* strong typedefs */
#define DECLARE_UEVR_HANDLE(name) struct name##__ { int unused; }; \
                             typedef struct name##__ *name

DECLARE_UEVR_HANDLE(UEVR_UGameEngineHandle);
DECLARE_UEVR_HANDLE(UEVR_UEngineHandle);
DECLARE_UEVR_HANDLE(UEVR_FSlateRHIRendererHandle);
DECLARE_UEVR_HANDLE(UEVR_FViewportInfoHandle);

// OpenXR stuff
DECLARE_UEVR_HANDLE(UEVR_XrInstance);
DECLARE_UEVR_HANDLE(UEVR_XrSession);
DECLARE_UEVR_HANDLE(UEVR_XrSpace);

#define UEVR_LEFT_EYE 0
#define UEVR_RIGHT_EYE 1

#define UEVR_OPENXR_SWAPCHAIN_LEFT_EYE 0
#define UEVR_OPENXR_SWAPCHAIN_RIGHT_EYE 1
#define UEVR_OPENXR_SWAPCHAIN_UI 2

typedef int UEVR_Eye;
typedef int UEVR_TrackedDeviceIndex;
typedef int UEVR_OpenXRSwapchainIndex;

typedef struct {
    float x;
    float y;
    float z;
} UEVR_Vector3f;

typedef struct {
    double x;
    double y;
    double z;
} UEVR_Vector3d;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} UEVR_Vector4f;

typedef struct {
    float w;
    float x;
    float y;
    float z;
} UEVR_Quaternionf;

typedef struct {
    float pitch;
    float yaw;
    float roll;
} UEVR_Rotatorf;

typedef struct {
    double pitch;
    double yaw;
    double roll;
} UEVR_Rotatord;

typedef struct {
    float m[4][4];
} UEVR_Matrix4x4f;

typedef struct {
    double m[4][4];
} UEVR_Matrix4x4d;


typedef void (*UEVR_OnPresentCb)();
typedef void (*UEVR_OnDeviceResetCb)();
typedef bool (*UEVR_OnMessageCb)(void*, unsigned int, unsigned long long, long long);
typedef void (*UEVR_Engine_TickCb)(UEVR_UGameEngineHandle engine, float delta_seconds);
typedef void (*UEVR_Slate_DrawWindow_RenderThreadCb)(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info);

DECLARE_UEVR_HANDLE(UEVR_StereoRenderingDeviceHandle);
/* the position and rotation must be converted to double format based on the is_double parameter. */
typedef void (*UEVR_Stereo_CalculateStereoViewOffsetCb)(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double);

typedef bool (*UEVR_OnPresentFn)(UEVR_OnPresentCb);
typedef bool (*UEVR_OnDeviceResetFn)(UEVR_OnDeviceResetCb);
typedef bool (*UEVR_OnMessageFn)(UEVR_OnMessageCb);
typedef bool (*UEVR_Engine_TickFn)(UEVR_Engine_TickCb);
typedef bool (*UEVR_Slate_DrawWindow_RenderThreadFn)(UEVR_Slate_DrawWindow_RenderThreadCb);
typedef bool (*UEVR_Stereo_CalculateStereoViewOffsetFn)(UEVR_Stereo_CalculateStereoViewOffsetCb);

typedef void (*UEVR_PluginRequiredVersionFn)(UEVR_PluginVersion*);

typedef struct {
    UEVR_OnPresentFn on_present;
    UEVR_OnDeviceResetFn on_device_reset;
    UEVR_OnMessageFn on_message;
} UEVR_PluginCallbacks;

typedef struct {
    void (*log_error)(const char* format, ...);
    void (*log_warn)(const char* format, ...);
    void (*log_info)(const char* format, ...);
    bool (*is_drawing_ui)();
} UEVR_PluginFunctions;

typedef struct {
    UEVR_Engine_TickFn on_pre_engine_tick;
    UEVR_Engine_TickFn on_post_engine_tick;
    UEVR_Slate_DrawWindow_RenderThreadFn on_pre_slate_draw_window_render_thread;
    UEVR_Slate_DrawWindow_RenderThreadFn on_post_slate_draw_window_render_thread;
    UEVR_Stereo_CalculateStereoViewOffsetFn on_pre_calculate_stereo_view_offset;
    UEVR_Stereo_CalculateStereoViewOffsetFn on_post_calculate_stereo_view_offset;
} UEVR_SDKCallbacks;

typedef struct {
    int renderer_type;
    void* device;
    void* swapchain;
    void* command_queue;
} UEVR_RendererData;

typedef struct {
    UEVR_UEngineHandle (*get_uengine)();
} UEVR_SDKFunctions;

typedef struct {
    const UEVR_SDKFunctions* functions;
    const UEVR_SDKCallbacks* callbacks;
} UEVR_SDKData;

DECLARE_UEVR_HANDLE(UEVR_IVRSystem);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperone);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperoneSetup);
DECLARE_UEVR_HANDLE(UEVR_IVRCompositor);
DECLARE_UEVR_HANDLE(UEVR_IVROverlay);
DECLARE_UEVR_HANDLE(UEVR_IVROverlayView);
DECLARE_UEVR_HANDLE(UEVR_IVRHeadsetView);
DECLARE_UEVR_HANDLE(UEVR_IVRScreenshots);
DECLARE_UEVR_HANDLE(UEVR_IVRRenderModels);
DECLARE_UEVR_HANDLE(UEVR_IVRApplications);
DECLARE_UEVR_HANDLE(UEVR_IVRSettings);
DECLARE_UEVR_HANDLE(UEVR_IVRResources);
DECLARE_UEVR_HANDLE(UEVR_IVRExtendedDisplay);
DECLARE_UEVR_HANDLE(UEVR_IVRTrackedCamera);
DECLARE_UEVR_HANDLE(UEVR_IVRDriverManager);
DECLARE_UEVR_HANDLE(UEVR_IVRInput);
DECLARE_UEVR_HANDLE(UEVR_IVRIOBuffer);
DECLARE_UEVR_HANDLE(UEVR_IVRSpatialAnchors);
DECLARE_UEVR_HANDLE(UEVR_IVRNotifications);
DECLARE_UEVR_HANDLE(UEVR_IVRDebug);

typedef struct {
    UEVR_IVRSystem (*get_vr_system)();
    UEVR_IVRChaperone (*get_vr_chaperone)();
    UEVR_IVRChaperoneSetup (*get_vr_chaperone_setup)();
    UEVR_IVRCompositor (*get_vr_compositor)();
    UEVR_IVROverlay (*get_vr_overlay)();
    UEVR_IVROverlayView (*get_vr_overlay_view)();
    UEVR_IVRHeadsetView (*get_vr_headset_view)();
    UEVR_IVRScreenshots (*get_vr_screenshots)();
    UEVR_IVRRenderModels (*get_vr_render_models)();
    UEVR_IVRApplications (*get_vr_applications)();
    UEVR_IVRSettings (*get_vr_settings)();
    UEVR_IVRResources (*get_vr_resources)();
    UEVR_IVRExtendedDisplay (*get_vr_extended_display)();
    UEVR_IVRTrackedCamera (*get_vr_tracked_camera)();
    UEVR_IVRDriverManager (*get_vr_driver_manager)();
    UEVR_IVRInput (*get_vr_input)();
    UEVR_IVRIOBuffer (*get_vr_io_buffer)();
    UEVR_IVRSpatialAnchors (*get_vr_spatial_anchors)();
    UEVR_IVRNotifications (*get_vr_notifications)();
    UEVR_IVRDebug (*get_vr_debug)();
} UEVR_OpenVRData;

typedef struct {
    UEVR_XrInstance (*get_xr_instance)();
    UEVR_XrSession (*get_xr_session)();

    UEVR_XrSpace (*get_stage_space)(); /* XrSpace */
    UEVR_XrSpace (*get_view_space)(); /* XrSpace */
} UEVR_OpenXRData;

typedef struct {
    bool (*is_runtime_ready)();
    bool (*is_openvr)();
    bool (*is_openxr)();

    bool (*is_hmd_active)();

    void (*get_standing_origin)(UEVR_Vector3f* out_origin);
    void (*get_rotation_offset)(UEVR_Quaternionf* out_rotation);
    void (*set_standing_origin)(const UEVR_Vector3f* origin);
    void (*set_rotation_offset)(const UEVR_Quaternionf* rotation);

    UEVR_TrackedDeviceIndex (*get_hmd_index)();
    UEVR_TrackedDeviceIndex (*get_left_controller_index)();
    UEVR_TrackedDeviceIndex (*get_right_controller_index)();

    /* OpenVR/OpenXR space. */
    void (*get_pose)(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation);
    void (*get_transform)(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform);

    void (*get_eye_offset)(UEVR_Eye eye, UEVR_Vector3f* out_position);

    /* Converted to UE projection matrix */
    void (*get_ue_projection_matrix)(UEVR_Eye eye, UEVR_Matrix4x4f* put_projection);
} UEVR_VRData;

typedef struct {
    void* uevr_module;
    const UEVR_PluginVersion* version;
    const UEVR_PluginFunctions* functions;
    const UEVR_PluginCallbacks* callbacks;
    const UEVR_RendererData* renderer;

    const UEVR_VRData* vr;
    const UEVR_OpenVRData* openvr;
    const UEVR_OpenXRData* openxr;

    /* Engine/Game specific functions and data */
    const UEVR_SDKData* sdk;
} UEVR_PluginInitializeParam;

typedef bool (*UEVR_PluginInitializeFn)(const UEVR_PluginInitializeParam*);

#endif