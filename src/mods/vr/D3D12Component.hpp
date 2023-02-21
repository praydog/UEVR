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
    auto& get_blank_tex() { return m_blank_tex; }

private:
    bool setup();
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
        ComPtr<ID3D12DescriptorHeap> rtv_heap{};

        bool create_rtv(ID3D12Device* device, DXGI_FORMAT format) {
            rtv_heap.Reset();

            // create descriptor heap
            D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
            rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            rtv_heap_desc.NumDescriptors = 1;
            rtv_heap_desc.NodeMask = 1;

            if (!FAILED(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap))) && rtv_heap.Get() != nullptr) {
                D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
                rtv_desc.Format = (DXGI_FORMAT)format;
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = 0;
                rtv_desc.Texture2D.PlaneSlice = 0;
                device->CreateRenderTargetView(texture.Get(), &rtv_desc, get_rtv());
                return true;
            }

            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE get_rtv() {
            return D3D12_CPU_DESCRIPTOR_HANDLE{rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr};
        }

        void reset() {
            copier.reset();
            rtv_heap.Reset();
            texture.Reset();
        }
    };

    TextureContext m_ui_tex{};
    TextureContext m_blank_tex{};
    TextureContext m_game_ui_tex{};
    std::array<TextureContext, 3> m_backbuffer_textures{};

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
            struct TextureContext {
                ResourceCopier copier{};
            };

            std::vector<XrSwapchainImageD3D12KHR> textures{};
            std::vector<std::unique_ptr<TextureContext>> texture_contexts{};
            uint32_t num_textures_acquired{0};
        };

        std::unordered_map<uint32_t, SwapchainContext> contexts{};
        std::recursive_mutex mtx{};
        std::array<uint32_t, 2> last_resolution{};
    } m_openxr;

    uint32_t m_backbuffer_size[2]{};

    uint32_t m_last_rendered_frame{0};
    bool m_force_reset{false};
    bool m_last_afr_state{false};
    bool m_submitted_left_eye{false};
};
} // namespace vrmod