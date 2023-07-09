#pragma once

#include "UClass.hpp"

namespace sdk {
class FProperty;

class UProperty : public UField {
public:
    static UClass* static_class();
    static void update_offsets();

    int32_t get_offset() const {
        return *(int32_t*)((uintptr_t)this + s_offset_offset);
    }

private:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_offset_offset{0x0};

    friend class UStruct;
    friend class FProperty;
};
}