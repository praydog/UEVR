#pragma once

#include "UObject.hpp"

namespace sdk {
class UClass;
class UStruct;
class FField;
class FProperty;
class UProperty;

class UField : public UObject {
public:
    static UClass* static_class();
    static void update_offsets();

    UField* get_next() const {
        return *(UField**)((uintptr_t)this + s_next_offset);
    }

protected:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_next_offset{0x28}; // not correct always, we bruteforce it later

    friend class UStruct;
};

class UStruct : public UField {
public:
    static UClass* static_class();
    static void update_offsets();

    UStruct* get_super_struct() const {
        return *(UStruct**)((uintptr_t)this + s_super_struct_offset);
    }
    
    UField* get_children() const {
        return *(UField**)((uintptr_t)this + s_children_offset);
    }

    FField* get_child_properties() const {
        return *(FField**)((uintptr_t)this + s_child_properties_offset);
    }

    bool is_a(UStruct* other) const {
        for (auto super = this; super != nullptr; super = super->get_super_struct()) {
            if (super == other) {
                return true;
            }
        }

        return false;
    }

    FProperty* find_property(std::wstring_view name) const;
    UProperty* find_uproperty(std::wstring_view name) const;
    UFunction* find_function(std::wstring_view name) const;

protected:
    static void resolve_field_offsets(uint32_t child_search_start);
    static void resolve_function_offsets(uint32_t child_search_start);

    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_super_struct_offset{0x40}; // not correct always, we bruteforce it later
    static inline uint32_t s_children_offset{0x48}; // For UField variants
    static inline uint32_t s_child_properties_offset{0x48}; // not correct always, we bruteforce it later // Children (old)/ChildProperties
    static inline uint32_t s_properties_size_offset{0x50}; // not correct always, we bruteforce it later

    friend class UField;
};

class UClass : public UStruct {
public:
    static UClass* static_class();
    static void update_offsets();

    UObject* get_class_default_object() const {
        return *(UObject**)((uintptr_t)this + s_default_object_offset);
    }

    template<typename T>
    T* get_class_default_object() const {
        return static_cast<T*>(get_class_default_object());
    }

protected:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_default_object_offset{0x118}; // not correct always, we bruteforce it later
};

class UScriptStruct : public UStruct {
public:
    struct StructOps {
        virtual ~StructOps() {};

        int32_t size;
        int32_t alignment;
    };

    static UClass* static_class();
    static void update_offsets();

    StructOps* get_struct_ops() const {
        return *(StructOps**)((uintptr_t)this + s_struct_ops_offset);
    }

    int32_t get_struct_size() const {
        const auto ops = get_struct_ops();
        if (ops == nullptr) {
            return 0;
        }

        return ops->size;
    }

protected:
    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_struct_ops_offset{0xB8}; // not correct always, we bruteforce it later
};
}