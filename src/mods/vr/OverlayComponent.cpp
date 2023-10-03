#include "Framework.hpp"
#include "../VR.hpp"

#include "OverlayComponent.hpp"

namespace vrmod {
void OverlayComponent::on_reset() {
    m_overlay_data = {};
}

std::optional<std::string> OverlayComponent::on_initialize_openvr() {
    m_overlay_data = {};

    auto create_overlay = [this](const std::string& name, float size, vr::VROverlayHandle_t& out) -> std::optional<std::string> {
        // create vr overlay
        auto overlay_error = vr::VROverlay()->CreateOverlay(name.c_str(), name.c_str(), &out);

        if (overlay_error != vr::VROverlayError_None) {
            return "VROverlay failed to create overlay: " + std::string{vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error)};
        }

        // set overlay to visible
        vr::VROverlay()->ShowOverlay(out);

        overlay_error = vr::VROverlay()->SetOverlayWidthInMeters(out, size);

        if (overlay_error != vr::VROverlayError_None) {
            return "VROverlay failed to set overlay width: " + std::string{vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error)};
        }

        // same thing as above but absolute instead
        // get absolute tracking pose of hmd with GetDeviceToAbsoluteTrackingPose
        // then get the matrix from that
        // then set it as the overlay transform
        vr::TrackedDevicePose_t pose{};
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, &pose, 1);
        vr::VROverlay()->SetOverlayTransformAbsolute(out, vr::TrackingUniverseStanding, &pose.mDeviceToAbsoluteTracking);

        // set overlay flag to receive smooth scroll events
        overlay_error = vr::VROverlay()->SetOverlayFlag(out, vr::VROverlayFlags::VROverlayFlags_SendVRSmoothScrollEvents, true);

        if (overlay_error != vr::VROverlayError_None) {
            return "VROverlay failed to set overlay flag: " + std::string{vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error)};
        }

        spdlog::info("Made overlay with handle {}", out);
        return std::nullopt;
    };

    if (!create_overlay("REFramework", 0.25f, m_overlay_handle)) {
        auto overlay_error = vr::VROverlay()->SetOverlayInputMethod(m_overlay_handle, vr::VROverlayInputMethod_Mouse);

        if (overlay_error != vr::VROverlayError_None) {
            return "VROverlay failed to set overlay input method: " + std::string{vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error)};
        }
    }

    if (!create_overlay("Slate", 2.0f, m_slate_overlay_handle)) {
        auto overlay_error = vr::VROverlay()->SetOverlayInputMethod(m_slate_overlay_handle, vr::VROverlayInputMethod_None);

        if (overlay_error != vr::VROverlayError_None) {
            return "VROverlay failed to set overlay input method: " + std::string{vr::VROverlay()->GetOverlayErrorNameFromEnum(overlay_error)};
        }
    }

    return std::nullopt;
}

void OverlayComponent::on_pre_imgui_frame() {
    this->update_input_openvr();
}

void OverlayComponent::on_post_compositor_submit() {
    this->update_overlay_openvr();
    this->update_slate_openvr();
}


void OverlayComponent::on_config_save(utility::Config& cfg) {
    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }
}

void OverlayComponent::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (IModValue& option : m_options) {
        option.config_load(cfg, set_defaults);
    }
}

void OverlayComponent::on_draw_ui() {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Overlay Options")) {
        if (VR::get()->get_runtime()->is_cylinder_layer_allowed()) {
            m_slate_overlay_type->draw("Overlay Type");

            if ((OverlayType)m_slate_overlay_type->value() == OverlayType::CYLINDER)  {
                m_slate_cylinder_angle->draw("UI Cylinder Angle");
            }
        }

        float ui_offset[] { m_slate_x_offset->value(), m_slate_y_offset->value(), m_slate_distance->value() };

        if (ImGui::SliderFloat3("UI Offset", ui_offset, -10.0f, 10.0f)) {
            m_slate_x_offset->value() = ui_offset[0];
            m_slate_y_offset->value() = ui_offset[1];
            m_slate_distance->value() = ui_offset[2];
        }

        m_slate_distance->draw("UI Distance");
        m_slate_size->draw("UI Size");
        m_ui_follows_view->draw("UI Follows View");

        m_framework_distance->draw("Framework Distance");
        m_framework_size->draw("Framework Size");
        m_framework_ui_follows_view->draw("Framework Follows View");
        ImGui::SameLine();
        m_framework_wrist_ui->draw("Framework Wrist UI");
        ImGui::TreePop();
    }
}

