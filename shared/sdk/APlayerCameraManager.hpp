#pragma once

#include "UObject.hpp"

namespace sdk {
class UClass;
class APlayerController;

class APlayerCameraManager : public UObject {
public:
    static UClass* static_class();

    APlayerController* get_owning_player_controller();
};
}