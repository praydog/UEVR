#include "UObjectArray.hpp"

#include "ScriptRotator.hpp"

namespace sdk {
UScriptStruct* ScriptRotator::static_struct() {
    static auto modern_class = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Rotator");
    static auto old_class = modern_class == nullptr ? sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Object.Rotator") : nullptr;

    return modern_class != nullptr ? modern_class : old_class;
}
}