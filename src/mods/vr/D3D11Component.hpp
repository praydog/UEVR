#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <openvr.h>
#include <wrl.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <DirectXMath.h>
#include <SpriteBatch.h>

class VR;

namespace vrmod {
class D3D11Component {
public:
    D3D11Component() 
        : m_openxr{this}
    {

    }

    vr::EVRCompositorError on_frame(VR* vr);
    void on_post_present(VR* vr);
    void on_reset(VR* vr);

    auto& openxr() { return m_openxr; }

    auto& get_ui_tex() { return m_ui_tex; }

    bool clear_tex(ID3D11Resource* rsrc, std::optional<DXGI_FORMAT> format = std::nullopt);
    bool clear_tex(ID3D11Resource* rsrc, float* color, std::optional<DXGI_FORMAT> format = std::nullopt);
    void copy_tex(ID3D11Resource* src, ID3D11Resource* dst);

    void force_reset() { m_force_reset = true; }

private:
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    struct TextureContext;

    void render_srv_to_rtv(
        DirectX::DX11::SpriteBatch* batch,
        TextureContext& srv,
        TextureContext& rtv,
        float w, float h);

    void render_srv_to_rtv(
        DirectX::DX11::SpriteBatch* batch,
        TextureContext& srv,
        TextureContext& rtv);

    void render_srv_to_rtv(
        DirectX::DX11::SpriteBatch* batch,
        TextureContext& srv,
        TextureContext& rtv,
        const RECT& src_rect);

    bool ensure_ui_invert_resources();
    void render_ui_invert_to_rt(ID3D11Texture2D* render_target, TextureContext& srv, float invert_amount);

    struct ShaderGlobals {
        DirectX::XMMATRIX mvp{};
		DirectX::XMFLOAT4 resolution{};
        float time{};
    } m_shader_globals{};

    glm::mat4 m_model_mat{glm::identity<glm::mat4>()};

    struct Vertex {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 color{};
        DirectX::XMFLOAT2 texuv0{};
        DirectX::XMFLOAT2 texuv1{};
        DirectX::XMFLOAT3 normal{};
        DirectX::XMFLOAT3 tangent{};
        DirectX::XMFLOAT3 bitangent{};
	};

    struct TextureContext {
        ComPtr<ID3D11Resource> tex{};
        ComPtr<ID3D11RenderTargetView> rtv{};
        ComPtr<ID3D11ShaderResourceView> srv{};

        TextureContext(ID3D11Resource* in_tex, std::optional<DXGI_FORMAT> rtv_format = std::nullopt, std::optional<DXGI_FORMAT> srv_format = std::nullopt);
        TextureContext() = default;

        virtual ~TextureContext() {
            reset();
        }

        bool set(ID3D11Resource* in_tex, std::optional<DXGI_FORMAT> rtv_format = std::nullopt, std::optional<DXGI_FORMAT> srv_format = std::nullopt);
        bool clear_rtv(float* color);

        void reset() {
            tex.Reset();
            rtv.Reset();
            srv.Reset();
        }

        bool has_texture() const {
            return tex != nullptr;
        }

        bool has_rtv() const {
            return rtv != nullptr;
        }

        bool has_srv() const {
            return srv != nullptr;
        }

        operator bool() const {
            return tex != nullptr;
        }

        operator ID3D11Resource*() const {
            return tex.Get();
        }

        operator ID3D11Texture2D*() const {
            return (ID3D11Texture2D*)tex.Get();
        }

        operator ID3D11RenderTargetView*() const {
            return rtv.Get();
        }

        operator ID3D11ShaderResourceView*() const {
            return srv.Get();
        }
    };

