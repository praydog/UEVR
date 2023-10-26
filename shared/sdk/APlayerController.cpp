#include <sdk/Math.hpp>

#include <vector>
#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "FProperty.hpp"

#include "APlayerController.hpp"

namespace sdk {
UClass* AController::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.Controller");
}

void AController::set_control_rotation(const glm::vec3& newrotation) {
    static const auto func = static_class()->find_function(L"SetControlRotation");
    const auto frotator = sdk::ScriptRotator::static_struct();

    const auto is_ue5 = frotator->get_struct_size() == sizeof(glm::vec<3, double>);

    // Need to dynamically allocate the params because of unknown FRotator size
    std::vector<uint8_t> params{};
    // add a vec3
    if (!is_ue5) {
        params.insert(params.end(), (uint8_t*)&newrotation, (uint8_t*)&newrotation + sizeof(glm::vec3));
    } else {
        glm::vec<3, double> rot = newrotation;
        params.insert(params.end(), (uint8_t*)&rot, (uint8_t*)&rot + sizeof(glm::vec<3, double>));
    }

    this->process_event(func, params.data());
}

glm::vec3 AController::get_control_rotation() {
    static const auto func = static_class()->find_function(L"GetControlRotation");
    const auto frotator = sdk::ScriptVector::static_struct();

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

UClass* APlayerController::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.PlayerController");
}

APawn* APlayerController::get_acknowledged_pawn() const {
    return get_property<APawn*>(L"AcknowledgedPawn");
}

APlayerCameraManager* APlayerController::get_player_camera_manager() const {
    return get_property<APlayerCameraManager*>(L"PlayerCameraManager");
}

void APlayerController::add_rotation_input(const glm::vec3& rotation) {
    //static const auto field = static_class()->find_property(L"RotationInput");
    const auto frotator = sdk::ScriptRotator::static_struct();
    const auto is_ue5 = frotator->get_struct_size() == sizeof(glm::vec<3, double>);

    //if (field == nullptr) {
       //return;
    //}

    static const auto nc_field = static_class()->find_property(L"NetConnection");
    
    if (is_ue5) {
        *(glm::vec<3, double>*)((uintptr_t)this + nc_field->get_offset() + sizeof(void*)) += glm::vec<3, double>{rotation};
    } else {
        *(glm::vec3*)((uintptr_t)this + nc_field->get_offset() + sizeof(void*)) += rotation;
    }
}
}
