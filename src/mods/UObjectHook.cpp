#include <future>

#include <utility/Logging.hpp>
#include <utility/String.hpp>

#include <sdk/UObjectBase.hpp>
#include <sdk/UObjectArray.hpp>
#include <sdk/UClass.hpp>
#include <sdk/threading/GameThreadWorker.hpp>

#include "UObjectHook.hpp"

//#define VERBOSE_UOBJECTHOOK

std::shared_ptr<UObjectHook>& UObjectHook::get() {
    static std::shared_ptr<UObjectHook> instance = std::make_shared<UObjectHook>();
    return instance;
}

void UObjectHook::activate() {
    if (m_hooked) {
        return;
    }

    if (GameThreadWorker::get().is_same_thread()) {
        hook();
        return;
    }

    m_wants_activate = true;
}

void UObjectHook::hook() {
    if (m_hooked) {
        return;
    }

    SPDLOG_INFO("[UObjectHook} Hooking UObjectBase");

    m_hooked = true;
    m_wants_activate = false;

    auto destructor_fn = sdk::UObjectBase::get_destructor();

    if (!destructor_fn) {
        SPDLOG_ERROR("[UObjectHook] Failed to find UObjectBase::destructor, cannot hook UObjectBase");
        return;
    }

    auto add_object_fn = sdk::UObjectBase::get_add_object();

    if (!add_object_fn) {
        SPDLOG_ERROR("[UObjectHook] Failed to find UObjectBase::AddObject, cannot hook UObjectBase");
        return;
    }

    m_destructor_hook = safetyhook::create_inline((void**)destructor_fn.value(), &destructor);

    if (!m_destructor_hook) {
        SPDLOG_ERROR("[UObjectHook] Failed to hook UObjectBase::destructor, cannot hook UObjectBase");
        return;
    }

    m_add_object_hook = safetyhook::create_inline((void**)add_object_fn.value(), &add_object);

    if (!m_add_object_hook) {
        SPDLOG_ERROR("[UObjectHook] Failed to hook UObjectBase::AddObject, cannot hook UObjectBase");
        return;
    }

    SPDLOG_INFO("[UObjectHook] Hooked UObjectBase");

    // Add all the objects that already exist
    auto uobjectarray = sdk::FUObjectArray::get();

    for (auto i = 0; i < uobjectarray->get_object_count(); ++i) {
        auto object = uobjectarray->get_object(i);

        if (object == nullptr || object->object == nullptr) {
            continue;
        }

        add_new_object(object->object);
    }

    SPDLOG_INFO("[UObjectHook] Added {} existing objects", m_objects.size());

    m_fully_hooked = true;
}

void UObjectHook::add_new_object(sdk::UObjectBase* object) {
    std::unique_lock _{m_mutex};
    std::unique_ptr<MetaObject> meta_object{};

    if (!m_reusable_meta_objects.empty()) {
        meta_object = std::move(m_reusable_meta_objects.back());
        m_reusable_meta_objects.pop_back();
    } else {
        meta_object = std::make_unique<MetaObject>();
    }

    m_objects.insert(object);
    meta_object->full_name = object->get_full_name();
    meta_object->uclass = object->get_class();

    m_meta_objects[object] = std::move(meta_object);
    m_objects_by_class[object->get_class()].insert(object);

#ifdef VERBOSE_UOBJECTHOOK
    SPDLOG_INFO("Adding object {:x} {:s}", (uintptr_t)object, utility::narrow(m_meta_objects[object]->full_name));
#endif
}

void UObjectHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (m_wants_activate) {
        hook();
    }
}

std::future<std::vector<sdk::UClass*>> sorting_task{};

