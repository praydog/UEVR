#pragma once

#include <cstdint>
#include <array>
#include <memory>

#include <sdk/StereoStuff.hpp>

#include "Mod.hpp"

namespace sdk {
class UEngine;
class IXRTrackingSystem;
class IXRCamera;
}

class FFakeStereoRenderingHook;

class IXRTrackingSystemHook : public ModComponent {
public:
    IXRTrackingSystemHook(FFakeStereoRenderingHook* stereo_hook, size_t offset_in_engine);

    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;

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

    static void apply_hmd_rotation(sdk::IXRCamera*, void* player_controller, Rotator<float>* rot);
    static bool update_player_camera(sdk::IXRCamera*, glm::quat* rel_rot, glm::vec3* rel_pos);

    static void process_view_rotation(void* player_controller, float delta_time, Rotator<float>* rot, Rotator<float>* delta_rot);

    FFakeStereoRenderingHook* m_stereo_hook{nullptr};

    struct TrackingSystem {
        void* vtable;
    } m_xr_tracking_system;

    struct Camera {
        void* vtable;
    } m_xr_camera;

    std::unique_ptr<ReferenceController> m_ref_controller{std::make_unique<ReferenceController>()};
    std::array<uintptr_t, 200> m_vtable{};
    std::array<uintptr_t, 200> m_camera_vtable{};

    SharedPtr m_xr_camera_shared{};

    std::unique_ptr<PointerHook> m_process_view_rotation_hook{};
    bool m_attempted_hook_view_rotation{false};
    bool m_initialized{false};
    bool m_is_leq_4_25{false}; // <= 4.25, IsHeadTrackingAllowedForWorld does not exist

    size_t m_offset_in_engine{0};

    union {
        Rotator<float> m_last_view_rotation;
        Rotator<double> m_last_view_rotation_double{};
    };
};