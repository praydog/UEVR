#pragma once

#include <sdk/FMalloc.hpp>

namespace sdk {
template<typename T>
struct TArrayLite {
    T* data{nullptr};
    int32_t count{0};
    int32_t capacity{0};
};

template<typename T>
struct TArray {
    ~TArray() {
        if (data == nullptr) {
            return;
        }

        // TODO: call destructors?

        if (auto m = FMalloc::get(); m != nullptr) {
            m->free(data);
        }
    }

    T* data{nullptr};
    int32_t count{0};
    int32_t capacity{0};
};
}