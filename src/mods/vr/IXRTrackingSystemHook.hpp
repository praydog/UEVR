#pragma once

#include <cstdint>
#include <array>
#include <memory>

#include <sdk/StereoStuff.hpp>
#include <safetyhook.hpp>

#include "Mod.hpp"

namespace sdk {
class UEngine;
class IXRTrackingSystem;
class IXRCamera;
}

class FFakeStereoRenderingHook;

class IXRTrackingSystemHook : public ModComponent {
public:
    struct ProcessViewRotationData {
        std::chrono::high_resolution_clock::time_point last_update{std::chrono::high_resolution_clock::now()};
        glm::quat last_aim_rot{glm::identity<glm::quat>()};
        bool was_called{false};
    };

public:
    IXRTrackingSystemHook(FFakeStereoRenderingHook* stereo_hook, size_t offset_in_engine);

    void on_draw_ui() override;
    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;

    auto& get_process_view_rotation_data() { return m_process_view_rotation_data; }

private:
    struct ReferenceController {
        virtual void destroy() { }
        virtual ~ReferenceController() {}

        int32_t refcount{1000000};
        int32_t weakrefcount{1000000};
        uint8_t padding[0x100]{};
    };

    struct SharedPtr {
        void* obj{nullptr};
        ReferenceController* ref_controller { nullptr };
    };

    void initialize();
    bool analyze_head_tracking_allowed(uintptr_t return_address);

    static bool is_head_tracking_allowed(sdk::IXRTrackingSystem*);
    static bool is_head_tracking_allowed_for_world(sdk::IXRTrackingSystem*, void* world);
    static SharedPtr* get_xr_camera(sdk::IXRTrackingSystem*, SharedPtr* out, size_t device_id);
    static void get_motion_controller_data(sdk::IXRTrackingSystem*, void* world, uint32_t hand, void* motion_controller_data);
    static void get_current_pose(sdk::IXRTrackingSystem*, int32_t device_id, Quat<float>* out_rot, glm::vec3* out_pos);

    static void apply_hmd_rotation(sdk::IXRCamera*, void* player_controller, Rotator<float>* rot);
    static bool update_player_camera(sdk::IXRCamera*, Quat<float>* rel_rot, glm::vec3* rel_pos);
    // This function is the precursor to actually hooking ProcessViewRotation
    // Because there's a very real possibility that we can accidentally hook the wrong function
    // We need to verify that arg 2 and 3 are on the stack
    // And the function that calls it is also a virtual function (APlayerController::UpdateRotation)
    static void* process_view_rotation_analyzer(void*, size_t, size_t, size_t, size_t, size_t);
    static void process_view_rotation(void* player_controller, float delta_time, Rotator<float>* rot, Rotator<float>* delta_rot);

    void pre_update_view_rotation(Rotator<float>* rot);
    void update_view_rotation(Rotator<float>* rot);

    FFakeStereoRenderingHook* m_stereo_hook{nullptr};

    struct TrackingSystem {
        void* vtable;
    } m_xr_tracking_system;

    struct Camera {
        void* vtable;
    } m_xr_camera;

    struct HMDDevice {
        void* vtable;
        void* stereo_rendering_vtable;
        void* unk{nullptr};
    } m_hmd_device;

    ProcessViewRotationData m_process_view_rotation_data{};

    std::unique_ptr<ReferenceController> m_ref_controller{std::make_unique<ReferenceController>()};
    std::unique_ptr<ReferenceController> m_ref_controller2{std::make_unique<ReferenceController>()};
    std::array<uintptr_t, 200> m_xrtracking_vtable{}; // >= 4.18
    std::array<uintptr_t, 200> m_camera_vtable{};
    std::array<uintptr_t, 200> m_hmd_vtable{}; // < 4.18
    std::array<uintptr_t, 200> m_stereo_rendering_vtable{}; // Placeholder for now, we have a real one in FFakeStereoRenderingHook


    SharedPtr m_xr_camera_shared{};

    uintptr_t m_addr_of_process_view_rotation_ptr{};
    //std::unique_ptr<PointerHook> m_process_view_rotation_hook{};
    safetyhook::InlineHook m_process_view_rotation_hook{};
    bool m_attempted_hook_view_rotation{false};
    bool m_initialized{false};
    bool m_is_leq_4_25{false}; // <= 4.25, IsHeadTrackingAllowedForWorld does not exist
    bool m_is_leq_4_17{false}; // <= 4.17, IXRTrackingSystem and IXRCamera do not exist

    size_t m_offset_in_engine{0};

    union {
        Rotator<float> m_last_view_rotation;
        Rotator<double> m_last_view_rotation_double{};
    };
};