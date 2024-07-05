/*
This file (API.h) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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

#define UEVR_PLUGIN_VERSION_MAJOR 2
#define UEVR_PLUGIN_VERSION_MINOR 27
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
DECLARE_UEVR_HANDLE(UEVR_UGameViewportClientHandle);
DECLARE_UEVR_HANDLE(UEVR_FViewportHandle);
DECLARE_UEVR_HANDLE(UEVR_FCanvasHandle);
DECLARE_UEVR_HANDLE(UEVR_UObjectArrayHandle);
DECLARE_UEVR_HANDLE(UEVR_UObjectHandle);
DECLARE_UEVR_HANDLE(UEVR_FFieldHandle);
DECLARE_UEVR_HANDLE(UEVR_FPropertyHandle);
DECLARE_UEVR_HANDLE(UEVR_UStructHandle);
DECLARE_UEVR_HANDLE(UEVR_UClassHandle);
DECLARE_UEVR_HANDLE(UEVR_UFunctionHandle);
DECLARE_UEVR_HANDLE(UEVR_FNameHandle);
DECLARE_UEVR_HANDLE(UEVR_FFieldClassHandle);
DECLARE_UEVR_HANDLE(UEVR_FConsoleManagerHandle);
DECLARE_UEVR_HANDLE(UEVR_IConsoleObjectHandle);
DECLARE_UEVR_HANDLE(UEVR_IConsoleCommandHandle);
DECLARE_UEVR_HANDLE(UEVR_IConsoleVariableHandle);
DECLARE_UEVR_HANDLE(UEVR_TArrayHandle);
DECLARE_UEVR_HANDLE(UEVR_FMallocHandle);
DECLARE_UEVR_HANDLE(UEVR_FRHITexture2DHandle);
DECLARE_UEVR_HANDLE(UEVR_UScriptStructHandle);
DECLARE_UEVR_HANDLE(UEVR_FArrayPropertyHandle);
DECLARE_UEVR_HANDLE(UEVR_FBoolPropertyHandle);
DECLARE_UEVR_HANDLE(UEVR_FStructPropertyHandle);
DECLARE_UEVR_HANDLE(UEVR_FEnumPropertyHandle);
DECLARE_UEVR_HANDLE(UEVR_UEnumHandle);
DECLARE_UEVR_HANDLE(UEVR_FNumericPropertyHandle);

/* OpenXR stuff */
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
} UEVR_Vector2f;

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

/* Generic DX renderer callbacks */
typedef void (*UEVR_OnPresentCb)();
typedef void (*UEVR_OnDeviceResetCb)();

/* VR Specific renderer callbacks */
typedef void (*UEVR_OnPostRenderVRFrameworkDX11Cb)(void*, void*, void*); /* immediate_context, ID3D11Texture2D* resource, ID3D11RenderTargetView* rtv */
/* On DX12 the resource state is D3D12_RESOURCE_STATE_RENDER_TARGET */
typedef void (*UEVR_OnPostRenderVRFrameworkDX12Cb)(void*, void*, void*); /* command_list, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE* rtv */

/* Windows callbacks*/
typedef bool (*UEVR_OnMessageCb)(void*, unsigned int, unsigned long long, long long);
typedef void (*UEVR_OnXInputGetStateCb)(unsigned int*, unsigned int, void*); /* retval, dwUserIndex, pState, read MSDN for details */
typedef void (*UEVR_OnXInputSetStateCb)(unsigned int*, unsigned int, void*); /* retval, dwUserIndex, pVibration, read MSDN for details */

/* UE Callbacks */
typedef void (*UEVR_Engine_TickCb)(UEVR_UGameEngineHandle engine, float delta_seconds);
typedef void (*UEVR_Slate_DrawWindow_RenderThreadCb)(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info);
typedef void (*UEVR_ViewportClient_DrawCb)(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas);

DECLARE_UEVR_HANDLE(UEVR_StereoRenderingDeviceHandle);
/* the position and rotation must be converted to double format based on the is_double parameter. */
typedef void (*UEVR_Stereo_CalculateStereoViewOffsetCb)(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double);

/* Generic DX Renderer */
typedef bool (*UEVR_OnPresentFn)(UEVR_OnPresentCb);
typedef bool (*UEVR_OnDeviceResetFn)(UEVR_OnDeviceResetCb);

