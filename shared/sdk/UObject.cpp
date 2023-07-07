#include "UObjectArray.hpp"

#include "UObject.hpp"

namespace sdk {
UClass* UObject::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Object");
}
}