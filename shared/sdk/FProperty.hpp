#pragma once

#include <cstdint>

#include "FField.hpp"
#include "UClass.hpp"

namespace sdk {
class UProperty;
class FStructProperty;

class FProperty : public FField {
public:
    int32_t get_offset() const {
        return *(int32_t*)((uintptr_t)this + s_offset_offset);
    }

    template<typename T>
    T* get_data(sdk::UObject* object) const {
        return (T*)((uintptr_t)object + get_offset());
    }

    // Given xyz props from FVector, find the offset which matches up with all of them
    static void bruteforce_fproperty_offset(FProperty* x_prop, FProperty* y_prop, FProperty* z_prop);

protected:
    static inline uint32_t s_offset_offset{0x0}; // idk

    friend class UStruct;
    friend class UProperty;
    friend class FStructProperty;
};
}