/* VR Renderer */
typedef bool (*UEVR_OnPostRenderVRFrameworkDX11Fn)(UEVR_OnPostRenderVRFrameworkDX11Cb);
typedef bool (*UEVR_OnPostRenderVRFrameworkDX12Fn)(UEVR_OnPostRenderVRFrameworkDX12Cb);

/* Windows */
typedef bool (*UEVR_OnMessageFn)(UEVR_OnMessageCb);
typedef bool (*UEVR_OnXInputGetStateFn)(UEVR_OnXInputGetStateCb);
typedef bool (*UEVR_OnXInputSetStateFn)(UEVR_OnXInputSetStateCb);

/* Engine */
typedef bool (*UEVR_Engine_TickFn)(UEVR_Engine_TickCb);
typedef bool (*UEVR_Slate_DrawWindow_RenderThreadFn)(UEVR_Slate_DrawWindow_RenderThreadCb);
typedef bool (*UEVR_Stereo_CalculateStereoViewOffsetFn)(UEVR_Stereo_CalculateStereoViewOffsetCb);
typedef bool (*UEVR_ViewportClient_DrawFn)(UEVR_ViewportClient_DrawCb);

typedef void (*UEVR_PluginRequiredVersionFn)(UEVR_PluginVersion*);

typedef struct {
    UEVR_OnPresentFn on_present;
    UEVR_OnDeviceResetFn on_device_reset;
    UEVR_OnMessageFn on_message;
    UEVR_OnXInputGetStateFn on_xinput_get_state;
    UEVR_OnXInputSetStateFn on_xinput_set_state;
    UEVR_OnPostRenderVRFrameworkDX11Fn on_post_render_vr_framework_dx11;
    UEVR_OnPostRenderVRFrameworkDX12Fn on_post_render_vr_framework_dx12;
} UEVR_PluginCallbacks;

typedef struct {
    void (*log_error)(const char* format, ...);
    void (*log_warn)(const char* format, ...);
    void (*log_info)(const char* format, ...);
    bool (*is_drawing_ui)();
    bool (*remove_callback)(void* cb);
    unsigned int (*get_persistent_dir)(wchar_t* buffer, unsigned int buffer_size);
    int (*register_inline_hook)(void* target, void* dst, void** original);
    void (*unregister_inline_hook)(int hook_id);
} UEVR_PluginFunctions;

typedef struct {
    UEVR_Engine_TickFn on_pre_engine_tick;
    UEVR_Engine_TickFn on_post_engine_tick;
    UEVR_Slate_DrawWindow_RenderThreadFn on_pre_slate_draw_window_render_thread;
    UEVR_Slate_DrawWindow_RenderThreadFn on_post_slate_draw_window_render_thread;
    UEVR_Stereo_CalculateStereoViewOffsetFn on_pre_calculate_stereo_view_offset;
    UEVR_Stereo_CalculateStereoViewOffsetFn on_post_calculate_stereo_view_offset;
    UEVR_ViewportClient_DrawFn on_pre_viewport_client_draw;
    UEVR_ViewportClient_DrawFn on_post_viewport_client_draw;
} UEVR_SDKCallbacks;

typedef struct {
    int renderer_type;
    void* device;
    void* swapchain;
    void* command_queue;
} UEVR_RendererData;

typedef struct {
    UEVR_UEngineHandle (*get_uengine)();
    void (*set_cvar_int)(const char* module_name, const char* name, int value);
    UEVR_UObjectArrayHandle (*get_uobject_array)();

    UEVR_UObjectHandle (*get_player_controller)(int index);
    UEVR_UObjectHandle (*get_local_pawn)(int index);
    UEVR_UObjectHandle (*spawn_object)(UEVR_UClassHandle klass, UEVR_UObjectHandle outer);

    /* Handles exec commands, find_console_command does not */
    void (*execute_command)(const wchar_t* command);
    void (*execute_command_ex)(UEVR_UObjectHandle world, const wchar_t* command, void* output_device);

    UEVR_FConsoleManagerHandle (*get_console_manager)();
} UEVR_SDKFunctions;

