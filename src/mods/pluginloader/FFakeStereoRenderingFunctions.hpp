#pragma once

#include "uevr/API.h"

namespace uevr {
namespace stereo_hook {
UEVR_FRHITexture2DHandle get_scene_render_target();
UEVR_FRHITexture2DHandle get_ui_render_target();

extern UEVR_FFakeStereoRenderingHookFunctions functions;
}
}