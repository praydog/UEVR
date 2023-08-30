#include "UObjectArray.hpp"

#include "UEnum.hpp"

namespace sdk {
sdk::UClass* UEnum::static_class() {
    static auto ptr = (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Enum");
    return ptr;
}
}