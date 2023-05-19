#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <mutex>
#include <wrl.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <../../directxtk12-src/Inc/GraphicsMemory.h>
#include <../../directxtk12-src/Inc/SpriteBatch.h>
#include <../../directxtk12-src/Inc/DescriptorHeap.h>

class VR;

namespace vrmod {
class D3D12Component {
public:
    D3D12Component() 
        : m_openvr{this}
    {

    }

    vr::EVRCompositorError on_frame(VR* vr);
    void on_post_present(VR* vr);
    void on_reset(VR* vr);

    void force_reset() { m_force_reset = true; }

    const auto& get_backbuffer_size() const { return m_backbuffer_size; }

    auto is_initialized() const { return m_openvr.left_eye_tex[0].texture != nullptr; }

    auto& openxr() { return m_openxr; }
    auto& get_ui_tex() { return m_ui_tex; }

private:
    bool setup();
    void setup_sprite_batch_pso(DXGI_FORMAT output_format);
    void draw_spectator_view(ID3D12GraphicsCommandList* command_list, bool is_right_eye_frame);
    void clear_backbuffer();

    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct ResourceCopier {
        virtual ~ResourceCopier() { this->reset(); }

        bool setup(const wchar_t* name = L"ResourceCopier object");
        void reset();
        void wait(uint32_t ms);
        void copy(ID3D12Resource* src, ID3D12Resource* dst, 
            D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATES dst_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        void copy_region(ID3D12Resource* src, ID3D12Resource* dst, D3D12_BOX* src_box, 
            D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATES dst_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        void clear_rtv(ID3D12Resource* dst, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float* color, 
            D3D12_RESOURCE_STATES dst_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        void execute();

        ComPtr<ID3D12CommandAllocator> cmd_allocator{};
        ComPtr<ID3D12GraphicsCommandList> cmd_list{};
        ComPtr<ID3D12Fence> fence{};
        UINT64 fence_value{};
        HANDLE fence_event{};

        std::recursive_mutex mtx{};

        bool waiting_for_fence{false};
        bool has_commands{false};

        std::wstring internal_name{L"ResourceCopier object"};
    };

    ComPtr<ID3D12Resource> m_prev_backbuffer{};
    std::array<ResourceCopier, 3> m_generic_copiers{};

    struct TextureContext {
        ResourceCopier copier{};
        ComPtr<ID3D12Resource> texture{};
        std::unique_ptr<DirectX::DescriptorHeap> rtv_heap{};
        std::unique_ptr<DirectX::DescriptorHeap> srv_heap{};

        bool setup(ID3D12Device* device, ID3D12Resource* rsrc, std::optional<DXGI_FORMAT> rtv_format, std::optional<DXGI_FORMAT> srv_format, const wchar_t* name = L"TextureContext object") {
            reset();

            copier.setup(name);

            texture.Reset();
            texture = rsrc;

            if (rsrc == nullptr) {
                return false;
            }

            return create_rtv(device, rtv_format) && create_srv(device, srv_format);
        }

        bool create_rtv(ID3D12Device* device, std::optional<DXGI_FORMAT> format = std::nullopt) {
            rtv_heap.reset();

            // create descriptor heap
            try {
                rtv_heap = std::make_unique<DirectX::DescriptorHeap>(device,
                    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                    1);
            } catch(...) {
                spdlog::error("Failed to create RTV descriptor heap");
                return false;
            }

            if (rtv_heap->Heap() == nullptr) {
                return false;
            }

            if (format) {
                D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
                rtv_desc.Format = (DXGI_FORMAT)*format;
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = 0;
                rtv_desc.Texture2D.PlaneSlice = 0;
                device->CreateRenderTargetView(texture.Get(), &rtv_desc, get_rtv());
            } else {
                device->CreateRenderTargetView(texture.Get(), nullptr, get_rtv());
            }

            return true;
        }

        bool create_srv(ID3D12Device* device, std::optional<DXGI_FORMAT> format = std::nullopt) {
            srv_heap.reset();

            // create descriptor heap
            try {
                srv_heap = std::make_unique<DirectX::DescriptorHeap>(device,
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                    1);
            } catch(...) {
                spdlog::error("Failed to create SRV descriptor heap");
                return false;
            }

            if (srv_heap->Heap() == nullptr) {
                return false;
            }

            if (format) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = (DXGI_FORMAT)*format;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.Texture2D.MipLevels = 1;
                srv_desc.Texture2D.MostDetailedMip = 0;
                srv_desc.Texture2D.PlaneSlice = 0;
                srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
                device->CreateShaderResourceView(texture.Get(), &srv_desc, get_srv_cpu());
            } else {
                device->CreateShaderResourceView(texture.Get(), nullptr, get_srv_cpu());
            }

            return true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE get_rtv() {
            return rtv_heap->GetCpuHandle(0);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_srv_gpu() {
            return srv_heap->GetGpuHandle(0);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE get_srv_cpu() {
            return srv_heap->GetCpuHandle(0);
        }

        void reset() {
            copier.reset();
            rtv_heap.reset();
            srv_heap.reset();
            texture.Reset();
        }
    };

    TextureContext m_ui_tex{};
    TextureContext m_game_ui_tex{};
    TextureContext m_game_tex{};
    std::array<TextureContext, 3> m_backbuffer_textures{};

    std::unique_ptr<DirectX::DX12::GraphicsMemory> m_graphics_memory{};
    std::unique_ptr<DirectX::DX12::SpriteBatch> m_backbuffer_batch{};

    // Mimicking what OpenXR does.
    struct OpenVR {
        OpenVR(D3D12Component* p) : parent{p} {}
        
        TextureContext& get_left() {
            auto& ctx = this->left_eye_tex[this->texture_counter % left_eye_tex.size()];

            return ctx;
        }

        TextureContext& get_right() {
            auto& ctx = this->right_eye_tex[this->texture_counter % right_eye_tex.size()];

            return ctx;
        }

        TextureContext& acquire_left() {
            auto& ctx = get_left();
            ctx.copier.wait(INFINITE);

            return ctx;
        }

        TextureContext& acquire_right() {
            auto& ctx = get_right();
            ctx.copier.wait(INFINITE);

            return ctx;
        }

        void copy_left(ID3D12Resource* src, D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT) {
            auto& ctx = this->acquire_left();
            //ctx.copier.copy(src, ctx.texture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            // Copy the left half of the backbuffer to the left eye texture.
            D3D12_BOX src_box{};
            src_box.left = 0;
            src_box.top = 0;
            src_box.right = parent->m_backbuffer_size[0] / 2;
            src_box.bottom = parent->m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            ctx.copier.copy_region(src, ctx.texture.Get(), &src_box, src_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            ctx.copier.execute();
        }

        void copy_right(ID3D12Resource* src, D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT) {
            auto& ctx = this->acquire_right();
            //ctx.copier.copy(src, ctx.texture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            // Copy the right half of the backbuffer to the right eye texture.
            D3D12_BOX src_box{};
            src_box.left = parent->m_backbuffer_size[0] / 2;
            src_box.top = 0;
            src_box.right = parent->m_backbuffer_size[0];
            src_box.bottom = parent->m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            ctx.copier.copy_region(src, ctx.texture.Get(), &src_box, src_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            ctx.copier.execute();
        }
        
        // For AFR
        void copy_left_to_right(ID3D12Resource* src, D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT) {
            auto& ctx = this->acquire_right();
            //ctx.copier.copy(src, ctx.texture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            // Copy the right half of the backbuffer to the right eye texture.
            D3D12_BOX src_box{};
            src_box.left = 0;
            src_box.top = 0;
            src_box.right = parent->m_backbuffer_size[0] / 2;
            src_box.bottom = parent->m_backbuffer_size[1];
            src_box.front = 0;
            src_box.back = 1;
            ctx.copier.copy_region(src, ctx.texture.Get(), &src_box, src_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            ctx.copier.execute();
        }

        std::array<TextureContext, 3> left_eye_tex{};
        std::array<TextureContext, 3> right_eye_tex{};
        uint32_t texture_counter{0};
        D3D12Component* parent{};

        friend class D3D12Component;
    } m_openvr;

    struct OpenXR {
        void initialize(XrSessionCreateInfo& session_info);
        std::optional<std::string> create_swapchains();
        void destroy_swapchains();
        void copy(uint32_t swapchain_idx, ID3D12Resource* src, 
            std::optional<std::function<void(ResourceCopier&)>> additional_commands = std::nullopt,
            D3D12_RESOURCE_STATES src_state = D3D12_RESOURCE_STATE_PRESENT, D3D12_BOX* src_box = nullptr);
        void wait_for_all_copies() {
            std::scoped_lock _{this->mtx};

            for (auto& it : this->contexts) {
                for (auto& texture_ctx : it.second.texture_contexts) {
                    texture_ctx->copier.wait(INFINITE);
                }
            }
        }

        XrGraphicsBindingD3D12KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};

        struct SwapchainContext {
            std::vector<XrSwapchainImageD3D12KHR> textures{};
            std::vector<std::unique_ptr<D3D12Component::TextureContext>> texture_contexts{};
            uint32_t num_textures_acquired{0};
            uint32_t last_acquired_texture{0};
        };

        std::unordered_map<uint32_t, SwapchainContext> contexts{};
        std::recursive_mutex mtx{};
        std::array<uint32_t, 2> last_resolution{};
        bool made_depth_with_null_defaults{false};

        friend class D3D12Component;
    } m_openxr;

    uint32_t m_backbuffer_size[2]{};

    uint32_t m_last_rendered_frame{0};
    bool m_force_reset{false};
    bool m_last_afr_state{false};
    bool m_submitted_left_eye{false};
};
} // namespace vrmod