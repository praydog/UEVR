#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include <sdk/FRenderTargetPool.hpp>
#include <sdk/EngineModule.hpp>

#include "../VR.hpp"
#include "../../utility/Logging.hpp"
#include "RenderTargetPoolHook.hpp"

RenderTargetPoolHook* g_hook{nullptr};

RenderTargetPoolHook::RenderTargetPoolHook() {
    g_hook = this;
}

void RenderTargetPoolHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (!m_attempted_hook && VR::get()->is_depth_enabled()) {
        m_attempted_hook = true;
        m_hooked = hook();
    }
}

bool RenderTargetPoolHook::hook() {
    SPDLOG_INFO("Attempting to hook RenderTargetPool::FindFreeElement");

    const auto find_free_element = sdk::FRenderTargetPool::get_find_free_element_fn();

    if (!find_free_element) {
        SPDLOG_ERROR("Failed to find FRenderTargetPool::FindFreeElement, cannot hook");
        return false;
    }

    if (VR::get()->get_fake_stereo_hook()->has_double_precision()) {
        spdlog::error("Render target pool hook is temporarily disabled on UE5, sorry :(");
        return false;
    }

    SPDLOG_INFO("Performing hook...");

    m_find_free_element_hook = safetyhook::create_inline((void*)*find_free_element, find_free_element_hook);

    if (m_find_free_element_hook) {
        SPDLOG_INFO("Successfully hooked RenderTargetPool::FindFreeElement");
    } else {
        SPDLOG_ERROR("Failed to hook RenderTargetPool::FindFreeElement");
    }

    return true;
}

bool RenderTargetPoolHook::find_free_element_hook(
    sdk::FRenderTargetPool* pool, sdk::FRHICommandListBase* cmd_list,
    sdk::FPooledRenderTargetDesc* desc, TRefCountPtr<IPooledRenderTarget>* out,
    const wchar_t* name, 
    uintptr_t a6, uintptr_t a7, uintptr_t a8, uintptr_t a9, uintptr_t a10)
{
    SPDLOG_INFO_ONCE("FRenderTargetPool::FindFreeElement called for the first time!");

    const auto result = g_hook->m_find_free_element_hook.call<bool>(pool, cmd_list, desc, out, name, a6, a7, a8, a9, a10);

    SPDLOG_INFO_ONCE("Finished calling FRenderTargetPool::FindFreeElement!");

    // Right now we are only using this for depth
    // and on some games it will crash if we mess with anything
    // so, TODO: fix the games that crash with depth enabled
    if (!VR::get()->is_depth_enabled()) {
        std::scoped_lock _{g_hook->m_mutex};
        g_hook->m_render_targets.clear();
        return result;
    }

    if (name != nullptr) {
        //SPDLOG_INFO("FRenderTargetPool::FindFreeElement called with name {}", utility::narrow(name));

        if (out != nullptr) {
            std::scoped_lock _{g_hook->m_mutex};
            g_hook->m_render_targets[name] = out->reference;

            /*if (out->reference != nullptr && out->reference->item.texture.texture != nullptr) {
                const auto resource = out->reference->item.texture.texture->get_native_resource();
            }*/
        }
    }

    return result;
}