void OverlayComponent::update_input_openvr() {
    if (!VR::get()->get_runtime()->is_openvr()) {
        return;
    }

    if (!m_framework_wrist_ui->value()) {
        return;
    }

    auto vr = VR::get();
    auto& io = ImGui::GetIO();
    const auto is_initial_frame = !vr->is_using_afr() || vr->get_frame_count() % 2 == vr->m_left_eye_interval;

    // Restore the previous frame's input state
    memcpy(io.KeysDown, m_initial_imgui_input_state.KeysDown, sizeof(io.KeysDown));
    memcpy(io.MouseDown, m_initial_imgui_input_state.MouseDown, sizeof(io.MouseDown));
    io.MousePos = m_initial_imgui_input_state.MousePos;
    io.MouseWheel = m_initial_imgui_input_state.MouseWheel;
    io.MouseWheelH = m_initial_imgui_input_state.MouseWheelH;
    io.KeyCtrl = m_initial_imgui_input_state.KeyCtrl;
    io.KeyShift = m_initial_imgui_input_state.KeyShift;
    io.KeyAlt = m_initial_imgui_input_state.KeyAlt;
    io.KeySuper = m_initial_imgui_input_state.KeySuper;

    if (!is_initial_frame) {
        return;
    }

    io.MouseWheel = 0;
    io.MouseWheelH = 0;

    const auto is_d3d11 = g_framework->get_renderer_type() == Framework::RendererType::D3D11;

    const auto last_window_pos = g_framework->get_last_window_pos();
    const auto last_window_size = g_framework->get_last_window_size();
    const auto rendertarget_width = is_d3d11 ? g_framework->get_rendertarget_width_d3d11() : g_framework->get_rendertarget_width_d3d12();
    const auto rendertarget_height = is_d3d11 ? g_framework->get_rendertarget_height_d3d11() : g_framework->get_rendertarget_height_d3d12();

    // Poll overlay events
    vr::VREvent_t event{};
    const auto hwnd = g_framework->get_window();

    while (vr::VROverlay()->PollNextOverlayEvent(m_overlay_handle, &event, sizeof(event))) {
        switch (event.eventType) {
            case vr::VREvent_MouseButtonDown:
                m_initial_imgui_input_state.MouseDown[0] = true;
                io.MouseDown[0] = true;
                break;
            case vr::VREvent_MouseButtonUp:
                m_initial_imgui_input_state.MouseDown[0] = false;
                io.MouseDown[0] = false;
                break;
            case vr::VREvent_MouseMove: {
                const std::array<float, 2> raw_coords { event.data.mouse.x, event.data.mouse.y };

                // Convert from GL space (bottom left is 0,0) to window space (top left is 0,0)
                const auto mouse_point = ImVec2{
                    raw_coords[0],
                    (rendertarget_height - raw_coords[1])
                };

                // override imgui mouse position
                m_initial_imgui_input_state.MousePos = mouse_point;
                io.MousePos = mouse_point;
            } break;
            case vr::VREvent_ScrollSmooth: {
                m_initial_imgui_input_state.MouseWheelH += event.data.scroll.xdelta;
                m_initial_imgui_input_state.MouseWheel += event.data.scroll.ydelta;
                io.MouseWheelH = event.data.scroll.xdelta;
                io.MouseWheel = event.data.scroll.ydelta;
            } break;
            default:
                break;
        }
    }
}

