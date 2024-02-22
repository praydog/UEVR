#include <sdk/StereoStuff.hpp>

#include "FRHITexture2DFunctions.hpp"

namespace uevr {
namespace frhitexture2d {
void* get_native_resource(UEVR_FRHITexture2DHandle handle) {
    const auto texture = (::FRHITexture2D*)handle;

    if (texture == nullptr) {
        return nullptr;
    }

    return texture->get_native_resource();
}

UEVR_FRHITexture2DFunctions functions {
    &uevr::frhitexture2d::get_native_resource
};
}
}