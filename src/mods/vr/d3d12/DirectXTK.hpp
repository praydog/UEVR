#pragma once

#include <../../directxtk12-src/Inc/SpriteBatch.h>
#include <array>
#include <optional>
#include <DirectXMath.h>

#include "TextureContext.hpp"

namespace d3d12 {
void render_srv_to_rtv(
    DirectX::DX12::SpriteBatch* sprite_batch,
    ID3D12GraphicsCommandList* command_list, 
    const d3d12::TextureContext& src, 
    const d3d12::TextureContext& dst,
    D3D12_RESOURCE_STATES src_state,
    D3D12_RESOURCE_STATES dst_state,
    std::optional<std::array<float, 4>> blend_factor = std::nullopt,
    std::optional<DirectX::XMFLOAT4> tint = std::nullopt);

void render_srv_to_rtv(
    DirectX::DX12::SpriteBatch* sprite_batch,
    ID3D12GraphicsCommandList* command_list, 
    const d3d12::TextureContext& src, 
    const d3d12::TextureContext& dst,
    std::optional<RECT> src_rect,
    D3D12_RESOURCE_STATES src_state,
    D3D12_RESOURCE_STATES dst_state,
    std::optional<std::array<float, 4>> blend_factor = std::nullopt,
    std::optional<DirectX::XMFLOAT4> tint = std::nullopt);

void render_srv_to_rtv(
    DirectX::DX12::SpriteBatch* sprite_batch,
    ID3D12GraphicsCommandList* command_list, 
    const d3d12::TextureContext& src, 
    const d3d12::TextureContext& dst,
    std::optional<RECT> src_rect,
    std::optional<RECT> dest_rect,
    D3D12_RESOURCE_STATES src_state,
    D3D12_RESOURCE_STATES dst_state,
    std::optional<std::array<float, 4>> blend_factor = std::nullopt,
    std::optional<DirectX::XMFLOAT4> tint = std::nullopt);
}