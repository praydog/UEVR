#include <fstream>

#include <utility/Logging.hpp>
#include <utility/String.hpp>
#include <utility/ScopeGuard.hpp>

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
#include <sdk/UGameplayStatics.hpp>
#include <sdk/APlayerController.hpp>
#include <sdk/APawn.hpp>
#include <sdk/APlayerCameraManager.hpp>
#include <sdk/ScriptVector.hpp>
#include <sdk/FBoolProperty.hpp>
#include <sdk/FObjectProperty.hpp>
#include <sdk/FArrayProperty.hpp>

#include "VR.hpp"

#include "UObjectHook.hpp"

//#define VERBOSE_UOBJECTHOOK

std::shared_ptr<UObjectHook>& UObjectHook::get() {
    static std::shared_ptr<UObjectHook> instance = std::make_shared<UObjectHook>();
    return instance;
}

UObjectHook::MotionControllerState::~MotionControllerState() {
    if (this->adjustment_visualizer != nullptr) {
        GameThreadWorker::get().enqueue([vis = this->adjustment_visualizer]() {
            SPDLOG_INFO("[UObjectHook::MotionControllerState] Destroying adjustment visualizer for component {:x}", (uintptr_t)vis);

            if (!UObjectHook::get()->exists(vis)) {
                return;
            }

            vis->destroy_actor();

            SPDLOG_INFO("[UObjectHook::MotionControllerState] Destroyed adjustment visualizer for component {:x}", (uintptr_t)vis);
        });
    }
}

nlohmann::json UObjectHook::MotionControllerStateBase::to_json() const {
    return {
        {"rotation_offset", utility::math::to_json(rotation_offset)},
        {"location_offset", utility::math::to_json(location_offset)},
        {"hand", hand},
        {"permanent", permanent}
    };
}

void UObjectHook::MotionControllerStateBase::from_json(const nlohmann::json& data) {
    if (data.contains("rotation_offset")) {
        rotation_offset = utility::math::from_json_quat(data["rotation_offset"]);
    }

    if (data.contains("location_offset")) {
        location_offset = utility::math::from_json_vec3(data["location_offset"]);
    }

    if (data.contains("hand")) {
        hand = data["hand"].get<uint8_t>();
        hand = hand % 2;
    }

    if (data.contains("permanent") && data["permanent"].is_boolean()) {
        permanent = data["permanent"].get<bool>();
    }
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

    SPDLOG_INFO("[UObjectHook] Deserializing persistent states");
    m_persistent_states = deserialize_all_mc_states();
    m_persistent_camera_state = deserialize_camera_state();
    m_persistent_properties = deserialize_all_persistent_properties();
    SPDLOG_INFO("[UObjectHook] Deserialized {} persistent states", m_persistent_states.size());

    m_fully_hooked = true;
}

void UObjectHook::add_new_object(sdk::UObjectBase* object) {
    std::unique_lock _{m_mutex};
    std::unique_ptr<MetaObject> meta_object{};

    /*static const auto prim_comp_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.PrimitiveComponent");

    if (prim_comp_t != nullptr && object->get_class()->is_a(prim_comp_t)) {
        static const auto bGenerateOverlapEvents = (sdk::FBoolProperty*)prim_comp_t->find_property(L"bGenerateOverlapEvents");

        if (bGenerateOverlapEvents != nullptr) {
            bGenerateOverlapEvents->set_value_in_object(object, true);
        }
    }*/

    const auto c = object->get_class();

    if (c == nullptr) {
        return;
    }

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
    
    if (m_fully_hooked) {
        {
            std::shared_lock _{m_mutex};
            const auto ui_active = g_framework->is_drawing_ui();

            for (auto& state : m_motion_controller_attached_components) {
                if (m_overlap_detection_actor == nullptr && state.second->adjusting && ui_active) {
                    state.second->adjusting = false;
                }
            }
        }

        update_persistent_states();
    }
}

