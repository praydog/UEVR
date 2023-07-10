#include "UObjectArray.hpp"

#include "APawn.hpp"

namespace sdk {
UClass* APawn::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.Pawn");
};
}