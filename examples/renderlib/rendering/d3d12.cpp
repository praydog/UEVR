#include "uevr/API.hpp"
#include "../imgui/imgui_impl_dx12.h"

#include "d3d12.hpp"

D3D12 g_d3d12{};

bool D3D12::initialize() {
    const auto renderer_data = uevr::API::get()->param()->renderer;
    auto device = (ID3D12Device*)renderer_data->device;
    auto swapchain = (IDXGISwapChain3*)renderer_data->swapchain;

    for (auto& cmd : this->cmds) {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd.allocator)))) {
            uevr::API::get()->log_error("[D3D12 plugin] Failed to create command allocator.");
            return false;
        }

        cmd.allocator->SetName(L"D3D12 renderlib command allocator");

        if (FAILED(device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd.allocator.Get(), nullptr, IID_PPV_ARGS(&cmd.list)))) 
        {
            uevr::API::get()->log_error("[D3D12 plugin] Failed to create command list.");
            return false;
        }

        cmd.list->SetName(L"D3D12 renderlib command list");

        if (FAILED(cmd.list->Close())) {
            uevr::API::get()->log_error("[D3D12 plugin] Failed to close command list.");
            return false;
        }

        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&cmd.fence)))) {
            uevr::API::get()->log_error("[D3D12 plugin] Failed to create fence.");
            return false;
        }

        cmd.fence->SetName(L"D3D12 renderlib fence");
        cmd.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};

        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = (int)D3D12::RTV::COUNT; 
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->rtv_desc_heap)))) {
            return false;
        }
    }
    
    { 
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = (int)D3D12::SRV::COUNT;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->srv_desc_heap)))) {
            return false;
        }
    }

    {
        for (auto i = 0; i <= (int)D3D12::RTV::BACKBUFFER_3; ++i) {
            if (SUCCEEDED(swapchain->GetBuffer(i, IID_PPV_ARGS(&this->rts[i])))) {
                device->CreateRenderTargetView(this->rts[i].Get(), nullptr, this->get_cpu_rtv(device, (D3D12::RTV)i));
            }
        }

        // Create our imgui and blank rts.
        auto& backbuffer = this->get_rt(D3D12::RTV::BACKBUFFER_0);
        auto desc = backbuffer->GetDesc();

        D3D12_HEAP_PROPERTIES props{};
        props.Type = D3D12_HEAP_TYPE_DEFAULT;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = desc.Format;

        if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
                IID_PPV_ARGS(&this->get_rt(D3D12::RTV::IMGUI))))) {
            return false;
        }

        if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
                IID_PPV_ARGS(&this->get_rt(D3D12::RTV::BLANK))))) {
            return false;
        }

        // Create imgui and blank rtvs and srvs.
        device->CreateRenderTargetView(this->get_rt(D3D12::RTV::IMGUI).Get(), nullptr, this->get_cpu_rtv(device, D3D12::RTV::IMGUI));
        device->CreateRenderTargetView(this->get_rt(D3D12::RTV::BLANK).Get(), nullptr, this->get_cpu_rtv(device, D3D12::RTV::BLANK));
        device->CreateShaderResourceView(
            this->get_rt(D3D12::RTV::IMGUI).Get(), nullptr, this->get_cpu_srv(device, D3D12::SRV::IMGUI));
        device->CreateShaderResourceView(this->get_rt(D3D12::RTV::BLANK).Get(), nullptr, this->get_cpu_srv(device, D3D12::SRV::BLANK));

        this->rt_width = (uint32_t)desc.Width;
        this->rt_height = (uint32_t)desc.Height;
    }

    auto& backbuffer = this->get_rt(D3D12::RTV::BACKBUFFER_0);
    auto desc = backbuffer->GetDesc();

    if (!ImGui_ImplDX12_Init(device, 1, desc.Format, this->srv_desc_heap.Get(),
        this->get_cpu_srv(device, D3D12::SRV::IMGUI_FONT), this->get_gpu_srv(device, D3D12::SRV::IMGUI_FONT))) 
    {
        return false;
    }

    return true;
}

void D3D12::render_imgui() {        
    auto& cmd = this->cmds[this->frame_count++ % this->cmds.size()];

    if (cmd.fence_event != nullptr && cmd.fence != nullptr && cmd.fence->GetCompletedValue() < cmd.fence_value) {
        WaitForSingleObject(cmd.fence_event, 2000);
        ResetEvent(cmd.fence_event);
    }

    const auto renderer_data = uevr::API::get()->param()->renderer;
    auto command_queue = (ID3D12CommandQueue*)renderer_data->command_queue;

    cmd.allocator->Reset();
    cmd.list->Reset(cmd.allocator.Get(), nullptr);

    auto device = (ID3D12Device*)renderer_data->device;
    float clear_color[]{0.0f, 0.0f, 0.0f, 0.0f};
    D3D12_CPU_DESCRIPTOR_HANDLE rts[1]{};

    // Draw to our render target.
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    
    /*barrier.Transition.pResource = this->get_rt(D3D12::RTV::IMGUI).Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    this->cmd_list->ResourceBarrier(1, &barrier);

    this->cmd_list->ClearRenderTargetView(this->get_cpu_rtv(device, D3D12::RTV::IMGUI), clear_color, 0, nullptr);
    rts[0] = this->get_cpu_rtv(device, D3D12::RTV::IMGUI);
    this->cmd_list->OMSetRenderTargets(1, rts, FALSE, NULL);
    this->cmd_list->SetDescriptorHeaps(1, this->srv_desc_heap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), this->cmd_list.Get());
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    this->cmd_list->ResourceBarrier(1, &barrier);*/

    // Draw to the back buffer.
    auto swapchain = (IDXGISwapChain3*)renderer_data->swapchain;
    auto bb_index = swapchain->GetCurrentBackBufferIndex();
    barrier.Transition.pResource = this->rts[bb_index].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmd.list->ResourceBarrier(1, &barrier);
    rts[0] = this->get_cpu_rtv(device, (D3D12::RTV)bb_index);
    cmd.list->OMSetRenderTargets(1, rts, FALSE, NULL);
    cmd.list->SetDescriptorHeaps(1, this->srv_desc_heap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd.list.Get());
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmd.list->ResourceBarrier(1, &barrier);

    if (FAILED(cmd.list->Close())) {
        uevr::API::get()->log_error("[D3D12 plugin] Failed to close command list.");
        return;
    }

    command_queue->ExecuteCommandLists(1, (ID3D12CommandList* const*)cmd.list.GetAddressOf());

    ID3D12CommandList* const cmd_lists[] = {cmd.list.Get()};
    command_queue->ExecuteCommandLists(1, cmd_lists);
    command_queue->Signal(cmd.fence.Get(), ++cmd.fence_value);
    cmd.fence->SetEventOnCompletion(cmd.fence_value, cmd.fence_event);
}

void D3D12::render_imgui_vr(ID3D12GraphicsCommandList* command_list, D3D12_CPU_DESCRIPTOR_HANDLE* rtv) {
    if (command_list == nullptr || rtv == nullptr) {
        return;
    }

    auto draw_data = ImGui::GetDrawData();

    if (draw_data == nullptr) {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rts[1]{};
    rts[0] = *rtv;
    command_list->OMSetRenderTargets(1, rts, FALSE, NULL);
    command_list->SetDescriptorHeaps(1, this->srv_desc_heap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(draw_data, command_list);
}
