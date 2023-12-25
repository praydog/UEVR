#include <spdlog/spdlog.h>

#include "UObjectArray.hpp"
#include "UFunction.hpp"

#include "UProperty.hpp"
#include "FProperty.hpp"

namespace sdk {
void FProperty::bruteforce_fproperty_offset(FProperty* x_prop, FProperty* y_prop, FProperty* z_prop) {
    SPDLOG_INFO("[FProperty] Bruteforcing offset field via FVector heuristic...");

    if (x_prop == nullptr || y_prop == nullptr || z_prop == nullptr) {
        SPDLOG_ERROR("[FProperty] Failed to bruteforce offset field");
        return;
    }

    // It has to be at least ahead of the next offset
    bool found = false;
    for (auto i = FField::s_next_offset + sizeof(void*); i < 0x200; i += 4) try {
        const auto x_offset = *(uint32_t*)((uintptr_t)x_prop + i);
        const auto y_offset = *(uint32_t*)((uintptr_t)y_prop + i);
        const auto z_offset = *(uint32_t*)((uintptr_t)z_prop + i);

        // float for UE4, double for UE5
        if (x_offset == 0 && 
            (y_offset == sizeof(float) || y_offset == sizeof(double)) &&
            ((z_offset == sizeof(float) * 2) || (z_offset == sizeof(double) * 2)))
        {
            SPDLOG_INFO("[FProperty] Found offset field at offset 0x{:X}", i);
            s_offset_offset = i;
            found = true;

            if (FField::s_uses_ufield_only) {
                UProperty::s_offset_offset = i;
            }

            break;
        }
    } catch(...) {
        continue;
    }

    if (!found) {
        SPDLOG_ERROR("[FProperty] Failed to bruteforce offset field");
    }
}

void FProperty::update_offsets() try {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;
    SPDLOG_INFO("[FProperty] Updating offsets...");

    // Get ActorComponent class
    const auto actor_component_class = sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.ActorComponent");

    if (actor_component_class == nullptr) {
        SPDLOG_ERROR("[FProperty] Failed to find ActorComponent class");
        return;
    }

    // Find IsActive function
    const auto is_active_function = actor_component_class->find_function(L"IsActive");

    if (is_active_function == nullptr) {
        SPDLOG_ERROR("[FProperty] Failed to find IsActive function");
        return;
    }

    // Get first parameter
    const auto first_param = is_active_function->get_child_properties();

    if (first_param == nullptr) {
        SPDLOG_ERROR("[FProperty] Failed to find first parameter");
        return;
    }

    const auto start_raw = std::max(FField::s_next_offset, FField::s_name_offset) + sizeof(void*);

    // Alignup to sizeof(void*)
    const auto start = (start_raw + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    constexpr auto cpf_param = 0x80;
    constexpr auto cpf_out_param = 0x100;
    constexpr auto cpf_return_param = 0x400;
    constexpr auto cpf_is_pod = 0x40000000; // CPF_IsPlainOldData

    // 4 flags that will definitely always be set
    const auto wanted_flags = cpf_param | cpf_out_param | cpf_return_param | cpf_is_pod;

    for (auto i = start; i < 0x200; i += sizeof(void*)) try {
        const auto value = *(uint64_t*)((uintptr_t)first_param + i);
        if ((value & wanted_flags) == wanted_flags) {
            SPDLOG_INFO("[FProperty] Found PropertyFlags offset at 0x{:X} (full value {:x})", i, value);
            s_property_flags_offset = i;
            break;
        }
    } catch (...) {
        continue;
    }

    if (s_property_flags_offset == 0) {
        SPDLOG_ERROR("[FProperty] Failed to find PropertyFlags offset");
        return;
    }

    SPDLOG_INFO("[FProperty] done");
} catch(...) {
    SPDLOG_ERROR("[FProperty] Failed to update offsets");
}
}