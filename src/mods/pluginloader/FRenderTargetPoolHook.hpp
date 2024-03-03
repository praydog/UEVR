#pragma once

#include "uevr/API.h"

namespace uevr {
namespace render_target_pool_hook {
void activate();
UEVR_IPooledRenderTargetHandle get_render_target(const wchar_t* name);
extern UEVR_FRenderTargetPoolHookFunctions functions;
}
}