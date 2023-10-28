#pragma once

#include "FProperty.hpp"

namespace sdk {
class UClass;

class FObjectProperty : public FProperty {
public:
    static void update_offsets();

    UClass* get_property_class() const {
        return *(UClass**)((uintptr_t)this + s_property_class_offset);
    }

private:
    static inline bool s_offsets_updated{false};
    static inline uint32_t s_property_class_offset{0x0};
};
}