void UObjectHook::on_draw_ui() {
    if (ImGui::CollapsingHeader("UObjectHook")) {
        activate();

        if (!m_fully_hooked) {
            ImGui::Text("Waiting for UObjectBase to be hooked...");
            return;
        }

        std::shared_lock _{m_mutex};

        ImGui::Text("Objects: %zu (%zu actual)", m_objects.size(), sdk::FUObjectArray::get()->get_object_count());

        if (ImGui::TreeNode("Objects")) {
            for (auto& [object, meta_object] : m_meta_objects) {
                ImGui::Text("%s", utility::narrow(meta_object->full_name).data());
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Objects by class")) {
            const auto now = std::chrono::steady_clock::now();
            bool needs_sort = now - m_last_sort_time >= std::chrono::seconds(1) || m_sorted_classes.empty();

            if (sorting_task.valid()) {
                // Check if the sorting task is finished
                if (sorting_task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    // Do something if needed when sorting is done
                    m_sorted_classes = sorting_task.get();
                    needs_sort = true;
                } else {
                    needs_sort = false;
                }
            } else {
                needs_sort = true;
            }

            if (needs_sort) {
                auto sort_classes = [this](std::vector<sdk::UClass*> classes) {
                    std::sort(classes.begin(), classes.end(), [this](sdk::UClass* a, sdk::UClass* b) {
                        std::shared_lock _{m_mutex};
                        if (!m_objects.contains(a) || !m_objects.contains(b)) {
                            return false;
                        }

                        return m_meta_objects[a]->full_name < m_meta_objects[b]->full_name;
                    });

                    return classes;
                };

                auto unsorted_classes = std::vector<sdk::UClass*>{};

                for (auto& [c, set]: m_objects_by_class) {
                    unsorted_classes.push_back(c);
                }

                // Launch sorting in a separate thread
                sorting_task = std::async(std::launch::async, sort_classes, unsorted_classes);
                m_last_sort_time = now;
            }

            for (auto uclass : m_sorted_classes) {
                const auto& objects = m_objects_by_class[uclass];

                if (objects.empty()) {
                    continue;
                }

                const auto uclass_name = utility::narrow(m_meta_objects[uclass]->full_name);

                if (ImGui::TreeNode(uclass_name.data())) {
                    for (const auto& object : objects) {
                        ImGui::Text("%s", utility::narrow(m_meta_objects[object]->full_name).data());
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }
}

void* UObjectHook::add_object(void* rcx, void* rdx, void* r8, void* r9) {
    auto& hook = UObjectHook::get();
    auto result = hook->m_add_object_hook.unsafe_call<void*>(rcx, rdx, r8, r9);

    {
        static bool is_rcx = [&]() {
            if (!IsBadReadPtr(rcx, sizeof(void*)) && 
                !IsBadReadPtr(*(void**)rcx, sizeof(void*)) &&
                !IsBadReadPtr(**(void***)rcx, sizeof(void*))) 
            {
                SPDLOG_INFO("[UObjectHook] RCX is UObjectBase*");
                return true;
            } else {
                SPDLOG_INFO("[UObjectHook] RDX is UObjectBase*");
                return false;
            }
        }();

        sdk::UObjectBase* obj = nullptr;

        if (is_rcx) {
            obj = (sdk::UObjectBase*)rcx;
        } else {
            obj = (sdk::UObjectBase*)rdx;
        }

        hook->add_new_object(obj);
    }

    return result;
}

void* UObjectHook::destructor(sdk::UObjectBase* object, void* rdx, void* r8, void* r9) {
    auto& hook = UObjectHook::get();

    {
        std::unique_lock _{hook->m_mutex};

        if (auto it = hook->m_meta_objects.find(object); it != hook->m_meta_objects.end()) {
#ifdef VERBOSE_UOBJECTHOOK
            SPDLOG_INFO("Removing object {:x} {:s}", (uintptr_t)object, utility::narrow(it->second->full_name));
#endif
            hook->m_objects.erase(object);
            hook->m_objects_by_class[it->second->uclass].erase(object);

            hook->m_reusable_meta_objects.push_back(std::move(it->second));
            hook->m_meta_objects.erase(object);
        }
    }

    auto result = hook->m_destructor_hook.unsafe_call<void*>(object, rdx, r8, r9);

    return result;
}