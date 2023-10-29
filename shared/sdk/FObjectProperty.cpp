#include <windows.h>
#include <spdlog/spdlog.h>

#include "UClass.hpp"
#include "UObjectArray.hpp"

#include "FObjectProperty.hpp"

namespace sdk {
void FObjectProperty::update_offsets() {
    if (s_offsets_updated) {
        return;
    }
    
    s_offsets_updated = true;

    SPDLOG_INFO("[FObjectProperty::update_offsets] Updating offsets");

    const auto scene_component = sdk::find_uobject<UClass>(L"Class /Script/Engine.SceneComponent");

    if (scene_component == nullptr) {
        SPDLOG_ERROR("[FObjectProperty::update_offsets] Failed to find SceneComponent");
        return;
    }

    const auto attach_parent = scene_component->find_property(L"AttachParent");

    if (attach_parent == nullptr) {
        SPDLOG_ERROR("[FObjectProperty::update_offsets] Failed to find AttachParent");
        return;
    }

    // Start from FProperty's offset offset, and bruteforce until we find the correct offset
    const auto initial_start = FProperty::s_offset_offset + 4 + sizeof(void*) + sizeof(void*);
    // align up to sizeof(void*)
    const auto start = (initial_start + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    for (auto i = start; i < start + 0x100; i += sizeof(void*)) try {
        const auto value = *(UClass**)(attach_parent + i);

        // well this is easy!
        if (value == scene_component) {
            s_property_class_offset = i;
            SPDLOG_INFO("[FObjectProperty::update_offsets] Found property class offset: 0x{:X}", i);
            break;
        }
    } catch(...) {
        continue;
    }

    if (s_property_class_offset == 0) {
        SPDLOG_ERROR("[FObjectProperty::update_offsets] Failed to find property class offset");
        return;
    }

    SPDLOG_INFO("[FObjectProperty::update_offsets] Done");
}
}