#include "UObjectArray.hpp"
#include "ScriptVector.hpp"
#include "ScriptRotator.hpp"
#include "FProperty.hpp"

#include "UCameraComponent.hpp"

namespace sdk {
UClass* UCameraComponent::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.CameraComponent");
}
};