void OverlayComponent::update_slate_openvr() {
    auto vr = VR::get();

    if (!vr->get_runtime()->is_openvr()) {
        return;
    }

    if (!vr->is_gui_enabled()) {
        return;
    }

    const auto is_d3d11 = g_framework->get_renderer_type() == Framework::RendererType::D3D11;

    vr::VRTextureBounds_t bounds{};
    bounds.uMin = 0.0f;
    bounds.uMax = 1.0f;
    bounds.vMin = 0.0f;
    bounds.vMax = 1.0f;

    vr::VROverlay()->SetOverlayTextureBounds(m_slate_overlay_handle, &bounds);

    vr::TrackedDevicePose_t pose{};
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, &pose, 1);

    auto rotation_offset = glm::inverse(vr->get_rotation_offset());

    if (vr->is_decoupled_pitch_enabled() && vr->is_decoupled_pitch_ui_adjust_enabled()) {
        const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
        const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);

        // Add the inverse of the pitch rotation to the rotation offset
        rotation_offset = glm::normalize(glm::inverse(pre_flat_pitch * vr->get_rotation_offset()));
    }

    //auto glm_matrix = glm::rowMajor4(Matrix4x4f{*(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking});
    auto glm_matrix = Matrix4x4f{rotation_offset};
    glm_matrix[3] += vr->get_standing_origin();
    glm_matrix[3] -= glm_matrix[2] * m_slate_distance->value();
    glm_matrix[3] += m_slate_x_offset->value() * glm_matrix[0];
    glm_matrix[3] += m_slate_y_offset->value() * glm_matrix[1];
    glm_matrix[3].w = 1.0f;
    const auto steamvr_matrix = Matrix3x4f{glm::rowMajor4(glm_matrix)};
    vr::VROverlay()->SetOverlayTransformAbsolute(m_slate_overlay_handle, vr::TrackingUniverseStanding, (vr::HmdMatrix34_t*)&steamvr_matrix);

    const auto is_d3d12 = g_framework->get_renderer_type() == Framework::RendererType::D3D12;
    const auto size = is_d3d12 ? g_framework->get_d3d12_rt_size() : g_framework->get_d3d11_rt_size();
    const auto aspect = size.x / size.y;
    const auto width_meters = m_slate_size->value() * aspect;
    vr::VROverlay()->SetOverlayWidthInMeters(m_slate_overlay_handle, width_meters);

    if (is_d3d11) {
        if (vr->m_d3d11.get_ui_tex().Get() == nullptr) {
            return;
        }

        vr::Texture_t ui_tex{(void*)vr->m_d3d11.get_ui_tex().Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto};
        vr::VROverlay()->SetOverlayTexture(m_slate_overlay_handle, &ui_tex);   
    } else {
        if (vr->m_d3d12.get_openvr_ui_tex().texture.Get() == nullptr) {
            return;
        }

        vr::D3D12TextureData_t overlay_tex {
            vr->m_d3d12.get_openvr_ui_tex().texture.Get(),
            g_framework->get_d3d12_hook()->get_command_queue(),
            0
        };

        vr::Texture_t ui_tex{(void*)&overlay_tex, vr::TextureType_DirectX12, vr::ColorSpace_Auto};
        vr::VROverlay()->SetOverlayTexture(m_slate_overlay_handle, &ui_tex);   
    }
}

