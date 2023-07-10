#pragma once

#include <string>
#include <cstdint>
#include "UObjectBase.hpp"

namespace sdk {
UObjectBase* find_uobject(const std::wstring& full_name, bool cached = true);

template<typename T>
T* find_uobject(const std::wstring& full_name, bool cached = true) {
    return (T*)find_uobject(full_name, cached);
}

struct FUObjectItem {
    UObjectBase* object{nullptr};
    int32_t flags{0};
    int32_t cluster_index{0};
    int32_t serial_number{0};
};

struct FUObjectArray {
    static FUObjectArray* get();

    static bool is_chunked() {
        return s_is_chunked;
    }

    int32_t get_object_count() {
        if (s_is_inlined_array) {
            constexpr auto offs = OBJECTS_OFFSET + (MAX_INLINED_CHUNKS * sizeof(void*));
            return *(int32_t*)((uintptr_t)this + offs);
        }

        if (s_is_chunked) {
            constexpr auto offs = OBJECTS_OFFSET + sizeof(void*) + sizeof(void*) + 0x4;
            return *(int32_t*)((uintptr_t)this + offs);
        }

        return *(int32_t*)((uintptr_t)this + OBJECTS_OFFSET + 0x8);
    }

    FUObjectItem* get_object(int32_t index) {
        if (index < 0) {
            return nullptr;
        }

        if (s_is_inlined_array) {
            const auto chunk_index = index / OBJECTS_PER_CHUNK_INLINED;
            const auto chunk_offset = index % OBJECTS_PER_CHUNK_INLINED;
            const auto chunk = *(void**)((uintptr_t)get_objects_ptr() + (chunk_index * sizeof(void*)));

            if (chunk == nullptr) {
                return nullptr;
            }

            return (FUObjectItem*)((uintptr_t)chunk + (chunk_offset * s_item_distance));
        }

        if (s_is_chunked) {
            const auto chunk_index = index / OBJECTS_PER_CHUNK;
            const auto chunk_offset = index % OBJECTS_PER_CHUNK;
            const auto chunk = *(void**)((uintptr_t)get_objects_ptr() + (chunk_index * sizeof(void*)));

            if (chunk == nullptr) {
                return nullptr;
            }

            return (FUObjectItem*)((uintptr_t)chunk + (chunk_offset * s_item_distance));
        }

        return (FUObjectItem*)((uintptr_t)get_objects_ptr() + (index * s_item_distance));
    }

    void* get_objects_ptr() {
        if (s_is_inlined_array) {
            return (void*)((uintptr_t)this + OBJECTS_OFFSET);
        }

        return *(void**)((uintptr_t)this + OBJECTS_OFFSET);
    }

private:
    // for <= 4.10
    constexpr static inline auto MAX_INLINED_CHUNKS =  ((8 * 1024 * 1024) + 16384 - 1) / 16384;
    constexpr static inline auto OBJECTS_PER_CHUNK_INLINED = 16384;

    // for some newer versions
    constexpr static inline auto OBJECTS_PER_CHUNK = 64 * 1024;

    // has remained true for a long time
    constexpr static inline auto OBJECTS_OFFSET = 0x10;

    static inline bool s_is_chunked{false};
    static inline bool s_is_inlined_array{false};
    static inline int32_t s_item_distance{sizeof(FUObjectItem)}; // bruteforced later for older versions
};
}