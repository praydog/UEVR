#include <sdk/Math.hpp>

#include <vector>
#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "FProperty.hpp"

#include "USceneComponent.hpp"

namespace sdk {
UClass* USceneComponent::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.SceneComponent");
}

void USceneComponent::set_world_rotation(const glm::vec3& rotation, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_SetWorldRotation");
    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    if (fn == nullptr) {
        return;
    }

    const auto frotator = sdk::ScriptVector::static_struct();
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
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&sweep + sizeof(bool));
    // align
    params.insert(params.end(), 3, 0);
    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);
    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));

    this->process_event(fn, params.data());
}

void USceneComponent::add_world_rotation(const glm::vec3& rotation, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_AddWorldRotation");
    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    if (fn == nullptr) {
        return;
    }

    const auto frotator = sdk::ScriptVector::static_struct();
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
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&sweep + sizeof(bool));
    // align
    params.insert(params.end(), 3, 0);
    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);
    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));

    this->process_event(fn, params.data());
}

void USceneComponent::set_world_location(const glm::vec3& location, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_SetWorldLocation");
    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    if (fn == nullptr) {
        return;
    }

    const auto fvector = sdk::ScriptVector::static_struct();
    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    // Need to dynamically allocate the params because of unknown FVector size
    std::vector<uint8_t> params{};

    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), (uint8_t*)&location, (uint8_t*)&location + sizeof(glm::vec3));
    } else {
        glm::vec<3, double> loc = location;
        params.insert(params.end(), (uint8_t*)&loc, (uint8_t*)&loc + sizeof(glm::vec<3, double>));
    }

    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&sweep + sizeof(bool));
    // align
    params.insert(params.end(), 3, 0);
    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);
    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));

    this->process_event(fn, params.data());
}
}