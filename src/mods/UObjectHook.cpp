#include <future>

#include <utility/Logging.hpp>
#include <utility/String.hpp>

#include <sdk/UObjectBase.hpp>
#include <sdk/UObjectArray.hpp>
#include <sdk/UClass.hpp>
#include <sdk/FField.hpp>
#include <sdk/FProperty.hpp>
#include <sdk/UFunction.hpp>
#include <sdk/AActor.hpp>
#include <sdk/threading/GameThreadWorker.hpp>
#include <sdk/FMalloc.hpp>
#include <sdk/FStructProperty.hpp>
#include <sdk/USceneComponent.hpp>

#include "VR.hpp"

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

    SPDLOG_INFO("[UObjectHook] Hooking UObjectBase");

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

    m_most_recent_objects.push_front((sdk::UObject*)object);

    if (m_most_recent_objects.size() > 50) {
        m_most_recent_objects.pop_back();
    }

    for (auto super = (sdk::UStruct*)object->get_class(); super != nullptr; super = super->get_super_struct()) {
        m_objects_by_class[(sdk::UClass*)super].insert(object);

        if (auto it = m_on_creation_add_component_jobs.find((sdk::UClass*)super); it != m_on_creation_add_component_jobs.end()) {
            GameThreadWorker::get().enqueue([object, this]() {
                if (!this->exists(object)) {
                    return;
                }

                for (auto super = (sdk::UStruct*)object->get_class(); super != nullptr; super = super->get_super_struct()) {
                    std::function<void(sdk::UObject*)> job{};

                    {
                        std::shared_lock _{m_mutex};

                        if (auto it = this->m_on_creation_add_component_jobs.find((sdk::UClass*)super); it != this->m_on_creation_add_component_jobs.end()) {
                            job = it->second;
                        }
                    }

                    if (job) {
                        job((sdk::UObject*)object);
                    }
                }
            });
        }
    }

#ifdef VERBOSE_UOBJECTHOOK
    SPDLOG_INFO("Adding object {:x} {:s}", (uintptr_t)object, utility::narrow(m_meta_objects[object]->full_name));
#endif
}

void UObjectHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (m_wants_activate) {
        hook();
    }
}

