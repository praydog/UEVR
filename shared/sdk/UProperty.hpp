#pragma once

#include "UClass.hpp"

namespace sdk {
class UProperty : public UField {
public:
    static UClass* static_class();
    static void update_offsets();

    FName& get_property_name() const {
        return *(FName*)((uintptr_t)this + s_name_offset);
    }

private:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_name_offset{0x0}; // idk what it is usually.
    static inline uint32_t s_structure_size{0x0}; // idk what it is usually.

    friend class UStruct;
};
}