// TODO: split this into some functions because its getting a bit massive
void UObjectHook::on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                        const float world_to_meters, Vector3f* view_location, bool is_double)
{
    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    auto view_d = (Vector3d*)view_location;
    auto rot_d = (Rotator<double>*)view_rotation;

    if (is_double) {
        m_last_camera_location = glm::vec3{*view_d};
    } else {
        m_last_camera_location = *view_location;
    }

    if (m_camera_attach.object != nullptr) {
        if (m_camera_attach.object->is_a(sdk::AActor::static_class())) {
            const auto actor = (sdk::AActor*)m_camera_attach.object;
            const auto location = actor->get_actor_location();

            if (is_double) {
                *view_d = glm::vec<3, double>{location + m_camera_attach.offset};
            } else {
                *view_location = location + m_camera_attach.offset;
            }
        } else if (m_camera_attach.object->is_a(sdk::USceneComponent::static_class())) {
            const auto comp = (sdk::USceneComponent*)m_camera_attach.object;
            const auto location = comp->get_world_location();

            if (is_double) {
                *view_d = glm::vec<3, double>{location + m_camera_attach.offset};
            } else {
                *view_location = location + m_camera_attach.offset;
            }
        } // else todo?
    }

    if ((view_index + 1) % 2 == 0) {
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
        const auto offset1 = quat_converter * (glm::normalize(view_quat_inverse_flat) * (pos * world_to_meters));

        glm::vec3 final_position{};

        if (is_double) {
            final_position = glm::vec3{*view_location} - offset1;
        } else {
            final_position = *view_location - offset1;
        }

        if (vr->is_using_controllers()) {
            Vector3f right_hand_position = vr->get_grip_position(vr->get_right_controller_index());
            glm::quat right_hand_rotation = vr->get_aim_rotation(vr->get_right_controller_index());

            const auto original_right_hand_rotation = right_hand_rotation;
            const auto original_right_hand_position = right_hand_position - hmd_origin;

            Vector3f left_hand_position = vr->get_grip_position(vr->get_left_controller_index());
            glm::quat left_hand_rotation = vr->get_aim_rotation(vr->get_left_controller_index());

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

            //right_hand_rotation = glm::normalize(right_hand_rotation * right_hand_offset_q);
            auto right_hand_euler = glm::degrees(utility::math::euler_angles_from_steamvr(right_hand_rotation));

            //left_hand_rotation = glm::normalize(left_hand_rotation * left_hand_offset_q);
            auto left_hand_euler = glm::degrees(utility::math::euler_angles_from_steamvr(left_hand_rotation));

            auto with_mutex = [this](auto fn) {
                std::shared_lock _{m_mutex};
                auto result = fn();

                return result;
            };

            auto comps = with_mutex([this]() { return m_motion_controller_attached_components; });

            sdk::TArray<sdk::UPrimitiveComponent*> overlapped_components{};
            sdk::TArray<sdk::UPrimitiveComponent*> overlapped_components_left{};

            // Update overlapped components and overlap actor transform
            if (m_overlap_detection_actor != nullptr && this->exists(m_overlap_detection_actor)) {
                m_overlap_detection_actor->set_actor_location(right_hand_position, false, false);
                m_overlap_detection_actor->set_actor_rotation(right_hand_euler, false);

                if (!g_framework->is_drawing_ui()) {
                    overlapped_components = std::move(m_overlap_detection_actor->get_overlapping_components());
                }
            }

            // Update overlapped components and overlap actor transform (left)
            if (m_overlap_detection_actor_left != nullptr && this->exists(m_overlap_detection_actor_left)) {
                m_overlap_detection_actor_left->set_actor_location(left_hand_position, false, false);
                m_overlap_detection_actor_left->set_actor_rotation(left_hand_euler, false);

                if (!g_framework->is_drawing_ui()) {
                    overlapped_components_left = std::move(m_overlap_detection_actor_left->get_overlapping_components());
                }
            }

            // Check intuitive attachment for overlapped components
            if (!g_framework->is_drawing_ui() && (!overlapped_components.empty() || !overlapped_components_left.empty())) {
                static bool prev_right_a_pressed = false;
                static bool prev_left_a_pressed = false;
                const auto is_a_down_raw_right = vr->is_action_active_any_joystick(vr->get_action_handle(VR::s_action_a_button_right));
                const auto was_a_pressed_right = !prev_right_a_pressed && is_a_down_raw_right;

                const auto is_a_down_raw_left = vr->is_action_active_any_joystick(vr->get_action_handle(VR::s_action_a_button_left));
                const auto was_a_pressed_left = !prev_left_a_pressed && is_a_down_raw_left;

                prev_right_a_pressed = is_a_down_raw_right;
                prev_left_a_pressed = is_a_down_raw_left;

                // Update existing attached components before moving onto overlapped ones.
                for (auto& it : comps) {
                    auto& state = *it.second;

                    if (state.hand == 0) {
                        if (is_a_down_raw_left) {
                            state.adjusting = true;
                        } else if (!is_a_down_raw_left) {
                            state.adjusting = false;
                        }
                    } else if (state.hand == 1) {
                        if (is_a_down_raw_right) {
                            state.adjusting = true;
                        } else if (!is_a_down_raw_right) {
                            state.adjusting = false;
                        }
                    }
                }
                
                auto update_overlaps = [&](int32_t hand, const sdk::TArray<sdk::UPrimitiveComponent*>& components) {
                    const auto was_pressed = hand == 0 ? was_a_pressed_left : was_a_pressed_right;
                    const auto is_pressed = hand == 0 ? is_a_down_raw_left : is_a_down_raw_right;

                    for (auto overlap : components) {
                        static const auto capsule_component_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.CapsuleComponent");
                        if (overlap->get_class()->is_a(capsule_component_t)) {
                            continue;
                        }

                        {
                            std::shared_lock _{m_mutex};
                            if (m_spawned_spheres_to_components.contains(overlap)) {
                                overlap = (sdk::UPrimitiveComponent*)m_spawned_spheres_to_components[overlap];
                            }
                        }

                        const auto owner = overlap->get_owner();
                        bool owner_is_adjustment_vis = false;

                        if (owner == m_overlap_detection_actor || owner == m_overlap_detection_actor_left) {
                            continue;
                        }

                        // Make sure we don't try to attach to the adjustment visualizer
                        {
                            std::shared_lock _{m_mutex};
                            auto it = std::find_if(m_motion_controller_attached_components.begin(), m_motion_controller_attached_components.end(),
                                [&](auto& it) {
                                    if (it.second->adjustment_visualizer != nullptr && this->exists_unsafe(it.second->adjustment_visualizer)) {
                                        if (it.second->adjustment_visualizer == owner) {
                                            owner_is_adjustment_vis = true;
                                            return true;
                                        }
                                    }

                                    return false;
                                });
                            
                            if (owner_is_adjustment_vis) {
                                continue;
                            }
                        }

                        if (was_pressed) {
                            auto state = get_or_add_motion_controller_state(overlap);
                            state->adjusting = true;
                            state->hand = hand;
                        } /*else if (!is_pressed) {
                            auto state = get_motion_controller_state(overlap);

                            if (state && (*state)->hand == hand) {
                                (*state)->adjusting = false;
                            }
                        }*/
                    }
                };

                update_overlaps(0, overlapped_components_left);
                update_overlaps(1, overlapped_components);
            }

            for (auto& it : comps) {
                auto comp = it.first;
                if (!this->exists(comp) || it.second == nullptr) {
                    continue;
                }
                
                auto& state = *it.second;
                const auto orig_position = comp->get_world_location();
                const auto orig_rotation = comp->get_world_rotation();

                // Convert orig_rotation to quat
                const auto orig_rotation_mat = glm::yawPitchRoll(
                    glm::radians(-orig_rotation.y),
                    glm::radians(orig_rotation.x),
                    glm::radians(-orig_rotation.z));
                const auto orig_rotation_quat = glm::quat{orig_rotation_mat};

                const auto& hand_rotation = state.hand == 1 ? right_hand_rotation : left_hand_rotation;
                const auto& hand_position = state.hand == 1 ? right_hand_position : left_hand_position;
                const auto& hand_euler = state.hand == 1 ? right_hand_euler : left_hand_euler;

                const auto adjusted_rotation = hand_rotation * glm::inverse(state.rotation_offset);
                const auto adjusted_euler = glm::degrees(utility::math::euler_angles_from_steamvr(adjusted_rotation));

                const auto adjusted_location = hand_position + (quat_converter * (adjusted_rotation * state.location_offset));

                if (state.adjusting) {
                    // Create a temporary actor that visualizes how we're adjusting the component
                    if (state.adjustment_visualizer == nullptr) {
                        auto ugs = sdk::UGameplayStatics::get();
                        auto visualizer = ugs->spawn_actor(sdk::UGameEngine::get()->get_world(), sdk::AActor::static_class(), orig_position);

                        if (visualizer != nullptr) {
                            auto add_comp = [&](sdk::UClass* c, std::function<void(sdk::UActorComponent*)> fn) -> sdk::UActorComponent* {
                                if (c == nullptr) {
                                    SPDLOG_ERROR("[UObjectHook] Cannot add component of null class");
                                    return nullptr;
                                }

                                auto new_comp = visualizer->add_component_by_class(c);

                                if (new_comp != nullptr) {
                                    fn(new_comp);

                                    if (new_comp->is_a(sdk::USceneComponent::static_class())) {
                                        auto scene_comp = (sdk::USceneComponent*)new_comp;
                                        scene_comp->set_hidden_in_game(false);
                                    }

                                    visualizer->finish_add_component(new_comp);
                                } else {
                                    SPDLOG_ERROR("[UObjectHook] Failed to add component {} to adjustment visualizer", utility::narrow(c->get_full_name()));
                                }
                                
                                return new_comp;
                            };

                            add_comp(sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.SphereComponent"), [](sdk::UActorComponent* new_comp) {
                                struct SphereRadiusParams {
                                    float radius{};
                                    bool update_overlaps{false};
                                };

                                auto params = SphereRadiusParams{};
                                params.radius = 10.f;

                                const auto fn = new_comp->get_class()->find_function(L"SetSphereRadius");

                                if (fn != nullptr) {
                                    new_comp->process_event(fn, &params);
                                }
                            });

                            /*add_comp(sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.ArrowComponent"), [](sdk::UActorComponent* new_comp) {
                                struct {
                                    float color[4]{1.0f, 0.0f, 0.0f, 1.0f};
                                } params{};

                                const auto fn = new_comp->get_class()->find_function(L"SetArrowColor");

                                if (fn != nullptr) {
                                    new_comp->process_event(fn, &params);
                                }
                            });*/

                            // Ghetto way of making a "mesh" out of the box components
                            for (auto j = 0; j < 3; ++j) {
                                for (auto i = 1; i < 25; ++i) {
                                    add_comp(sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.BoxComponent"), [i, j](sdk::UActorComponent* new_comp) {
                                        auto color = (uint8_t*)new_comp->get_property_data(L"ShapeColor");

                                        if (color != nullptr) {
                                            color[0] = 255;
                                            color[1] = 255;
                                            color[2] = 0;
                                            color[3] = 0;
                                        }

                                        auto extent = (void*)new_comp->get_property_data(L"BoxExtent");

                                        if (extent != nullptr) {
                                            static const bool is_ue5 = sdk::ScriptVector::static_struct()->get_struct_size() == sizeof(glm::vec<3, double>);

                                            const auto ratio = (float)i / 100.0f;

                                            const auto x = j == 0 ? ratio * 25.0f : 25.0f;
                                            const auto y = j == 1 ? ratio * 5.0f : 5.0f;
                                            const auto z = j == 2 ? ratio * 5.0f : 5.0f;

                                            glm::vec3 wanted_ext = glm::vec3{x, y, z};

                                            if (is_ue5) {
                                                *((glm::vec<3, double>*)extent) = wanted_ext;
                                            } else {
                                                *((glm::vec3*)extent) = wanted_ext;
                                            }
                                        }
                                    });
                                }
                            }

                            //ugs->finish_spawning_actor(visualizer, orig_position);

                            std::unique_lock _{m_mutex};
                            state.adjustment_visualizer = visualizer;
                        } else {
                            SPDLOG_ERROR("[UObjectHook] Failed to spawn actor for adjustment visualizer");
                        }
                    } else {
                        state.adjustment_visualizer->set_actor_location(hand_position, false, false);
                        state.adjustment_visualizer->set_actor_rotation(hand_euler, false);
                    }

                    std::unique_lock _{m_mutex};
                    const auto mat_inverse = 
                        glm::yawPitchRoll(
                            glm::radians(-orig_rotation.y),
                            glm::radians(orig_rotation.x),
                            glm::radians(-orig_rotation.z));

                    const auto mq = glm::quat{mat_inverse};
                    const auto mqi = glm::inverse(mq);

                    state.rotation_offset = mqi * hand_rotation;
                    state.location_offset = mqi * utility::math::ue4_to_glm(hand_position - orig_position);
                } else {
                    if (state.adjustment_visualizer != nullptr) {
                        state.adjustment_visualizer->destroy_actor();

                        std::unique_lock _{m_mutex};
                        state.adjustment_visualizer = nullptr;
                    }

                    comp->set_world_location(adjusted_location, false, false);
                    comp->set_world_rotation(adjusted_euler, false, false);
                }

                if (!state.permanent) {
                    GameThreadWorker::get().enqueue([this, comp, orig_position, orig_rotation]() {
                        if (!this->exists(comp)) {
                            return;
                        }

                        comp->set_world_location(orig_position, false, false);
                        comp->set_world_rotation(orig_rotation, false, false);
                    });
                }
            }
        }
    }
}

