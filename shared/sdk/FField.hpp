#pragma once

#include <cstdint>
#include "FName.hpp"
#include "FFieldClass.hpp"

namespace sdk {
class UStruct;
class FFieldClass;

// Will be used for actual FFields
// but also as an interface for UField
// so i dont need to call this class FFieldOrUField
class FField {
public:
    static void update_offsets();
    static bool is_ufield_only() {
        return s_uses_ufield_only;
    }

    FField* get_next() const {
        return *(FField**)((uintptr_t)this + s_next_offset);
    }

    FName& get_field_name() const {
        return *(FName*)((uintptr_t)this + s_name_offset);
    }

    FFieldClass* get_class() const {
        return *(FFieldClass**)((uintptr_t)this + s_class_offset);
    }

protected:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_class_offset{sizeof(void*)}; // not correct always, we bruteforce it later
    static inline uint32_t s_next_offset{0x20}; // not correct always, we bruteforce it later
    static inline uint32_t s_name_offset{0x28}; // not correct always, we bruteforce it later

    static inline bool s_uses_ufield_only{false};

    friend class UStruct;
};
}