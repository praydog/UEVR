#pragma once

#include <atomic>
#include <cstdint>

#include "Math.hpp"

enum ETextureCreateFlags : uint64_t { RenderTargetable = 1ull << 0, ResolveTargetable = 1ull << 1, ShaderResource = 1ull << 3, };
enum EStereoscopicPass { eSSP_FULL, eSSP_PRIMARY, eSSP_SECONDARY };

template<typename T>
struct Rotator {
    T pitch, yaw, roll;
};

template<typename T>
struct Quat {
    T x, y, z, w;
};

struct FRHIResource {
    virtual ~FRHIResource(){};

    void add_ref() {
        ++ref_count;
    }

    void release() {
        --ref_count;

        // how 2 delete?
    }

    // This is how it is internally
    // but hopefully different compiler versions
    // don't change anything here...
    // there are also various ifdefs that can change the layout here...
    // maybe we just won't care?
    std::atomic<int32_t> ref_count{ 0 }; // ifdefs can change it to be stored in 30 bits.
    std::atomic<bool> marked_for_delete{}; // not the case if ifdefs are not met
};

struct FRHITexture : FRHIResource {
    void* get_native_resource() const;
};

struct FRHITexture2D : FRHITexture {};

struct FTexture2DRHIRef {
    FTexture2DRHIRef() = default;
    FTexture2DRHIRef(FRHITexture2D* tex) 
        : texture{tex}
    {
        this->texture->add_ref();
    }

    FTexture2DRHIRef(FRHITexture2D& tex)
        : texture{&tex}
    {
        this->texture->add_ref();
    }

    ~FTexture2DRHIRef() {
        if (this->texture != nullptr) {
            this->texture->release();
        }
    }

    FTexture2DRHIRef& operator=(FRHITexture2D* tex) {
        this->texture = tex;
        return *this;
    }

    FRHITexture2D* texture{nullptr};
};

struct FIntPoint {
    int32_t x, y;
};

struct FViewport {};

// 4.24+ or so only has the NeedReAllocateShadingRateTexture function.
struct IStereoRenderTargetManager {
    virtual bool ShouldUseSeparateRenderTarget() const = 0;
    virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) = 0;
    virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
    virtual bool NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) { return false; }
    virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
    virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) {
        return false;
    }

    // On 5.1 there is a GetActualColorSwapchainFormat virtual here
    // however it might not be necessary to add it because we don't actually
    // use the below two functions.
    
    virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) {
        return false;
    }
    virtual bool AllocateShadingRateTexture(uint32_t Index, uint32_t RenderSizeX, uint32_t RenderSizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) {
        return false;
    }

    // additional padding for unknown functions (necessary for 5.1+)
    virtual bool pad1() { return false; }
    virtual bool pad2() { return false; }
    virtual bool pad3() { return false; }
    virtual bool pad4() { return false; }
    virtual bool pad5() { return false; }
};

struct IStereoRenderTargetManager_Special {
    virtual bool ShouldUseSeparateRenderTarget() const = 0;
    virtual bool UnkFunction() { return true; }
    virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
    virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) = 0;
    virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
    // virtual bool NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) { return false; }
    virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
    virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) {
        return false;
    }
    virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture,
        FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) {
        return false;
    }
    virtual bool AllocateShadingRateTexture(uint32_t Index, uint32_t RenderSizeX, uint32_t RenderSizeY, uint8_t Format, uint32_t NumMips,
        ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) {
        return false;
    }

    virtual bool pad1() { return false; }
    virtual bool pad2() { return false; }
    virtual bool pad3() { return false; }
    virtual bool pad4() { return false; }
    virtual bool pad5() { return false; }
};

// says 418 but its actually 4.23 or 4.24 or so and below.
struct IStereoRenderTargetManager_418 {
    virtual bool ShouldUseSeparateRenderTarget() const = 0;
    virtual void UpdateViewport(
        bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
    virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
    virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) = 0;
    virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
    virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
    virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips,
        uint32_t Flags, uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples = 1) {
        return false;
    }
    virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags,
        uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture,
        uint32_t NumSamples = 1) {
        return false;
    }

    virtual bool pad1() { return false; }
    virtual bool pad2() { return false; }
    virtual bool pad3() { return false; }
    virtual bool pad4() { return false; }
    virtual bool pad5() { return false; }
};

struct FFakeStereoRendering {
    virtual ~FFakeStereoRendering(){};

    float fov;
    int32_t width, height;
    int32_t num_views;
};


struct IRefCountedObject {
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
    virtual uint32_t GetRefCount() = 0;
};

struct FSceneRenderTargetItem {
    FTexture2DRHIRef texture;
    FTexture2DRHIRef srt;
    void** uav;
    char pad[0x20];
};

struct IPooledRenderTarget : public IRefCountedObject {
    virtual bool IsFree() = 0;
    virtual void* GetDesc() = 0;

    FSceneRenderTargetItem item;
};

template<typename ReferencedType>
struct TRefCountPtr {
    // todo: actually implement this
    ReferencedType* reference;
};

template<typename Mode>
struct FWeakReferencer {
    struct FReferenceControllerBase {
        virtual void DestroyObject() {};
        virtual ~FReferenceControllerBase() {}

        int32_t shared_reference_count{1};
        int32_t weak_reference_count{1};

        uint64_t poopdata{};
    };

    void allocate_naive() {
        controller = new FReferenceControllerBase{};
    }

    FReferenceControllerBase* controller{};
};

template<typename ReferencedType>
struct TWeakPtr {
    void allocate_naive() {
        counter.allocate_naive();
        reference = new ReferencedType{};
    }

    ReferencedType* reference;
    FWeakReferencer<int> counter;
};

struct ISceneViewExtension {
    virtual ~ISceneViewExtension() {}

    virtual bool dummy_1() { return false; }
    virtual bool dummy_2() { return false; }
    virtual bool dummy_3() { return false; }
    virtual bool dummy_4() { return false; }
    virtual bool dummy_5() { return false; }
    virtual bool dummy_6() { return false; }
    virtual bool dummy_7() { return false; }
    virtual bool dummy_8() { return false; }
    virtual bool dummy_9() { return false; }
    virtual bool dummy_10() { return false; }
    virtual bool dummy_11() { return false; }
    virtual bool dummy_12() { return false; }
    virtual bool dummy_13() { return false; }
    virtual bool dummy_14() { return false; }
    virtual bool dummy_15() { return false; }
    virtual bool dummy_16() { return false; }
    virtual bool dummy_17() { return false; }
    virtual bool dummy_18() { return false; }
    virtual bool dummy_19() { return false; }
    virtual bool dummy_20() { return false; }
    virtual bool dummy_21() { return false; }
    virtual bool dummy_22() { return false; }
    virtual bool dummy_23() { return false; }
    virtual bool dummy_24() { return false; }
    virtual bool dummy_25() { return false; }
};

template<typename T>
struct TArray {
    T* data{nullptr};
    int32_t count{0};
    int32_t capacity{0};
};

struct FSceneViewExtensions {
    TArray<TWeakPtr<ISceneViewExtension>> extensions;
};

struct FSceneView {

};

struct FSceneViewFamily {
    TArray<FSceneView*> views{};
    char padding[0x100];
};