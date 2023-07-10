#pragma once

#include "UClass.hpp"

namespace sdk {
class APlayerController;

class UGameplayStatics : public UObject {
public:
    static UGameplayStatics* get();
    static sdk::UClass* static_class();

    APlayerController* get_player_controller(UObject* world_context, int32_t index);

protected:
};
}