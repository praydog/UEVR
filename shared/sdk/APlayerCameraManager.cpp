#include <sdk/UObjectArray.hpp>
#include <sdk/UClass.hpp>

#include "APlayerCameraManager.hpp"

namespace sdk {
sdk::UClass* APlayerCameraManager::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/Engine.PlayerCameraManager");
}

sdk::APlayerController* APlayerCameraManager::get_owning_player_controller() {
    static const auto sc = static_class();
    if (sc == nullptr) {
        return nullptr;
    }

    static const auto func = sc->find_function(L"GetOwningPlayerController");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        APlayerController* result{};
    } params;

    this->process_event(func, &params);
    return params.result;
}
}