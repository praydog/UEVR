#pragma once

#include "UObject.hpp"

#include "USceneComponent.hpp"

namespace sdk {
class UCameraComponent : public USceneComponent {
public:
    static UClass* static_class();
};
}