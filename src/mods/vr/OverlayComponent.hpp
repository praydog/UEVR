#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include "Mod.hpp"

#include "imgui.h"

namespace vrmod{
class D3D11Component;
class D3D12Component;

class OverlayComponent : public ModComponent {
public:
    void on_reset();
    std::optional<std::string> on_initialize_openvr();

    void on_pre_imgui_frame() override;
    void on_post_compositor_submit();

    void on_config_save(utility::Config& cfg) override;
    void on_config_load(const utility::Config& cfg, bool set_defaults)  override;
    void on_draw_ui() override;

    auto& get_openxr() {
        return m_openxr;
    }

    struct IntersectState {
        bool intersecting{false};
        glm::vec2 quad_intersection_point{};
        glm::vec2 swapchain_intersection_point{};
    };

    const auto& get_intersect_state() const {
        return m_intersect_state;
    }

    const auto& get_framework_intersect_state() const {
        return m_intersect_state;
    }

    bool should_invert_ui_alpha() const {
        return m_ui_invert_alpha->value();
    }

private:
    // Cached data for imgui VR overlay so we know when we need to update it
    // instead of doing it constantly every frame
    struct {
        uint32_t last_render_target_width{};
        uint32_t last_render_target_height{};
        float last_width{};
        float last_height{};
        float last_x{};
        float last_y{};
    } m_overlay_data;

    // initial input state for imgui on the left eye frame
    struct {
        ImVec2      MousePos{};
        bool        MouseDown[5]{};
        float       MouseWheel{};
        float       MouseWheelH{};
        bool        KeyCtrl{};
        bool        KeyShift{};
        bool        KeyAlt{};
        bool        KeySuper{};
        bool        KeysDown[512]{};
    } m_initial_imgui_input_state;

    // overlay handle
    vr::VROverlayHandle_t m_overlay_handle{};
    vr::VROverlayHandle_t m_thumbnail_handle{};
    vr::VROverlayHandle_t m_slate_overlay_handle{};

    bool m_closed_ui{false};
    bool m_just_closed_ui{false};
    bool m_just_opened_ui{false};
    bool m_forced_aim{false};
    
    glm::vec2 m_last_mouse_pos{};
    std::chrono::steady_clock::time_point m_last_mouse_move_time{};

    IntersectState m_intersect_state{};
    IntersectState m_framework_intersect_state{};

    enum OverlayType {
        DEFAULT = 0,
        QUAD = 0,
        CYLINDER = 1,
        MAX
    };

    static const inline std::vector<std::string> s_overlay_type_names{
        "Quad",
        "Cylinder"
    };

    const ModCombo::Ptr m_slate_overlay_type{ ModCombo::create("UI_OverlayType", s_overlay_type_names) };
    const ModSlider::Ptr m_slate_distance{ ModSlider::create("UI_Distance", 0.5f, 10.0f, 2.0f) };
    const ModSlider::Ptr m_slate_x_offset{ ModSlider::create("UI_X_Offset", -10.0f, 10.0f, 0.0f) };
    const ModSlider::Ptr m_slate_y_offset{ ModSlider::create("UI_Y_Offset", -10.0f, 10.0f, 0.0f) };
    const ModSlider::Ptr m_slate_size{ ModSlider::create("UI_Size", 0.5f, 10.0f, 2.0f) };
    const ModSlider::Ptr m_slate_cylinder_angle{ ModSlider::create("UI_Cylinder_Angle", 0.0f, 360.0f, 90.0f) };
    const ModToggle::Ptr m_ui_follows_view{ ModToggle::create("UI_FollowView", false) };
    const ModToggle::Ptr m_ui_invert_alpha{ ModToggle::create("UI_InvertAlpha", false) };

    const ModSlider::Ptr m_framework_distance{ ModSlider::create("UI_Framework_Distance", 0.5f, 10.0f, 1.75f) };
    const ModSlider::Ptr m_framework_size{ ModSlider::create("UI_Framework_Size", 0.5f, 10.0f, 2.0f) };
    const ModToggle::Ptr m_framework_ui_follows_view{ ModToggle::create("UI_Framework_FollowView", false) };
    const ModToggle::Ptr m_framework_wrist_ui{ ModToggle::create("UI_Framework_WristUI", false) };
    const ModToggle::Ptr m_framework_mouse_emulation{ ModToggle::create("UI_Framework_MouseEmulation", true) };

public:
    OverlayComponent() 
        : m_openxr{this}
    {
        m_options = { 
            *m_slate_overlay_type,
            *m_slate_x_offset,
            *m_slate_y_offset,
            *m_slate_distance,
            *m_slate_size,
            *m_slate_cylinder_angle,
            *m_ui_follows_view,
            *m_ui_invert_alpha,
            *m_framework_distance,
            *m_framework_size,
            *m_framework_ui_follows_view,
            *m_framework_wrist_ui,
            *m_framework_mouse_emulation
        };
    }

    // OpenXR
private:
    class OpenXR {
    public:
        OpenXR(OverlayComponent* parent)
            : m_parent{parent}
        {

        }

        std::optional<std::reference_wrapper<XrCompositionLayerQuad>> generate_slate_quad(
            runtimes::OpenXR::SwapchainIndex swapchain = runtimes::OpenXR::SwapchainIndex::UI, 
            XrEyeVisibility eye = XR_EYE_VISIBILITY_BOTH
        );
        std::optional<std::reference_wrapper<XrCompositionLayerCylinderKHR>> generate_slate_cylinder(
            runtimes::OpenXR::SwapchainIndex swapchain = runtimes::OpenXR::SwapchainIndex::UI, 
            XrEyeVisibility eye = XR_EYE_VISIBILITY_BOTH
        );
        std::optional<std::reference_wrapper<XrCompositionLayerBaseHeader>> generate_slate_layer(
            runtimes::OpenXR::SwapchainIndex swapchain = runtimes::OpenXR::SwapchainIndex::UI, 
            XrEyeVisibility eye = XR_EYE_VISIBILITY_BOTH
        );
        std::optional<std::reference_wrapper<XrCompositionLayerQuad>> generate_framework_ui_quad();
        
    private:
        XrCompositionLayerQuad m_slate_layer{};
        XrCompositionLayerQuad m_slate_layer_right{};
        XrCompositionLayerCylinderKHR m_slate_layer_cylinder{};
        XrCompositionLayerCylinderKHR m_slate_layer_cylinder_right{};
        XrCompositionLayerQuad m_framework_ui_layer{};
        OverlayComponent* m_parent{ nullptr };
        
        friend class OverlayComponent;
    } m_openxr;

private:
    void update_input_openvr();
    void update_input_mouse_emulation();
    void update_overlay_openvr();
    bool update_wrist_overlay_openvr();
    void update_slate_openvr();
};}