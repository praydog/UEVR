#pragma once

#include "FName.hpp"

namespace sdk {
class UStruct;
class UClass;
class UObject;
class UFunction;

class UObjectBase {
public:
    void update_offsets();
    std::wstring get_full_name() const;
    void process_event(UFunction* function, void* params);
    void call_function(const wchar_t* name, void* params);

    UClass* get_class() const {
        return *(UClass**)((uintptr_t)this + s_class_private_offset);
    }

    UObject* get_outer() const {
        return *(UObject**)((uintptr_t)this + s_outer_private_offset);
    }

    FName& get_fname() const {
        return *(FName*)((uintptr_t)this + s_fname_offset);
    }

    uint32_t get_object_flags() const {
        return *(uint32_t*)((uintptr_t)this + s_object_flags_offset);
    }

    uint32_t get_internal_index() const {
        return *(uint32_t*)((uintptr_t)this + s_internal_index_offset);
    }

    static uint32_t get_class_size() {
        return s_outer_private_offset + sizeof(void*);
    }

    static uint32_t get_process_event_index() {
        return s_process_event_index;
    }

    static uint32_t get_object_flags_offset() {
        return s_object_flags_offset;
    }

    static uint32_t get_internal_index_offset() {
        return s_internal_index_offset;
    }

    static uint32_t get_class_private_offset() {
        return s_class_private_offset;
    }

    static uint32_t get_fname_offset() {
        return s_fname_offset;
    }

    static uint32_t get_outer_private_offset() {
        return s_outer_private_offset;
    }

    static std::optional<uintptr_t> get_vtable();
    static std::optional<uintptr_t> get_destructor();
    static std::optional<uintptr_t> get_add_object();

private:
    using ProcessEventFn = void(*)(UObjectBase*, UFunction*, void*);
    static void update_process_event_index();
    static void update_offsets_post_uobjectarray();

    static inline bool s_updated_post_uobjectarray{false};
    static inline std::optional<uintptr_t> s_vtable{};
    static inline std::optional<uintptr_t> s_destructor{};
    static inline std::optional<uintptr_t> s_add_object{};

    static inline bool s_attempted_update_offsets{false};
    static inline uint32_t s_object_flags_offset{0x8};
    static inline uint32_t s_internal_index_offset{0xC};
    static inline uint32_t s_class_private_offset{0x10};
    static inline uint32_t s_fname_offset{0x18};
    static inline uint32_t s_outer_private_offset{0x20};

    static inline uint32_t s_process_event_index{0};

    friend class UStruct;
};
}