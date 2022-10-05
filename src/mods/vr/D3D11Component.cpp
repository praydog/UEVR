#include <imgui.h>
#include <imgui_internal.h>
#include <openvr.h>

#include "Framework.hpp"
#include "../VR.hpp"

#include "D3D11Component.hpp"

//#define VERBOSE_D3D11

#ifdef VERBOSE_D3D11
#define LOG_VERBOSE(...) spdlog::info(__VA_ARGS__)
#else
#define LOG_VERBOSE 
#endif

namespace vrmod {
vr::EVRCompositorError D3D11Component::on_frame(VR* vr) {
    if (m_left_eye_tex == nullptr) {
        setup();
    }

    auto& hook = g_framework->get_d3d11_hook();

    // get device
    auto device = hook->get_device();

    // Get the context.
    ComPtr<ID3D11DeviceContext> context{};

    device->GetImmediateContext(&context);

    // get swapchain
    auto swapchain = hook->get_swap_chain();

    // get back buffer
    ComPtr<ID3D11Texture2D> real_backbuffer{};
    swapchain->GetBuffer(0, IID_PPV_ARGS(&real_backbuffer));

    ComPtr<ID3D11Texture2D> backbuffer{};
    auto ue4_texture = VR::get()->m_fake_stereo_hook->get_render_target_manager()->get_render_target();

    if (ue4_texture != nullptr) {
        backbuffer = (ID3D11Texture2D*)ue4_texture->get_native_resource();
    }

    if (backbuffer == nullptr) {
        spdlog::error("[VR] Failed to get back buffer.");
        return vr::VRCompositorError_None;
    }

    const auto& ffsr = VR::get()->m_fake_stereo_hook;
    const auto ui_target = ffsr->get_render_target_manager()->get_ui_target();

    // Update the UI overlay.
    auto runtime = vr->get_runtime();

    if (runtime->is_openxr() && runtime->ready() && !vr->m_use_afr->value()) {
        if (!vr->m_openxr->frame_began) {
            LOG_VERBOSE("Beginning frame.");
            vr->m_openxr->begin_frame();
        }
    }

    if (ui_target != nullptr) {
        if (runtime->is_openvr() && get_ui_tex().Get() != nullptr) {
            copy_tex((ID3D11Resource*)ui_target->get_native_resource(), get_ui_tex().Get());
        } else if (runtime->is_openxr() && vr->m_openxr->frame_began) {
            m_openxr.copy((uint32_t)OpenXR::SwapchainIndex::UI, (ID3D11Texture2D*)ui_target->get_native_resource());
        }

        clear_tex((ID3D11Resource*)ui_target->get_native_resource());
    }

    // If m_frame_count is even, we're rendering the left eye.
    if (vr->m_render_frame_count % 2 == vr->m_left_eye_interval && vr->m_use_afr->value()) {
        if (runtime->is_openxr() && runtime->ready()) {
            LOG_VERBOSE("Copying left eye");
            m_openxr.copy(0, backbuffer.Get());
        }

        const auto has_skip_present_fix = vr->m_desktop_fix->value() && vr->m_desktop_fix_skip_present->value();

        // Copy the back buffer to the left eye texture
        // always do it because are using this for the desktop recording fix
        bool already_copied = false;

        if (!has_skip_present_fix) {
            context->CopyResource(m_left_eye_tex.Get(), backbuffer.Get());
            already_copied = true;
        }

        if (runtime->is_openvr()) {
            if (has_skip_present_fix && !already_copied) {
                context->CopyResource(m_left_eye_tex.Get(), backbuffer.Get());
            }

            vr::Texture_t left_eye{(void*)m_left_eye_tex.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto};

            auto e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds);

            bool submitted = true;

            if (e != vr::VRCompositorError_None) {
                spdlog::error("[VR] VRCompositor failed to submit left eye: {}", (int)e);
                vr->m_submitted = false;
                return e;
            }
        }
    } else {
        if (runtime->ready() && runtime->is_openxr()) {
            if (!vr->m_use_afr->value()) {
                LOG_VERBOSE("Copying left eye");
                D3D11_BOX src_box{};
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
                m_openxr.copy((uint32_t)OpenXR::SwapchainIndex::LEFT_EYE, backbuffer.Get(), &src_box);
            }

            LOG_VERBOSE("Copying right eye");

            D3D11_BOX src_box{};
            src_box.left = m_backbuffer_size[0] / 2;
            src_box.right = m_backbuffer_size[0];
            src_box.top = 0;
            src_box.bottom = m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            m_openxr.copy((uint32_t)OpenXR::SwapchainIndex::RIGHT_EYE, backbuffer.Get(), &src_box);

            LOG_VERBOSE("Ending frame");
            std::vector<XrCompositionLayerQuad> quad_layers{};

            auto& layer = quad_layers.emplace_back();
            layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
            const auto& ui_swapchain = vr->m_openxr->swapchains[(uint32_t)OpenXR::SwapchainIndex::UI];
            layer.subImage.swapchain = ui_swapchain.handle;
            layer.subImage.imageRect.offset.x = 0;
            layer.subImage.imageRect.offset.y = 0;
            layer.subImage.imageRect.extent.width = ui_swapchain.width;
            layer.subImage.imageRect.extent.height = ui_swapchain.height;
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layer.eyeVisibility = XrEyeVisibility::XR_EYE_VISIBILITY_BOTH;
            layer.space = vr->m_openxr->stage_space;
            
            const auto size_meters = 2.0f;
            const auto meters_w = (float)ui_swapchain.width / (float)ui_swapchain.height * size_meters;
            const auto meters_h = size_meters;
            layer.size = {meters_w, meters_h};

            auto glm_matrix = Matrix4x4f{vr->get_rotation_offset()};
            glm_matrix[3] += vr->get_standing_origin();
            glm_matrix[3] -= glm_matrix[2] * 1.5f;
            glm_matrix[3].w = 1.0f;

            layer.pose.orientation = runtimes::OpenXR::to_openxr(glm::quat_cast(glm_matrix));
            layer.pose.position = runtimes::OpenXR::to_openxr(glm_matrix[3]);

            auto result = vr->m_openxr->end_frame(quad_layers);

            vr->m_openxr->needs_pose_update = true;
            vr->m_submitted = result == XR_SUCCESS;
        }

        if (runtime->is_openvr()) {
            auto openvr = vr->get_runtime<runtimes::OpenVR>();
            const auto submit_pose = openvr->get_pose_for_submit();

            vr::EVRCompositorError e = vr::VRCompositorError_None;

            if (!vr->m_use_afr->value()) {
                D3D11_BOX src_box{};
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;

                context->CopySubresourceRegion(m_left_eye_tex.Get(), 0, 0, 0, 0, backbuffer.Get(), 0, &src_box);

                vr::VRTextureWithPose_t left_eye{
                    (void*)m_left_eye_tex.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto,
                    submit_pose
                };

                e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds, vr::EVRSubmitFlags::Submit_TextureWithPose);

                if (e != vr::VRCompositorError_None) {
                    spdlog::error("[VR] VRCompositor failed to left eye: {}", (int)e);
                    vr->m_submitted = false;
                    return e;
                }
            }

            // Copy the back buffer to the right eye texture.
            D3D11_BOX src_box{};
            src_box.left = m_backbuffer_size[0] / 2;
            src_box.right = m_backbuffer_size[0];
            src_box.top = 0;
            src_box.bottom = m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            context->CopySubresourceRegion(m_right_eye_tex.Get(), 0, 0, 0, 0, backbuffer.Get(), 0, &src_box);

            vr::VRTextureWithPose_t right_eye{
                (void*)m_right_eye_tex.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto,
                submit_pose
            };

            e = vr::VRCompositor()->Submit(vr::Eye_Right, &right_eye, &vr->m_right_bounds, vr::EVRSubmitFlags::Submit_TextureWithPose);

            bool submitted = true;

            if (e != vr::VRCompositorError_None) {
                spdlog::error("[VR] VRCompositor failed to submit right eye: {}", (int)e);
                vr->m_submitted = false;
                return e;
            }

            vr->m_submitted = true;
        }

