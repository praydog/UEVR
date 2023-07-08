#pragma once

#include "UClass.hpp"

namespace sdk{
class FFieldClass {
public:
    FName& get_name() const {
        return *(FName*)((uintptr_t)this + s_name_offset);
    }

private:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_name_offset{0}; // no vtable afaik so its 0

    friend class UStruct;
};
}