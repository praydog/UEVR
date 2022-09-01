#pragma once

#include <memory>
#include <SafetyHook.hpp>

#include <utility/PointerHook.hpp>

#include "StereoStuff.hpp"

struct FRHICommandListImmediate;

struct VRRenderTargetManager : IStereoRenderTargetManager {
    bool allocated_views{false};

    // This seems a bit too complex right now,
    // if anyone can figure this out just make it return true and it will execute the
    // other virtuals in here to allocate the view targets
    virtual bool ShouldUseSeparateRenderTarget() const override {
        return false;
    }

    virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override {
        // do nothing
    }

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override {
        // do nothing
    }

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        // TODO: check if we need to reallocate
        const auto ret = !allocated_views;

        allocated_views = true;

        return ret;
    }
};

class FFakeStereoRenderingHook {
public:
    FFakeStereoRenderingHook();

    static void calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator& view_rotation, const float world_to_meters, Vector3f& view_location);
    static Matrix4x4f* calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f& out, const int32_t view_index);
    static void render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list, FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size);

    static IStereoRenderTargetManager* get_render_target_manager(FFakeStereoRendering* stereo);

private:
    bool hook();
    std::optional<uintptr_t> locate_fake_stereo_rendering_vtable();
    std::optional<uint32_t> get_stereo_view_offset_index(uintptr_t vtable);

    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_view_offset_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_projection_matrix_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_render_texture_render_thread_hook{};

    std::unique_ptr<PointerHook> m_get_render_target_manager_hook{};

    // Does nothing at the moment.
    VRRenderTargetManager m_rtm{};
};