void UObjectHook::on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                    const float world_to_meters, Vector3f* view_location, bool is_double)
{
    if (!VR::get()->is_hmd_active()) {
        return;
    }

    std::shared_lock _{m_mutex};
    bool any_adjusting = false;
    for (auto& it : m_motion_controller_attached_components) {
        if (it.second->adjusting) {
            any_adjusting = true;
            break;
        }
    }

    VR::get()->set_aim_allowed(!any_adjusting);
}

void UObjectHook::spawn_overlapper(uint32_t hand) {
    GameThreadWorker::get().enqueue([this, hand]() {
        auto ugs = sdk::UGameplayStatics::get();
        auto world = sdk::UGameEngine::get()->get_world();

        if (ugs == nullptr || world == nullptr) {
            return;
        }

        auto overlapper = ugs->spawn_actor(world, sdk::AActor::static_class(), glm::vec3{0, 0, 0});

        if (hand == 1) {
            if (m_overlap_detection_actor != nullptr && this->exists(m_overlap_detection_actor)) {
                m_overlap_detection_actor->destroy_actor();
            }

            m_overlap_detection_actor = overlapper;
        } else {
            if (m_overlap_detection_actor_left != nullptr && this->exists(m_overlap_detection_actor_left)) {
                m_overlap_detection_actor_left->destroy_actor();
            }

            m_overlap_detection_actor_left = overlapper;
        }

        if (overlapper != nullptr) {
            auto add_comp = [&](sdk::AActor* target, sdk::UClass* c, std::function<void(sdk::UActorComponent*)> fn) -> sdk::UActorComponent* {
                if (c == nullptr) {
                    SPDLOG_ERROR("[UObjectHook] Cannot add component of null class");
                    return nullptr;
                }

                auto new_comp = target->add_component_by_class(c);

                if (new_comp != nullptr) {
                    fn(new_comp);

                    if (new_comp->is_a(sdk::USceneComponent::static_class())) {
                        auto scene_comp = (sdk::USceneComponent*)new_comp;
                        scene_comp->set_hidden_in_game(false);
                    }

                    target->finish_add_component(new_comp);
                } else {
                    SPDLOG_ERROR("[UObjectHook] Failed to add component {} to target", utility::narrow(c->get_full_name()));
                }
                
                return new_comp;
            };

            const auto sphere_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.SphereComponent");

            add_comp(overlapper, sphere_t, [](sdk::UActorComponent* new_comp) {
                struct SphereRadiusParams {
                    float radius{};
                    bool update_overlaps{true};
                } params{};

                params.radius = 10.0f;

                const auto fn = new_comp->get_class()->find_function(L"SetSphereRadius");

                if (fn != nullptr) {
                    new_comp->process_event(fn, &params);
                }
            });

            const auto skeletal_mesh_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.SkeletalMeshComponent");
            const auto meshes = get_objects_by_class(skeletal_mesh_t);
            
            for (auto obj : meshes) {
                if (!this->exists(obj)) {
                    continue;
                }

                // Dont add the same one twice
                if (m_components_with_spheres.contains((sdk::USceneComponent*)obj)) {
                    continue;
                }

                const auto default_obj = obj->get_class()->get_class_default_object();

                if (obj == default_obj) {
                    continue;
                }

                SPDLOG_INFO("[UObjectHook] Spawning sphere for skeletal mesh {}", utility::narrow(obj->get_full_name()));

                auto mesh = (sdk::USceneComponent*)obj;
                auto owner = mesh->get_owner();

                if (owner == nullptr) {
                    continue;
                }

                const auto owner_default = owner->get_class()->get_class_default_object();

                if (owner == owner_default) {
                    continue;
                }

                SPDLOG_INFO("[UObjectHook] Owner of skeletal mesh is {}", utility::narrow(owner->get_full_name()));

                auto new_sphere = (sdk::USceneComponent*)add_comp(owner, sphere_t, [](sdk::UActorComponent* new_comp) {
                    struct SphereRadiusParams {
                        float radius{};
                        bool update_overlaps{true};
                    } params{};

                    params.radius = 10.0f;

                    const auto fn = new_comp->get_class()->find_function(L"SetSphereRadius");

                    if (fn != nullptr) {
                        new_comp->process_event(fn, &params);
                    }
                });

                if (new_sphere != nullptr) {
                    new_sphere->attach_to(mesh, L"None", 0, true);
                    new_sphere->set_local_transform(glm::vec3{}, glm::vec4{0, 0, 0, 1}, glm::vec3{1, 1, 1});

                    std::unique_lock _{m_mutex};
                    m_spawned_spheres.insert(new_sphere);
                    m_spawned_spheres_to_components[new_sphere] = mesh;
                    m_components_with_spheres.insert(mesh);
                }
            }
        } else {
            SPDLOG_ERROR("[UObjectHook] Failed to spawn actor for overlapper");
        }
    });
}

void UObjectHook::destroy_overlapper() {
    GameThreadWorker::get().enqueue([this]() {
        // Destroy all spawned spheres
        auto spheres = get_spawned_spheres();

        for (auto sphere : spheres) {
            if (!this->exists(sphere)) {
                continue;
            }

            sphere->destroy_component();
        }

        {
            std::unique_lock _{m_mutex};
            m_spawned_spheres.clear();
            m_spawned_spheres_to_components.clear();
            m_components_with_spheres.clear();
        }

        if (m_overlap_detection_actor != nullptr && this->exists(m_overlap_detection_actor)) {
            m_overlap_detection_actor->destroy_actor();
            m_overlap_detection_actor = nullptr;
        }

        if (m_overlap_detection_actor_left != nullptr && this->exists(m_overlap_detection_actor_left)) {
            m_overlap_detection_actor_left->destroy_actor();
            m_overlap_detection_actor_left = nullptr;
        }
    });
}

