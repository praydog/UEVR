#pragma once

#include <cstdint>
#include <array>

#include <dxgi1_4.h>
#include <d3d12.h>
#include "shared.hpp"

struct D3D12 {
    struct Cmd {
        ComPtr<ID3D12CommandAllocator> allocator{};
        ComPtr<ID3D12GraphicsCommandList> list{};
        ComPtr<ID3D12Fence> fence{};
        UINT64 fence_value{};
        HANDLE fence_event{};
    };

    enum class RTV : int{
        BACKBUFFER_0,
        BACKBUFFER_1,
        BACKBUFFER_2,
        BACKBUFFER_3,
        IMGUI,
        BLANK,
        COUNT,
    };

    enum class SRV : int {
        IMGUI_FONT,
        IMGUI,
        BLANK,
        COUNT
    };

    void reset() {
        for (auto& cmd : cmds) {
            if (cmd.fence != nullptr && cmd.fence->GetCompletedValue() < cmd.fence_value) {
                WaitForSingleObject(cmd.fence_event, 2000);
                ResetEvent(cmd.fence_event);
            }

            cmd.fence_value = 0;
            cmd.fence_event = 0;
            cmd.allocator.Reset();
            cmd.list.Reset();
            cmd.fence.Reset();
        }
    }

    ~D3D12() { this->reset(); }

    std::array<Cmd, 3> cmds{};
    size_t frame_count{};

    ComPtr<ID3D12DescriptorHeap> rtv_desc_heap{};
    ComPtr<ID3D12DescriptorHeap> srv_desc_heap{};
    ComPtr<ID3D12Resource> rts[(int)RTV::COUNT]{};

    auto& get_rt(RTV rtv) { return rts[(int)rtv]; }

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_rtv(ID3D12Device* device, RTV rtv) {
        return {rtv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                (int)rtv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_srv(ID3D12Device* device, SRV srv) {
        return {srv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                (int)srv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_srv(ID3D12Device* device, SRV srv) {
        return {srv_desc_heap->GetGPUDescriptorHandleForHeapStart().ptr +
                (int)srv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
    }

    uint32_t rt_width{};
    uint32_t rt_height{};

    bool initialize();
    void render_imgui();
    void render_imgui_vr(ID3D12GraphicsCommandList* command_list, D3D12_CPU_DESCRIPTOR_HANDLE* rtv);
};

extern D3D12 g_d3d12;
