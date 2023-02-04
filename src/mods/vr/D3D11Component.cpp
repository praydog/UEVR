#include <imgui.h>
#include <imgui_internal.h>
#include <openvr.h>
#include <d3dcompiler.h>

namespace vertex_shader1 {
#include "shaders/vs.hpp"
}

namespace pixel_shader1 {
#include "shaders/ps.hpp"
}

#include <utility/ScopeGuard.hpp>

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
    if (m_left_eye_tex == nullptr || m_force_reset) {
        if (!setup()) {
            spdlog::error("Failed to setup D3D11Component, trying again next frame");
            m_force_reset = true;
            return vr::VRCompositorError_None;
        }
    }

    auto& hook = g_framework->get_d3d11_hook();

    hook->set_next_present_interval(0); // disable vsync for vr

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

    auto runtime = vr->get_runtime();

    const auto is_same_frame = m_last_rendered_frame > 0 && m_last_rendered_frame == vr->m_render_frame_count;
    m_last_rendered_frame = vr->m_render_frame_count;

    const auto is_afr = !is_same_frame && vr->is_using_afr();
    const auto is_left_eye_frame = is_afr && vr->m_render_frame_count % 2 == vr->m_left_eye_interval;
    const auto is_right_eye_frame = !is_afr || vr->m_render_frame_count % 2 == vr->m_right_eye_interval;

    if (runtime->is_openxr() && runtime->ready() && !is_afr) {
        if (!vr->m_openxr->frame_began) {
            LOG_VERBOSE("Beginning frame.");
            vr->m_openxr->begin_frame();
        }
    }

    // Update the UI overlay.
    const auto& ffsr = VR::get()->m_fake_stereo_hook;
    const auto ui_target = ffsr->get_render_target_manager()->get_ui_target();

    if (ui_target != nullptr) {
        // Duplicate frames can sometimes cause the UI to get stuck on the screen.
        // and can lock up the compositor.
        if (is_right_eye_frame) {
            if (runtime->is_openvr() && get_ui_tex().Get() != nullptr) {
                copy_tex((ID3D11Resource*)ui_target->get_native_resource(), get_ui_tex().Get());
            } else if (runtime->is_openxr() && vr->m_openxr->frame_began) {
                m_openxr.copy((uint32_t)runtimes::OpenXR::SwapchainIndex::UI, (ID3D11Texture2D*)ui_target->get_native_resource());
            }
        }

        clear_tex((ID3D11Resource*)ui_target->get_native_resource());
    }

    // If m_frame_count is even, we're rendering the left eye.
    if (is_left_eye_frame) {
        m_submitted_left_eye = true;

        if (runtime->is_openxr() && runtime->ready()) {
            LOG_VERBOSE("Copying left eye");
            //m_openxr.copy(0, backbuffer.Get());
            D3D11_BOX src_box{};
            src_box.left = 0;
            src_box.right = m_backbuffer_size[0] / 2;
            src_box.top = 0;
            src_box.bottom = m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            m_openxr.copy((uint32_t)runtimes::OpenXR::SwapchainIndex::LEFT_EYE, backbuffer.Get(), &src_box);
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

            const auto submit_pose = vr->m_openvr->get_pose_for_submit();

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

            const auto e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds, vr::EVRSubmitFlags::Submit_TextureWithPose);

            if (e != vr::VRCompositorError_None) {
                spdlog::error("[VR] VRCompositor failed to submit left eye: {}", (int)e);
                vr->m_submitted = false;
                return e;
            }
        }
    } else {
        utility::ScopeGuard __{[&]() {
            m_submitted_left_eye = false;
        }};

        if (runtime->ready() && runtime->is_openxr()) {
            if (!is_afr && !m_submitted_left_eye) {
                LOG_VERBOSE("Copying left eye");
                D3D11_BOX src_box{};
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
                m_openxr.copy((uint32_t)runtimes::OpenXR::SwapchainIndex::LEFT_EYE, backbuffer.Get(), &src_box);
            }

            LOG_VERBOSE("Copying right eye");

            D3D11_BOX src_box{};
            if (!is_afr) {
                src_box.left = m_backbuffer_size[0] / 2;
                src_box.right = m_backbuffer_size[0];
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
            } else { // Copy the left eye on AFR
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
            }

            m_openxr.copy((uint32_t)runtimes::OpenXR::SwapchainIndex::RIGHT_EYE, backbuffer.Get(), &src_box);

            LOG_VERBOSE("Ending frame");
            std::vector<XrCompositionLayerQuad> quad_layers{};

            const auto slate_quad = vr->get_overlay_component().get_openxr().generate_slate_quad();
            if (slate_quad) {
                quad_layers.push_back(*slate_quad);
            }
            
            auto result = vr->m_openxr->end_frame(quad_layers);

            vr->m_openxr->needs_pose_update = true;
            vr->m_submitted = result == XR_SUCCESS;
        }

        if (runtime->is_openvr()) {
            auto openvr = vr->get_runtime<runtimes::OpenVR>();
            const auto submit_pose = openvr->get_pose_for_submit();

            vr::EVRCompositorError e = vr::VRCompositorError_None;

            if (!is_afr) {
                D3D11_BOX src_box{};
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;

                context->CopySubresourceRegion(m_left_eye_tex.Get(), 0, 0, 0, 0, backbuffer.Get(), 0, &src_box);

                if (m_is_shader_setup) {
                    ID3D11RenderTargetView* views[] = { m_left_eye_rtv.Get() };
                    //context->OMSetRenderTargets(1, views, nullptr);
                    //context->ClearRenderTargetView(m_right_eye_rtv.Get(), clear_color);
                    //invoke_shader(vr->m_render_frame_count, 0, m_backbuffer_size[0] / 2, m_backbuffer_size[1]);
                }

                vr::VRTextureWithPose_t left_eye{
                    (void*)m_left_eye_tex.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto,
                    submit_pose
                };

                e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds, vr::EVRSubmitFlags::Submit_TextureWithPose);

                if (e != vr::VRCompositorError_None) {
                    spdlog::error("[VR] VRCompositor failed to submit left eye: {}", (int)e);
                    vr->m_submitted = false;
                    return e;
                }
            }

            // Copy the back buffer to the right eye texture.
            D3D11_BOX src_box{};
            if (!is_afr) {
                src_box.left = m_backbuffer_size[0] / 2;
                src_box.right = m_backbuffer_size[0];
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
            } else { // Copy the left eye on AFR
                src_box.left = 0;
                src_box.right = m_backbuffer_size[0] / 2;
                src_box.top = 0;
                src_box.bottom = m_backbuffer_size[1];
                src_box.front = 0;
                src_box.back = 1;
            }

            context->CopySubresourceRegion(m_right_eye_tex.Get(), 0, 0, 0, 0, backbuffer.Get(), 0, &src_box);

            if (m_is_shader_setup) {
                ID3D11RenderTargetView* views[] = { m_right_eye_rtv.Get() };
                //context->OMSetRenderTargets(1, views, nullptr);
                //invoke_shader(vr->m_render_frame_count, 1, m_backbuffer_size[0] / 2, m_backbuffer_size[1]);
                //context->OMSetRenderTargets(1, &prev_rtv, prev_depth_rtv.Get());     
            }

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
    m_left_eye_rtv.Reset();
    m_right_eye_rtv.Reset();
    m_left_eye_srv.Reset();
    m_right_eye_srv.Reset();
    m_ui_tex.Reset();
    m_blank_tex.Reset();
    m_left_eye_depthstencil.Reset();
    m_right_eye_depthstencil.Reset();
    m_vs_shader_blob.Reset();
    m_ps_shader_blob.Reset();
    m_vs_shader.Reset();
    m_ps_shader.Reset();
    m_input_layout.Reset();
    m_constant_buffer.Reset();
    m_is_shader_setup = false;

    if (vr->get_runtime()->is_openxr() && vr->get_runtime()->loaded) {
        if (m_openxr.last_resolution[0] != vr->get_hmd_width() || m_openxr.last_resolution[1] != vr->get_hmd_height() ||
            vr->m_openxr->swapchains.empty() ||
            g_framework->get_d3d11_rt_size()[0] != vr->m_openxr->swapchains[(uint32_t)runtimes::OpenXR::SwapchainIndex::UI].width ||
            g_framework->get_d3d11_rt_size()[1] != vr->m_openxr->swapchains[(uint32_t)runtimes::OpenXR::SwapchainIndex::UI].height) 
        {
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

bool D3D11Component::setup() {
    spdlog::info("[VR] Setting up D3D11 textures...");

    on_reset(VR::get().get());

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
        return false;
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
    backbuffer_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    // Create eye textures.
    if (FAILED(device->CreateTexture2D(&backbuffer_desc, nullptr, &m_left_eye_tex))) {
        spdlog::error("[VR] Failed to create left eye texture (D3D11).");
        return false;
    }

    if (FAILED(device->CreateTexture2D(&backbuffer_desc, nullptr, &m_right_eye_tex))) {
        spdlog::error("[VR] Failed to create right eye texture (D3D11).");
        return false;
    }

    backbuffer_desc.Width = (uint32_t)g_framework->get_d3d11_rt_size().x;
    backbuffer_desc.Height = (uint32_t)g_framework->get_d3d11_rt_size().y;
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_ui_tex);
    device->CreateTexture2D(&backbuffer_desc, nullptr, &m_blank_tex);

    // copy backbuffer into right eye
    // Get the context.
    ComPtr<ID3D11DeviceContext> context{};

    device->GetImmediateContext(&context);
    context->CopyResource(m_right_eye_tex.Get(), backbuffer.Get());
    context->CopyResource(m_left_eye_tex.Get(), backbuffer.Get());

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = -1;

    if (FAILED(device->CreateShaderResourceView(m_left_eye_tex.Get(), &srv_desc, &m_left_eye_srv))) {
        spdlog::error("[VR] Failed to create left eye shader resource view (D3D11).");
        return false;
    }

    if (FAILED(device->CreateShaderResourceView(m_right_eye_tex.Get(), &srv_desc, &m_right_eye_srv))) {
        spdlog::error("[VR] Failed to create right eye shader resource view (D3D11).");
        return false;
    }

    // Create render target views for the eye textures.
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
    rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    if (FAILED(device->CreateRenderTargetView(m_left_eye_tex.Get(), &rtv_desc, &m_left_eye_rtv))) {
        spdlog::error("[VR] Failed to create left eye render target view (D3D11).");
        return false;
    }

    if (FAILED(device->CreateRenderTargetView(m_right_eye_tex.Get(), &rtv_desc, &m_right_eye_rtv))) {
        spdlog::error("[VR] Failed to create right eye render target view (D3D11).");
        return false;
    }

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

    m_is_shader_setup = setup_shader();
    m_force_reset = false;

    return true;
}

bool D3D11Component::setup_shader() {
    spdlog::info("[VR] Setting up D3D11 shader...");

    auto& hook = g_framework->get_d3d11_hook();
    auto device = hook->get_device();

    // Create a new shader blob and copy the memory from the shader.
    const auto vs_shader_size = sizeof(vertex_shader1::g_main);
    const auto ps_shader_size = sizeof(pixel_shader1::g_main);

    if (FAILED(D3DCreateBlob(vs_shader_size, &m_vs_shader_blob))) {
        spdlog::error("[VR] Failed to create vertex shader blob.");
        return false;
    }

    if (FAILED(D3DCreateBlob(ps_shader_size, &m_ps_shader_blob))) {
        spdlog::error("[VR] Failed to create pixel shader blob.");
        return false;
    }

    memcpy(m_vs_shader_blob->GetBufferPointer(), vertex_shader1::g_main, vs_shader_size);
    memcpy(m_ps_shader_blob->GetBufferPointer(), pixel_shader1::g_main, ps_shader_size);

    // Create the vertex shader.
    if (FAILED(device->CreateVertexShader(m_vs_shader_blob->GetBufferPointer(), m_vs_shader_blob->GetBufferSize(), nullptr, &m_vs_shader))) {
        spdlog::error("[VR] Failed to create vertex shader.");
        return false;
    }

    // Create the pixel shader.
    if (FAILED(device->CreatePixelShader(m_ps_shader_blob->GetBufferPointer(), m_ps_shader_blob->GetBufferSize(), nullptr, &m_ps_shader))) {
        spdlog::error("[VR] Failed to create pixel shader.");
        return false;
    }

    /*
    struct VSInput {
        float3 position : POSITION;
        float4 color : COLOR;
        float2 texuv0 : TEXCOORD0;
        float2 texuv1 : TEXCOORD1;
        float3 normal : NORMAL;
        float3 tangent : TANGENT;
        float3 bitangent : BITANGENT;
    };

    struct PSInput {
        float4 position : SV_POSITION;
        float4 color : COLOR;
        float2 texuv0 : TEXCOORD0;
        float2 texuv1 : TEXCOORD1;
        float3 normal : NORMAL;
        float3 tangent : TANGENT;
        float3 bitangent : BITANGENT;
    };
    */

    // Create the input layout.
    D3D11_INPUT_ELEMENT_DESC input_layout_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (FAILED(device->CreateInputLayout(input_layout_desc, 7, m_vs_shader_blob->GetBufferPointer(), m_vs_shader_blob->GetBufferSize(), &m_input_layout))) {
        spdlog::error("[VR] Failed to create input layout.");
        return false;
    }

    // Create the constant buffer.
    D3D11_BUFFER_DESC constant_buffer_desc{};
    constant_buffer_desc.ByteWidth = sizeof(D3D11Component::ShaderGlobals);
    constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constant_buffer_desc.MiscFlags = 0;
    constant_buffer_desc.StructureByteStride = 0;

    m_shader_globals.model = DirectX::XMMatrixIdentity();
    m_shader_globals.view = DirectX::XMMatrixIdentity();
    m_shader_globals.proj = DirectX::XMMatrixIdentity();
    m_shader_globals.resolution = DirectX::XMFLOAT4(1920.0f, 1080.0f, 0.0f, 1920.0f / 1080.0f);
    m_shader_globals.time = 1.0f;

    DirectX::XMVECTOR loc = DirectX::XMVectorSet(0, 0, -5, 0);
    DirectX::XMVECTOR negloc = DirectX::XMVectorSet(0, 0, 5, 0);
    DirectX::XMVECTOR target = DirectX::XMVectorSet(0, 0, 0, 0);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
    m_shader_globals.view = DirectX::XMMatrixLookAtLH(loc, target, up);

    DirectX::XMVECTOR model_pos = DirectX::XMVectorSet(0, 0, 0, 0);

    m_shader_globals.model = DirectX::XMMatrixLookAtLH(model_pos, negloc, up);

    const auto hmd_transform = VR::get()->get_hmd_transform(0);

    const auto model_pos2 = hmd_transform[3] + (hmd_transform[2] * 2.0f);
    m_shader_globals.model.r[3] = DirectX::XMVECTOR{model_pos2.x, model_pos2.y, model_pos2.z, 1.0f};

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = &m_shader_globals;
    init_data.SysMemPitch = 0;
    init_data.SysMemSlicePitch = 0;

    if (FAILED(device->CreateBuffer(&constant_buffer_desc, &init_data, &m_constant_buffer))) {
        spdlog::error("[VR] Failed to create constant buffer.");
        return false;
    };

    if (m_constant_buffer == nullptr) {
        spdlog::error("[VR] Failed to create constant buffer (nullptr).");
        return false;
    }

    // Create the vertex buffer.
    D3D11_BUFFER_DESC vertex_buffer_desc{};
    vertex_buffer_desc.ByteWidth = sizeof(D3D11Component::Vertex) * 4;
    vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertex_buffer_desc.CPUAccessFlags = 0;

    Vertex vertices[] =
    {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    D3D11_SUBRESOURCE_DATA vertex_buffer_data{};
    ZeroMemory(&vertex_buffer_data, sizeof(D3D11_SUBRESOURCE_DATA));
    vertex_buffer_data.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&vertex_buffer_desc, &vertex_buffer_data, &m_vertex_buffer))) {
        spdlog::error("[VR] Failed to create vertex buffer.");
        return false;
    }

    // Create the index buffer.
    D3D11_BUFFER_DESC index_buffer_desc{};
    index_buffer_desc.ByteWidth = sizeof(uint32_t) * 6;
    index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = 0;
    index_buffer_desc.MiscFlags = 0;

    // Quad (2 triangles)
    uint32_t indices[] = { 
        0, 1, 2, 
        2, 1, 3
    };

    D3D11_SUBRESOURCE_DATA index_buffer_data{};
    ZeroMemory(&index_buffer_data, sizeof(D3D11_SUBRESOURCE_DATA));
    index_buffer_data.pSysMem = indices;
    index_buffer_data.SysMemPitch = 0;
    index_buffer_data.SysMemSlicePitch = 0;

    if (FAILED(device->CreateBuffer(&index_buffer_desc, &index_buffer_data, &m_index_buffer))) {
        spdlog::error("[VR] Failed to create index buffer.");
        return false;
    }

    spdlog::info("[VR] D3D11 shaders initialized.");

    return true;
}

