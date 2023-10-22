#pragma once

namespace sdk {
class FMalloc {
public:
    static FMalloc* get();

    virtual ~FMalloc() {}

    void* malloc(size_t size, uint32_t alignment = 0) {
        if (!s_malloc_index.has_value()) {
            return nullptr;
        }

        return ((void*(__thiscall*)(FMalloc*, size_t, uint32_t))(*(void***)this)[*s_malloc_index])(this, size, alignment);
    }

    void* realloc(void* original, size_t size, uint32_t alignment = 0) {
        if (!s_realloc_index.has_value()) {
            return nullptr;
        }

        return ((void*(__thiscall*)(FMalloc*, void*, size_t, uint32_t))(*(void***)this)[*s_realloc_index])(this, original, size, alignment);
    }

    void free(void* original) {
        if (!s_free_index.has_value()) {
            return;
        }

        ((void(__thiscall*)(FMalloc*, void*))(*(void***)this)[*s_free_index])(this, original);
    }

private:
    static inline std::optional<uint32_t> s_malloc_index{};
    static inline std::optional<uint32_t> s_realloc_index{};
    static inline std::optional<uint32_t> s_free_index{};
};
}