typedef struct {
    UEVR_TArrayHandle (*get_console_objects)(UEVR_FConsoleManagerHandle mgr);
    UEVR_IConsoleObjectHandle (*find_object)(UEVR_FConsoleManagerHandle mgr, const wchar_t* name);
    UEVR_IConsoleVariableHandle (*find_variable)(UEVR_FConsoleManagerHandle mgr, const wchar_t* name);
    UEVR_IConsoleCommandHandle (*find_command)(UEVR_FConsoleManagerHandle mgr, const wchar_t* name);

    UEVR_IConsoleCommandHandle (*as_command)(UEVR_IConsoleObjectHandle object);

    void (*variable_set)(UEVR_IConsoleVariableHandle cvar, const wchar_t* value);
    void (*variable_set_ex)(UEVR_IConsoleVariableHandle cvar, const wchar_t* value, unsigned int flags);
    int (*variable_get_int)(UEVR_IConsoleVariableHandle cvar);
    float (*variable_get_float)(UEVR_IConsoleVariableHandle cvar);

    /* better to just use execute_command if possible */
    void (*command_execute)(UEVR_IConsoleCommandHandle cmd, const wchar_t* args);
} UEVR_ConsoleFunctions;

DECLARE_UEVR_HANDLE(UEVR_FUObjectItemHandle);

typedef struct {
    UEVR_UObjectHandle (*find_uobject)(const wchar_t* name);

    bool (*is_chunked)();
    bool (*is_inlined)();
    unsigned int (*get_objects_offset)();
    unsigned int (*get_item_distance)();

    int (*get_object_count)(UEVR_UObjectArrayHandle array);
    void* (*get_objects_ptr)(UEVR_UObjectArrayHandle array);
    UEVR_UObjectHandle (*get_object)(UEVR_UObjectArrayHandle array, int index);
    UEVR_FUObjectItemHandle (*get_item)(UEVR_UObjectArrayHandle array, int index);
} UEVR_UObjectArrayFunctions;

typedef struct {
    UEVR_FFieldHandle (*get_next)(UEVR_FFieldHandle field);
    UEVR_FFieldClassHandle (*get_class)(UEVR_FFieldHandle field);
    UEVR_FNameHandle (*get_fname)(UEVR_FFieldHandle field);
} UEVR_FFieldFunctions;

typedef struct {
    int (*get_offset)(UEVR_FPropertyHandle prop);
    unsigned long long (*get_property_flags)(UEVR_FPropertyHandle prop);
    bool (*is_param)(UEVR_FPropertyHandle prop);
    bool (*is_out_param)(UEVR_FPropertyHandle prop);
    bool (*is_return_param)(UEVR_FPropertyHandle prop);
    bool (*is_reference_param)(UEVR_FPropertyHandle prop);
    bool (*is_pod)(UEVR_FPropertyHandle prop);
} UEVR_FPropertyFunctions;

typedef struct {
    UEVR_UStructHandle (*get_super_struct)(UEVR_UStructHandle klass);
    UEVR_FFieldHandle (*get_child_properties)(UEVR_UStructHandle klass);
    UEVR_UFunctionHandle (*find_function)(UEVR_UStructHandle klass, const wchar_t* name);
    UEVR_FPropertyHandle (*find_property)(UEVR_UStructHandle klass, const wchar_t* name);
    int (*get_properties_size)(UEVR_UStructHandle klass); /* size in bytes */
    int (*get_min_alignment)(UEVR_UStructHandle klass);
} UEVR_UStructFunctions;

typedef struct {
    UEVR_UObjectHandle (*get_class_default_object)(UEVR_UClassHandle klass);
} UEVR_UClassFunctions;

typedef struct {
    void* (*get_native_function)(UEVR_UFunctionHandle function);
} UEVR_UFunctionFunctions;

typedef struct {
    UEVR_UClassHandle (*get_class)(UEVR_UObjectHandle object);
    UEVR_UObjectHandle (*get_outer)(UEVR_UObjectHandle object);

    /* pointer to the property data, not the UProperty/FProperty */
    void* (*get_property_data)(UEVR_UObjectHandle object, const wchar_t* name);
    bool (*is_a)(UEVR_UObjectHandle object, UEVR_UClassHandle other);

    void (*process_event)(UEVR_UObjectHandle object, UEVR_UFunctionHandle function, void* params);
    void (*call_function)(UEVR_UObjectHandle object, const wchar_t* name, void* params);

    UEVR_FNameHandle (*get_fname)(UEVR_UObjectHandle object);

    bool (*get_bool_property)(UEVR_UObjectHandle object, const wchar_t* name);
    void (*set_bool_property)(UEVR_UObjectHandle object, const wchar_t* name, bool value);
} UEVR_UObjectFunctions;

DECLARE_UEVR_HANDLE(UEVR_UObjectHookMotionControllerStateHandle);

