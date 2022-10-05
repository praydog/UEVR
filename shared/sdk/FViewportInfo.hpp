#pragma once

#include "StereoStuff.hpp"

namespace sdk {
struct FSlateResource {
    virtual FRHITexture2D* get_typed_resource() = 0;

    FRHITexture2D*& get_mutable_resource();
    static inline uint32_t resource_offset = sizeof(void*);
};

struct IViewportRenderTargetProvider {
    virtual FSlateResource* get_viewport_render_target_texture() = 0;
};

class FViewportInfo {
public:
    // The known_tex is used to bruteforce the offset for it.
    // This is one of those things that's hard to just make a pattern for,
    // so bruteforcing is the next best option.
    IViewportRenderTargetProvider* get_rt_provider(FRHITexture2D* known_tex);
};
}