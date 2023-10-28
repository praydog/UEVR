#pragma once

#include "FProperty.hpp"

namespace sdk {
class FArrayProperty : public FProperty {
public:
    static void update_offsets();

    FProperty* get_inner() const {
        return *(FProperty**)((uintptr_t)this + s_inner_offset);
    }

private:
    static inline bool s_offsets_updated{false};
    static inline uint32_t s_inner_offset{0x0};
};
}