bool OverlayComponent::update_wrist_overlay_openvr() {
    if (!VR::get()->get_runtime()->is_openvr()) {
        return false;
    }

    auto& vr = VR::get();

    if (!m_framework_wrist_ui->value()) {
        return true;
    }

    const auto is_d3d11 = g_framework->get_renderer_type() == Framework::RendererType::D3D11;

    // update REFramework menu overlay
    const auto last_window_pos = g_framework->get_last_window_pos();
    const auto last_window_size = g_framework->get_last_window_size();
    const auto render_target_width = is_d3d11 ? g_framework->get_rendertarget_width_d3d11() : g_framework->get_rendertarget_width_d3d12();
    const auto render_target_height = is_d3d11 ? g_framework->get_rendertarget_height_d3d11() : g_framework->get_rendertarget_height_d3d12();

    // only update certain parts of the overlay
    // if things like the width or position of the window change
    if (m_overlay_data.last_x != last_window_pos.x || m_overlay_data.last_y != last_window_pos.y ||
        m_overlay_data.last_width != last_window_size.x || m_overlay_data.last_height != last_window_size.y ||
        m_overlay_data.last_render_target_width != render_target_width || m_overlay_data.last_render_target_height != render_target_height || m_just_closed_ui || m_just_opened_ui) 
    {
        // scaling for the intersection mask
        // so it doesn't become too intrusive during gameplay
        const auto scale = m_closed_ui ? 0.25f : 1.0f;

        vr::VRTextureBounds_t bounds{};
        bounds.uMin = last_window_pos.x / render_target_width ;
        bounds.uMax = (last_window_pos.x + last_window_size.x) / render_target_width;
        bounds.vMin = last_window_pos.y / render_target_height;
        bounds.vMax = (last_window_pos.y + last_window_size.y) / render_target_height;

        vr::VROverlay()->SetOverlayTextureBounds(m_overlay_handle, &bounds);

        // necessary, fixes all sorts of issues with ray intersection
        const auto mouse_scale = vr::HmdVector2_t{(float)render_target_width, (float)render_target_height};
        vr::VROverlay()->SetOverlayMouseScale(m_overlay_handle, &mouse_scale);

        vr::VROverlayIntersectionMaskPrimitive_t intersection_mask{};

        intersection_mask.m_nPrimitiveType = vr::EVROverlayIntersectionMaskPrimitiveType::OverlayIntersectionPrimitiveType_Rectangle;
        intersection_mask.m_Primitive.m_Rectangle.m_flTopLeftX = last_window_pos.x;
        intersection_mask.m_Primitive.m_Rectangle.m_flTopLeftY = last_window_pos.y;
        intersection_mask.m_Primitive.m_Rectangle.m_flWidth = last_window_size.x;
        intersection_mask.m_Primitive.m_Rectangle.m_flHeight = last_window_size.y;

        vr::VROverlay()->SetOverlayIntersectionMask(m_overlay_handle, &intersection_mask, 1);

        // and now set the last known values
        m_overlay_data.last_x = last_window_pos.x;
        m_overlay_data.last_y = last_window_pos.y;
        m_overlay_data.last_width = last_window_size.x;
        m_overlay_data.last_height = last_window_size.y;
        m_overlay_data.last_render_target_width = render_target_width;
        m_overlay_data.last_render_target_height = render_target_height;
    }

    // Fire an intersection test and enable the laser pointer if we're intersecting
    const auto& controllers = vr->get_controllers();

    bool should_show_overlay = !m_closed_ui;

    if (controllers.size() >= 2 && !vr->is_any_action_down()) {
        Matrix4x4f left_controller_world_transform{glm::identity<Matrix4x4f>()};

        // Attach the overlay to the left controller
        if (controllers[0] != vr::k_unTrackedDeviceIndexInvalid) {
            const auto position_offset = vr->m_overlay_position;
            const auto rotation_offset = vr->m_overlay_rotation;
 
            left_controller_world_transform = vr->get_transform(controllers[0]) * Matrix4x4f{glm::quat{rotation_offset}};
            left_controller_world_transform[3] -= glm::extractMatrixRotation(left_controller_world_transform) * position_offset;
            left_controller_world_transform[3].w = 1.0f;

            const auto steamvr_transform = Matrix3x4f{ glm::rowMajor4(left_controller_world_transform) };
            
            vr::VROverlay()->SetOverlayTransformAbsolute(m_overlay_handle, vr::ETrackingUniverseOrigin::TrackingUniverseStanding, (vr::HmdMatrix34_t*)&steamvr_transform);
        }

        bool any_intersected = false;

        for (const auto& controller_index : controllers) {
            const auto is_left = controller_index == controllers[0];

            vr::VRInputValueHandle_t controller_handle{};

            if (is_left) {
                controller_handle = vr->m_left_joystick;
            } else {
                controller_handle = vr->m_right_joystick;
            }

            char render_name[vr::k_unMaxPropertyStringSize]{};
            vr::VRSystem()->GetStringTrackedDeviceProperty(controller_index, vr::Prop_RenderModelName_String, render_name, vr::k_unMaxPropertyStringSize);

            vr::RenderModel_ControllerMode_State_t controller_state{};
            vr::RenderModel_ComponentState_t component_state{};

            // get tip component state
            if (!vr::VRRenderModels()->GetComponentStateForDevicePath(render_name, vr::k_pch_Controller_Component_Tip, controller_handle, &controller_state, &component_state)) {
                continue;
            }

            // obtain tip world transform
            const auto controller_world_transform = vr->get_transform(controller_index);
            const auto tip_local_transform = glm::rowMajor4(Matrix4x4f{*(Matrix3x4f*)&component_state.mTrackingToComponentLocal});
            const auto tip_world_transform = controller_world_transform * tip_local_transform;

            // Set up intersection data
            vr::VROverlayIntersectionResults_t intersection_results{};
            vr::VROverlayIntersectionParams_t intersection_params{};
            intersection_params.eOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
            intersection_params.vSource.v[0] = tip_world_transform[3][0];
            intersection_params.vSource.v[1] = tip_world_transform[3][1];
            intersection_params.vSource.v[2] = tip_world_transform[3][2];
            intersection_params.vDirection.v[0] = -tip_world_transform[2][0];
            intersection_params.vDirection.v[1] = -tip_world_transform[2][1];
            intersection_params.vDirection.v[2] = -tip_world_transform[2][2];

            // Do the intersection test
            if (vr::VROverlay()->ComputeOverlayIntersection(m_overlay_handle, &intersection_params, &intersection_results)) {
                auto normal = Vector4f{intersection_results.vNormal.v[0], intersection_results.vNormal.v[1], intersection_results.vNormal.v[2], 1.0f};
                normal = glm::inverse(glm::extractMatrixRotation(tip_world_transform)) * normal;

                if (m_closed_ui) {
                    const auto u = ((intersection_results.vUVs.v[0] * m_overlay_data.last_render_target_width) - m_overlay_data.last_x) / m_overlay_data.last_width;
                    const auto v = ((m_overlay_data.last_render_target_height - (intersection_results.vUVs.v[1] * m_overlay_data.last_render_target_height)) - m_overlay_data.last_y) / m_overlay_data.last_height;

                    any_intersected = u >= 0.25f &&
                                    u <= 0.75f && 
                                    v >= 0.25f && 
                                    v <= 0.75f;

                    // Make sure the intersection hit the front of the overlay, not the back
                    any_intersected = any_intersected && normal.z > 0.0f;
                } else {
                    any_intersected = normal.z > 0.0f;
                }
            }
        }

        // Do the same intersection test with the user's view/head gaze
        // Both the head and controller needs to be aimed at the overlay for it to appear
        if (any_intersected) {
            auto head_world_transform = vr->get_transform(vr::k_unTrackedDeviceIndex_Hmd);
            vr::VROverlayIntersectionResults_t intersection_results{};
            vr::VROverlayIntersectionParams_t intersection_params{};
            intersection_params.eOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
            intersection_params.vSource.v[0] = head_world_transform[3][0];
            intersection_params.vSource.v[1] = head_world_transform[3][1];
            intersection_params.vSource.v[2] = head_world_transform[3][2];
            intersection_params.vDirection.v[0] = -head_world_transform[2][0];
            intersection_params.vDirection.v[1] = -head_world_transform[2][1];
            intersection_params.vDirection.v[2] = -head_world_transform[2][2];

            if (vr::VROverlay()->ComputeOverlayIntersection(m_overlay_handle, &intersection_params, &intersection_results)) {
                if (m_closed_ui) {
                    const auto u = ((intersection_results.vUVs.v[0] * m_overlay_data.last_render_target_width) - m_overlay_data.last_x) / m_overlay_data.last_width;
                    const auto v = ((m_overlay_data.last_render_target_height - (intersection_results.vUVs.v[1] * m_overlay_data.last_render_target_height)) - m_overlay_data.last_y) / m_overlay_data.last_height;

                    any_intersected = u >= 0.25f &&
                                    u <= 0.75f && 
                                    v >= 0.25f && 
                                    v <= 0.75f;
                }

                auto normal = Vector4f{intersection_results.vNormal.v[0], intersection_results.vNormal.v[1], intersection_results.vNormal.v[2], 1.0f};
                normal = glm::inverse(glm::extractMatrixRotation(head_world_transform)) * normal;

                // Make sure the intersection hit the front of the overlay, not the back
                any_intersected = any_intersected && normal.z > 0.0f;
            } else {
                any_intersected = false;
            }
        }

        // set overlay flag
        if (any_intersected) {
            should_show_overlay = true;
            vr::VROverlay()->SetOverlayFlag(m_overlay_handle, vr::VROverlayFlags::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true);

            g_framework->set_draw_ui(true);

            if (m_closed_ui) {
                m_just_opened_ui = true;
            } else {
                m_just_opened_ui = false;
            }

            m_closed_ui = false;
            m_just_closed_ui = false;
        } else {
            should_show_overlay = false;
            vr::VROverlay()->SetOverlayFlag(m_overlay_handle, vr::VROverlayFlags::VROverlayFlags_MakeOverlaysInteractiveIfVisible, false);

            if (!m_closed_ui) {
                g_framework->set_draw_ui(false);

                m_closed_ui = true;
                m_just_closed_ui = true;
            } else {
                m_just_closed_ui = false;
            }

            m_just_opened_ui = false;
        }
    }

    return should_show_overlay;
}

