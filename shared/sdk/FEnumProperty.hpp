#pragma once

#include <cstdint>

#include "FProperty.hpp"

namespace sdk {
struct FNumericProperty;
struct UEnum;

class FEnumProperty : public FProperty {
public:
    static void update_offsets();

    FNumericProperty* get_underlying_prop() const {
        return *(FNumericProperty**)((uintptr_t)this + s_underlying_prop_offset);
    }
    
    UEnum* get_enum() const {
        return *(UEnum**)((uintptr_t)this + s_enum_offset);
    }

private:
    static inline bool s_updated_offsets{false};
    static inline uint32_t s_underlying_prop_offset{0x0}; // idk
    static inline uint32_t s_enum_offset{0x0}; // idk
};
}