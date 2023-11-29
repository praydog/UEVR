#include <vector>
#include <utility/String.hpp>

#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "UCameraComponent.hpp"
#include "TArray.hpp"
#include "FMalloc.hpp"
#include "UGameplayStatics.hpp"
#include "UFunction.hpp"
#include "FField.hpp"
#include "FProperty.hpp"
#include "UGameEngine.hpp"

#include "AActor.hpp"

namespace sdk {
UClass* AActor::static_class() {
    static auto result = sdk::find_uobject<UClass>(L"Class /Script/Engine.Actor");
    return result;
}

bool AActor::set_actor_location(const glm::vec3& location, bool sweep, bool teleport) {
    static const auto func = static_class()->find_function(L"K2_SetActorLocation");

    if (func == nullptr) {
        return false;
    }

    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");
    const auto fvector = sdk::ScriptVector::static_struct();

    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    // Need to dynamically allocate the params because of unknown FHitResult size
    std::vector<uint8_t> params{};

    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), (uint8_t*)&location, (uint8_t*)&location + sizeof(glm::vec3));
    } else {
        glm::vec<3, double> loc = location;
        params.insert(params.end(), (uint8_t*)&loc, (uint8_t*)&loc + sizeof(glm::vec<3, double>));
    }
    // add a bool
    params.insert(params.end(), (uint8_t*)&sweep, (uint8_t*)&sweep + sizeof(bool));
    // align    
    params.insert(params.end(), 3, 0);
    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);
    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));
    // add a bool
    bool ret = false;
    void* ret_ptr = &ret;
    params.insert(params.end(), (uint8_t*)&ret_ptr, (uint8_t*)&ret_ptr + sizeof(void*));

    this->process_event(func, params.data());

    return ret;
}

glm::vec3 AActor::get_actor_location() {
    static const auto func = static_class()->find_function(L"K2_GetActorLocation");

    if (func == nullptr) {
        return glm::vec3{0.0f, 0.0f, 0.0f};
    }

    const auto fvector = sdk::ScriptVector::static_struct();

    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    std::vector<uint8_t> params{};

    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), sizeof(glm::vec3), 0);
    } else {
        params.insert(params.end(), sizeof(glm::vec<3, double>), 0);
    }

    this->process_event(func, params.data());

    if (!is_ue5) {
        return *(glm::vec3*)params.data();
    }
        
    return *(glm::vec<3, double>*)params.data();
}
bool AActor::set_actor_rotation(const glm::vec3& rotation, bool teleport) {
    static const auto func = static_class()->find_function(L"K2_SetActorRotation");

    if (func == nullptr) {
        return false;
    }

    const auto frotator = sdk::ScriptRotator::static_struct();

    const auto is_ue5 = frotator->get_struct_size() == sizeof(glm::vec<3, double>);

    // Need to dynamically allocate the params because of unknown FRotator size
    std::vector<uint8_t> params{};
    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), (uint8_t*)&rotation, (uint8_t*)&rotation + sizeof(glm::vec3));
    } else {
        glm::vec<3, double> rot = rotation;
        params.insert(params.end(), (uint8_t*)&rot, (uint8_t*)&rot + sizeof(glm::vec<3, double>));
    }

    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));

    // align
    //params.insert(params.end(), 3, 0);

    // add a bool
    bool ret = false;
    void* ret_ptr = &ret;
    params.insert(params.end(), (uint8_t*)&ret_ptr, (uint8_t*)&ret_ptr + sizeof(void*));

    this->process_event(func, params.data());

    return ret;
};

glm::vec3 AActor::get_actor_rotation() {
    static const auto func = static_class()->find_function(L"K2_GetActorRotation");

    if (func == nullptr) {
        return glm::vec3{0.0f, 0.0f, 0.0f};
    }

    const auto frotator = sdk::ScriptRotator::static_struct();

    const auto is_ue5 = frotator->get_struct_size() == sizeof(glm::vec<3, double>);

    std::vector<uint8_t> params{};

    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), sizeof(glm::vec3), 0);
    } else {
        params.insert(params.end(), sizeof(glm::vec<3, double>), 0);
    }

    this->process_event(func, params.data());

    if (!is_ue5) {
        return *(glm::vec3*)params.data();
    }

    return *(glm::vec<3, double>*)params.data();
}

USceneComponent* AActor::get_component_by_class(UClass* uclass) {
    static const auto func = AActor::static_class()->find_function(L"GetComponentByClass");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        sdk::UClass* uclass;
        sdk::USceneComponent* ret;
    } params;

    params.uclass = uclass;
    params.ret = nullptr;

    this->process_event(func, &params);

    return params.ret;
}

