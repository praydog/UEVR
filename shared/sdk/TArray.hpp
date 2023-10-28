#pragma once

#include <type_traits>
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
    T* data{nullptr};
    int32_t count{0};
    int32_t capacity{0};

    // Move constructor
    TArray(TArray&& other) noexcept
        : data(other.data), 
        count(other.count),
        capacity(other.capacity) 
    {
        other.data = nullptr;
        other.count = 0;
        other.capacity = 0;
    }

    // Move assignment operator
    TArray& operator=(TArray&& other) noexcept {
        if (&other != this) {
            if (data != nullptr) {
                this->~TArray();  // call destructor to free the current data
            }

            data = other.data;
            count = other.count;
            capacity = other.capacity;

            other.data = nullptr;
            other.count = 0;
            other.capacity = 0;
        }

        return *this;
    }

    TArray() = default;

    // Delete copy constructor and copy assignment operator
    TArray(const TArray&) = delete;
    TArray& operator=(const TArray&) = delete;

    ~TArray() {
        if (data == nullptr) {
            return;
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (auto i = 0; i < count; ++i) {
                data[i].~T();
            }
        }

        if (auto m = FMalloc::get(); m != nullptr) {
            m->free(data);
        }

        data = nullptr;
    }

    // begin/end
    T* begin() {
        return data;
    }

    T* end() {
        return data + count;
    }

    // operator[]
    T& operator[](int32_t index) {
        return data[index];
    }

    const T& operator[](int32_t index) const {
        return data[index];
    }

    // size
    int32_t size() const {
        return count;
    }

    // empty
    bool empty() const {
        return count == 0;
    }

    void clear(bool shrink = true) {
        // trigger destructors
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (auto i = 0; i < count; ++i) {
                data[i].~T();
            }
        }

        count = 0;
    }

    // todo: push_back and stuff...
};
}