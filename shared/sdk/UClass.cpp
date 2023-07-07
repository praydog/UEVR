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

    SPDLOG_INFO("[UStruct] UObject: 0x{:x}", (uintptr_t)object_class);
    SPDLOG_INFO("[UStruct] UField: 0x{:x}", (uintptr_t)field_class);
    SPDLOG_INFO("[UStruct] UStruct: 0x{:x}", (uintptr_t)struct_class);
    SPDLOG_INFO("[UStruct] UClass: 0x{:x}", (uintptr_t)class_class);

    if (class_class != nullptr && struct_class != nullptr && field_class != nullptr) {
        for (auto i = sdk::UObjectBase::get_class_size(); i < 0x100; i += sizeof(void*)) try {
            const auto value = *(sdk::UStruct**)((uintptr_t)class_class + i);

            // SuperStruct heuristic
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

void UClass::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UClass] Bruteforcing offsets...");

    const auto object_class = sdk::UObject::static_class();
    const auto field_class = sdk::UField::static_class();
    const auto struct_class = sdk::UStruct::static_class();

    if (object_class == nullptr || field_class == nullptr || struct_class == nullptr) {
        SPDLOG_ERROR("[UClass] Failed to find UObject/UStruct/UField!");
        return;
    }

    for (auto i = UObjectBase::get_class_size(); i < 0x300; i += sizeof(void*)) try {
        const auto value = *(sdk::UObject**)((uintptr_t)object_class + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto struct_value = *(sdk::UStruct**)((uintptr_t)struct_class + i);
        const auto field_value = *(sdk::UField**)((uintptr_t)field_class + i);

        if (struct_value == nullptr || field_value == nullptr) {
            continue;
        }

        if (value == object_class || struct_value == struct_class || field_value == field_class) {
            continue;
        }

        if (value->get_class() == object_class &&
            field_value->get_class() == field_class &&
            struct_value->get_class() == struct_class)
        {
            SPDLOG_INFO("[UClass] Found DefaultObject at offset 0x{:X}", i);
            s_default_object_offset = i;
            break;
        }
    } catch(...) {
        continue;
    }
}
}