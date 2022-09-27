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

class VR;

namespace vrmod {
class D3D11Component {
public:
    vr::EVRCompositorError on_frame(VR* vr);
    void on_reset(VR* vr);

    auto& openxr() { return m_openxr; }

    auto& get_ui_tex() { return m_ui_tex; }
    auto& get_blank_tex() { return m_blank_tex; }

    void clear_tex(ID3D11Resource* rsrc);
    void copy_tex(ID3D11Resource* src, ID3D11Resource* dst);

private:
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D11Texture2D> m_ui_tex{};
    ComPtr<ID3D11Texture2D> m_blank_tex{};
    ComPtr<ID3D11Texture2D> m_left_eye_tex{};
    ComPtr<ID3D11Texture2D> m_right_eye_tex{};
    ComPtr<ID3D11Texture2D> m_left_eye_depthstencil{};
    ComPtr<ID3D11Texture2D> m_right_eye_depthstencil{};
    vr::HmdMatrix44_t m_left_eye_proj{};
    vr::HmdMatrix44_t m_right_eye_proj{};

    std::array<ComPtr<ID3D11Texture2D>, 2> m_backbuffer_references{};

    struct OpenXR {
        void initialize(XrSessionCreateInfo& session_info);
        std::optional<std::string> create_swapchains();
        void destroy_swapchains();
        void copy(uint32_t swapchain_idx, ID3D11Texture2D* resource);

        XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};

        struct SwapchainContext {
            std::vector<XrSwapchainImageD3D11KHR> textures{};
            uint32_t num_textures_acquired{0};
        };

        std::vector<SwapchainContext> contexts{};
        std::recursive_mutex mtx{};
        std::array<uint32_t, 2> last_resolution{};
    } m_openxr;

    void setup();
};
} // namespace vrmod