std::filesystem::path UObjectHook::get_persistent_dir() const {
    const auto base_dir = Framework::get_persistent_dir();
    const auto uobjecthook_dir = base_dir / "uobjecthook";

    try {
        if (!std::filesystem::exists(uobjecthook_dir)) {
            std::filesystem::create_directories(uobjecthook_dir);
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[UObjectHook] Failed to create persistent directory: {}", e.what());
    } catch (...) {
        SPDLOG_ERROR("[UObjectHook] Failed to create persistent directory");
    }

    return uobjecthook_dir;
}

nlohmann::json UObjectHook::serialize_mc_state(const std::vector<std::string>& path, const std::shared_ptr<MotionControllerState>& state) {
    nlohmann::json result{};

    result["path"] = path;
    result["state"] = state->to_json();
    result["type"] = "motion_controller";

    return result;
}

nlohmann::json UObjectHook::serialize_camera(const std::vector<std::string>& path) {
    nlohmann::json result{};

    result["path"] = path;
    result["offset"] = utility::math::to_json(m_camera_attach.offset);
    result["type"] = "camera";

    // todo: adjustments/offsets, etc...? all it needs is the camera object which is fine

    return result;
}

void UObjectHook::save_camera_state(const std::vector<std::string>& path) {
    auto json = serialize_camera(path);

    const auto wanted_dir = UObjectHook::get_persistent_dir() / "camera_state.json";

    // Create dir if necessary
    try {
        std::filesystem::create_directories(wanted_dir.parent_path());

        if (std::filesystem::exists(wanted_dir.parent_path())) {
            std::ofstream file{wanted_dir};
            file << json.dump(4);
            file.close();

            m_persistent_camera_state = deserialize_camera_state();
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[UObjectHook] Failed to save camera state: {}", e.what());
    } catch (...) {
        SPDLOG_ERROR("[UObjectHook] Failed to save camera state");
    }
}

std::optional<UObjectHook::StatePath> UObjectHook::deserialize_path(const nlohmann::json& data) {
    if (!data.contains("path")) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (missing path)");
        return std::nullopt;
    }

    // make sure path is an array
    if (!data["path"].is_array()) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (path is not an array)");
        return std::nullopt;
    }

    const auto path = data["path"].get<std::vector<std::string>>();

    if (path.empty()) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (path is empty)");
        return std::nullopt;
    }

    return StatePath{path};
}

std::shared_ptr<UObjectHook::PersistentState> UObjectHook::deserialize_mc_state(nlohmann::json& data) {
    SPDLOG_INFO("[UObjectHook] inside deserialize_mc_state");

    if (!data.contains("path") || !data.contains("state")) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (missing path or state)");
        return nullptr;
    }

    // make sure path is an array
    if (!data["path"].is_array()) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (path is not an array)");
        return nullptr;
    }

    // make sure state is an object
    if (!data["state"].is_object()) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (state is not an object)");
        return nullptr;
    }

    if (data.contains("type") && data["type"].is_string()) {
        const auto type = data["type"].get<std::string>();

        if (type != "motion_controller") {
            SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (type is not motion_controller)");
            return nullptr;
        }
    }

    SPDLOG_INFO("[UObjectHook] Deserializing state path...");
    auto path = data["path"].get<std::vector<std::string>>();

    auto persistent_state = std::make_shared<PersistentState>();
    persistent_state->path = path;

    SPDLOG_INFO("[UObjectHook] Deserializing state...");
    persistent_state->state.from_json(data["state"]);

    return persistent_state;
}

std::shared_ptr<UObjectHook::PersistentState> UObjectHook::deserialize_mc_state(std::filesystem::path json_path) {
    if (!std::filesystem::exists(json_path)) {
        return nullptr;
    }

    try {
        auto f = std::ifstream{json_path};

        if (f.is_open()) {
            // Log the file data to make sure we're getting it correctly...
            const auto file_contents = std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};

            SPDLOG_INFO("[UObjectHook] JSON file contents:");
            SPDLOG_INFO("{}", file_contents);

            nlohmann::json data = nlohmann::json::parse(file_contents);

            return deserialize_mc_state(data);
        }

        SPDLOG_ERROR("[UObjectHook] Failed to open JSON file {}", json_path.string());
        return nullptr;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}: {}", json_path.string(), e.what());
    } catch (...) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}", json_path.string());
    }

    return nullptr;
}

std::vector<std::shared_ptr<UObjectHook::PersistentState>> UObjectHook::deserialize_all_mc_states() try {
    const auto uobjecthook_dir = get_persistent_dir();

    if (!std::filesystem::exists(uobjecthook_dir)) {
        return {};
    }
    
    // Gather all .json files in this directory
    std::vector<std::filesystem::path> json_files{};
    for (const auto& p : std::filesystem::directory_iterator(uobjecthook_dir)) {
        if (p.path().extension() == ".json") {
            json_files.push_back(p.path());
        }
    }

    std::vector<std::shared_ptr<PersistentState>> result{};
    for (const auto& json_file : json_files) {
        auto state = deserialize_mc_state(json_file);

        if (state != nullptr) {
            result.push_back(state);
        }
    }

    return result;
} catch (const std::exception& e) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize all motion controller states: {}", e.what());
    return {};
} catch (...) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize all motion controller states");
    return {};
}

std::shared_ptr<UObjectHook::PersistentCameraState> UObjectHook::deserialize_camera(const nlohmann::json& data) {
    const auto path = deserialize_path(data);

    if (!path.has_value()) {
        SPDLOG_ERROR("[UObjectHook] Failed to deserialize camera path");
        return nullptr;
    }

    if (!data.contains("type") || !data["type"].is_string()) {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (missing type)");
        return nullptr;
    }

    if (data["type"].get<std::string>() != "camera") {
        SPDLOG_ERROR("[UObjectHook] Malfomed JSON file (type is not camera)");
        return nullptr;
    }

    auto persistent_state = std::make_shared<PersistentCameraState>();
    persistent_state->path = path.value();

    if (data.contains("offset") && data["offset"].is_object()) {
        persistent_state->offset = utility::math::from_json_vec3(data["offset"]);
    }

    return persistent_state;
}

std::shared_ptr<UObjectHook::PersistentCameraState> UObjectHook::deserialize_camera_state() {
    // Look for camera_state.json
    const auto uobjecthook_dir = get_persistent_dir();
    const auto camera_state_path = uobjecthook_dir / "camera_state.json";

    if (!std::filesystem::exists(camera_state_path)) {
        SPDLOG_ERROR("[UObjectHook] Failed to find camera_state.json");
        return nullptr;
    }

    try {
        auto f = std::ifstream{camera_state_path};

        if (f.is_open()) {
            // Log the file data to make sure we're getting it correctly...
            const auto file_contents = std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};

            SPDLOG_INFO("[UObjectHook] JSON file contents:");
            SPDLOG_INFO("{}", file_contents);

            nlohmann::json data = nlohmann::json::parse(file_contents);

            return deserialize_camera(data);
        }

        SPDLOG_ERROR("[UObjectHook] Failed to open JSON file {}", camera_state_path.string());
        return nullptr;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}: {}", camera_state_path.string(), e.what());
    } catch (...) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}", camera_state_path.string());
    }

    return nullptr;
}

