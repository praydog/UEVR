#pragma once

#include <memory>

#include <SafetyHook.hpp>

#include <utility/PointerHook.hpp>

#include "StereoStuff.hpp"

struct FRHICommandListImmediate;
struct VRRenderTargetManager_418;

struct VRRenderTargetManager : IStereoRenderTargetManager {
    uint32_t GetNumberOfBufferedFrames() const override { return 1; }

    // This seems a bit too complex right now,
    // if anyone can figure this out just make it return true and it will execute the
    // other virtuals in here to allocate the view targets
    virtual bool ShouldUseSeparateRenderTarget() const override { return true; }

    virtual void UpdateViewport(
        bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override {
        // do nothing
    }

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override;

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        // TODO: check if we need to reallocate
        /*const auto ret = !allocated_views;

        allocated_views = true;

        return ret;*/

        return false;
    }

    // We will use this to keep track of the game-allocated render targets.
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) override;

public:
    FRHITexture2D* get_render_target() { return render_target; }

private:
    FRHITexture2D* render_target{};
    static void texture_hook_callback(safetyhook::Context& ctx);

    FTexture2DRHIRef* texture_hook_ref{nullptr};
    std::unique_ptr<safetyhook::MidHook> texture_hook{};
    uint32_t last_texture_index{0};
    bool allocated_views{false};
    bool set_up_texture_hook{false};
};

struct VRRenderTargetManager_418 : IStereoRenderTargetManager_418 {
    uint32_t GetNumberOfBufferedFrames() const override { return 1; }

    // This seems a bit too complex right now,
    // if anyone can figure this out just make it return true and it will execute the
    // other virtuals in here to allocate the view targets
    virtual bool ShouldUseSeparateRenderTarget() const override { return true; }

    virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override {
        // do nothing
    }

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override;

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        // TODO: check if we need to reallocate
        /*const auto ret = !allocated_views;

        allocated_views = true;

        return ret;*/

        return false;
    }

    // We will use this to keep track of the game-allocated render targets.
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags,
        uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples = 1);
};

class FFakeStereoRenderingHook {
public:
    FFakeStereoRenderingHook();

    VRRenderTargetManager* get_render_target_manager() { return &m_rtm; }

private:
    bool hook();
    std::optional<uintptr_t> locate_fake_stereo_rendering_vtable();
    std::optional<uint32_t> get_stereo_view_offset_index(uintptr_t vtable);

    // Hooks
    static bool is_stereo_enabled(FFakeStereoRendering* stereo);
    static void adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h);
    static void calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator* view_rotation,
        const float world_to_meters, Vector3f* view_location);
    static Matrix4x4f* calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index);
    static void render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
        FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size);

    static IStereoRenderTargetManager* get_render_target_manager_hook(FFakeStereoRendering* stereo);

    std::unique_ptr<safetyhook::InlineHook> m_adjust_view_rect_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_view_offset_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_projection_matrix_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_render_texture_render_thread_hook{};

    std::unique_ptr<PointerHook> m_is_stereo_enabled_hook{};
    std::unique_ptr<PointerHook> m_get_render_target_manager_hook{};

    // Does nothing at the moment.
    VRRenderTargetManager m_rtm{};
};