void D3D11Component::invoke_shader(uint32_t frame_count, uint32_t eye, uint32_t width, uint32_t height) {
    if (m_constant_buffer == nullptr) {
        spdlog::error("[VR] Constant buffer is null. Cannot invoke shader.");
        return;
    }

    if (m_vertex_buffer == nullptr) {
        spdlog::error("[VR] Vertex buffer is null. Cannot invoke shader.");
        return;
    }

    auto& hook = g_framework->get_d3d11_hook();
    auto& vr = VR::get();
    auto runtime = vr->get_runtime();

    auto device = hook->get_device();
    ComPtr<ID3D11DeviceContext> context{};

    device->GetImmediateContext(&context);

    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
    struct BACKUP_DX11_STATE {
        UINT                        ScissorRectsCount, ViewportsCount;
        D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState*      RS;
        ID3D11BlendState*           BlendState;
        FLOAT                       BlendFactor[4];
        UINT                        SampleMask;
        UINT                        StencilRef;
        ID3D11DepthStencilState*    DepthStencilState;
        ID3D11ShaderResourceView*   PSShaderResource;
        ID3D11SamplerState*         PSSampler;
        ID3D11PixelShader*          PS;
        ID3D11VertexShader*         VS;
        ID3D11GeometryShader*       GS;
        UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];   // 256 is max according to PSSetShader documentation
        D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
        ID3D11Buffer*               IndexBuffer, *VertexBuffer, *VSConstantBuffer;
        UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT                 IndexBufferFormat;
        ID3D11InputLayout*          InputLayout;
    };
    BACKUP_DX11_STATE old = {};
    old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    context->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
    context->RSGetViewports(&old.ViewportsCount, old.Viewports);
    context->RSGetState(&old.RS);
    context->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
    context->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
    context->PSGetShaderResources(0, 1, &old.PSShaderResource);
    context->PSGetSamplers(0, 1, &old.PSSampler);
    old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
    context->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
    context->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
    context->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
    context->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

    context->IAGetPrimitiveTopology(&old.PrimitiveTopology);
    context->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
    context->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
    context->IAGetInputLayout(&old.InputLayout);

    // Update the constant buffer.
    const auto glm_eye_transform_offset = vr->get_eye_transform(eye);
    auto glm_proj = vr->get_projection_matrix((VRRuntime::Eye)eye);

    const auto hmd_transform = vr->get_hmd_transform(frame_count);
    auto glm_view = hmd_transform * glm_eye_transform_offset;

    if (GetAsyncKeyState(VK_SPACE)) {
        const auto model_pos = hmd_transform[3] + (hmd_transform[2] * 5.0f);

        DirectX::XMVECTOR loc = DirectX::XMVectorSet(model_pos.x, model_pos.y, model_pos.z, 0);
        DirectX::XMVECTOR target = DirectX::XMVectorSet(hmd_transform[3].x, hmd_transform[3].y, hmd_transform[3].z, 0);
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
        m_shader_globals.model = DirectX::XMMatrixLookAtLH(loc, target, up);

    }

    //m_shader_globals.view = DirectX::XMMatrixTranspose(glm_view); // we can do this later, right now the shader transposes it
    m_shader_globals.view.r[0] = DirectX::XMVECTOR{glm_view[0].x, glm_view[0].y, glm_view[0].z, glm_view[0].w};
    m_shader_globals.view.r[1] = DirectX::XMVECTOR{glm_view[1].x, glm_view[1].y, glm_view[1].z, glm_view[1].w};
    m_shader_globals.view.r[2] = DirectX::XMVECTOR{glm_view[2].x, glm_view[2].y, glm_view[2].z, glm_view[2].w};
    m_shader_globals.view.r[3] = DirectX::XMVECTOR{glm_view[3].x, glm_view[3].y, glm_view[3].z, glm_view[3].w};
    //m_shader_globals.view = DirectX::XMMatrixTranspose(m_shader_globals.view);

    m_shader_globals.proj.r[0] = DirectX::XMVECTOR{glm_proj[0].x, glm_proj[0].y, glm_proj[0].z, glm_proj[0].w};
    m_shader_globals.proj.r[1] = DirectX::XMVECTOR{glm_proj[1].x, glm_proj[1].y, glm_proj[1].z, glm_proj[1].w};
    m_shader_globals.proj.r[2] = DirectX::XMVECTOR{glm_proj[2].x, glm_proj[2].y, glm_proj[2].z, glm_proj[2].w};
    m_shader_globals.proj.r[3] = DirectX::XMVECTOR{glm_proj[3].x, glm_proj[3].y, glm_proj[3].z, glm_proj[3].w};

    // Set everything to default values for now to make sure everything is working correctly.
    /*{
        DirectX::XMVECTOR loc = DirectX::XMVectorSet(0, 0, -5, 0);
        DirectX::XMVECTOR negloc = DirectX::XMVectorSet(0, 0, 5, 0);
		DirectX::XMVECTOR target = DirectX::XMVectorSet(0, 0, 0, 0);
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
		m_shader_globals.view = DirectX::XMMatrixLookAtLH(loc, target, up);

		DirectX::XMVECTOR model_pos = DirectX::XMVectorSet(0, 0, 0, 0);

		m_shader_globals.model = DirectX::XMMatrixLookAtLH(model_pos, negloc, up);
		m_shader_globals.proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, (float)width / (float)height, 0.01f, 100.0f);
    }*/

    m_shader_globals.resolution = DirectX::XMFLOAT4((float)width, (float)height, 0.0f, (float)width / (float)height);
    m_shader_globals.time += 0.01f;

    D3D11_MAPPED_SUBRESOURCE mapped_resource{};
    if (FAILED(context->Map(m_constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource))) {
        spdlog::error("[VR] Failed to map constant buffer.");
        return;
    }
    
    memcpy(mapped_resource.pData, &m_shader_globals, sizeof(D3D11Component::ShaderGlobals));
    context->Unmap(m_constant_buffer.Get(), 0);

    // Set the vertex input layout.
    context->IASetInputLayout(m_input_layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = sizeof(D3D11Component::Vertex);
    const UINT offset = 0;

    ID3D11Buffer* vertex_buffers[] = {m_vertex_buffer.Get()};
    context->IASetVertexBuffers(0, 1, vertex_buffers, &stride, &offset);
    
    ID3D11Buffer* index_buffers[] = {m_index_buffer.Get()};
    context->IASetIndexBuffer(m_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // Set the vertex and pixel shaders that will be used to render this triangle.
    context->VSSetShader(m_vs_shader.Get(), nullptr, 0);
    context->PSSetShader(m_ps_shader.Get(), nullptr, 0);

    // Set the constant buffer in the vertex shader with the updated values.
    ID3D11Buffer* constant_buffers[] = {m_constant_buffer.Get()};
    context->VSSetConstantBuffers(0, 1, constant_buffers);

    // Set the constant buffer in the pixel shader with the updated values.
    constant_buffers[0] = m_constant_buffer.Get();
    context->PSSetConstantBuffers(0, 1, constant_buffers);

    // Set the sampler state in the pixel shader.
    //context->PSSetSamplers(0, 1, &m_sampler_state);

    // Set the texture in the pixel shader.
    //ID3D11ShaderResourceView* srvs[] = {m_right_eye_srv.Get()};
    //context->PSSetShaderResources(0, 1, srvs);

    // Render the quad.
    context->DrawIndexed(6, 0, 0);

    // Restore modified DX state
    context->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
    context->RSSetViewports(old.ViewportsCount, old.Viewports);
    context->RSSetState(old.RS); if (old.RS) old.RS->Release();
    context->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();
    context->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();
    context->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();
    context->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();
    context->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
    for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
    context->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
    context->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
    context->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
    for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
    context->IASetPrimitiveTopology(old.PrimitiveTopology);
    context->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();
    context->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();
    context->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();
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
    if (auto err = create_swapchain((uint32_t)runtimes::OpenXR::SwapchainIndex::UI, g_framework->get_d3d11_rt_size().x, g_framework->get_d3d11_rt_size().y)) {
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
