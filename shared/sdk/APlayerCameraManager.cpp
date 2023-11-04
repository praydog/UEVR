#include <sdk/UObjectArray.hpp>

#include "APlayerCameraManager.hpp"

namespace sdk {
sdk::UClass* APlayerCameraManager::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/Engine.PlayerCameraManager");
}
}