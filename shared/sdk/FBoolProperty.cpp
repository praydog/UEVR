#include <spdlog/spdlog.h>

#include "UObjectArray.hpp"
#include "UClass.hpp"

#include "FBoolProperty.hpp"

namespace sdk {
void FBoolProperty::update_offsets() {
    if (s_updated_offsets) {
        return;
    }

    s_updated_offsets = true;

    SPDLOG_INFO("[FBoolProperty::update_offsets] Updating offsets");

    const auto fhitresult = sdk::find_uobject<sdk::UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    if (fhitresult == nullptr) {
        SPDLOG_ERROR("[FBoolProperty::update_offsets] Failed to find FHitResult");
        return;
    }

    // Need to iterate over FHItResult's properties to find the first bool property
    // on <= 4.27 or so it's always the first property but not in the newer ones.
    FBoolProperty* first_bool_property{nullptr};

    for (auto prop = fhitresult->get_child_properties(); prop != nullptr; prop = prop->get_next()) {
        const auto c = prop->get_class();

        if (c == nullptr) {
            continue;
        }

        if (c->get_name().to_string() == L"BoolProperty") {
            first_bool_property = (FBoolProperty*)prop;
            break;
        }
    }

    if (first_bool_property == nullptr) {
        SPDLOG_ERROR("[FBoolProperty::update_offsets] Failed to find first bool property");
        return;
    }

    FBoolProperty* second_bool_property{nullptr};

    auto next_prop = first_bool_property->get_next();

    if (next_prop == nullptr) {
        SPDLOG_ERROR("[FBoolProperty::update_offsets] Failed to find second bool property");
        return;
    }

    second_bool_property = (FBoolProperty*)next_prop;

    const auto second_c = next_prop->get_class();

    if (second_c == nullptr || second_c->get_name().to_string() != L"BoolProperty") {
        SPDLOG_ERROR("[FBoolProperty::update_offsets] Failed to find second bool property (class check)");
        return;
    }

    // Start from FProperty's offset offset, and bruteforce until we find the correct offset
    // Since there's two BoolProperties sequentially laid out, we should be able to figure out what the mask is
    const auto initial_start = FProperty::s_offset_offset + 4 + sizeof(void*) + sizeof(void*);
    // align up to sizeof(void*)
    const auto start = (initial_start + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    for (auto i = start; i < start + 0x100; ++i) try {
        const auto potential_field_size_a = *(uint8_t*)((uintptr_t)first_bool_property + i);
        const auto potential_field_size_b = *(uint8_t*)((uintptr_t)second_bool_property + i);
        const auto potential_byte_offset_a = *(uint8_t*)((uintptr_t)first_bool_property + i + 1);
        const auto potential_byte_offset_b = *(uint8_t*)((uintptr_t)second_bool_property + i + 1);
        const auto potential_byte_mask_a = *(uint8_t*)((uintptr_t)first_bool_property + i + 2);
        const auto potential_byte_mask_b = *(uint8_t*)((uintptr_t)second_bool_property + i + 2);
        const auto potential_field_mask_a = *(uint8_t*)((uintptr_t)first_bool_property + i + 3);
        const auto potential_field_mask_b = *(uint8_t*)((uintptr_t)second_bool_property + i + 3);

        // Log all of these for debugging even if they arent right
        /*SPDLOG_INFO("[FBoolProperty::update_offsets] Field size A: 0x{:X} ({:x})", potential_field_size_a, i);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Field size B: 0x{:X} ({:x})", potential_field_size_b, i);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Byte offset A: 0x{:X} ({:x})", potential_byte_offset_a, i + 1);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Byte offset B: 0x{:X} ({:x})", potential_byte_offset_b, i + 1);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Byte mask A: 0x{:X} ({:x})", potential_byte_mask_a, i + 2);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Byte mask B: 0x{:X} ({:x})", potential_byte_mask_b, i + 2);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Field mask A: 0x{:X} ({:x})", potential_field_mask_a, i + 3);
        SPDLOG_INFO("[FBoolProperty::update_offsets] Field mask B: 0x{:X} ({:x})", potential_field_mask_b, i + 3);*/
        
        if (potential_field_size_a == 1 && potential_field_size_b == 1 &&
            potential_byte_offset_a == 0 && potential_byte_offset_b == 0 &&
            potential_byte_mask_a == 1 && potential_byte_mask_b == 2 &&
            potential_field_mask_a == 1 && potential_field_mask_b == 2) 
        {
            SPDLOG_INFO("[FBoolProperty::update_offsets] Field size offset: 0x{:X}", i);
            SPDLOG_INFO("[FBoolProperty::update_offsets] Byte offset offset: 0x{:X}", i + 1);
            SPDLOG_INFO("[FBoolProperty::update_offsets] Byte mask offset: 0x{:X}", i + 2);
            SPDLOG_INFO("[FBoolProperty::update_offsets] Field mask offset: 0x{:X}", i + 3);
            s_field_size_offset = i;
            s_byte_offset_offset = i + 1;
            s_byte_mask_offset = i + 2;
            s_field_mask_offset = i + 3;
            break;
        }
    } catch(...) {
        continue;
    }

    if (s_field_size_offset == 0) {
        SPDLOG_ERROR("[FBoolProperty::update_offsets] Failed to find offsets");
        return;
    }

    SPDLOG_INFO("[FBoolProperty::update_offsets] Found offsets");
}
}