#include <windows.h>

#include <spdlog/spdlog.h>

#include <utility/String.hpp>

#include "UObjectArray.hpp"
#include "ScriptMatrix.hpp"
#include "FProperty.hpp"

#include "FStructProperty.hpp"

namespace sdk {
void FStructProperty::update_offsets() {
    const auto matrix_t = ScriptMatrix::static_struct();

    if (matrix_t == nullptr) {
        SPDLOG_ERROR("[FStructProperty] Failed to bruteforce Struct field (Matrix not found)");
        return;
    }

    for (auto child = matrix_t->get_child_properties(); child != nullptr; child = child->get_next()) {
        SPDLOG_INFO("[UStruct] Checking child property {}", utility::narrow(child->get_field_name().to_string()));
    }

    const auto xplane_prop = matrix_t->find_property(L"XPlane");
    const auto yplane_prop = matrix_t->find_property(L"YPlane");
    const auto zplane_prop = matrix_t->find_property(L"ZPlane");
    const auto wplane_prop = matrix_t->find_property(L"WPlane");

    bruteforce_struct_offset(xplane_prop, yplane_prop, zplane_prop, wplane_prop);
}

void FStructProperty::bruteforce_struct_offset(FProperty* xplane_prop, FProperty* yplane_prop, FProperty* zplane_prop, FProperty* wplane_prop) {
    SPDLOG_INFO("[FStructProperty] Bruteforcing Struct field via FMatrix heuristic...");

    if (xplane_prop == nullptr || yplane_prop == nullptr || zplane_prop == nullptr || wplane_prop == nullptr) {
        SPDLOG_ERROR("[FStructProperty] Failed to bruteforce Struct field (one of the planes is null)");
        return;
    }

    // It has to be at least ahead of the Offset field offset
    bool found = false;
    const auto alignment = sizeof(void*);
    const auto offset_aligned = (FProperty::s_offset_offset + alignment - 1) & ~(alignment - 1);

    for (auto i = offset_aligned; i < 0x200; i += sizeof(void*)) try {
        const auto x_struct = *(UScriptStruct**)((uintptr_t)xplane_prop + i);
        const auto y_struct = *(UScriptStruct**)((uintptr_t)yplane_prop + i);
        const auto z_struct = *(UScriptStruct**)((uintptr_t)zplane_prop + i);
        const auto w_struct = *(UScriptStruct**)((uintptr_t)wplane_prop + i);

        if (x_struct == nullptr || IsBadReadPtr(x_struct, sizeof(void*)) ||
            y_struct == nullptr || IsBadReadPtr(y_struct, sizeof(void*)) ||
            z_struct == nullptr || IsBadReadPtr(z_struct, sizeof(void*)) ||
            w_struct == nullptr || IsBadReadPtr(w_struct, sizeof(void*))) 
        {
            continue;
        }

        if (x_struct->get_full_name() != L"ScriptStruct /Script/CoreUObject.Plane" ||
            y_struct->get_full_name() != L"ScriptStruct /Script/CoreUObject.Plane" ||
            z_struct->get_full_name() != L"ScriptStruct /Script/CoreUObject.Plane" ||
            w_struct->get_full_name() != L"ScriptStruct /Script/CoreUObject.Plane")
        {
            continue;
        }

        SPDLOG_INFO("[FStructProperty] Found Struct field at offset 0x{:X}", i);
        s_struct_offset = i;
        found = true;
        break;
    } catch(...) {
        continue;
    }

    if (!found) {
        SPDLOG_ERROR("[FStructProperty] Failed to bruteforce Struct field (no matching planes found)");
    }
}
}