UCameraComponent* AActor::get_camera_component() {
    return (UCameraComponent*)get_component_by_class(UCameraComponent::static_class());
}

struct FTransform {
    glm::vec4 Rotation{0.0f, 0.0f, 0.0f, 1.0f}; // quat
    glm::vec4 Translation{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 Scale3D{1.0f, 1.0f, 1.0f, 1.0f};
};

struct FTransformUE5 {
    glm::vec<4, double> Rotation{0.0, 0.0, 0.0, 1.0}; // quat
    glm::vec<4, double> Translation{0.0, 0.0, 0.0, 1.0};
    glm::vec<4, double> Scale3D{1.0, 1.0, 1.0, 1.0};
};

UActorComponent* AActor::add_component_by_class(UClass* uclass, bool deferred) {
    static const auto func = AActor::static_class()->find_function(L"AddComponentByClass");

    if (func == nullptr) {
        return add_component_by_class_ex(uclass, deferred);
    }

    std::vector<uint8_t> params{};
    
    // Add a uclass pointer
    params.insert(params.end(), (uint8_t*)&uclass, (uint8_t*)&uclass + sizeof(UClass*));

    // Add a bool
    bool bManualAttachment = false;
    params.insert(params.end(), (uint8_t*)&bManualAttachment, (uint8_t*)&bManualAttachment + sizeof(bool));

    // Align
    params.insert(params.end(), 7, 0);

    const auto fvector = sdk::ScriptVector::static_struct();
    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    // Add a FTransform or FTransformUE5
    if (!is_ue5) {
        FTransform transform{};
        params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransform));
    } else {
        FTransformUE5 transform{};
        params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransformUE5));
    }

    // Add a bool
    params.insert(params.end(), (uint8_t*)&deferred, (uint8_t*)&deferred + sizeof(bool));

    // Align
    params.insert(params.end(), 7, 0);
    
    const auto ret_offset = params.size();

    // Add a ucomponent pointer
    UActorComponent* ret = nullptr;
    params.insert(params.end(), (uint8_t*)&ret, (uint8_t*)&ret + sizeof(UActorComponent*));

    this->process_event(func, params.data());

    return *(UActorComponent**)(params.data() + ret_offset);
}

UActorComponent* AActor::add_component_by_class_ex(UClass* uclass, bool deferred) {
    auto ugs = sdk::UGameplayStatics::get();

    if (ugs == nullptr) {
        return nullptr;
    }

    auto new_comp = ugs->spawn_object(uclass, this);

    if (new_comp == nullptr) {
        return nullptr;
    }

    if (!deferred) {
        this->finish_add_component(new_comp);
    }

    return (UActorComponent*)new_comp;
}

void AActor::finish_add_component(sdk::UObject* component) {
    static const auto func = AActor::static_class()->find_function(L"FinishAddComponent");

    if (func == nullptr) {
        finish_add_component_ex(component);
        return;
    }

    static const auto fvector = sdk::ScriptVector::static_struct();
    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    /*struct {
        sdk::UObject* Component{}; // 0x0
        bool bManualAttachment{}; // 0x8
        char pad_9[0x7];
        FTransform RelativeTransform{}; // 0x10
    } params;

    params.Component = component;*/

    std::vector<uint8_t> params{};
    params.insert(params.end(), (uint8_t*)&component, (uint8_t*)&component + sizeof(UObject*));

    bool bManualAttachment = false;
    params.insert(params.end(), (uint8_t*)&bManualAttachment, (uint8_t*)&bManualAttachment + sizeof(bool));

    params.insert(params.end(), 7, 0);

    if (!is_ue5) {
        FTransform transform{};
        params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransform));
    } else {
        FTransformUE5 transform{};
        params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransformUE5));
    }
    
    /*std::vector<uint8_t> params{};
    uint32_t current_offset = 0;
    for (auto param = func->get_child_properties(); param != nullptr; param = param->get_next()) {
        auto pad_until = [&](uint32_t offset) {
            while (current_offset < offset) {
                params.push_back(0);
                ++current_offset;
            }
        };

        const auto param_prop = (sdk::FProperty*)param;
        const auto offset = param_prop->get_offset();

        pad_until(offset);

        switch (utility::hash(utility::narrow(param_prop->get_field_name().to_string()))) {
        case "Component"_fnv:
            params.insert(params.end(), (uint8_t*)&component, (uint8_t*)&component + sizeof(UObject*));
            current_offset += sizeof(UObject*);
            break;
        case "RelativeTransform"_fnv:
            if (!is_ue5) {
                FTransform transform{};
                params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransform));
            } else {
                FTransformUE5 transform{};
                params.insert(params.end(), (uint8_t*)&transform, (uint8_t*)&transform + sizeof(FTransformUE5));
            }

            break;
        default:
            if (param->get_next() == nullptr) {
                // add some padding
                pad_until(current_offset + 0x40);
            }

            break;
        };
    }*/

    this->process_event(func, params.data());
}