void UObjectHook::on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                        const float world_to_meters, Vector3f* view_location, bool is_double)
{
    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    if ((view_index + 1) % 2 == 0) {
        const auto view_d = (Vector3d*)view_location;
        const auto rot_d = (Rotator<double>*)view_rotation;

        const auto view_mat = !is_double ? 
            glm::yawPitchRoll(
                glm::radians(view_rotation->yaw),
                glm::radians(view_rotation->pitch),
                glm::radians(view_rotation->roll)) : 
            glm::yawPitchRoll(
                glm::radians((float)rot_d->yaw),
                glm::radians((float)rot_d->pitch),
                glm::radians((float)rot_d->roll));

        const auto view_mat_inverse = !is_double ? 
            glm::yawPitchRoll(
                glm::radians(-view_rotation->yaw),
                glm::radians(view_rotation->pitch),
                glm::radians(-view_rotation->roll)) : 
            glm::yawPitchRoll(
                glm::radians(-(float)rot_d->yaw),
                glm::radians((float)rot_d->pitch),
                glm::radians(-(float)rot_d->roll));

        const auto view_quat_inverse = glm::quat {
            view_mat_inverse
        };

        const auto view_quat = glm::quat {
            view_mat
        };

        const auto quat_converter = glm::quat{Matrix4x4f {
            0, 0, -1, 0,
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1
        }};

        auto vqi_norm = glm::normalize(view_quat_inverse);
        const auto vqi_norm_unmodified = vqi_norm;

        // Decoupled Pitch
        if (vr->is_decoupled_pitch_enabled()) {
            vr->set_pre_flattened_rotation(vqi_norm);
            vqi_norm = utility::math::flatten(vqi_norm);
        }

        const auto rotation_offset = vr->get_rotation_offset();
        const auto hmd_origin = glm::vec3{vr->get_transform(0)[3]};
        const auto pos = glm::vec3{rotation_offset * (hmd_origin - glm::vec3{vr->get_standing_origin()})};

        const auto view_quat_inverse_flat = utility::math::flatten(view_quat_inverse);
        const auto offset1 = quat_converter* (glm::normalize(view_quat_inverse_flat) * (pos * world_to_meters));

        glm::vec3 final_position{};

        if (is_double) {
            final_position = glm::vec3{*view_location} - offset1;
        } else {
            final_position = *view_location - offset1;
        }

        if (vr->is_using_controllers()) {
            Vector3f right_hand_position = vr->get_grip_position(vr->get_right_controller_index());
            glm::quat right_hand_rotation = vr->get_grip_rotation(vr->get_right_controller_index());

            Vector3f left_hand_position = vr->get_grip_position(vr->get_left_controller_index());
            glm::quat left_hand_rotation = vr->get_grip_rotation(vr->get_left_controller_index());

            right_hand_position = glm::vec3{rotation_offset * (right_hand_position - hmd_origin)};
            left_hand_position = glm::vec3{rotation_offset * (left_hand_position - hmd_origin)};

            right_hand_position = quat_converter * (glm::normalize(view_quat_inverse_flat) * (right_hand_position * world_to_meters));
            left_hand_position = quat_converter * (glm::normalize(view_quat_inverse_flat) * (left_hand_position * world_to_meters));

            right_hand_position = final_position - right_hand_position;
            left_hand_position = final_position - left_hand_position;

            right_hand_rotation = rotation_offset * right_hand_rotation;
            right_hand_rotation = (glm::normalize(view_quat_inverse_flat) * right_hand_rotation);

            left_hand_rotation = rotation_offset * left_hand_rotation;
            left_hand_rotation = (glm::normalize(view_quat_inverse_flat) * left_hand_rotation);

            /*const auto right_hand_offset_q = glm::quat{glm::yawPitchRoll(
                glm::radians(m_right_hand_rotation_offset.Yaw),
                glm::radians(m_right_hand_rotation_offset.Pitch),
                glm::radians(m_right_hand_rotation_offset.Roll))
            };

            const auto left_hand_offset_q = glm::quat{glm::yawPitchRoll(
                glm::radians(m_left_hand_rotation_offset.Yaw),
                glm::radians(m_left_hand_rotation_offset.Pitch),
                glm::radians(m_left_hand_rotation_offset.Roll))
            };*/

            const auto right_hand_offset_q = glm::quat{glm::yawPitchRoll(
                glm::radians(0.f),
                glm::radians(0.f),
                glm::radians(0.f))
            };

            const auto left_hand_offset_q = glm::quat{glm::yawPitchRoll(
                glm::radians(0.f),
                glm::radians(0.f),
                glm::radians(0.f))
            };

            right_hand_rotation = glm::normalize(right_hand_rotation * right_hand_offset_q);
            auto right_hand_euler = glm::degrees(utility::math::euler_angles_from_steamvr(right_hand_rotation));

            left_hand_rotation = glm::normalize(left_hand_rotation * left_hand_offset_q);
            auto left_hand_euler = glm::degrees(utility::math::euler_angles_from_steamvr(left_hand_rotation));

            auto with_mutex = [this](auto fn) {
                std::shared_lock _{m_mutex};
                auto result = fn();

                return result;
            };

            const auto objs = with_mutex([this]() { return m_motion_controller_attached_objects; });
            
            for (auto object : objs) {
                if (!this->exists(object)) {
                    continue;
                }

                auto actor = (sdk::AActor*)object;

                actor->set_actor_location(right_hand_position, false, false);
                actor->set_actor_rotation(right_hand_euler, false);
            }

            const auto comps =  with_mutex([this]() { return m_motion_controller_attached_components; });

            for (auto comp : comps) {
                if (!this->exists(comp)) {
                    continue;
                }

                comp->set_world_location(right_hand_position, false, false);
                comp->set_world_rotation(right_hand_euler, false);
            }
        }
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

        if (ImGui::TreeNode("Recent Objects")) {
            for (auto& object : m_most_recent_objects) {
                if (!this->exists_unsafe(object)) {
                    continue;
                }

                if (ImGui::TreeNode(utility::narrow(object->get_full_name()).data())) {
                    ui_handle_object(object);
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Objects by class")) {
            static char filter[256]{};
            ImGui::InputText("Filter", filter, sizeof(filter));

            const bool filter_empty = std::string_view{filter}.empty();

            const auto now = std::chrono::steady_clock::now();
            bool needs_sort = true;

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

            const auto wide_filter = utility::widen(filter);

            for (auto uclass : m_sorted_classes) {
                const auto& objects_ref = m_objects_by_class[uclass];

                if (objects_ref.empty()) {
                    continue;
                }

                if (!m_meta_objects.contains(uclass)) {
                    continue;
                }

                const auto uclass_name = utility::narrow(m_meta_objects[uclass]->full_name);
                bool valid = true;

                if (!filter_empty) {
                    valid = false;

                    for (auto super = (sdk::UStruct*)uclass; super; super = super->get_super_struct()) {
                        if (auto it = m_meta_objects.find(super); it != m_meta_objects.end()) {
                            if (it->second->full_name.find(wide_filter) != std::wstring::npos) {
                                valid = true;
                                break;
                            }
                        }
                    }
                }

                if (!valid) {
                    continue;
                }

                if (ImGui::TreeNode(uclass_name.data())) {
                    std::vector<sdk::UObjectBase*> objects{};

                    for (auto object : objects_ref) {
                        objects.push_back(object);
                    }

                    std::sort(objects.begin(), objects.end(), [this](sdk::UObjectBase* a, sdk::UObjectBase* b) {
                        return m_meta_objects[a]->full_name < m_meta_objects[b]->full_name;
                    });

                    if (uclass->is_a(sdk::AActor::static_class())) {
                        static char component_add_name[256]{};

                        if (ImGui::InputText("Add Component Permanently", component_add_name, sizeof(component_add_name), ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue)) {
                            const auto component_c = sdk::find_uobject<sdk::UClass>(utility::widen(component_add_name));

                            if (component_c != nullptr) {
                                m_on_creation_add_component_jobs[uclass] = [this, component_c](sdk::UObject* object) {
                                    if (!this->exists(object)) {
                                        return;
                                    }

                                    if (object == object->get_class()->get_class_default_object()) {
                                        return;
                                    }

                                    auto actor = (sdk::AActor*)object;
                                    auto component = (sdk::UObject*)actor->add_component_by_class(component_c);

                                    if (component != nullptr) {
                                        if (component->get_class()->is_a(sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.SphereComponent"))) {
                                            struct SphereRadiusParams {
                                                float radius{};
                                            };

                                            auto params = SphereRadiusParams{};
                                            params.radius = 100.f;

                                            const auto fn = component->get_class()->find_function(L"SetSphereRadius");

                                            if (fn != nullptr) {
                                                component->process_event(fn, &params);
                                            }
                                        }

                                        struct {
                                            bool hidden{false};
                                            bool propagate{true};
                                        } set_hidden_params{};

                                        const auto fn = component->get_class()->find_function(L"SetHiddenInGame");

                                        if (fn != nullptr) {
                                            component->process_event(fn, &set_hidden_params);
                                        }

                                        actor->finish_add_component(component);

                                        // Set component_add_name to empty
                                        component_add_name[0] = '\0';
                                    } else {
                                        component_add_name[0] = 'e';
                                        component_add_name[1] = 'r';
                                        component_add_name[2] = 'r';
                                        component_add_name[3] = '\0';
                                    }
                                };
                            } else {
                                strcpy_s(component_add_name, "Nonexistent component");
                            }
                        }
                    }

                    for (const auto& object : objects) {
                        const auto made = ImGui::TreeNode(utility::narrow(m_meta_objects[object]->full_name).data());
                        // make right click context
                        if (ImGui::BeginPopupContextItem()) {
                            auto sc = [](const std::string& text) {
                                if (OpenClipboard(NULL)) {
                                    EmptyClipboard();
                                    HGLOBAL hcd = GlobalAlloc(GMEM_DDESHARE, text.size() + 1);
                                    char* data = (char*)GlobalLock(hcd);
                                    strcpy(data, text.c_str());
                                    GlobalUnlock(hcd);
                                    SetClipboardData(CF_TEXT, hcd);
                                    CloseClipboard();
                                }
                            };

                            if (ImGui::Button("Copy Name")) {
                                sc(utility::narrow(m_meta_objects[object]->full_name));
                            }

                            if (ImGui::Button("Copy Address")) {
                                const auto hex = (std::stringstream{} << std::hex << (uintptr_t)object).str();
                                sc(hex);
                            }

                            ImGui::EndPopup();
                        }

                        if (made) {
                            ui_handle_object((sdk::UObject*)object);
                            
                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }
}

void UObjectHook::ui_handle_object(sdk::UObject* object) {
    if (object == nullptr) {
        return;
    }

    const auto uclass = object->get_class();

    if (uclass == nullptr) {
        return;
    }

    static const auto material_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.MaterialInterface");

    if (uclass->is_a(material_t)) {
        ui_handle_material_interface(object);
    }

    static auto widget_component_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/UMG.WidgetComponent");

    if (uclass->is_a(widget_component_t)) {
        if (ImGui::Button("Set to Screen Space")) {
            static const auto set_widget_space_fn = uclass->find_function(L"SetWidgetSpace");

            if (set_widget_space_fn != nullptr) {
                struct {
                    uint32_t space{1};
                } params{};

                object->process_event(set_widget_space_fn, &params);
            }
        }
    }

    if (uclass->is_a(sdk::UActorComponent::static_class())) {
        if (ImGui::Button("Destroy Component")) {
            auto comp = (sdk::UActorComponent*)object;

            comp->destroy_component();
        }
    }

    if (uclass->is_a(sdk::USceneComponent::static_class())) {
        if (ImGui::Button("Attach to motion controller")) {
            m_motion_controller_attached_components.insert((sdk::USceneComponent*)object);
        }
    }

    if (uclass->is_a(sdk::AActor::static_class())) {
        ui_handle_actor(object);
    }

    ui_handle_struct(object, uclass);
}

void UObjectHook::ui_handle_material_interface(sdk::UObject* object) {
    if (object == nullptr) {
        return;
    }

    const auto uclass = object->get_class();

    if (uclass == nullptr) {
        return;
    }

    if (ImGui::Button("Apply to all actors")) {
        static const auto mesh_component_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.StaticMeshComponent");
        static const auto create_dynamic_mat = mesh_component_t->find_function(L"CreateDynamicMaterialInstance");
        static const auto set_material_fn = mesh_component_t->find_function(L"SetMaterial");
        static const auto get_num_materials_fn = mesh_component_t->find_function(L"GetNumMaterials");

        //const auto actors = m_objects_by_class[sdk::AActor::static_class()];
        const auto components = m_objects_by_class[mesh_component_t];

        //auto components = actor->get_all_components();

        for (auto comp_obj : components) {
            auto comp = (sdk::UObject*)comp_obj;
            if (comp->is_a(mesh_component_t)) {
                if (comp == comp->get_class()->get_class_default_object()) {
                    continue;
                }

                struct {
                    int32_t num{};
                } get_num_materials_params{};

                comp->process_event(get_num_materials_fn, &get_num_materials_params);

                for (int i = 0; i < get_num_materials_params.num; ++i) {
                    GameThreadWorker::get().enqueue([this, i, object, comp]() {
                        if (!this->exists(comp) || !this->exists(object)) {
                            return;
                        }

                        /*struct {
                            int32_t index{};
                            sdk::UObject* material{};
                        } params{};

                        params.index = i;
                        params.material = object;

                        comp->process_event(set_material_fn, &params);*/

                        struct {
                            int32_t index{};
                            sdk::UObject* material{};
                            sdk::FName name{L"None"};
                            sdk::UObject* ret{};
                        } params{};

                        params.index = i;
                        params.material = object;

                        comp->process_event(create_dynamic_mat, &params);

                        if (params.ret != nullptr && object->get_full_name().find(L"GizmoMaterial") != std::wstring::npos) {
                            const auto c = params.ret->get_class();
                            static const auto set_vector_param_fn = c->find_function(L"SetVectorParameterValue");

                            struct {
                                sdk::FName name{L"GizmoColor"};
                                glm::vec4 color{};
                            } set_vector_param_params{};

                            set_vector_param_params.color.x = 1.0f;
                            set_vector_param_params.color.y = 0.0f;
                            set_vector_param_params.color.z = 0.0f;
                            set_vector_param_params.color.w = 1.0f;

                            params.ret->process_event(set_vector_param_fn, &set_vector_param_params);
                        }

                        if (params.ret != nullptr && object->get_full_name().find(L"BasicShapeMaterial") != std::wstring::npos) {
                            const auto c = params.ret->get_class();
                            static const auto set_vector_param_fn = c->find_function(L"SetVectorParameterValue");

                            struct {
                                sdk::FName name{L"Color"};
                                glm::vec4 color{};
                            } set_vector_param_params{};

                            set_vector_param_params.color.x = 1.0f;
                            set_vector_param_params.color.y = 0.0f;
                            set_vector_param_params.color.z = 0.0f;
                            set_vector_param_params.color.w = 1.0f;

                            params.ret->process_event(set_vector_param_fn, &set_vector_param_params);
                        }
                    });
                }
            }
        }
    }
}

void UObjectHook::ui_handle_actor(sdk::UObject* object) {
    if (object == nullptr) {
        return;
    }

    const auto uclass = object->get_class();

    if (uclass == nullptr) {
        return;
    }

    if (ImGui::Button("Attach to motion controller")) {
        m_motion_controller_attached_objects.insert(object);
    }

    auto actor = (sdk::AActor*)object;

    static char component_add_name[256]{};

    if (ImGui::InputText("Add Component", component_add_name, sizeof(component_add_name), ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue)) {
        const auto component_c = sdk::find_uobject<sdk::UClass>(utility::widen(component_add_name));

        if (component_c != nullptr) {
            GameThreadWorker::get().enqueue([=, this]() {
                if (!this->exists(object)) {
                    return;
                }

                auto component = (sdk::UObject*)actor->add_component_by_class(component_c);

                if (component != nullptr) {
                    if (component->get_class()->is_a(sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.SphereComponent"))) {
                        struct SphereRadiusParams {
                            float radius{};
                        };

                        auto params = SphereRadiusParams{};
                        params.radius = 100.f;

                        const auto fn = component->get_class()->find_function(L"SetSphereRadius");

                        if (fn != nullptr) {
                            component->process_event(fn, &params);
                        }
                    }

                    struct {
                        bool hidden{false};
                        bool propagate{true};
                    } set_hidden_params{};

                    const auto fn = component->get_class()->find_function(L"SetHiddenInGame");

                    if (fn != nullptr) {
                        component->process_event(fn, &set_hidden_params);
                    }

                    actor->finish_add_component(component);

                    // Set component_add_name to empty
                    component_add_name[0] = '\0';
                } else {
                    component_add_name[0] = 'e';
                    component_add_name[1] = 'r';
                    component_add_name[2] = 'r';
                    component_add_name[3] = '\0';
                }
            });
        } else {
            strcpy_s(component_add_name, "Nonexistent component");
        }
    }

    if (ImGui::TreeNode("Components")) {
        auto components = actor->get_all_components();

        std::sort(components.begin(), components.end(), [](sdk::UObject* a, sdk::UObject* b) {
            return a->get_full_name() < b->get_full_name();
        });

        for (auto comp : components) {
            auto comp_obj = (sdk::UObject*)comp;

            ImGui::PushID(comp_obj);
            const auto made = ImGui::TreeNode(utility::narrow(comp_obj->get_full_name()).data());
            if (made) {
                ui_handle_object(comp_obj);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

void UObjectHook::ui_handle_functions(void* object, sdk::UStruct* uclass) {
    if (uclass == nullptr) {
        return;
    }

    const bool is_real_object = object != nullptr && m_objects.contains((sdk::UObject*)object);
    auto object_real = (sdk::UObject*)object;

    std::vector<sdk::UFunction*> sorted_functions{};
    static const auto ufunction_t = sdk::UFunction::static_class();

    for (auto super = (sdk::UStruct*)uclass; super != nullptr; super = super->get_super_struct()) {
        auto funcs = super->get_children();

        for (auto func = funcs; func != nullptr; func = func->get_next()) {
            if (func->get_class()->is_a(ufunction_t)) {
                sorted_functions.push_back((sdk::UFunction*)func);
            }
        }
    }

    std::sort(sorted_functions.begin(), sorted_functions.end(), [](sdk::UFunction* a, sdk::UFunction* b) {
        return a->get_fname().to_string() < b->get_fname().to_string();
    });

    for (auto func : sorted_functions) {
        if (is_real_object && ImGui::TreeNode(utility::narrow(func->get_fname().to_string()).data())) {
            auto parameters = func->get_child_properties();

            if (parameters == nullptr) {
                if (ImGui::Button("Call")) {
                    struct {
                        char poop[0x100]{};
                    } params{};

                    object_real->process_event(func, &params);
                }
            } else if (parameters->get_next() == nullptr) {
                switch (utility::hash(utility::narrow(parameters->get_class()->get_name().to_string()))) {
                case "BoolProperty"_fnv:
                    {
                        if (ImGui::Button("Enable")) {
                            struct {
                                bool enabled{true};
                                char padding[0x10];
                            } params{};

                            object_real->process_event(func, &params);
                        }

                        ImGui::SameLine();

                        if (ImGui::Button("Disable")) {
                            struct {
                                bool enabled{false};
                                char padding[0x10];
                            } params{};

                            object_real->process_event(func, &params);
                        }
                    }
                    break;

                default:
                    break;
                };
            }

            for (auto param = parameters; param != nullptr; param = param->get_next()) {
                ImGui::Text("%s %s", utility::narrow(param->get_class()->get_name().to_string()), utility::narrow(param->get_field_name().to_string()).data());
            }

            ImGui::TreePop();
        }
    }
}

void UObjectHook::ui_handle_properties(void* object, sdk::UStruct* uclass) {
    if (uclass == nullptr) {
        return;
    }

    const bool is_real_object = object != nullptr && m_objects.contains((sdk::UObject*)object);

    std::vector<sdk::FField*> sorted_fields{};

    for (auto super = (sdk::UStruct*)uclass; super != nullptr; super = super->get_super_struct()) {
        auto props = super->get_child_properties();

        for (auto prop = props; prop != nullptr; prop = prop->get_next()) {
            sorted_fields.push_back(prop);
        }
    }

    std::sort(sorted_fields.begin(), sorted_fields.end(), [](sdk::FField* a, sdk::FField* b) {
        return a->get_field_name().to_string() < b->get_field_name().to_string();
    });

    for (auto prop : sorted_fields) {
        auto propc = prop->get_class();
        const auto propc_type = utility::narrow(propc->get_name().to_string());

        if (!is_real_object) {
            const auto name = utility::narrow(propc->get_name().to_string());
            const auto field_name = utility::narrow(prop->get_field_name().to_string());
            ImGui::Text("%s %s", name.data(), field_name.data());

            continue;
        }

        switch (utility::hash(propc_type)) {
        case "FloatProperty"_fnv:
            {
                auto& value = *(float*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragFloat(utility::narrow(prop->get_field_name().to_string()).data(), &value, 0.01f);
            }
            break;
        case "DoubleProperty"_fnv:
            {
                auto& value = *(double*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragFloat(utility::narrow(prop->get_field_name().to_string()).data(), (float*)&value, 0.01f);
            }
            break;
        case "IntProperty"_fnv:
            {
                auto& value = *(int32_t*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragInt(utility::narrow(prop->get_field_name().to_string()).data(), &value, 1);
            }
            break;

        // TODO: handle bitfields
        case "BoolProperty"_fnv:
            {
                auto& value = *(bool*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::Checkbox(utility::narrow(prop->get_field_name().to_string()).data(), &value);
            }
            break;
        case "ObjectProperty"_fnv:
            {
                auto& value = *(sdk::UObject**)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                
                if (ImGui::TreeNode(utility::narrow(prop->get_field_name().to_string()).data())) {
                    ui_handle_object(value);
                    ImGui::TreePop();
                }
            }
            break;
        case "StructProperty"_fnv:
            {
                void* addr = (void*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());

                if (ImGui::TreeNode(utility::narrow(prop->get_field_name().to_string()).data())) {
                    ui_handle_struct(addr, ((sdk::FStructProperty*)prop)->get_struct());
                    ImGui::TreePop();
                }
            }
            break;
        case "Function"_fnv:
            break;
        default:
            {
                const auto name = utility::narrow(propc->get_name().to_string());
                const auto field_name = utility::narrow(prop->get_field_name().to_string());
                ImGui::Text("%s %s", name.data(), field_name.data());
            }
            break;
        };
    }
}

void UObjectHook::ui_handle_struct(void* addr, sdk::UStruct* uclass) {
    if (uclass == nullptr) {
        return;
    }

    // Display inheritance tree
    if (ImGui::TreeNode("Inheritance")) {
        for (auto super = (sdk::UStruct*)uclass; super != nullptr; super = super->get_super_struct()) {
            if (ImGui::TreeNode(utility::narrow(super->get_full_name()).data())) {
                ui_handle_struct(addr, super);
                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Functions")) {
        ui_handle_functions(addr, uclass);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Properties")) {
        ui_handle_properties(addr, uclass);
        ImGui::TreePop();
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

            for (auto super = (sdk::UStruct*)it->second->uclass; super != nullptr; super = super->get_super_struct()) {
                hook->m_objects_by_class[(sdk::UClass*)super].erase(object);
            }

            //hook->m_objects_by_class[it->second->uclass].erase(object);

            hook->m_reusable_meta_objects.push_back(std::move(it->second));
            hook->m_meta_objects.erase(object);
        }
    }

    auto result = hook->m_destructor_hook.unsafe_call<void*>(object, rdx, r8, r9);

    return result;
}