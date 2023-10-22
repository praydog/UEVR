#include <vector>
#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "UCameraComponent.hpp"
#include "TArray.hpp"
#include "FMalloc.hpp"

#include "AActor.hpp"

namespace sdk {
UClass* UActorComponent::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.ActorComponent");
}

AActor* UActorComponent::get_owner() {
    static const auto func = UActorComponent::static_class()->find_function(L"GetOwner");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        AActor* ReturnValue;
    } params;

    params.ReturnValue = nullptr;

    this->process_event(func, &params);

    return params.ReturnValue;
}

void UActorComponent::destroy_component() {
    auto owner = this->get_owner();

    if (owner != nullptr) {
        owner->destroy_component(this);
    }
}

UClass* AActor::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.Actor");
}

bool AActor::set_actor_location(const glm::vec3& location, bool sweep, bool teleport) {
    static const auto func = static_class()->find_function(L"K2_SetActorLocation");
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


UActorComponent* AActor::add_component_by_class(UClass* uclass) {
    static const auto func = AActor::static_class()->find_function(L"AddComponentByClass");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        sdk::UClass* uclass;
        bool bManualAttachment{false};
        char pad_9[0x7];
        FTransform RelativeTransform{};
        bool bDeferredFinish{false};
        char pad_41[0x7];
        sdk::UActorComponent* ret{nullptr};
    } params;

    params.uclass = uclass;
    params.ret = nullptr;

    this->process_event(func, &params);

    return params.ret;
}

void AActor::finish_add_component(sdk::UObject* component) {
    static const auto func = AActor::static_class()->find_function(L"FinishAddComponent");

    if (func == nullptr) {
        return;
    }

    struct {
        sdk::UObject* Component{}; // 0x0
        bool bManualAttachment{}; // 0x8
        char pad_9[0x7];
        FTransform RelativeTransform{}; // 0x10
    } params;

    params.Component = component;

    this->process_event(func, &params);
}

std::vector<UActorComponent*> AActor::get_components_by_class(UClass* uclass) {
    static const auto func = AActor::static_class()->find_function(L"K2_GetComponentsByClass");

    if (func == nullptr) {
        return {};
    }

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
}