typedef struct {
    void (*set_rotation_offset)(UEVR_UObjectHookMotionControllerStateHandle, const UEVR_Quaternionf* rotation);
    void (*set_location_offset)(UEVR_UObjectHookMotionControllerStateHandle, const UEVR_Vector3f* location);
    void (*set_hand)(UEVR_UObjectHookMotionControllerStateHandle, unsigned int hand);
    void (*set_permanent)(UEVR_UObjectHookMotionControllerStateHandle, bool permanent);
} UEVR_UObjectHookMotionControllerStateFunctions;

typedef struct {
    void (*activate)();
    bool (*exists)(UEVR_UObjectHandle object);

    /* if 0 or nullptr is passed, it will return how many objects are in the array */
    /* so you can allocate the right amount of memory */
    int (*get_objects_by_class)(UEVR_UClassHandle klass, UEVR_UObjectHandle* out_objects, unsigned int max_objects, bool allow_default);
    int (*get_objects_by_class_name)(const wchar_t* class_name, UEVR_UObjectHandle* out_objects, unsigned int max_objects, bool allow_default);

    UEVR_UObjectHandle (*get_first_object_by_class)(UEVR_UClassHandle klass, bool allow_default);
    UEVR_UObjectHandle (*get_first_object_by_class_name)(const wchar_t* class_name, bool allow_default);

    UEVR_UObjectHookMotionControllerStateHandle (*get_or_add_motion_controller_state)(UEVR_UObjectHandle object);
    UEVR_UObjectHookMotionControllerStateHandle (*get_motion_controller_state)(UEVR_UObjectHandle object);

    UEVR_UObjectHookMotionControllerStateFunctions* mc_state;

    bool (*is_disabled)();
    void (*set_disabled)(bool disabled);
} UEVR_UObjectHookFunctions;

typedef struct {
    UEVR_FNameHandle (*get_fname)(UEVR_FFieldClassHandle field_class);
} UEVR_FFieldClassFunctions;

typedef struct {
    unsigned int (*to_string)(UEVR_FNameHandle name, wchar_t* buffer, unsigned int buffer_size);
    void (*constructor)(UEVR_FNameHandle name, const wchar_t* data, unsigned int find_type);
} UEVR_FNameFunctions;

typedef struct {
    UEVR_FMallocHandle (*get)();

    void* (*malloc)(UEVR_FMallocHandle instance, unsigned int size, unsigned int alignment);
    void* (*realloc)(UEVR_FMallocHandle instance, void* ptr, unsigned int size, unsigned int alignment);
    void (*free)(UEVR_FMallocHandle instance, void* ptr);
} UEVR_FMallocFunctions;

DECLARE_UEVR_HANDLE(UEVR_IPooledRenderTargetHandle);

typedef struct {
    void (*activate)();
    UEVR_IPooledRenderTargetHandle (*get_render_target)(const wchar_t* name);
} UEVR_FRenderTargetPoolHookFunctions;

typedef struct {
    UEVR_FRHITexture2DHandle (*get_scene_render_target)();
    UEVR_FRHITexture2DHandle (*get_ui_render_target)();
} UEVR_FFakeStereoRenderingHookFunctions;

typedef struct {
    void* (*get_native_resource)(UEVR_FRHITexture2DHandle texture);
} UEVR_FRHITexture2DFunctions;

DECLARE_UEVR_HANDLE(UEVR_StructOpsHandle);

typedef struct {
    UEVR_StructOpsHandle (*get_struct_ops)(UEVR_UScriptStructHandle script_struct);
    int (*get_struct_size)(UEVR_UScriptStructHandle script_struct);
} UEVR_UScriptStructFunctions;

typedef struct {
    UEVR_FPropertyHandle (*get_inner)(UEVR_FArrayPropertyHandle prop);
} UEVR_FArrayPropertyFunctions;

typedef struct {
    unsigned int (*get_field_size)(UEVR_FBoolPropertyHandle prop);
    unsigned int (*get_byte_offset)(UEVR_FBoolPropertyHandle prop);
    unsigned int (*get_byte_mask)(UEVR_FBoolPropertyHandle prop);
    unsigned int (*get_field_mask)(UEVR_FBoolPropertyHandle prop);
    bool (*get_value_from_object)(UEVR_FBoolPropertyHandle prop, void* object);
    bool (*get_value_from_propbase)(UEVR_FBoolPropertyHandle prop, void* addr);
    void (*set_value_in_object)(UEVR_FBoolPropertyHandle prop, void* object, bool value);
    void (*set_value_in_propbase)(UEVR_FBoolPropertyHandle prop, void* addr, bool value);
} UEVR_FBoolPropertyFunctions;

