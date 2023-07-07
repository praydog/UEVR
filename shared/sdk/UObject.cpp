#include "UObjectArray.hpp"

#include "UClass.hpp"
#include "UObject.hpp"

namespace sdk {
UClass* UObject::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Object");
}

bool UObject::is_a(UClass* other) const {
    return get_class()->is_a((UStruct*)other);
}
}