#pragma once

#include <memory>
#include <array>

#include <SafetyHook.hpp>

#include <utility/PointerHook.hpp>

#include <sdk/StereoStuff.hpp>
#include <sdk/FViewportInfo.hpp>

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
    bool allocate_render_target_texture(uintptr_t return_address, FTexture2DRHIRef* tex, FTexture2DRHIRef* shader_resource);

    uint32_t get_number_of_buffered_frames() const { return 1; }

    bool should_use_separate_render_target() const { return true; }

    void update_viewport(bool use_separate_rt, const FViewport& vp, class SViewport* vp_widget = nullptr);

    void calculate_render_target_size(const FViewport& viewport, uint32_t& x, uint32_t& y);
    bool need_reallocate_view_target(const FViewport& Viewport);

public:
    FRHITexture2D* get_ui_target() { return ui_target; }
    FRHITexture2D* get_render_target() { return render_target; }
    void set_render_target(FRHITexture2D* rt) { render_target = rt; }

protected:
    FRHITexture2D* ui_target{};
    FRHITexture2D* render_target{};
    static void pre_texture_hook_callback(safetyhook::Context& ctx); // only used if pixel format cvar is missing
    static void texture_hook_callback(safetyhook::Context& ctx);

    FTexture2DRHIRef* texture_hook_ref{nullptr};
    FTexture2DRHIRef* shader_resource_hook_ref{nullptr};
    std::unique_ptr<safetyhook::MidHook> pre_texture_hook{}; // only used if pixel format cvar is missing
    std::unique_ptr<safetyhook::MidHook> texture_hook{};
    uint32_t last_texture_index{0};
    bool allocated_views{false};
    bool set_up_texture_hook{false};
    bool is_pre_texture_call_e8{false};
    bool is_using_texture_desc{false};
    bool is_version_greq_5_1{false};

    uint32_t last_width{0};
    uint32_t last_height{0};

    std::vector<uint8_t> texture_create_insn_bytes{};
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

    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) override;
    virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) override; // Not actually used, we are just checking the return address
    virtual bool NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) override; // Not actually used, we are just checking the return address

    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override {
        return VRRenderTargetManager_Base::need_reallocate_view_target(Viewport);
    }

    // We will use this to keep track of the game-allocated render targets.
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) override;

public:
    uintptr_t m_last_calculate_render_size_return_address{0};
    uintptr_t m_last_needs_reallocate_depth_texture_return_address{0};
    uintptr_t m_last_allocate_render_target_return_address{0};
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

struct VRRenderTargetManager_Special : IStereoRenderTargetManager_Special, VRRenderTargetManager_Base {
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
    bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) override;
};

class FFakeStereoRenderingHook {
public:
    FFakeStereoRenderingHook();

    VRRenderTargetManager_Base* get_render_target_manager() {
        if (m_uses_old_rendertarget_manager) {
            return static_cast<VRRenderTargetManager_Base*>(&m_rtm_418);
        }

        if (m_special_detected) {
            return static_cast<VRRenderTargetManager_Base*>(&m_rtm_special);
        }

        return static_cast<VRRenderTargetManager_Base*>(&m_rtm);
    }

    /*void switch_to_old_rendertarget_manager() {
        m_uses_old_rendertarget_manager = true;
    }*/
    
    bool has_pixel_format_cvar() const {
        return m_pixel_format_cvar_found;
    }

    void attempt_hooking();
    void attempt_hook_game_engine_tick();
    void attempt_hook_slate_thread(uintptr_t return_address = 0);
    bool has_attempted_to_hook_slate() const {
        return m_attempted_hook_slate_thread;
    }

    bool is_slate_hooked() const {
        return m_hooked_slate_thread;
    }

    void on_frame() {
        attempt_hook_game_engine_tick();
        attempt_hook_slate_thread();

        // Ideally we want to do all hooking
        // from game engine tick. if it fails
        // we will fall back to doing it here.
        if (!m_hooked_game_engine_tick && m_attempted_hook_game_engine_tick) {
            attempt_hooking();
        }
    }

private:
    bool hook();
    bool standard_fake_stereo_hook(uintptr_t vtable);
    bool nonstandard_create_stereo_device_hook();
    
    static std::optional<uintptr_t> locate_fake_stereo_rendering_constructor();
    static std::optional<uintptr_t> locate_fake_stereo_rendering_vtable();
    static std::optional<uintptr_t> locate_active_stereo_rendering_device();
    static inline uintptr_t s_stereo_rendering_device_offset{0}; // GEngine