void OverlayComponent::update_overlay_openvr() {
    if (!VR::get()->get_runtime()->is_openvr()) {
        return;
    }

    auto& vr = VR::get();

    const auto is_d3d11 = g_framework->get_renderer_type() == Framework::RendererType::D3D11;

    bool should_show_overlay = update_wrist_overlay_openvr();

    if (m_framework_wrist_ui->value()) {
        vr::VROverlay()->ShowOverlay(m_overlay_handle); // always show overlay idk look at it later

        if (should_show_overlay) {
            // finally set the texture
            if (is_d3d11) {
                vr::Texture_t imgui_tex{(void*)g_framework->get_rendertarget_d3d11().Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto};
                vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);   
            } else {
                auto& hook = g_framework->get_d3d12_hook();

                vr::D3D12TextureData_t texture_data {
                    g_framework->get_rendertarget_d3d12().Get(),
                    hook->get_command_queue(),
                    0
                };
                
                vr::Texture_t imgui_tex{(void*)&texture_data, vr::TextureType_DirectX12, vr::ColorSpace_Auto};
                vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);
            }
        } else {
            if (is_d3d11) {
                // draw a blank texture (don't just call HideOverlay, we'll no longer be able to use intersection tests)
                vr::Texture_t imgui_tex{(void*)g_framework->get_blank_rendertarget_d3d11().Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto};
                vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);
            } else {
                auto& hook = g_framework->get_d3d12_hook();

                vr::D3D12TextureData_t texture_data {
                    g_framework->get_blank_rendertarget_d3d12().Get(),
                    hook->get_command_queue(),
                    0
                };
                
                vr::Texture_t imgui_tex{(void*)&texture_data, vr::TextureType_DirectX12, vr::ColorSpace_Auto};
                vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);
            }
        }

        return;
    }

    // Draw the UI as a plane in front of the user instead
    if (!m_framework_wrist_ui->value() && g_framework->is_drawing_anything()) {
        vr::VROverlay()->ShowOverlay(m_overlay_handle);

        // Show the entire texture
        vr::VRTextureBounds_t bounds{};
        bounds.uMin = 0.0f;
        bounds.uMax = 1.0f;
        bounds.vMin = 0.0f;
        bounds.vMax = 1.0f;

        vr::VROverlay()->SetOverlayTextureBounds(m_overlay_handle, &bounds);

        vr::TrackedDevicePose_t pose{};
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, &pose, 1);

        //auto glm_matrix = glm::rowMajor4(Matrix4x4f{*(Matrix3x4f*)&pose.mDeviceToAbsoluteTracking});
        auto rotation_offset = glm::inverse(vr->get_rotation_offset());

        // If we're not drawing the UI, this means we want to draw the cursor all the time
        // So we need to rotate the UI's pitch as well
        if (!g_framework->is_drawing_ui() && vr->is_decoupled_pitch_enabled() && vr->is_decoupled_pitch_ui_adjust_enabled()) {
            const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
            const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);

            // Add the inverse of the pitch rotation to the rotation offset
            rotation_offset = glm::normalize(glm::inverse(pre_flat_pitch * vr->get_rotation_offset()));
        }

        auto glm_matrix = Matrix4x4f{rotation_offset};

        if (m_framework_ui_follows_view->value()) {
            // todo
        }

        glm_matrix[3] += vr->get_standing_origin();

        if (g_framework->is_drawing_ui()) {
            glm_matrix[3] -= glm_matrix[2] * m_framework_distance->value();
        } else {
            glm_matrix[3] -= glm_matrix[2] * (m_slate_distance->value() - 0.01f);

            glm_matrix[3] += m_slate_x_offset->value() * glm_matrix[0];
            glm_matrix[3] += m_slate_y_offset->value() * glm_matrix[1];
        }

        glm_matrix[3].w = 1.0f;
        const auto steamvr_matrix = Matrix3x4f{glm::rowMajor4(glm_matrix)};
        vr::VROverlay()->SetOverlayTransformAbsolute(m_overlay_handle, vr::TrackingUniverseStanding, (vr::HmdMatrix34_t*)&steamvr_matrix);

        const auto is_d3d12 = g_framework->get_renderer_type() == Framework::RendererType::D3D12;
        const auto size = is_d3d12 ? g_framework->get_d3d12_rt_size() : g_framework->get_d3d11_rt_size();
        const auto aspect = size.x / size.y;
        const auto width_meters = g_framework->is_drawing_ui() ? (m_framework_size->value() * aspect) : (m_slate_size->value() * aspect);
        vr::VROverlay()->SetOverlayWidthInMeters(m_overlay_handle, width_meters);

        if (is_d3d11) {
            vr::Texture_t imgui_tex{(void*)g_framework->get_rendertarget_d3d11().Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto};
            vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);   
        } else {
            auto& hook = g_framework->get_d3d12_hook();

            vr::D3D12TextureData_t texture_data {
                g_framework->get_rendertarget_d3d12().Get(),
                hook->get_command_queue(),
                0
            };
            
            vr::Texture_t imgui_tex{(void*)&texture_data, vr::TextureType_DirectX12, vr::ColorSpace_Auto};
            vr::VROverlay()->SetOverlayTexture(m_overlay_handle, &imgui_tex);
        }
    } else {
        vr::VROverlay()->ClearOverlayTexture(m_overlay_handle);
        vr::VROverlay()->HideOverlay(m_overlay_handle);
    }
}