typedef struct {
    UEVR_UScriptStructHandle (*get_struct)(UEVR_FStructPropertyHandle prop);
} UEVR_FStructPropertyFunctions;

typedef struct {
    UEVR_FNumericPropertyHandle (*get_underlying_prop)(UEVR_FEnumPropertyHandle prop);
    UEVR_UEnumHandle (*get_enum)(UEVR_FEnumPropertyHandle prop);
} UEVR_FEnumPropertyFunctions;

typedef struct {
    const UEVR_SDKFunctions* functions;
    const UEVR_SDKCallbacks* callbacks;
    const UEVR_UObjectFunctions* uobject;
    const UEVR_UObjectArrayFunctions* uobject_array;
    const UEVR_FFieldFunctions* ffield;
    const UEVR_FPropertyFunctions* fproperty;
    const UEVR_UStructFunctions* ustruct;
    const UEVR_UClassFunctions* uclass;
    const UEVR_UFunctionFunctions* ufunction;
    const UEVR_UObjectHookFunctions* uobject_hook;
    const UEVR_FFieldClassFunctions* ffield_class;
    const UEVR_FNameFunctions* fname;
    const UEVR_ConsoleFunctions* console;
    const UEVR_FMallocFunctions* malloc;
    const UEVR_FRenderTargetPoolHookFunctions* render_target_pool_hook;
    const UEVR_FFakeStereoRenderingHookFunctions* stereo_hook;
    const UEVR_FRHITexture2DFunctions* frhitexture2d;
    const UEVR_UScriptStructFunctions* uscriptstruct;
    const UEVR_FArrayPropertyFunctions* farrayproperty;
    const UEVR_FBoolPropertyFunctions* fboolproperty;
    const UEVR_FStructPropertyFunctions* fstructproperty;
    const UEVR_FEnumPropertyFunctions* fenumproperty;
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

DECLARE_UEVR_HANDLE(UEVR_ActionHandle);
DECLARE_UEVR_HANDLE(UEVR_InputSourceHandle);

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

    /* For getting grip or aim poses for the controllers */
    void (*get_grip_pose)(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation);
    void (*get_aim_pose)(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation);
    void (*get_grip_transform)(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform);
    void (*get_aim_transform)(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform);

    void (*get_eye_offset)(UEVR_Eye eye, UEVR_Vector3f* out_position);

    /* Converted to UE projection matrix */
    void (*get_ue_projection_matrix)(UEVR_Eye eye, UEVR_Matrix4x4f* out_projection);

    UEVR_InputSourceHandle (*get_left_joystick_source)();
    UEVR_InputSourceHandle (*get_right_joystick_source)();
    
    UEVR_ActionHandle (*get_action_handle)(const char* action_path);

    bool (*is_action_active)(UEVR_ActionHandle action, UEVR_InputSourceHandle source);
    bool (*is_action_active_any_joystick)(UEVR_ActionHandle action);
    void (*get_joystick_axis)(UEVR_InputSourceHandle source, UEVR_Vector2f* out_axis);
    void (*trigger_haptic_vibration)(float seconds_from_now, float duration, float frequency, float amplitude, UEVR_InputSourceHandle source);
    /* if any controller action is active or has been active within certain previous timeframe */
    bool (*is_using_controllers)();
    bool (*is_decoupled_pitch_enabled)();
    
    unsigned int (*get_movement_orientation)();
    unsigned int (*get_lowest_xinput_index)();

    void (*recenter_view)();
    void (*recenter_horizon)();

    unsigned int (*get_aim_method)();
    void (*set_aim_method)(unsigned int method);
    bool (*is_aim_allowed)();
    void (*set_aim_allowed)(bool allowed);

    unsigned int (*get_hmd_width)();
    unsigned int (*get_hmd_height)();
    unsigned int (*get_ui_width)();
    unsigned int (*get_ui_height)();

    bool (*is_snap_turn_enabled)();
    void (*set_snap_turn_enabled)(bool enabled);
    void (*set_decoupled_pitch_enabled)(bool enabled);

    void (*set_mod_value)(const char* key, const char* value);
    void (*get_mod_value)(const char* key, char* value, unsigned int value_size);
    void (*save_config)();
    void (*reload_config)();
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