        /*if (runtime->ready() && vr->m_desktop_fix->value()) {
            if (vr->m_desktop_fix_skip_present->value()) {
                hook->ignore_next_present();
            } else {
                context->CopyResource(real_backbuffer.Get(), m_right_eye_tex.Get());
            }
        }*/
    }

    return vr::VRCompositorError_None;
}

void D3D11Component::on_reset(VR* vr) {
    m_left_eye_tex.Reset();
    m_right_eye_tex.Reset();
    m_ui_tex.Reset();
    m_blank_tex.Reset();
    m_left_eye_depthstencil.Reset();
    m_right_eye_depthstencil.Reset();

    if (vr->get_runtime()->is_openxr() && vr->get_runtime()->loaded) {
        if (m_openxr.last_resolution[0] != vr->get_hmd_width() || m_openxr.last_resolution[1] != vr->get_hmd_height()) {
            m_openxr.create_swapchains();
        }
    }
}

void D3D11Component::clear_tex(ID3D11Resource* rsrc) {
    auto& hook = g_framework->get_d3d11_hook();
    auto device = hook->get_device();

    ComPtr<ID3D11DeviceContext> context{};
    device->GetImmediateContext(&context);

    context->CopyResource(rsrc, get_blank_tex().Get());
}

void D3D11Component::copy_tex(ID3D11Resource* src, ID3D11Resource* dst) {
    auto& hook = g_framework->get_d3d11_hook();
    auto device = hook->get_device();

    ComPtr<ID3D11DeviceContext> context{};
    device->GetImmediateContext(&context);

    context->CopyResource(dst, src);
}

