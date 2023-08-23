#include "UObjectArray.hpp"

#include "ScriptMatrix.hpp"

namespace sdk {
UScriptStruct* ScriptMatrix::static_struct() {
    static auto modern_class = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Matrix");
    static auto old_class = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Object.Matrix");

    return modern_class != nullptr ? modern_class : old_class;
}
}