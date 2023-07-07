#include <spdlog/spdlog.h>

#include "UObjectArray.hpp"

#include "UProperty.hpp"

namespace sdk{
UClass* UProperty::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Property");
}

void UProperty::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    UField::update_offsets();

    SPDLOG_INFO("[UProperty] Bruteforcing offsets...");
}
}