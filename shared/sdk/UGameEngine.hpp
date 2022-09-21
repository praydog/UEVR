#pragma once

#include <cstdint>

#include "UEngine.hpp"

namespace sdk {
class UGameEngine : public UEngine {
public:
    static uintptr_t get_tick_address();
};
}