    ComPtr<ID3D11Texture2D> m_ui_tex{};
    TextureContext m_engine_ui_ref{};
    TextureContext m_engine_tex_ref{};
    TextureContext m_scene_capture_tex_ref{};
    std::array<TextureContext, 2> m_2d_screen_tex{};
    ComPtr<ID3D11Texture2D> m_left_eye_tex{};
    ComPtr<ID3D11Texture2D> m_right_eye_tex{};
    ComPtr<ID3DBlob> m_vs_shader_blob{};
    ComPtr<ID3DBlob> m_ps_shader_blob{};

    ComPtr<ID3D11VertexShader> m_vs_shader{};
    ComPtr<ID3D11PixelShader> m_ps_shader{};
    ComPtr<ID3D11InputLayout> m_input_layout{};
    ComPtr<ID3D11Buffer> m_constant_buffer{};
    ComPtr<ID3D11Buffer> m_vertex_buffer{};
    ComPtr<ID3D11Buffer> m_index_buffer{};
    ComPtr<ID3D11RenderTargetView> m_left_eye_rtv{};
    ComPtr<ID3D11RenderTargetView> m_right_eye_rtv{};
    ComPtr<ID3D11ShaderResourceView> m_left_eye_srv{};
    ComPtr<ID3D11ShaderResourceView> m_right_eye_srv{};

    std::unique_ptr<DirectX::DX11::SpriteBatch> m_backbuffer_batch{};
    std::unique_ptr<DirectX::DX11::SpriteBatch> m_game_batch{};
    ComPtr<ID3D11PixelShader> m_ui_invert_ps{};
    ComPtr<ID3D11BlendState> m_ui_invert_blend{};
    bool m_ui_invert_ready{false};

    vr::HmdMatrix44_t m_left_eye_proj{};
    vr::HmdMatrix44_t m_right_eye_proj{};

    ComPtr<ID3D11Texture2D> m_backbuffer{};
    ComPtr<ID3D11RenderTargetView> m_backbuffer_rtv{};
    ComPtr<ID3D11Texture2D> m_spectator_view_backbuffer{};
    ComPtr<ID3D11Texture2D> m_extreme_compat_backbuffer{};
    ComPtr<ID3D11Texture2D> m_converted_backbuffer{};
    TextureContext m_extreme_compat_backbuffer_ctx{};
    std::array<uint32_t, 2> m_backbuffer_size{};
    std::array<uint32_t, 2> m_real_backbuffer_size{};

    ID3D11Texture2D* m_last_checked_native{nullptr};

    uint32_t m_last_rendered_frame{0};
    bool m_force_reset{true};
    bool m_submitted_left_eye{false};
    bool m_is_shader_setup{false};
    bool m_last_afr_state{false};

    struct OpenXR {
        OpenXR(D3D11Component* p) : parent(p) {}

        void initialize(XrSessionCreateInfo& session_info);
        std::optional<std::string> create_swapchains();
        void destroy_swapchains();
        void copy(uint32_t swapchain_idx, ID3D11Texture2D* resource, D3D11_BOX* src_box = nullptr, std::function<void(ID3D11Texture2D*)> pre_commands = nullptr);

        bool ever_acquired(uint32_t swapchain_idx) {
            std::scoped_lock _{this->mtx};

            auto it = this->contexts.find(swapchain_idx);
            if (it == this->contexts.end()) {
                return false;
            }

            return it->second.ever_acquired;
        }

        XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};

        struct SwapchainContext {
            std::vector<XrSwapchainImageD3D11KHR> textures{};
            uint32_t num_textures_acquired{0};
            bool ever_acquired{false};
        };

        std::unordered_map<uint32_t, SwapchainContext> contexts{};
        std::recursive_mutex mtx{};
        std::array<uint32_t, 2> last_resolution{};
        bool made_depth_with_null_defaults{false};

        D3D11Component* parent{};
        friend class D3D11Component;
    } m_openxr;

    bool setup();
    bool setup_shader();
    void invoke_shader(uint32_t frame_count, uint32_t eye, uint32_t width, uint32_t height);
};
} // namespace vrmod
