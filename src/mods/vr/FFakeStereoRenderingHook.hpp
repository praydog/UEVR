#pragma once

#include <memory>

#include <SafetyHook.hpp>

#include <utility/PointerHook.hpp>

#include "StereoStuff.hpp"

struct FRHICommandListImmediate;
struct VRRenderTargetManager_418;
struct FSceneView;
struct UCanvas;
struct IStereoLayers;

// Injector-specific structure for VRRenderTargetManager that they will all secondarily inherit from
// because different engine versions can have a different IStereoRenderTargetManager virtual table
// so we need a unified way of storing data that can be used for all versions
struct VRRenderTargetManager_Base {
public:
    bool allocate_render_target_texture(uintptr_t return_address, FTexture2DRHIRef* tex);

    uint32_t get_number_of_buffered_frames() const { return 1; }

    bool should_use_separate_render_target() const { return true; }

    void update_viewport(bool use_separate_rt, const FViewport& vp, class SViewport* vp_widget = nullptr);

    void calculate_render_target_size(const FViewport& viewport, uint32_t& x, uint32_t& y);
    bool need_reallocate_view_target(const FViewport& Viewport);

public:
    FRHITexture2D* get_render_target() { return render_target; }

private:
    FRHITexture2D* render_target{};
    static void pre_texture_hook_callback(safetyhook::Context& ctx); // only used if pixel format cvar is missing
    static void texture_hook_callback(safetyhook::Context& ctx);

    FTexture2DRHIRef* texture_hook_ref{nullptr};
    std::unique_ptr<safetyhook::MidHook> pre_texture_hook{}; // only used if pixel format cvar is missing
    std::unique_ptr<safetyhook::MidHook> texture_hook{};
    uint32_t last_texture_index{0};
    bool allocated_views{false};
    bool set_up_texture_hook{false};
};

struct VRRenderTargetManager : IStereoRenderTargetManager, VRRenderTargetManager_Base {
public:
    uint32_t GetNumberOfBufferedFrames() const override { return VRRenderTargetManager_Base::get_number_of_buffered_frames(); }
    virtual bool ShouldUseSeparateRenderTarget() const override { return VRRenderTargetManager_Base::should_use_separate_render_target(); }

    virtual void UpdateViewport(
        bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override 
    {
        VRRenderTargetManager_Base::update_viewport(bUseSeparateRenderTarget, Viewport, ViewportWidget);
    }

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override 
    {
        VRRenderTargetManager_Base::calculate_render_target_size(Viewport, InOutSizeX, InOutSizeY);
    }

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        return VRRenderTargetManager_Base::need_reallocate_view_target(Viewport);
    }

    // We will use this to keep track of the game-allocated render targets.
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) override;
};

struct VRRenderTargetManager_418 : IStereoRenderTargetManager_418, VRRenderTargetManager_Base {
    uint32_t GetNumberOfBufferedFrames() const override { return VRRenderTargetManager_Base::get_number_of_buffered_frames(); }
    virtual bool ShouldUseSeparateRenderTarget() const override { return VRRenderTargetManager_Base::should_use_separate_render_target(); }

    virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override {
        VRRenderTargetManager_Base::update_viewport(bUseSeparateRenderTarget, Viewport, ViewportWidget);
    }

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override {
        VRRenderTargetManager_Base::calculate_render_target_size(Viewport, InOutSizeX, InOutSizeY);
    }

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        return VRRenderTargetManager_Base::need_reallocate_view_target(Viewport);
    }

    // We will use this to keep track of the game-allocated render targets.
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags,
        uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples = 1) override;
};

class FFakeStereoRenderingHook {
public:
    FFakeStereoRenderingHook();

    VRRenderTargetManager* get_render_target_manager() { return &m_rtm; }
    bool has_pixel_format_cvar() const {
        return m_pixel_format_cvar_found;
    }

    void on_frame() {
        if (!m_injected_stereo_at_runtime) {
            attempt_runtime_inject_stereo();
            m_injected_stereo_at_runtime = true;
        }
    }

private:
    bool hook();
    std::optional<uintptr_t> locate_fake_stereo_rendering_constructor();
    std::optional<uintptr_t> locate_fake_stereo_rendering_vtable();
    std::optional<uint32_t> get_stereo_view_offset_index(uintptr_t vtable);

    bool patch_vtable_checks();
    bool attempt_runtime_inject_stereo();

    // Hooks
    static bool is_stereo_enabled(FFakeStereoRendering* stereo);
    static void adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h);
    static void calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator* view_rotation,
        const float world_to_meters, Vector3f* view_location);
    static Matrix4x4f* calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index);
    static void render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
        FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size);
    static void init_canvas(FFakeStereoRendering* stereo, FSceneView* view, UCanvas* canvas);

    static IStereoRenderTargetManager* get_render_target_manager_hook(FFakeStereoRendering* stereo);
    static IStereoLayers* get_stereo_layers_hook(FFakeStereoRendering* stereo);

    std::unique_ptr<safetyhook::InlineHook> m_adjust_view_rect_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_view_offset_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_projection_matrix_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_render_texture_render_thread_hook{};

    std::unique_ptr<PointerHook> m_is_stereo_enabled_hook{};
    std::unique_ptr<PointerHook> m_get_render_target_manager_hook{};
    std::unique_ptr<PointerHook> m_get_stereo_layers_hook{};
    std::unique_ptr<PointerHook> m_init_canvas_hook{};

    VRRenderTargetManager m_rtm{};
    VRRenderTargetManager_418 m_rtm_418{};

    bool m_418_detected{false};
    bool m_pixel_format_cvar_found{false};
    bool m_injected_stereo_at_runtime{false};
};