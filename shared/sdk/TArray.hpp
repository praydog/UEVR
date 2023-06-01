#pragma once

namespace sdk {
template<typename T>
struct TArray {
    T* data{nullptr};
    int32_t count{0};
    int32_t capacity{0};
};
}