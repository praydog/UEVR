#pragma once

#include <cstdint>
#include "Math.hpp"

enum ETextureCreateFlags {
    NONE = 0
};

enum EStereoscopicPass {
    eSSP_FULL,
    eSSP_PRIMARY,
    eSSP_SECONDARY
};

struct Rotator {
    float pitch, yaw, roll;
};

struct FRHIResource {
    virtual ~FRHIResource(){};
};

struct FRHITexture : FRHIResource {
	virtual struct FRHITexture2D* GetTexture2D() { return nullptr; }
	virtual struct FRHITexture2DArray* GetTexture2DArray() { return nullptr; }
	virtual struct FRHITexture3D* GetTexture3D() { return nullptr; }
	virtual struct FRHITextureCube* GetTextureCube() { return nullptr; }
	virtual struct FRHITextureReference* GetTextureReference() { return nullptr; }
    virtual glm::vec<2, int> GetSizeXYZ() const = 0;

    // Maybe bruteforce scan for this instead?
    // most D3D types should have RTTI so we can use that
    virtual void* GetNativeResource() const {
        return nullptr;
    }

    virtual void* GetNativeShaderResourceView() const {
        return nullptr;
    }

    // maybe some other virtuals we don't care about
};

struct FRHITexture2D : FRHITexture {
};

struct FTexture2DRHIRef {
    FRHITexture2D* texture; // we dont care about the reference counting utilities of this, so this is just essentially what this struct is.
};

struct FIntPoint {
    int x, y;
};

struct FViewport {

};

struct IStereoRenderTargetManager {
	virtual bool ShouldUseSeparateRenderTarget() const = 0;
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) = 0;
	virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
	virtual bool NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) { return false; }
	virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
	virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
	virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
	virtual bool AllocateShadingRateTexture(uint32_t Index, uint32_t RenderSizeX, uint32_t RenderSizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) { return false; }
};

struct IStereoRenderTargetManager_Special {
	virtual bool ShouldUseSeparateRenderTarget() const = 0;
	virtual bool UnkFunction() { return true; }
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) = 0;
	virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
	//virtual bool NeedReAllocateShadingRateTexture(const void* ShadingRateTarget) { return false; }
	virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
	virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
	virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
	virtual bool AllocateShadingRateTexture(uint32_t Index, uint32_t RenderSizeX, uint32_t RenderSizeY, uint8_t Format, uint32_t NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) { return false; }

	virtual bool pad1() { return false; }
	virtual bool pad2() { return false; }
	virtual bool pad3() { return false; }
	virtual bool pad4() { return false; }
	virtual bool pad5() { return false; }
};

struct IStereoRenderTargetManager_418 {
	virtual bool ShouldUseSeparateRenderTarget() const = 0;
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32_t& InOutSizeX, uint32_t& InOutSizeY) = 0;
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) = 0;
	virtual bool NeedReAllocateDepthTexture(const void* DepthTarget) { return false; }
	virtual uint32_t GetNumberOfBufferedFrames() const { return 1; }
	virtual bool AllocateRenderTargetTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags, uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
	virtual bool AllocateDepthTexture(uint32_t Index, uint32_t SizeX, uint32_t SizeY, uint8_t Format, uint32_t NumMips, uint32_t Flags, uint32_t TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32_t NumSamples = 1) { return false; }
};

struct FFakeStereoRendering {
    virtual ~FFakeStereoRendering(){};

    float fov;
    int32_t width, height;
    int32_t num_views;
};