void D3D11Component::setup() {
    spdlog::info("[VR] Setting up D3D11 textures...");

    // Get device and swapchain.
    auto& hook = g_framework->get_d3d11_hook();
    auto device = hook->get_device();
    auto swapchain = hook->get_swap_chain();

    // Get back buffer.
    //ComPtr<ID3D11Texture2D> backbuffer{};
    // swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));

    ComPtr<ID3D11Texture2D> backbuffer{};
    auto ue4_texture = VR::get()->m_fake_stereo_hook->get_render_target_manager()->get_render_target();

    if (ue4_texture != nullptr) {
        backbuffer = (ID3D11Texture2D*)ue4_texture->get_native_resource();
    }

    if (backbuffer == nullptr) {
        spdlog::error("[VR] Failed to get back buffer (D3D11).");
        return;
    }

    // Get backbuffer description.
    D3D11_TEXTURE2D_DESC backbuffer_desc{};
    backbuffer->GetDesc(&backbuffer_desc);

    m_backbuffer_size[0] = backbuffer_desc.Width;
    m_backbuffer_size[1] = backbuffer_desc.Height;

    backbuffer_desc.Width = backbuffer_desc.Width / 2;

    spdlog::info("[VR] W: {}, H: {}", backbuffer_desc.Width, backbuffer_desc.Height);
    spdlog::info("[VR] Format: {}", backbuffer_desc.Format);

    backbuffer_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    // Create eye textures.
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_left_eye_tex);
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_right_eye_tex);

    backbuffer_desc.Width = (uint32_t)g_framework->get_d3d11_rt_size().x;
    backbuffer_desc.Height = (uint32_t)g_framework->get_d3d11_rt_size().y;
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_ui_tex);
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_blank_tex);

    // copy backbuffer into right eye
    // Get the context.
    ComPtr<ID3D11DeviceContext> context{};

    device->GetImmediateContext(&context);
    context->CopyResource(m_right_eye_tex.Get(), backbuffer.Get());

    // Make depth stencils for both eyes.
    auto depthstencil = hook->get_last_depthstencil_used();

    if (depthstencil != nullptr) {
        D3D11_TEXTURE2D_DESC depthstencil_desc{};

        depthstencil->GetDesc(&depthstencil_desc);

        // Create eye depthstencils.
        device->CreateTexture2D(&depthstencil_desc, nullptr, &m_left_eye_depthstencil);
        device->CreateTexture2D(&depthstencil_desc, nullptr, &m_right_eye_depthstencil);

        // Copy the current depthstencil into the right eye.
        context->CopyResource(m_right_eye_depthstencil.Get(), depthstencil.Get());
    }

    spdlog::info("[VR] d3d11 textures have been setup");
}

void D3D11Component::OpenXR::initialize(XrSessionCreateInfo& session_info) {
    std::scoped_lock _{this->mtx};

    auto& hook = g_framework->get_d3d11_hook();

    auto device = hook->get_device();

    this->binding.device = device;

    PFN_xrGetD3D11GraphicsRequirementsKHR fn = nullptr;
    xrGetInstanceProcAddr(VR::get()->m_openxr->instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&fn));

    if (fn == nullptr) {
        spdlog::error("[VR] xrGetD3D11GraphicsRequirementsKHR not found");
        return;
    }

    // get existing adapter from device
    ComPtr<IDXGIDevice> dxgi_device{};
    
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)))) {
        spdlog::error("[VR] failed to get DXGI device from D3D11 device");
        return;
    }
    
    ComPtr<IDXGIAdapter> adapter{};

    if (FAILED(dxgi_device->GetAdapter(&adapter))) {
        spdlog::error("[VR] failed to get DXGI adapter from DXGI device");
        return;
    }

    DXGI_ADAPTER_DESC desc{};

    if (FAILED(adapter->GetDesc(&desc))) {
        spdlog::error("[VR] failed to get DXGI adapter description");
        return;
    }
    
    XrGraphicsRequirementsD3D11KHR gr{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    gr.adapterLuid = desc.AdapterLuid;
    gr.minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    fn(VR::get()->m_openxr->instance, VR::get()->m_openxr->system, &gr);

    session_info.next = &this->binding;
}

