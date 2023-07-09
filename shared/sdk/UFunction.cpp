#include "UObjectArray.hpp"

#include "UFunction.hpp"

namespace sdk {
sdk::UClass* UFunction::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Function");
}
}