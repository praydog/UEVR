#include <spdlog/spdlog.h>
#include <utility/String.hpp>

#include "UObjectArray.hpp"
#include "UClass.hpp"
#include "UEnum.hpp"

#include "FEnumProperty.hpp"

namespace sdk {
void FEnumProperty::update_offsets() {
    if (s_updated_offsets) {
        return;
    }

    s_updated_offsets = true;

    SPDLOG_INFO("[FEnumProperty::update_offsets] Updating offsets");

    const auto repmovement = sdk::find_uobject<sdk::UScriptStruct>(L"ScriptStruct /Script/Engine.RepMovement");

    if (repmovement == nullptr) {
        SPDLOG_ERROR("[FEnumProperty::update_offsets] Failed to find RepMovement");
        return;
    }

    // Find the first EnumProperty
    FEnumProperty* first_enum_property{nullptr};
    FEnumProperty* second_enum_property{nullptr};

    for (auto prop = repmovement->get_child_properties(); prop != nullptr; prop = prop->get_next()) {
        const auto c = prop->get_class();

        if (c == nullptr) {
            continue;
        }

        if (c->get_name().to_string() == L"EnumProperty") {
            if (first_enum_property == nullptr) {
                first_enum_property = (FEnumProperty*)prop;
            } else if (second_enum_property == nullptr) {
                second_enum_property = (FEnumProperty*)prop;
            } else {
                break;
            }
        }
    }

    if (first_enum_property == nullptr) {
        SPDLOG_ERROR("[FEnumProperty::update_offsets] Failed to find first enum property");
        return;
    }

    if (second_enum_property == nullptr) {
        SPDLOG_ERROR("[FEnumProperty::update_offsets] Failed to find second enum property");
        return;
    }

    const auto first_enum_c = first_enum_property->get_class();
    const auto second_enum_c = second_enum_property->get_class();

    if (first_enum_c == nullptr || second_enum_c == nullptr) {
        SPDLOG_ERROR("[FEnumProperty::update_offsets] Failed to find first or second enum class");
        return;
    }

    // Start from FProperty's offset offset, and bruteforce until we find the correct offset
    const auto initial_start = FProperty::s_offset_offset + 4 + sizeof(void*) + sizeof(void*);
    // align up to sizeof(void*)
    const auto start = (initial_start + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    // underlying prop
    for (auto i = start; i < start + 0x100; i += sizeof(void*)) try {
        const auto potential_numeric_property_a = *(sdk::FProperty**)(first_enum_property + i);
        const auto potential_numeric_property_b = *(sdk::FProperty**)(second_enum_property + i);

        if (potential_numeric_property_a == nullptr || potential_numeric_property_b == nullptr) {
            continue;
        }
        
        const auto potential_numeric_property_a_c = potential_numeric_property_a->get_class();
        const auto potential_numeric_property_b_c = potential_numeric_property_b->get_class();
        
        if (potential_numeric_property_a_c != nullptr && potential_numeric_property_b_c != nullptr) {
            const auto potential_prop_a_c_name = potential_numeric_property_a_c->get_name().to_string();
            const auto potential_prop_b_c_name = potential_numeric_property_b_c->get_name().to_string();

            if (potential_prop_a_c_name == L"ByteProperty" && potential_prop_b_c_name == L"ByteProperty") {
                s_underlying_prop_offset = i;
                SPDLOG_INFO("[FEnumProperty::update_offsets] Found underlying prop offset: 0x{:X}", i);
                break;
            }
        }
    } catch(...) {
        continue;
    }

    // UEnum
    for (auto i = start; i < start + 0x100; i += sizeof(void*)) try {
        const auto potential_enum_a = *(sdk::UEnum**)(first_enum_property + i);
        const auto potential_enum_b = *(sdk::UEnum**)(second_enum_property + i);

        if (potential_enum_a == nullptr || potential_enum_b == nullptr) {
            continue;
        }
        
        const auto poential_enum_a_c = potential_enum_a->get_class();
        const auto poential_enum_b_c = potential_enum_b->get_class();
    

        if (poential_enum_a_c != nullptr && poential_enum_b_c != nullptr) {
            const auto potential_enum_a_c_name = poential_enum_a_c->get_fname().to_string();
            const auto potential_enum_b_c_name = poential_enum_b_c->get_fname().to_string();
            
            if (potential_enum_a_c_name == L"Enum" && potential_enum_b_c_name == L"Enum") {
                s_enum_offset = i;
                SPDLOG_INFO("[FEnumProperty::update_offsets] Found enum offset: 0x{:X}", i);
                break;
            }
        }
    } catch(...) {
        continue;
    }
}
}