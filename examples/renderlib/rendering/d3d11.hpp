#pragma once

#include <d3d11.h>
#include "shared.hpp"

struct D3D11 {
    ComPtr<ID3D11Texture2D> rt{};
    ComPtr<ID3D11RenderTargetView> rt_rtv{};
    ComPtr<ID3D11ShaderResourceView> rt_srv{};
    uint32_t rt_width{};
    uint32_t rt_height{};
    ComPtr<ID3D11RenderTargetView> bb_rtv{};

    bool initialize();
    void render_imgui();
    void render_imgui_vr(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);
};

extern D3D11 g_d3d11;