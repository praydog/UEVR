#pragma once

namespace sdk {
class UObjectBase;

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
        if (s_is_chunked) {
            return *(int32_t*)((uintptr_t)this + OBJECTS_OFFSET + sizeof(void*) + sizeof(void*) + 0x4);
        }

        return *(int32_t*)((uintptr_t)this + OBJECTS_OFFSET + 0x8);
    }

    FUObjectItem* get_object(int32_t index) {
        if (index < 0) {
            return nullptr;
        }

        if (s_is_chunked) {
            const auto chunk_index = index / OBJECTS_PER_CHUNK;
            const auto chunk_offset = index % OBJECTS_PER_CHUNK;
            const auto chunk = *(void**)((uintptr_t)get_objects_ptr() + (chunk_index * sizeof(void*)));

            if (chunk == nullptr) {
                return nullptr;
            }

            return (FUObjectItem*)((uintptr_t)chunk + (chunk_offset * sizeof(FUObjectItem)));
        }

        return (FUObjectItem*)((uintptr_t)get_objects_ptr() + (index * sizeof(FUObjectItem)));
    }

    void* get_objects_ptr() {
        return *(void**)((uintptr_t)this + OBJECTS_OFFSET);
    }

private:
    constexpr static inline auto OBJECTS_PER_CHUNK = 64 * 1024;
    constexpr static inline auto OBJECTS_OFFSET = 0x10;
    static inline bool s_is_chunked{false};
};
}