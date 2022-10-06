#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include "imgui.h"

namespace vrmod{
class D3D11Component;
class D3D12Component;

class OverlayComponent {
public:
    void on_reset();
    std::optional<std::string> on_initialize_openvr();

    void on_pre_imgui_frame();
    void on_post_compositor_submit();

    auto& get_openxr() {
        return m_openxr;
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

    // OpenXR
private:
    class OpenXR {
    public:
        XrCompositionLayerQuad& generate_slate_quad();
        
    private:
        XrCompositionLayerQuad slate_layer{};
    } m_openxr{};

private:
    void update_input();
    void update_overlay();
    void update_slate();
};}