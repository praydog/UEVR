#pragma once

#include "UClass.hpp"

namespace sdk {
class UFunction : public UStruct {
public:
    static UClass* static_class();

    using NativeFunction = void(*)(sdk::UObject*, void*, void*);
    NativeFunction get_native_function() const {
        return *(NativeFunction*)((uintptr_t)this + s_native_function_offset);
    }

private:
    static inline uint32_t s_native_function_offset{0x0};

    friend class UStruct;
};
}