void UObjectHook::update_persistent_states() {
    // Camera state
    if (m_persistent_camera_state != nullptr) {
        auto obj = m_persistent_camera_state->path.resolve();

        if (obj != nullptr) {
            m_camera_attach.object = obj;
            m_camera_attach.offset = m_persistent_camera_state->offset;
        }
    }

    // Motion controller states
    if (!m_persistent_states.empty()) {
        for (const auto& state : m_persistent_states) {
            if (state == nullptr) {
                continue;
            }

            auto obj = state->path.resolve();

            if (obj == nullptr) {
                continue;
            }

            static const auto scene_component_t = sdk::USceneComponent::static_class();

            // TODO? will need some reworking to support properties from arbitrary objects
            if (!obj->get_class()->is_a(scene_component_t)) {
                continue;
            }

            auto mc_state = get_or_add_motion_controller_state((sdk::USceneComponent*)obj);

            if (mc_state == nullptr) {
                continue;
            }

            if (mc_state->adjusting) {
                state->state.location_offset = mc_state->location_offset;
                state->state.rotation_offset = mc_state->rotation_offset;
                state->state.hand = mc_state->hand;
            } else {
                mc_state->location_offset = state->state.location_offset;
                mc_state->rotation_offset = state->state.rotation_offset;
                mc_state->hand = state->state.hand;
            }
        }
    }

    // Persistent properties
    if (!m_persistent_properties.empty()) {
        for (const auto& prop_base : m_persistent_properties) {
            if (prop_base == nullptr) {
                continue;
            }

            auto obj = prop_base->path.resolve();

            if (obj == nullptr) {
                continue;
            }

            for (const auto& prop_state : prop_base->properties) {
                const auto prop_desc = obj->get_class()->find_property(prop_state->name);
            
                if (prop_desc == nullptr) {
                    continue;
                }

                const auto prop_t = prop_desc->get_class();

                if (prop_t == nullptr) {
                    continue;
                }

                const auto prop_t_name = prop_t->get_name().to_string();

                switch (utility::hash(utility::narrow(prop_t_name))) {
                case "FloatProperty"_fnv:
                    {
                        auto& value = *(float*)((uintptr_t)obj + ((sdk::FProperty*)prop_desc)->get_offset());
                        value = prop_state->data.f;
                    }
                    break;
                case "DoubleProperty"_fnv:
                    {
                        auto& value = *(double*)((uintptr_t)obj + ((sdk::FProperty*)prop_desc)->get_offset());
                        value = prop_state->data.d;
                    }
                    break;
                case "IntProperty"_fnv:
                    {
                        auto& value = *(int32_t*)((uintptr_t)obj + ((sdk::FProperty*)prop_desc)->get_offset());
                        value = prop_state->data.i;
                    }
                    break;
                case "BoolProperty"_fnv:
                    {
                        auto boolprop = (sdk::FBoolProperty*)prop_desc;
                        boolprop->set_value_in_object(obj, prop_state->data.b);
                    }
                    break;
                default:
                    // OH NO!!!!! anyways
                    break;
                };
            }
        }
    }
}

sdk::UObject* UObjectHook::StatePath::resolve_base_object() const {
    if (!this->has_valid_base()) {
        return nullptr;
    }

    auto engine = sdk::UGameEngine::get();
    if (engine == nullptr) {
        return nullptr;
    }

    // TODO: Convert these into an enum or something when we initially parse the JSON file.
    switch (utility::hash(m_path.front())) {
    case "Acknowledged Pawn"_fnv:
    {
        auto world = engine->get_world();
        if (world == nullptr) {
            return nullptr;
        }

        auto player_controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

        if (player_controller == nullptr) {
            return nullptr;
        }
        
        return player_controller->get_acknowledged_pawn();
        break;
    }

    case "Player Controller"_fnv:
    {
        auto world = engine->get_world();
        if (world == nullptr) {
            return nullptr;
        }

        return sdk::UGameplayStatics::get()->get_player_controller(world, 0);
        break;
    }

    case "Camera Manager"_fnv:
    {      
        auto world = engine->get_world();
        if (world == nullptr) {
            return nullptr;
        }

        auto player_controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

        if (player_controller == nullptr) {
            return nullptr;
        }
        
        return player_controller->get_player_camera_manager();
        break;
    }

    case "World"_fnv:
    {
        return engine->get_world();
        break;
    }

    default:
        break;
    };

    return nullptr;
}

sdk::UObject* UObjectHook::StatePath::resolve() const {
    const auto base = resolve_base_object();

    if (base == nullptr) {
        return nullptr;
    }

    auto previous_object = base;

    for (auto it = m_path.begin() + 1; it != m_path.end(); ++it) {
        switch (utility::hash(*it)) {
        case "Components"_fnv:
        {
            // Make sure the base is an AActor
            static const auto actor_t = sdk::AActor::static_class();

            if (!previous_object->get_class()->is_a(actor_t)) {
                return nullptr;
            }

            const auto components = ((sdk::AActor*)previous_object)->get_all_components();

            if (components.empty()) {
                return nullptr;
            }

            auto next_it = it + 1;

            if (next_it == m_path.end()) {
                return nullptr;
            }

            for (auto comp : components) {
                const auto comp_name = comp->get_class()->get_fname().to_string() + L" " + comp->get_fname().to_string();

                if (utility::narrow(comp_name) == *next_it) {
                    previous_object = comp;
                    ++it;
                    break;
                }
            }

            break;
        }
        case "Properties"_fnv:
        {
            auto next_it = it + 1;

            if (next_it == m_path.end()) {
                return nullptr;
            }

            const auto prop_name = *next_it;
            const auto prop_desc = previous_object->get_class()->find_property(utility::widen(prop_name));

            if (prop_desc == nullptr) {
                return nullptr;
            }

            const auto prop_t = prop_desc->get_class();

            if  (prop_t == nullptr) {
                return nullptr;
            }

            const auto prop_t_name = prop_t->get_name().to_string();
            switch (utility::hash(utility::narrow(prop_t_name))) {
            case "ObjectProperty"_fnv:
            {
                const auto obj_ptr = prop_desc->get_data<sdk::UObject*>(previous_object);
                const auto obj = obj_ptr != nullptr ? *obj_ptr : nullptr;

                if (obj == nullptr) {
                    return nullptr;
                }

                previous_object = obj;
                ++it;
                break;
            }

            // Need to handle StructProperty and ArrayProperty but whatever.
            default:
                SPDLOG_ERROR("[UObjectHook] Unsupported persistent property type {}", utility::narrow(prop_t_name));
                break;
            };

            break;
        }

        default:
            return nullptr;
            break;
        };
    }

    return previous_object;
}

