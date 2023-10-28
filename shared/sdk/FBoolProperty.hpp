#pragma once

#include <cstdint>

#include "FProperty.hpp"

namespace sdk {
class FBoolProperty : public FProperty {
public:
    static void update_offsets();

    uint8_t get_field_size() const {
        return *(uint8_t*)((uintptr_t)this + s_field_size_offset);
    }

    uint8_t get_byte_offset() const {
        return *(uint8_t*)((uintptr_t)this + s_byte_offset_offset);
    }

    uint8_t get_byte_mask() const {
        return *(uint8_t*)((uintptr_t)this + s_byte_mask_offset);
    }

    uint8_t get_field_mask() const {
        return *(uint8_t*)((uintptr_t)this + s_field_mask_offset);
    }

    bool get_value_from_object(void* object) const {
        return get_value_from_propbase((void*)((uintptr_t)object + get_offset()));
    }

    bool get_value_from_propbase(void* addr) const {
        return (*(uint8_t*)((uintptr_t)addr + get_byte_offset()) & get_byte_mask()) != 0;
    }

    void set_value_in_object(void* object, bool value) const {
        set_value_in_propbase((void*)((uintptr_t)object + get_offset()), value);
    }

    void set_value_in_propbase(void* addr, bool value) const {
        const auto cur_value = *(uint8_t*)((uintptr_t)addr + get_byte_offset());
        *(uint8_t*)((uintptr_t)addr + get_byte_offset()) = (cur_value & ~get_byte_mask()) | (value ? get_byte_mask() : 0);
    }

private:
    static inline bool s_updated_offsets{false};
    static inline uint32_t s_field_size_offset{0x0}; // idk
    static inline uint32_t s_byte_offset_offset{0x0}; // idk
    static inline uint32_t s_byte_mask_offset{0x0}; // idk
    static inline uint32_t s_field_mask_offset{0x0}; // idk
};
}