    std::optional<uint32_t> get_stereo_view_offset_index(uintptr_t vtable);

    bool patch_vtable_checks();
    bool attempt_runtime_inject_stereo();
    void post_init_properties(uintptr_t localplayer);

    // Hooks
    static bool is_stereo_enabled(FFakeStereoRendering* stereo);
    static void adjust_view_rect(FFakeStereoRendering* stereo, int32_t index, int* x, int* y, uint32_t* w, uint32_t* h);
    static void calculate_stereo_view_offset(FFakeStereoRendering* stereo, const int32_t view_index, Rotator<float>* view_rotation,
        const float world_to_meters, Vector3f* view_location);
    static Matrix4x4f* calculate_stereo_projection_matrix(FFakeStereoRendering* stereo, Matrix4x4f* out, const int32_t view_index);
    static void render_texture_render_thread(FFakeStereoRendering* stereo, FRHICommandListImmediate* rhi_command_list,
        FRHITexture2D* backbuffer, FRHITexture2D* src_texture, double window_size);
    static void init_canvas(FFakeStereoRendering* stereo, FSceneView* view, UCanvas* canvas);

    static IStereoRenderTargetManager* get_render_target_manager_hook(FFakeStereoRendering* stereo);
    static IStereoLayers* get_stereo_layers_hook(FFakeStereoRendering* stereo);

    static void post_calculate_stereo_projection_matrix(safetyhook::Context& ctx);
    static void pre_get_projection_data(safetyhook::Context& ctx);

    static void* slate_draw_window_render_thread(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info, 
                                                 void* elements, void* params, void* unk1, void* unk2);

    std::unique_ptr<safetyhook::InlineHook> m_tick_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_adjust_view_rect_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_view_offset_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_calculate_stereo_projection_matrix_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_render_texture_render_thread_hook{};
    std::unique_ptr<safetyhook::InlineHook> m_slate_thread_hook{};

    // both of these are used to figure out where the localplayer is, they aren't actively
    // used for anything else, the second one is an alternative hook if the first one
    // deems fruitless.
    std::unique_ptr<safetyhook::MidHook> m_calculate_stereo_projection_matrix_post_hook{};
    std::unique_ptr<safetyhook::MidHook> m_get_projection_data_pre_hook{};

    std::unique_ptr<PointerHook> m_is_stereo_enabled_hook{};
    std::unique_ptr<PointerHook> m_get_render_target_manager_hook{};
    std::unique_ptr<PointerHook> m_get_stereo_layers_hook{};
    std::unique_ptr<PointerHook> m_init_canvas_hook{};

    VRRenderTargetManager m_rtm{};
    VRRenderTargetManager_418 m_rtm_418{};
    VRRenderTargetManager_Special m_rtm_special{};

    std::vector<uintptr_t> m_projection_matrix_stack{};
    bool m_hooked_alternative_localplayer_scan{false};

    bool m_hooked{false};
    bool m_tried_hooking{false};
    bool m_finished_hooking{false};
    bool m_hooked_game_engine_tick{false};
    bool m_hooked_slate_thread{false};
    bool m_attempted_hook_game_engine_tick{false};
    bool m_attempted_hook_slate_thread{false};
    bool m_uses_old_rendertarget_manager{false};
    bool m_rendertarget_manager_embedded_in_stereo_device{false}; // 4.17 and below...?
    bool m_special_detected{false};
    bool m_pixel_format_cvar_found{false};
    bool m_injected_stereo_at_runtime{false};
    bool m_has_double_precision{false}; // for the projection matrix... AND the view offset... IS UE5 DOING THIS NOW???
    bool m_fixed_localplayer_view_count{false};

    /*FFakeStereoRendering m_stereo_recreation {
        90.0f, 
        (int32_t)1920, 
        (int32_t)1080, 
        (int32_t)2
    };*/

    struct FallbackDevice {
        void* vtable;
    } m_fallback_device;
    std::vector<void*> m_fallback_vtable{};

    // Seems to be the case in <= 4.17
    struct EmbeddedRenderTargetManagerInfo {
        std::unique_ptr<PointerHook> should_use_separate_render_target_hook{};
        std::unique_ptr<PointerHook> calculate_render_target_size_hook{};
        std::unique_ptr<PointerHook> allocate_render_target_texture_hook{};
    } m_embedded_rtm;
};