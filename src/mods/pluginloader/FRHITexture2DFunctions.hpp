#pragma once

#include "uevr/API.h"

namespace uevr {
namespace frhitexture2d {
void* get_native_resource(UEVR_FRHITexture2DHandle handle);

extern UEVR_FRHITexture2DFunctions functions;
}
}