std::optional<std::reference_wrapper<XrCompositionLayerQuad>> OverlayComponent::OpenXR::generate_slate_quad(
    runtimes::OpenXR::SwapchainIndex swapchain, 
    XrEyeVisibility eye) 
{
    auto& vr = VR::get();

    if (!vr->is_gui_enabled()) {
        return std::nullopt;
    }

    if (!vr->m_openxr->swapchains.contains((uint32_t)swapchain)) {
        return std::nullopt;
    }

    const auto is_left_eye = eye == XR_EYE_VISIBILITY_BOTH || eye == XR_EYE_VISIBILITY_LEFT;

    auto& layer = is_left_eye ? this->m_slate_layer : this->m_slate_layer_right;

    layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    const auto& ui_swapchain = vr->m_openxr->swapchains[(uint32_t)swapchain];
    layer.subImage.swapchain = ui_swapchain.handle;
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.subImage.imageRect.offset.x = 0;
    layer.subImage.imageRect.offset.y = 0;
    layer.subImage.imageRect.extent.width = ui_swapchain.width;
    layer.subImage.imageRect.extent.height = ui_swapchain.height;
    layer.eyeVisibility = eye;

    auto glm_matrix = glm::identity<glm::mat4>();

    if (vr->m_overlay_component.m_ui_follows_view->value()) {
        layer.space = vr->m_openxr->view_space;
    } else {
        auto rotation_offset = glm::inverse(vr->get_rotation_offset());

        if (vr->is_decoupled_pitch_enabled() && vr->is_decoupled_pitch_ui_adjust_enabled()) {
            const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
            const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);

            // Add the inverse of the pitch rotation to the rotation offset
            rotation_offset = glm::normalize(glm::inverse(pre_flat_pitch * vr->get_rotation_offset()));
        }

        glm_matrix = Matrix4x4f{rotation_offset};   
        glm_matrix[3] += vr->get_standing_origin();
        layer.space = vr->m_openxr->stage_space;
    }

    const auto size_meters = m_parent->m_slate_size->value();
    const auto meters_w = (float)ui_swapchain.width / (float)ui_swapchain.height * size_meters;
    const auto meters_h = size_meters;
    layer.size = {meters_w, meters_h};

    glm_matrix[3] -= glm_matrix[2] * m_parent->m_slate_distance->value();
    glm_matrix[3] += m_parent->m_slate_x_offset->value() * glm_matrix[0];
    glm_matrix[3] += m_parent->m_slate_y_offset->value() * glm_matrix[1];
    glm_matrix[3].w = 1.0f;

    layer.pose.orientation = runtimes::OpenXR::to_openxr(glm::quat_cast(glm_matrix));
    layer.pose.position = runtimes::OpenXR::to_openxr(glm_matrix[3]);

    return layer;
}

