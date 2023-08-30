#pragma once

#include "FProperty.hpp"

namespace sdk {
class UScriptStruct;

class FStructProperty : public FProperty {
public:
    UScriptStruct* get_struct() const {
        return *(UScriptStruct**)((uintptr_t)this + s_struct_offset);
    }

    static void update_offsets();

    // From FMatrix
    static void bruteforce_struct_offset(FProperty* xplane_prop, FProperty* yplane_prop, FProperty* zplane_prop, FProperty* wplane_prop);

private:
    static inline uint32_t s_struct_offset{0x0}; // idk

    friend class UStruct;
    friend class UProperty;
    friend class FProperty;
};
}