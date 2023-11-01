#include <sdk/Math.hpp>

#include <vector>
#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "FProperty.hpp"
#include "ScriptTransform.hpp"
#include "UFunction.hpp"

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

void USceneComponent::add_world_offset(const glm::vec3& location, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_AddWorldOffset");
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

void USceneComponent::add_local_rotation(const glm::vec3& rotation, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_AddLocalRotation");
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

void USceneComponent::set_local_transform(const glm::vec3& location, const glm::vec4& rotation, const glm::vec3& scale, bool sweep, bool teleport) {
    static auto fn = static_class()->find_function(L"K2_SetRelativeTransform");
    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    if (fn == nullptr) {
        return;
    }

    static const auto fvector = sdk::ScriptVector::static_struct();
    static const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    // Need to dynamically allocate the params because of unknown FVector size
    std::vector<uint8_t> params{};

    const auto transform_struct = sdk::ScriptTransform::create_dynamic_struct(location, rotation, scale);
    params.insert(params.end(), transform_struct.begin(), transform_struct.end());

    // add a bool
    params.insert(params.end(), (uint8_t*)&sweep, (uint8_t*)&sweep + sizeof(bool));

    // Pad until the offset of SweepHitResult
    static const auto sweep_hit_result_offset = fn->find_property(L"SweepHitResult")->get_offset();
    if (params.size() != sweep_hit_result_offset) {
        params.insert(params.end(), sweep_hit_result_offset - params.size(), 0);
    }

    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);

    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));

    this->process_event(fn, params.data());
}

glm::vec3 USceneComponent::get_world_location() {
    static const auto func = static_class()->find_function(L"K2_GetComponentLocation");
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

glm::vec3 USceneComponent::get_world_rotation() {
    static const auto func = static_class()->find_function(L"K2_GetComponentRotation");
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

bool USceneComponent::attach_to(USceneComponent* parent, const std::wstring& socket_name, uint8_t attach_type, bool weld) {
    static const auto func = static_class()->find_function(L"K2_AttachTo");

    if (func == nullptr) {
        return false;
    }

    /*struct {
        USceneComponent* parent{};
        FName socket_name{L"None"};
        uint8_t attach_type{};
        bool weld{};
        bool result{};
    } params{};

    params.parent = parent;
    params.socket_name = FName{socket_name};
    params.attach_type = attach_type;
    params.weld = weld;*/

    // Dynamically create params
    std::vector<uint8_t> params{};
    static const auto struct_size = func->find_property(L"bWeldSimulatedBodies")->get_offset() + (sizeof(bool) * 2);
    params.insert(params.end(), struct_size, 0);

    static const auto parent_offset = func->find_property(L"InParent")->get_offset();
    *(USceneComponent**)(params.data() + parent_offset) = parent;

    static const auto socket_name_offset = func->find_property(L"InSocketName")->get_offset();
    *(FName*)(params.data() + socket_name_offset) = FName{socket_name};

    static const auto attach_type_offset = func->find_property(L"AttachType")->get_offset();
    *(uint8_t*)(params.data() + attach_type_offset) = attach_type;

    static const auto weld_offset = func->find_property(L"bWeldSimulatedBodies")->get_offset();
    *(bool*)(params.data() + weld_offset) = weld;

    // add a bool
    params.insert(params.end(), sizeof(bool), 0);

    this->process_event(func, params.data());

    return (bool)params.back();
}

void USceneComponent::set_hidden_in_game(bool hidden, bool propagate) {
    static const auto func = static_class()->find_function(L"SetHiddenInGame");

    if (func == nullptr) {
        return;
    }

    struct {
        bool hidden{};
        bool propagate{};
    } params{};

    params.hidden = hidden;
    params.propagate = propagate;

    this->process_event(func, &params);
}

bool USceneComponent::is_visible() {
    static const auto func = static_class()->find_function(L"IsVisible");

    if (func == nullptr) {
        return false;
    }

    bool result{};

    this->process_event(func, &result);

    return result;
}

void USceneComponent::set_visibility(bool visible, bool propagate) {
    static const auto func = static_class()->find_function(L"SetVisibility");

    if (func == nullptr) {
        return;
    }

    struct {
        bool visible{};
        bool propagate{};
    } params{};

    params.visible = visible;
    params.propagate = propagate;

    this->process_event(func, &params);
}
}