std::optional<std::string> D3D11Component::OpenXR::create_swapchains() {
    std::scoped_lock _{this->mtx};

    spdlog::info("[VR] Creating OpenXR swapchains for D3D11");

    this->destroy_swapchains();

    auto& hook = g_framework->get_d3d11_hook();
    auto device = hook->get_device();
    auto swapchain = hook->get_swap_chain();

    // Get back buffer.
    ComPtr<ID3D11Texture2D> backbuffer{};
    ComPtr<ID3D11Texture2D> og_backbuffer{};

    auto vr = VR::get();

    if (vr != nullptr && vr->m_fake_stereo_hook != nullptr) {
        auto ue4_texture = vr->m_fake_stereo_hook->get_render_target_manager()->get_render_target();

        if (ue4_texture != nullptr) {
            backbuffer = (ID3D11Texture2D*)ue4_texture->get_native_resource();
        }
    }
    
    if (backbuffer == nullptr) {
        swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    }

    swapchain->GetBuffer(0, IID_PPV_ARGS(&og_backbuffer));
    
    if (backbuffer == nullptr || og_backbuffer == nullptr) {
        spdlog::error("[VR] Failed to get back buffer.");
        return "Failed to get back buffer.";
    }

    // Get backbuffer description.
    D3D11_TEXTURE2D_DESC backbuffer_desc{};
    backbuffer->GetDesc(&backbuffer_desc);

    backbuffer_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    auto& openxr = *vr->m_openxr;

    this->contexts.clear();
    this->contexts.resize(openxr.views.size() + 1); // +1 for the UI texture

    auto create_swapchain = [&](uint32_t i, uint32_t w, uint32_t h) -> std::optional<std::string> {
        const auto old_w = backbuffer_desc.Width;
        const auto old_h = backbuffer_desc.Height;

        backbuffer_desc.Width = w;
        backbuffer_desc.Height = h;

        XrSwapchainCreateInfo swapchain_create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchain_create_info.arraySize = 1;
        swapchain_create_info.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        swapchain_create_info.width = w;
        swapchain_create_info.height = h;
        swapchain_create_info.mipCount = 1;
        swapchain_create_info.faceCount = 1;
        swapchain_create_info.sampleCount = 1;
        swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        runtimes::OpenXR::Swapchain swapchain{};
        swapchain.width = swapchain_create_info.width;
        swapchain.height = swapchain_create_info.height;

        if (xrCreateSwapchain(openxr.session, &swapchain_create_info, &swapchain.handle) != XR_SUCCESS) {
            spdlog::error("[VR] D3D11: Failed to create swapchain.");
            return "Failed to create swapchain.";
        }

        vr->m_openxr->swapchains.push_back(swapchain);

        uint32_t image_count{};
        auto result = xrEnumerateSwapchainImages(swapchain.handle, 0, &image_count, nullptr);

        if (result != XR_SUCCESS) {
            spdlog::error("[VR] Failed to enumerate swapchain images.");
            return "Failed to enumerate swapchain images.";
        }

        auto& ctx = this->contexts[i];

        ctx.textures.clear();
        ctx.textures.resize(image_count);

        for (uint32_t j = 0; j < image_count; ++j) {
            spdlog::info("[VR] Creating swapchain image {} for swapchain {}", j, i);

            ctx.textures[j] = {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR};

            if (FAILED(device->CreateTexture2D(&backbuffer_desc, nullptr, &ctx.textures[j].texture))) {
                spdlog::error("[VR] Failed to create swapchain texture {} {}", i, j);
                return "Failed to create swapchain texture.";
            }

            // get immediate context
            ComPtr<ID3D11DeviceContext> context{};
            device->GetImmediateContext(&context);
            context->CopyResource(ctx.textures[j].texture, backbuffer.Get());
        }

        result = xrEnumerateSwapchainImages(swapchain.handle, image_count, &image_count, (XrSwapchainImageBaseHeader*)&ctx.textures[0]);

        if (result != XR_SUCCESS) {
            spdlog::error("[VR] Failed to enumerate swapchain images after texture creation.");
            return "Failed to enumerate swapchain images after texture creation.";
        }

        backbuffer_desc.Width = old_w;
        backbuffer_desc.Height = old_h;

        return std::nullopt;
    };

    // Create eye textures.
    for (auto i = 0; i < openxr.views.size(); ++i) {
        spdlog::info("[VR] Creating swapchain for eye {}", i);
        spdlog::info("[VR] Width: {}", vr->get_hmd_width());
        spdlog::info("[VR] Height: {}", vr->get_hmd_height());

        if (auto err = create_swapchain(i, vr->get_hmd_width(), vr->get_hmd_height())) {
            return err;
        }
    }

    // The UI texture
    if (auto err = create_swapchain((uint32_t)OpenXR::SwapchainIndex::UI, backbuffer_desc.Width, backbuffer_desc.Height)) {
        return err;
    }

    this->last_resolution = {vr->get_hmd_width(), vr->get_hmd_height()};

    spdlog::info("[VR] Successfully created OpenXR swapchains for D3D11");

    return std::nullopt;
}

