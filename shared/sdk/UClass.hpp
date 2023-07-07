#pragma once

#include "UObject.hpp"

namespace sdk {
class UClass;

class UField : public UObject {
public:
    static UClass* static_class();
};

class UStruct : public UField {
public:
    static UClass* static_class();
    static void update_offsets();

    UStruct* get_super_struct() const {
        return *(UStruct**)((uintptr_t)this + s_super_struct_offset);
    }

private:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_super_struct_offset{0x40}; // not correct always, we bruteforce it later
};

class UClass : public UStruct {
public:
    static UClass* static_class();
    static void update_offsets();

    UObject* get_class_default_object() const {
        return *(UObject**)((uintptr_t)this + s_default_object_offset);
    }

private:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_default_object_offset{0x40}; // not correct always, we bruteforce it later
};
}