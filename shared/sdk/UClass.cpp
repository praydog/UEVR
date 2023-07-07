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

void UField::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UField] Bruteforcing offsets...");

    UStruct::update_offsets();

    // best thing i can come up with for now.
    s_next_offset = UStruct::s_super_struct_offset - sizeof(void*);

    SPDLOG_INFO("[UField] Found Next at offset 0x{:X}", s_next_offset);
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

    UField::update_offsets();
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

UClass* UScriptStruct::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.ScriptStruct");
}

void UScriptStruct::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    UStruct::update_offsets();

    SPDLOG_INFO("[UScriptStruct] Bruteforcing offsets...");

    const auto matrix_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Matrix");
    const auto vector_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Vector");

    if (matrix_scriptstruct == nullptr || vector_scriptstruct == nullptr) {
        SPDLOG_ERROR("[UScriptStruct] Failed to find Matrix/Vector!");
        return;
    }

    constexpr auto MAT4_SIZE_FLOAT = 4 * 4 * sizeof(float);
    constexpr auto MAT4_SIZE_DOUBLE = 4 * 4 * sizeof(double);
    constexpr auto VECTOR_SIZE_FLOAT = 3 * sizeof(float);
    constexpr auto VECTOR_SIZE_DOUBLE = 3 * sizeof(double);

    for (auto i = UStruct::s_super_struct_offset; i < 0x300; i += sizeof(void*)) try {
        const auto value = *(sdk::UScriptStruct::StructOps**)((uintptr_t)matrix_scriptstruct + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto potential_vtable = *(void**)value;

        if (potential_vtable == nullptr || IsBadReadPtr(potential_vtable, sizeof(void*)) || ((uintptr_t)potential_vtable & 1) != 0) {
            continue;
        }

        const auto potential_vfunc = *(void**)potential_vtable;

        if (potential_vfunc == nullptr || IsBadReadPtr(potential_vfunc, sizeof(void*))) {
            continue;
        }

        const auto value2 = *(sdk::UScriptStruct::StructOps**)((uintptr_t)vector_scriptstruct + i);

        if (value2 == nullptr || IsBadReadPtr(value2, sizeof(void*)) || ((uintptr_t)value2 & 1) != 0) {
            continue;
        }

        if ((value->size == MAT4_SIZE_FLOAT && value2->size == VECTOR_SIZE_FLOAT) || 
            (value->size == MAT4_SIZE_DOUBLE && value2->size == VECTOR_SIZE_DOUBLE))
        {
            SPDLOG_INFO("[UScriptStruct] Found StructOps at offset 0x{:X}", i);
            s_struct_ops_offset = i;
            break;
        }
    } catch(...) {
        continue;
    }
}
}