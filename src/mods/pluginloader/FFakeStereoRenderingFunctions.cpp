#include "uevr/API.h"

#include "mods/VR.hpp"
#include "FFakeStereoRenderingFunctions.hpp"

namespace uevr {
UEVR_FRHITexture2DHandle stereo_hook::get_scene_render_target() {
    const auto& vr = VR::get();
    if (auto& hook = vr->get_fake_stereo_hook(); hook != nullptr) {
        auto rtm = hook->get_render_target_manager();
        if (auto rtm = hook->get_render_target_manager(); rtm != nullptr) {
            return (UEVR_FRHITexture2DHandle)rtm->get_render_target();
        }
    }

    return nullptr;
}

UEVR_FRHITexture2DHandle stereo_hook::get_ui_render_target() {
    const auto& vr = VR::get();
    if (auto& hook = vr->get_fake_stereo_hook(); hook != nullptr) {
        auto rtm = hook->get_render_target_manager();
        if (auto rtm = hook->get_render_target_manager(); rtm != nullptr) {
            return (UEVR_FRHITexture2DHandle)rtm->get_ui_target();
        }
    }

    return nullptr;
}

UEVR_FFakeStereoRenderingHookFunctions stereo_hook::functions {
    .get_scene_render_target = &stereo_hook::get_scene_render_target,
    .get_ui_render_target = &stereo_hook::get_ui_render_target
};
}