#pragma once

#include <memory>

#include "StereoStuff.hpp"

namespace sdk {
struct FCreateInfo {
    void* bulk_data{nullptr};
    void* rsrc_array{nullptr};

    struct {
        uint32_t color_binding{1};
        float color[4]{};
    } clear_value_binding;

    uint32_t gpu_mask{1};
    bool without_native_rsrc{false};
    const char* debug_name{"BufferedRT"};
    uint32_t extended_data{};
};

class FDynamicRHI {
public:
    static FDynamicRHI* get();

public:
    std::unique_ptr<FTexture2DRHIRef> create_texture_2d(
        uintptr_t command_list,
        uint32_t w,
        uint32_t h,
        uint8_t format,
        uint32_t mips,
        uint32_t samples,
        ETextureCreateFlags flags,
        FCreateInfo& create_info);
};
}