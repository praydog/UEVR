#include <windows.h>
#include <spdlog/spdlog.h>

#include "UObjectArray.hpp"

#include "UClass.hpp"

namespace sdk {
UClass* UField::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Field");
}

UClass* UStruct::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Struct");
}

UClass* UClass::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Class");
}

void UStruct::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UStruct] Bruteforcing offsets...");

    const auto object_class = sdk::UObject::static_class();
    const auto field_class = sdk::UField::static_class();
    const auto struct_class = sdk::UStruct::static_class();
    const auto class_class = sdk::UClass::static_class();

    SPDLOG_INFO("[FUObjectArray::get] UObject: 0x{:x}", (uintptr_t)object_class);
    SPDLOG_INFO("[FUObjectArray::get] UField: 0x{:x}", (uintptr_t)field_class);
    SPDLOG_INFO("[FUObjectArray::get] UStruct: 0x{:x}", (uintptr_t)struct_class);
    SPDLOG_INFO("[FUObjectArray::get] UClass: 0x{:x}", (uintptr_t)class_class);

    if (class_class != nullptr && struct_class != nullptr && field_class != nullptr) {
        for (auto i = sdk::UObjectBase::get_class_size(); i < 0x100; i += sizeof(void*)) try {
            const auto value = *(sdk::UStruct**)((uintptr_t)class_class + i);

            if (value == struct_class && 
                *(sdk::UStruct**)((uintptr_t)struct_class + i) == field_class && 
                *(sdk::UStruct**)((uintptr_t)field_class + i) == object_class) 
            {
                SPDLOG_INFO("[UStruct] Found SuperStruct at offset 0x{:X}", i);
                break;
            }
        } catch(...) {
            continue;
        }
    } else {
        SPDLOG_ERROR("[UStruct] Failed to find classes!");
    }
}
}