void D3D11Component::OpenXR::destroy_swapchains() {
    std::scoped_lock _{this->mtx};

	if (this->contexts.empty()) {
        return;
    }

    spdlog::info("[VR] Destroying swapchains.");

    for (auto i = 0; i < this->contexts.size(); ++i) {
        auto& ctx = this->contexts[i];

        auto result = xrDestroySwapchain(VR::get()->m_openxr->swapchains[i].handle);

        if (result != XR_SUCCESS) {
            spdlog::error("[VR] Failed to destroy swapchain {}.", i);
        } else {
            spdlog::info("[VR] Destroyed swapchain {}.", i);
        }

        for (auto& tex : ctx.textures) {
            tex.texture->Release();
        }
        
        ctx.textures.clear();
    }

    this->contexts.clear();
    VR::get()->m_openxr->swapchains.clear();
}

void D3D11Component::OpenXR::copy(uint32_t swapchain_idx, ID3D11Texture2D* resource, D3D11_BOX* src_box) {
    std::scoped_lock _{this->mtx};

    auto vr = VR::get();

    if (vr->m_openxr->frame_state.shouldRender != XR_TRUE) {
        return;
    }

    if (!vr->m_openxr->frame_began) {
        spdlog::error("[VR] OpenXR: Frame not begun when trying to copy.");
        //return;
    }

    if (this->contexts[swapchain_idx].num_textures_acquired > 0) {
        spdlog::info("[VR] Already acquired textures for swapchain {}?", swapchain_idx);
    }

    auto device = g_framework->get_d3d11_hook()->get_device();
    
    // get immediate context
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    const auto& swapchain = vr->m_openxr->swapchains[swapchain_idx];
    auto& ctx = this->contexts[swapchain_idx];

    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

    uint32_t texture_index{};
    LOG_VERBOSE("Acquiring swapchain image for {}", swapchain_idx);
    auto result = xrAcquireSwapchainImage(swapchain.handle, &acquire_info, &texture_index);

    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrAcquireSwapchainImage failed: {}", vr->m_openxr->get_result_string(result));
    } else {
        ctx.num_textures_acquired++;

        XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        //wait_info.timeout = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count();
        wait_info.timeout = XR_INFINITE_DURATION;

        LOG_VERBOSE("Waiting on swapchain image for {}", swapchain_idx);
        result = xrWaitSwapchainImage(swapchain.handle, &wait_info);

        if (result != XR_SUCCESS) {
            spdlog::error("[VR] xrWaitSwapchainImage failed: {}", vr->m_openxr->get_result_string(result));
        } else {
            LOG_VERBOSE("Copying swapchain image {} for {}", texture_index, swapchain_idx);

            if (src_box == nullptr) {
                context->CopyResource(ctx.textures[texture_index].texture, resource);
            } else {
                context->CopySubresourceRegion(ctx.textures[texture_index].texture, 0, 0, 0, 0, resource, 0, src_box);
            }

            XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

            LOG_VERBOSE("Releasing swapchain image for {}", swapchain_idx);
            auto result = xrReleaseSwapchainImage(swapchain.handle, &release_info);

            if (result != XR_SUCCESS) {
                spdlog::error("[VR] xrReleaseSwapchainImage failed: {}", vr->m_openxr->get_result_string(result));
                return;
            }

            ctx.num_textures_acquired--;
        }
    }
}
} // namespace vrmod
