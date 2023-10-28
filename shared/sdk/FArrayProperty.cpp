#include <windows.h>
#include <spdlog/spdlog.h>

#include "UClass.hpp"
#include "UObjectArray.hpp"
#include "FObjectProperty.hpp"

#include "FArrayProperty.hpp"

namespace sdk {
void FArrayProperty::update_offsets() {
    if (s_offsets_updated) {
        return;
    }

    s_offsets_updated = true;

    SPDLOG_INFO("[FArrayProperty::update_offsets] Updating offsets");

    const auto scene_component = sdk::find_uobject<UClass>(L"Class /Script/Engine.SceneComponent");

    if (scene_component == nullptr) {
        SPDLOG_ERROR("[FArrayProperty::update_offsets] Failed to find SceneComponent");
        return;
    }

    const auto attach_children = scene_component->find_property(L"AttachChildren");

    if (attach_children == nullptr) {
        SPDLOG_ERROR("[FArrayProperty::update_offsets] Failed to find AttachChildren");
        return;
    }

    // Start from FProperty's offset offset, and bruteforce until we find the correct offset
    const auto initial_start = FProperty::s_offset_offset + 4 + sizeof(void*) + sizeof(void*);
    // align up to sizeof(void*)
    const auto start = (initial_start + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    for (auto i = start; i < start + 0x100; i += sizeof(void*)) try {
        const auto value = *(FProperty**)(attach_children + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*))) {
            continue;
        }
        
        const auto c = value->get_class();

        if (c == nullptr || IsBadReadPtr(c, sizeof(void*))) {
            continue;
        }

        if (c->get_name().to_string() == L"ObjectProperty") {
            auto prop = (FObjectProperty*)value;

            if (prop->get_property_class() == scene_component) {
                s_inner_offset = i;
                SPDLOG_INFO("[FArrayProperty::update_offsets] Found property inner offset: 0x{:X}", i);
                break;
            }
        }
    } catch(...) {
        continue;
    }

    SPDLOG_INFO("[FArrayProperty::update_offsets] Done");
}
}