std::optional<std::reference_wrapper<XrCompositionLayerCylinderKHR>> OverlayComponent::OpenXR::generate_slate_cylinder(
    runtimes::OpenXR::SwapchainIndex swapchain, 
    XrEyeVisibility eye) 
{
    auto& vr = VR::get();

    if (!vr->is_gui_enabled()) {
        return std::nullopt;
    }

    if (!vr->m_openxr->swapchains.contains((uint32_t)swapchain)) {
        return std::nullopt;
    }

    const auto is_left_eye = eye == XR_EYE_VISIBILITY_BOTH || eye == XR_EYE_VISIBILITY_LEFT;

    auto& layer = is_left_eye ? this->m_slate_layer_cylinder : this->m_slate_layer_cylinder_right;

    layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
    const auto& ui_swapchain = vr->m_openxr->swapchains[(uint32_t)swapchain];
    layer.subImage.swapchain = ui_swapchain.handle;
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.subImage.imageRect.offset.x = 0;
    layer.subImage.imageRect.offset.y = 0;
    layer.subImage.imageRect.extent.width = ui_swapchain.width;
    layer.subImage.imageRect.extent.height = ui_swapchain.height;
    layer.eyeVisibility = eye;
    
    auto glm_matrix = glm::identity<glm::mat4>();

    if (vr->m_overlay_component.m_ui_follows_view->value()) {
        layer.space = vr->m_openxr->view_space;
    } else {
        auto rotation_offset = glm::inverse(vr->get_rotation_offset());

        if (vr->is_decoupled_pitch_enabled() && vr->is_decoupled_pitch_ui_adjust_enabled()) {
            const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
            const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);

            // Add the inverse of the pitch rotation to the rotation offset
            rotation_offset = glm::normalize(glm::inverse(pre_flat_pitch * vr->get_rotation_offset()));
        }

        glm_matrix = Matrix4x4f{rotation_offset};   
        glm_matrix[3] += vr->get_standing_origin();
        layer.space = vr->m_openxr->stage_space;
    }

    const auto size_meters = m_parent->m_slate_size->value();
    const auto meters_w = (float)ui_swapchain.width / (float)ui_swapchain.height * size_meters;
    const auto meters_h = size_meters;

    // OpenXR Docs:
    // radius is the non-negative radius of the cylinder. Values of zero or floating point positive infinity are treated as an infinite cylinder.
    // centralAngle is the angle of the visible section of the cylinder, based at 0 radians, in the range of [0, 2π). It grows symmetrically around the 0 radian angle.
    // aspectRatio is the ratio of the visible cylinder section width / height. The height of the cylinder is given by: (cylinder radius × cylinder angle) / aspectRatio.
    layer.centralAngle = glm::max<float>(1.0f, glm::radians(m_parent->m_slate_cylinder_angle->value()));
    layer.aspectRatio = (meters_w / meters_h);
    layer.radius = (meters_h / layer.centralAngle) * layer.aspectRatio;

    glm_matrix[3] -= glm_matrix[2] * m_parent->m_slate_distance->value();
    glm_matrix[3] += glm_matrix[2] * layer.radius;
    glm_matrix[3] += m_parent->m_slate_x_offset->value() * glm_matrix[0];
    glm_matrix[3] += m_parent->m_slate_y_offset->value() * glm_matrix[1];
    glm_matrix[3].w = 1.0f;

    layer.pose.orientation = runtimes::OpenXR::to_openxr(glm::quat_cast(glm_matrix));
    layer.pose.position = runtimes::OpenXR::to_openxr(glm_matrix[3]);

    return layer;
}

