#pragma once

#include <shared_mutex>
#include <unordered_set>
#include <memory>
#include <deque>

#include <safetyhook.hpp>

#include "Mod.hpp"

namespace sdk {
class UObjectBase;
class UObject;
class UClass;
}

class UObjectHook : public Mod {
public:
    static std::shared_ptr<UObjectHook>& get();
    std::unordered_set<sdk::UObjectBase*> get_objects_by_class(sdk::UClass* uclass) const {
        std::shared_lock _{m_mutex};
        if (auto it = m_objects_by_class.find(uclass); it != m_objects_by_class.end()) {
            return it->second;
        }

        return {};
    }

    void activate();

protected:
    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_draw_ui() override;

private:
    void hook();
    void add_new_object(sdk::UObjectBase* object);

    static void* add_object(void* rcx, void* rdx, void* r8, void* r9);
    static void* destructor(sdk::UObjectBase* object, void* rdx, void* r8, void* r9);

    bool m_hooked{false};
    bool m_fully_hooked{false};
    bool m_wants_activate{false};

    mutable std::shared_mutex m_mutex{};

    struct MetaObject {
        std::wstring full_name{};
        sdk::UClass* uclass{nullptr};
    };

    std::unordered_set<sdk::UObjectBase*> m_objects{};
    std::unordered_map<sdk::UObjectBase*, std::unique_ptr<MetaObject>> m_meta_objects{};
    std::unordered_map<sdk::UClass*, std::unordered_set<sdk::UObjectBase*>> m_objects_by_class{};

    std::deque<std::unique_ptr<MetaObject>> m_reusable_meta_objects{};

    SafetyHookInline m_add_object_hook{};
    SafetyHookInline m_destructor_hook{};
};