void UObjectHook::on_draw_ui() {
    activate();

    if (!m_fully_hooked) {
        ImGui::Text("Waiting for UObjectBase to be hooked...");
        return;
    }

    std::shared_lock _{m_mutex};

    if (ImGui::Button("Reload Persistent States")) {
        m_persistent_states = deserialize_all_mc_states();
        m_persistent_camera_state = deserialize_camera_state();
        m_persistent_properties = deserialize_all_persistent_properties();
    }

    ImGui::SameLine();

    if (ImGui::Button("Destroy Persistent States")) {
        m_persistent_states.clear();

        const auto uobjecthook_dir = get_persistent_dir();

        if (std::filesystem::exists(uobjecthook_dir)) {
            for (const auto& p : std::filesystem::directory_iterator(uobjecthook_dir)) {
                if (p.path().extension() == ".json") {
                    std::filesystem::remove(p.path());
                }
            }
        }
    }

    ImGui::Separator();

    if (!m_motion_controller_attached_components.empty()) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const auto made = ImGui::TreeNode("Attached Components");

        if (made) {
            if (ImGui::Button("Detach all")) {
                m_motion_controller_attached_components.clear();
            }

            // make a copy because the user could press the detach button while iterating
            auto attached = m_motion_controller_attached_components;

            for (auto& it : attached) {
                if (!this->exists_unsafe(it.first) || it.second == nullptr) {
                    continue;
                }

                auto comp = it.first;
                std::wstring comp_name = comp->get_class()->get_fname().to_string() + L" " + comp->get_fname().to_string();

                if (ImGui::TreeNode(utility::narrow(comp_name).data())) {
                    ui_handle_object(comp);

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }

    if (m_overlap_detection_actor == nullptr) {
        if (ImGui::Button("Spawn Overlapper")) {
            spawn_overlapper(0);
            spawn_overlapper(1);
        }
    } else if (!this->exists_unsafe(m_overlap_detection_actor)) {
        m_overlap_detection_actor = nullptr;
    } else {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const auto made = ImGui::TreeNode("Overlapped Objects");

        if (made) {
            if (ImGui::Button("Destroy Overlapper")) {
                destroy_overlapper();
            }

            ImGui::SameLine();
            bool attach_all = false;
            if (ImGui::Button("Attach all")) {
                attach_all = true;
            }

            auto overlapped_components = m_overlap_detection_actor->get_overlapping_components();

            for (auto& it : overlapped_components) {
                auto comp = (sdk::USceneComponent*)it;
                if (!this->exists_unsafe(comp)) {
                    continue;
                }

                if (m_spawned_spheres.contains(comp) && m_spawned_spheres_to_components.contains(comp)) {
                    comp = m_spawned_spheres_to_components[comp];
                }

                if (attach_all){ 
                    if (!m_motion_controller_attached_components.contains(comp)) {
                        m_motion_controller_attached_components[comp] = std::make_shared<MotionControllerState>();
                    }
                }

                std::wstring comp_name = comp->get_class()->get_fname().to_string() + L" " + comp->get_fname().to_string();

                if (ImGui::TreeNode(utility::narrow(comp_name).data())) {
                    ui_handle_object(comp);
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }

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

    // Display common objects like things related to the player
    if (ImGui::TreeNode("Common Objects")) {
        auto world = sdk::UGameEngine::get()->get_world();

        if (world != nullptr) {
            if (ImGui::TreeNode("PlayerController")) {
                auto scope = m_path.enter_clean("Player Controller");
                auto player_controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

                if (player_controller != nullptr) {
                    ui_handle_object(player_controller);
                } else {
                    ImGui::Text("No player controller");
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Acknowledged Pawn")) {
                auto scope = m_path.enter_clean("Acknowledged Pawn");
                auto player_controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

                if (player_controller != nullptr) {
                    auto pawn = player_controller->get_acknowledged_pawn();

                    if (pawn != nullptr) {
                        ui_handle_object(pawn);
                    } else {
                        ImGui::Text("No pawn");
                    }
                } else {
                    ImGui::Text("No player controller");
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Camera Manager")) {
                auto scope = m_path.enter_clean("Camera Manager");
                auto player_controller = sdk::UGameplayStatics::get()->get_player_controller(world, 0);

                if (player_controller != nullptr) {
                    auto camera_manager = player_controller->get_player_camera_manager();

                    if (camera_manager != nullptr) {
                        ui_handle_object((sdk::UObject*)camera_manager);
                    } else {
                        ImGui::Text("No camera manager");
                    }
                } else {
                    ImGui::Text("No player controller");
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("World")) {
                auto scope = m_path.enter_clean("World");
                ui_handle_object(world);
                ImGui::TreePop();
            }
        } else {
            ImGui::Text("No world");
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Objects by class")) {
        static char filter[256]{};
        ImGui::InputText("Filter", filter, sizeof(filter));

        const bool filter_empty = std::string_view{filter}.empty();

        const auto now = std::chrono::steady_clock::now();
        bool needs_sort = true;

        if (m_sorting_task.valid()) {
            // Check if the sorting task is finished
            if (m_sorting_task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                // Do something if needed when sorting is done
                m_sorted_classes = m_sorting_task.get();
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
            m_sorting_task = std::async(std::launch::async, sort_classes, unsorted_classes);
            m_last_sort_time = now;
        }

        const auto wide_filter = utility::widen(filter);
        const bool made_child = ImGui::BeginChild("Objects by class entries", ImVec2(0, 0), true, ImGuiWindowFlags_::ImGuiWindowFlags_HorizontalScrollbar);

        utility::ScopeGuard sg{[made_child]() {
            //if (made_child) {
                // well apparently BeginChild doesn't care about if it returned true or not so...
                // TODO: check this every time we update imgui?
                ImGui::EndChild();
            //}
        }};

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

void UObjectHook::ui_handle_object(sdk::UObject* object) {
    if (object == nullptr) {
        return;
    }

    const auto uclass = object->get_class();

    if (uclass == nullptr) {
        return;
    }

    ImGui::Text("%s", utility::narrow(object->get_full_name()).data());

    static const auto material_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.MaterialInterface");

    if (uclass->is_a(material_t)) {
        ui_handle_material_interface(object);
    }

    static const auto widget_component_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/UMG.WidgetComponent");

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
        /*if (ImGui::Button("Destroy Component")) {
            auto comp = (sdk::UActorComponent*)object;

            comp->destroy_component();
        }*/
    }

    if (uclass->is_a(sdk::USceneComponent::static_class())) {
        ui_handle_scene_component((sdk::USceneComponent*)object);
    }

    if (uclass->is_a(sdk::AActor::static_class())) {
        ui_handle_actor(object);
    }

    ui_handle_struct(object, uclass);
}

void UObjectHook::ui_handle_scene_component(sdk::USceneComponent* comp) {
    if (comp == nullptr) {
        return;
    }

    bool attached = m_motion_controller_attached_components.contains(comp);

    if (attached) {
        if (ImGui::Button("Detach")) {
            m_motion_controller_attached_components.erase(comp);
        }

        if (m_motion_controller_attached_components.contains(comp)) {
            ImGui::SameLine();
            auto& state = m_motion_controller_attached_components[comp];

            if (ImGui::Checkbox("Adjust", &state->adjusting)) {
                if (state->adjusting && m_overlap_detection_actor == nullptr) {
                    VR::get()->set_aim_allowed(false);
                    g_framework->set_draw_ui(false);
                }
            }

            if (ImGui::Checkbox("Permanent Change", &state->permanent)) {

            }

            auto save_state_logic = [&](const std::vector<std::string>& path) {
                auto json = serialize_mc_state(path, state);

                // Concat the entire path together and hash it to get a unique name
                std::string concat_path{};
                for (const auto& p : path) {
                    concat_path += p;
                }

                const auto hash_str = std::to_string(utility::hash(concat_path)) + "_mc_state.json";
                const auto wanted_dir = UObjectHook::get_persistent_dir() / hash_str;

                // Create dir if necessary
                try {
                    std::filesystem::create_directories(wanted_dir.parent_path());

                    if (std::filesystem::exists(wanted_dir.parent_path())) {
                        std::ofstream file{wanted_dir};
                        file << json.dump(4);
                        file.close();

                        m_persistent_states = deserialize_all_mc_states();
                    }
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("[UObjectHook] Failed to save motion controller state: {}", e.what());
                } catch (...) {
                    SPDLOG_ERROR("[UObjectHook] Failed to save motion controller state");
                }
            };

            if (m_path.has_valid_base()) {
                ImGui::SameLine();
                if (ImGui::Button("Save state")) {
                    save_state_logic(m_path.path());
                }
            } else {
                const auto component_name = utility::narrow(comp->get_class()->get_fname().to_string() + L" " + comp->get_fname().to_string());

                // Check if any of our bases can reach this component through their components list.
                bool found_any = false;
                for (const auto& allowed_base : s_allowed_bases) {
                    const auto possible_path = std::vector<std::string>{allowed_base, "Components", component_name};
                    const auto path = StatePath{possible_path};
                    const auto resolved = path.resolve();

                    if (resolved == comp) {
                        if (ImGui::Button("Save state")) {
                            save_state_logic(path.path());
                        }
                        
                        found_any = true;
                        break;
                    }
                }

                if (!found_any) {
                    ImGui::Text("Can't save, did not start from a valid base or none of the allowed bases can reach this component");
                }
            }
        }
    } else {
        if (m_camera_attach.object != comp) {
            if (ImGui::Button("Attach left")) {
                m_motion_controller_attached_components[comp] = std::make_shared<MotionControllerState>();
                m_motion_controller_attached_components[comp]->hand = 0;
            }

            ImGui::SameLine();

            if (ImGui::Button("Attach right")) {
                m_motion_controller_attached_components[comp] = std::make_shared<MotionControllerState>();
                m_motion_controller_attached_components[comp]->hand = 1;
            }

            if (ImGui::Button("Attach Camera to")) {
                m_camera_attach.object = comp;
                m_camera_attach.offset = glm::vec3{0.0f, 0.0f, 0.0f};
            }

            ImGui::SameLine();

            if (ImGui::Button("Attach Camera to (Relative)")) {
                m_camera_attach.object = comp;
                m_camera_attach.offset = glm::vec3{0.0f, 0.0f, m_last_camera_location.z - comp->get_world_location().z};
            }
        } else {
            if (ImGui::Button("Detach")) {
                m_camera_attach.object = nullptr;
                m_camera_attach.offset = glm::vec3{0.0f, 0.0f, 0.0f};
            }

            ImGui::SameLine();

            if (ImGui::Button("Save state")) {
                save_camera_state(m_path.path());
            }

            if (ImGui::DragFloat3("Camera Offset", &m_camera_attach.offset.x, 0.1f)) {
                if (m_persistent_camera_state != nullptr) {
                    m_persistent_camera_state->offset = m_camera_attach.offset;
                }
            }
        }
    }

    bool visible = comp->is_visible();

    if (ImGui::Checkbox("Visible", &visible)) {
        comp->set_visibility(visible, false);
    }
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
            auto comp = (sdk::UActorComponent*)comp_obj;
            if (comp->is_a(mesh_component_t)) {
                if (comp == comp->get_class()->get_class_default_object()) {
                    continue;
                }

                auto owner = comp->get_owner();

                if (owner == nullptr || owner->get_class()->get_class_default_object() == owner) {
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

    auto actor = (sdk::AActor*)object;

    if (m_camera_attach.object != object ){
        if (ImGui::Button("Attach Camera to")) {
            m_camera_attach.object = object;
            m_camera_attach.offset = glm::vec3{0.0f, 0.0f, 0.0f};
        }

        ImGui::SameLine();

        if (ImGui::Button("Attach Camera to (Relative)")) {
            m_camera_attach.object = object;
            m_camera_attach.offset = glm::vec3{0.0f, 0.0f, m_last_camera_location.z - actor->get_actor_location().z};
        }
    } else {
        if (ImGui::Button("Detach")) {
            m_camera_attach.object = nullptr;
            m_camera_attach.offset = glm::vec3{0.0f, 0.0f, 0.0f};
        }

        if (ImGui::Button("Save state")) {
            save_camera_state(m_path.path());
        }

        if (ImGui::DragFloat3("Camera Offset", &m_camera_attach.offset.x, 0.1f)) {
            if (m_persistent_camera_state != nullptr) {
                m_persistent_camera_state->offset = m_camera_attach.offset;
            }
        }
    }

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
        auto scope = m_path.enter("Components");
        auto components = actor->get_all_components();

        std::sort(components.begin(), components.end(), [](sdk::UObject* a, sdk::UObject* b) {
            return a->get_full_name() < b->get_full_name();
        });

        for (auto comp : components) {
            auto comp_obj = (sdk::UObject*)comp;

            ImGui::PushID(comp_obj);
            // not using full_name because its HUGE
            std::wstring comp_name = comp->get_class()->get_fname().to_string() + L" " + comp->get_fname().to_string();
            const auto made = ImGui::TreeNode(utility::narrow(comp_name).data());

            if (made) {
                auto scope2 = m_path.enter(utility::narrow(comp_name));
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
    auto previous_path = m_path;

    auto scope = m_path.enter("Properties");

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

        if (object == nullptr) {
            const auto name = utility::narrow(propc->get_name().to_string());
            const auto field_name = utility::narrow(prop->get_field_name().to_string());
            ImGui::Text("%s %s", name.data(), field_name.data());

            continue;
        }

        const auto hash_type = utility::hash(propc_type);

        // Right-click lambda for supported properties, usually for saving.
        auto display_context = [&](auto value) {
            if (!ImGui::BeginPopupContextItem()) {
                return;
            }

            if (!previous_path.has_valid_base()) {
                ImGui::Text("Can't save, did not start from a valid base");
                ImGui::EndPopup();
                return;
            }

            auto save_logic = [&](bool unsave = false) {
                const auto field_name = prop->get_field_name().to_string();
                std::shared_ptr<PersistentProperties> props{};

                // Find existing one if possible
                for (const auto& existing_prop : m_persistent_properties) {
                    if (existing_prop->path.resolve() == object) {
                        props = existing_prop;
                        break;
                    }
                }

                // Add new one if necessary
                if (props == nullptr) {
                    props = std::make_shared<PersistentProperties>();
                    props->path = StatePath{previous_path.path()};
                    m_persistent_properties.push_back(props);
                }

                // Add property to list if needed
                std::shared_ptr<PersistentProperties::PropertyState> state{};

                for (const auto& existing_state : props->properties) {
                    if (existing_state->name == prop->get_field_name().to_string()) {
                        state = existing_state;
                        break;
                    }
                }

                // Add new one if necessary
                if (state == nullptr) {
                    state = std::make_shared<PersistentProperties::PropertyState>();
                    state->name = prop->get_field_name().to_string();
                    props->properties.push_back(state);
                }

                memcpy(&state->data, &value, sizeof(value));

                if (unsave) {
                    props->properties.erase(
                        std::remove(props->properties.begin(), props->properties.end(), state), 
                        props->properties.end()
                    );
                }
                
                // Concat the entire path together and hash it to get a unique name
                std::string concat_path{};
                for (const auto& p : previous_path.path()) {
                    concat_path += p;
                }

                const auto hash_str = std::to_string(utility::hash(concat_path)) + "_props.json";
                const auto wanted_path = UObjectHook::get_persistent_dir() / hash_str;

                // Create dir if necessary
                try {
                    std::filesystem::create_directories(wanted_path.parent_path());

                    if (props->properties.empty()) {
                        // Delete the file if it exists. Happens if we unsave.
                        if (std::filesystem::exists(wanted_path)) {
                            std::filesystem::remove(wanted_path);
                        }

                        // Delete the property entry from m_peristent_properties.
                        m_persistent_properties.erase(
                            std::remove(m_persistent_properties.begin(), m_persistent_properties.end(), props), 
                            m_persistent_properties.end()
                        );

                        return;
                    }

                    if (std::filesystem::exists(wanted_path.parent_path())) {
                        std::ofstream file{wanted_path};
                        const auto j = props->to_json();
                        file << j.dump(4);
                        file.close();
                    }
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("[UObjectHook] Failed to save persistent properties: {}", e.what());
                } catch (...) {
                    SPDLOG_ERROR("[UObjectHook] Failed to save persistent properties");
                }
            };

            if (ImGui::Button("Save Property")) {
                save_logic();
            }

            if (ImGui::Button("Unsave Property")) {
                save_logic(true);
            }

            ImGui::EndPopup();
        };

        switch (hash_type) {
        case "FloatProperty"_fnv:
            {
                auto& value = *(float*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragFloat(utility::narrow(prop->get_field_name().to_string()).data(), &value, 0.01f);
                display_context(value);
            }
            break;
        case "DoubleProperty"_fnv:
            {
                auto& value = *(double*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragFloat(utility::narrow(prop->get_field_name().to_string()).data(), (float*)&value, 0.01f);
                display_context(value);
            }
            break;
        case "IntProperty"_fnv:
            {
                auto& value = *(int32_t*)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                ImGui::DragInt(utility::narrow(prop->get_field_name().to_string()).data(), &value, 1);
                display_context(value);
            }
            break;
        case "BoolProperty"_fnv:
            {
                auto boolprop = (sdk::FBoolProperty*)prop;
                auto value = boolprop->get_value_from_object(object);
                if (ImGui::Checkbox(utility::narrow(prop->get_field_name().to_string()).data(), &value)) {
                    boolprop->set_value_in_object(object, value);
                }
                display_context(value);
            }
            break;
        case "ObjectProperty"_fnv:
            {
                auto& value = *(sdk::UObject**)((uintptr_t)object + ((sdk::FProperty*)prop)->get_offset());
                
                if (ImGui::TreeNode(utility::narrow(prop->get_field_name().to_string()).data())) {
                    auto scope2 = m_path.enter(utility::narrow(prop->get_field_name().to_string()));
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
        case "ArrayProperty"_fnv:
            if (ImGui::TreeNode(utility::narrow(prop->get_field_name().to_string()).data())) {
                ui_handle_array_property(object, (sdk::FArrayProperty*)prop);
                ImGui::TreePop();
            }

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

void UObjectHook::ui_handle_array_property(void* addr, sdk::FArrayProperty* prop) {
    if (addr == nullptr || prop == nullptr) {
        return;
    }

    const auto& array_generic = *(sdk::TArray<void*>*)((uintptr_t)addr + prop->get_offset());

    if (array_generic.data == nullptr || array_generic.count == 0) {
        ImGui::Text("Empty array");
        return;
    }

    const auto inner = prop->get_inner();

    if (inner == nullptr) {
        ImGui::Text("Failed to get inner property");
        return;
    }
    
    const auto inner_c = inner->get_class();

    if (inner_c == nullptr) {
        ImGui::Text("Failed to get inner property class");
        return;
    }

    const auto inner_c_type = utility::narrow(inner_c->get_name().to_string());

    switch (utility::hash(inner_c_type)) {
    case "ObjectProperty"_fnv:
    {
        const auto& array_obj = *(sdk::TArray<sdk::UObject*>*)((uintptr_t)addr + prop->get_offset());

        for (auto obj : array_obj) {
            std::wstring name = obj->get_class()->get_fname().to_string() + L" " + obj->get_fname().to_string();

            if (ImGui::TreeNode(utility::narrow(name).data())) {
                ui_handle_object(obj);
                ImGui::TreePop();
            }
        }

        break;
    }

    default:
        ImGui::Text("Array of %s (unsupported)", inner_c_type.data());
        break;
    };
}

void UObjectHook::ui_handle_struct(void* addr, sdk::UStruct* uclass) {
    if (uclass == nullptr) {
        return;
    }

    if (addr != nullptr && this->exists_unsafe((sdk::UObject*)addr) && uclass->is_a(sdk::UStruct::static_class())) {
        uclass = (sdk::UStruct*)addr;
        addr = nullptr;
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

    ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
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
            hook->m_motion_controller_attached_components.erase((sdk::USceneComponent*)object);
            hook->m_spawned_spheres.erase((sdk::USceneComponent*)object);
            hook->m_spawned_spheres_to_components.erase((sdk::USceneComponent*)object);
            hook->m_components_with_spheres.erase((sdk::USceneComponent*)object);

            if (object == hook->m_overlap_detection_actor) {
                hook->m_overlap_detection_actor = nullptr;
            }

            if (object == hook->m_overlap_detection_actor_left) {
                hook->m_overlap_detection_actor_left = nullptr;
            }

            if (object == hook->m_camera_attach.object) {
                hook->m_camera_attach.object = nullptr;
            }

            for (auto super = (sdk::UStruct*)it->second->uclass; super != nullptr;) {
                hook->m_objects_by_class[(sdk::UClass*)super].erase(object);

                // Just make sure we don't do any operations on super because it might be invalid...
                if (!hook->m_objects.contains(super)) {
                    //SPDLOG_ERROR("Super for {:x} is not valid", (uintptr_t)object);
                    break;
                }

                super = super->get_super_struct();
            }

            //hook->m_objects_by_class[it->second->uclass].erase(object);

            hook->m_reusable_meta_objects.push_back(std::move(it->second));
            hook->m_meta_objects.erase(object);
        }
    }

    auto result = hook->m_destructor_hook.unsafe_call<void*>(object, rdx, r8, r9);

    return result;
}

nlohmann::json UObjectHook::PersistentProperties::to_json() const {
    nlohmann::json json{};

    json["path"] = path.path();
    json["properties"] = nlohmann::json::array();
    json["type"] = "properties";

    for (const auto& prop : properties) {
        json["properties"].push_back({
            {"name", utility::narrow(prop->name)},
            {"data", prop->data.u64}
        });
    }

    return json;
}

std::shared_ptr<UObjectHook::PersistentProperties> UObjectHook::PersistentProperties::from_json(const nlohmann::json& json) try {
    if (!json.contains("path") || !json.contains("properties") || !json.contains("type")) {
        throw std::runtime_error("Missing path or properties");
    }

    // Make sure we're loading the right type
    if (!json["type"].is_string() || json["type"].get<std::string>() != "properties") {
        throw std::runtime_error("Wrong type");
    }

    auto result = std::make_shared<UObjectHook::PersistentProperties>();

    result->path = StatePath{json["path"].get<std::vector<std::string>>()};
    result->properties.clear();

    for (const auto& prop : json["properties"]) {
        if (!prop.contains("name") || !prop.contains("data")) {
            throw std::runtime_error("Missing name or data");
        }

        if (!prop["data"].is_number_unsigned()) {
            throw std::runtime_error("Data is not unsigned");
        }

        if (!prop["name"].is_string()) {
            throw std::runtime_error("Name is not string");
        }

        auto state = std::make_shared<PropertyState>();
        state->name = utility::widen(prop["name"].get<std::string>());
        state->data.u64 = prop["data"].get<uint64_t>();
        result->properties.push_back(state);
    }

    return result;
} catch (const std::exception& e) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize persistent properties: {}", e.what());
    return nullptr;
} catch (...) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize persistent properties");
    return nullptr;
}

std::shared_ptr<UObjectHook::PersistentProperties> UObjectHook::PersistentProperties::from_json(std::filesystem::path json_path) {
    if (!std::filesystem::exists(json_path)) {
        return nullptr;
    }

    try {
        auto f = std::ifstream{json_path};

        if (f.is_open()) {
            const auto file_contents = std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};

            nlohmann::json data = nlohmann::json::parse(file_contents);

            return UObjectHook::PersistentProperties::from_json(data);
        }

        SPDLOG_ERROR("[UObjectHook] Failed to open JSON file {}", json_path.string());
        return nullptr;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}: {}", json_path.string(), e.what());
    } catch (...) {
        SPDLOG_ERROR("[UObjectHook] Failed to parse JSON file {}", json_path.string());
    }

    return nullptr;
}


std::vector<std::shared_ptr<UObjectHook::PersistentProperties>> UObjectHook::deserialize_all_persistent_properties() const try {
    const auto uobjecthook_dir = get_persistent_dir();

    if (!std::filesystem::exists(uobjecthook_dir)) {
        return {};
    }
    
    // Gather all .json files in this directory
    std::vector<std::filesystem::path> json_files{};
    for (const auto& p : std::filesystem::directory_iterator(uobjecthook_dir)) {
        if (p.path().extension() == ".json") {
            json_files.push_back(p.path());
        }
    }

    std::vector<std::shared_ptr<UObjectHook::PersistentProperties>> result{};
    for (const auto& json_file : json_files) {
        // load file
        auto state = UObjectHook::PersistentProperties::from_json(json_file);

        if (state != nullptr) {
            result.push_back(state);
        }
    }

    return result;
} catch (const std::exception& e) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize all persistent properties: {}", e.what());
    return {};
} catch (...) {
    SPDLOG_ERROR("[UObjectHook] Failed to deserialize all persistent properties");
    return {};
}