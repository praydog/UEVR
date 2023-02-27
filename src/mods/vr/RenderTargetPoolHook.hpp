#pragma once

#include <mutex>
#include <wrl.h>

#include "../../Mod.hpp"

#include <SafetyHook.hpp>

#include <sdk/RHICommandList.hpp>
#include <sdk/StereoStuff.hpp>

namespace sdk {
class FRenderTargetPool;
class FPooledRenderTargetDesc;
}

class RenderTargetPoolHook : public ModComponent {
public:
    RenderTargetPoolHook();
    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;

    IPooledRenderTarget* get_render_target(const std::wstring& name) {
        std::scoped_lock _{m_mutex};
        if (auto it = m_render_targets.find(name); it != m_render_targets.end()) {
            return it->second;
        }

        return nullptr;
    }

    template<typename T>
    Microsoft::WRL::ComPtr<T> get_texture(const std::wstring& name) {
        std::scoped_lock _{m_mutex};
        if (auto it = m_render_targets.find(name); it != m_render_targets.end()) {
            const auto& rt = it->second;
            const auto& tex = rt->item.texture.texture;

            if (tex == nullptr) {
                return nullptr;
            }

            auto native_resource = (T*)tex->get_native_resource();

            if (native_resource == nullptr) {
                return nullptr;
            }

            return native_resource;
        }

        return nullptr;
    }

private:
    bool hook();

    // Stuff past name param is added in newer UE versions.
    static bool find_free_element_hook(
        sdk::FRenderTargetPool* pool, 
        sdk::FRHICommandListBase* cmd_list,
        sdk::FPooledRenderTargetDesc* desc,
        TRefCountPtr<IPooledRenderTarget>* out,
        const wchar_t* name,
        // these arent uintptrs, just defending against future changes to the size of the params
        uintptr_t a6, uintptr_t a7, uintptr_t a8, uintptr_t a9, uintptr_t a10); 

    bool m_attempted_hook{false};
    bool m_hooked{false};

    std::recursive_mutex m_mutex{};
    SafetyHookInline m_find_free_element_hook{};
    std::unordered_map<std::wstring, IPooledRenderTarget*> m_render_targets{};
};