std::optional<std::reference_wrapper<XrCompositionLayerBaseHeader>> OverlayComponent::OpenXR::generate_slate_layer(
    runtimes::OpenXR::SwapchainIndex swapchain, 
    XrEyeVisibility eye)
{
    switch ((OverlayComponent::OverlayType)m_parent->m_slate_overlay_type->value()) {
    default:
    case OverlayComponent::OverlayType::QUAD:
        if (auto result = generate_slate_quad(swapchain, eye); result.has_value()) {
            return *(XrCompositionLayerBaseHeader*)&result.value().get();
        }

        return std::nullopt;
    case OverlayComponent::OverlayType::CYLINDER:
        if (!VR::get()->get_runtime()->is_cylinder_layer_allowed()) {
            if (auto result = generate_slate_quad(swapchain, eye); result.has_value()) {
                return *(XrCompositionLayerBaseHeader*)&result.value().get();
            }

            return std::nullopt;
        }

        if (auto result = generate_slate_cylinder(swapchain, eye); result.has_value()) {
            return *(XrCompositionLayerBaseHeader*)&result.value().get();
        }

        return std::nullopt;
    };
}


std::optional<std::reference_wrapper<XrCompositionLayerQuad>> OverlayComponent::OpenXR::generate_framework_ui_quad() {
    if (!g_framework->is_drawing_anything()) {
        return std::nullopt;
    }

    auto& vr = VR::get();

    auto& layer = this->m_framework_ui_layer;

    layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    const auto& ui_swapchain = vr->m_openxr->swapchains[(uint32_t)runtimes::OpenXR::SwapchainIndex::FRAMEWORK_UI];
    layer.subImage.swapchain = ui_swapchain.handle;
    layer.subImage.imageRect.offset.x = 0;
    layer.subImage.imageRect.offset.y = 0;
    layer.subImage.imageRect.extent.width = ui_swapchain.width;
    layer.subImage.imageRect.extent.height = ui_swapchain.height;
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.eyeVisibility = XrEyeVisibility::XR_EYE_VISIBILITY_BOTH;

    auto glm_matrix = glm::identity<glm::mat4>();

    if (vr->m_overlay_component.m_framework_ui_follows_view->value()) {
        layer.space = vr->m_openxr->view_space;
    } else {
        auto rotation_offset = glm::inverse(vr->get_rotation_offset());

        // If we're not drawing the UI, this means we want to draw the cursor all the time
        // So we need to rotate the UI's pitch as well
        if (!g_framework->is_drawing_ui() && vr->is_decoupled_pitch_enabled() && vr->is_decoupled_pitch_ui_adjust_enabled()) {
            const auto pre_flat_rotation = vr->get_pre_flattened_rotation();
            const auto pre_flat_pitch = utility::math::pitch_only(pre_flat_rotation);

            // Add the inverse of the pitch rotation to the rotation offset
            rotation_offset = glm::normalize(glm::inverse(pre_flat_pitch * vr->get_rotation_offset()));
        }

        glm_matrix = Matrix4x4f{rotation_offset};
        glm_matrix[3] += vr->get_standing_origin();
        layer.space = vr->m_openxr->stage_space;
    }

    const auto size_meters = g_framework->is_drawing_ui() ? m_parent->m_framework_size->value() : m_parent->m_slate_size->value();
    const auto meters_w = (float)ui_swapchain.width / (float)ui_swapchain.height * size_meters;
    const auto meters_h = size_meters;
    layer.size = {meters_w, meters_h};

    if (g_framework->is_drawing_ui()) {
        glm_matrix[3] -= glm_matrix[2] * m_parent->m_framework_distance->value();
    } else {
        glm_matrix[3] -= glm_matrix[2] * (m_parent->m_slate_distance->value() - 0.01f);

        glm_matrix[3] += m_parent->m_slate_x_offset->value() * glm_matrix[0];
        glm_matrix[3] += m_parent->m_slate_y_offset->value() * glm_matrix[1];
    }

    glm_matrix[3].w = 1.0f;

    layer.pose.orientation = runtimes::OpenXR::to_openxr(glm::quat_cast(glm_matrix));
    layer.pose.position = runtimes::OpenXR::to_openxr(glm_matrix[3]);

    return layer;
}
}