void AActor::finish_add_component_ex(sdk::UObject* new_comp) {
    if (new_comp == nullptr) {
        return;
    }

    // For older UE.
    const auto is_sc = new_comp->get_class()->is_a(sdk::USceneComponent::static_class());
    if (is_sc) {
        auto sc = (sdk::USceneComponent*)new_comp;

        if (get_root_component() == nullptr) {
            set_root_component(sc);
        } else {
            sc->attach_to(get_root_component());
        }

        sc->set_local_transform({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, false, false);
    }

    ((sdk::UActorComponent*)new_comp)->register_component_with_world(this->get_world());
}

std::vector<UActorComponent*> AActor::get_components_by_class(UClass* uclass) {
    static const auto func_candidate_1 = AActor::static_class()->find_function(L"K2_GetComponentsByClass");
    static const auto func_candidate_2 = AActor::static_class()->find_function(L"GetComponentsByClass");

    if (func_candidate_1 == nullptr && func_candidate_2 == nullptr) {
        return {};
    }

    auto func = func_candidate_1 != nullptr ? func_candidate_1 : func_candidate_2;

    struct Params_K2_GetComponentsByClass {
        UClass* ComponentClass{}; // 0x0
        TArray<UActorComponent*> ReturnValue{}; // 0x8
    }; // Size: 0x18

    Params_K2_GetComponentsByClass params{};
    params.ComponentClass = uclass;

    this->process_event(func, &params);

    std::vector<UActorComponent*> ret{};

    if (params.ReturnValue.data != nullptr) {
        for (int i = 0; i < params.ReturnValue.count; ++i) {
            ret.push_back(params.ReturnValue.data[i]);
        }
    }

    return ret;
}

std::vector<UActorComponent*> AActor::get_all_components() {
    static const auto actor_component_t = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.ActorComponent");

    if (actor_component_t == nullptr) {
        return {};
    }

    return get_components_by_class(actor_component_t);
}

void AActor::destroy_component(UActorComponent* component) {
    static const auto func = AActor::static_class()->find_function(L"K2_DestroyComponent");

    if (func == nullptr) {
        return;
    }

    struct {
        UActorComponent* Component;
    } params;

    params.Component = component;

    this->process_event(func, &params);
}

void AActor::destroy_actor() {
    static const auto func = AActor::static_class()->find_function(L"K2_DestroyActor");

    if (func == nullptr) {
        return;
    }

    struct {
        char padding[0x10]{};
    } params{};

    this->process_event(func, &params);
}

USceneComponent* AActor::get_root_component() {
    return get_property<USceneComponent*>(L"RootComponent");
}

void AActor::set_root_component(USceneComponent* component) {
    get_property<USceneComponent*>(L"RootComponent") = component;
}

UObject* AActor::get_level() {
    static const auto func = AActor::static_class()->find_function(L"GetLevel");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        UObject* return_value{nullptr};
    } params{};
    
    this->process_event(func, &params);

    return params.return_value;
}

UWorld* AActor::get_world() {
    auto level = get_level();

    if (level == nullptr) {
        return nullptr;
    }

    return level->get_property<UWorld*>(L"OwningWorld");
}

TArray<AActor*> AActor::get_overlapping_actors(UClass* uclass) {
    static const auto func = AActor::static_class()->find_function(L"GetOverlappingActors");

    if (func == nullptr) {
        return {};
    }

    struct {
        TArray<AActor*> out_actors{};
        UClass* actor_class{};
    } params{};

    params.actor_class = uclass;

    this->process_event(func, &params);

    return std::move(params.out_actors);
}

TArray<UPrimitiveComponent*> AActor::get_overlapping_components() {
    static const auto func = AActor::static_class()->find_function(L"GetOverlappingComponents");

    if (func == nullptr) {
        return {};
    }

    struct {
        TArray<UPrimitiveComponent*> out_components{};
    } params{};

    this->process_event(func, &params);

    return std::move(params.out_components);
}
}