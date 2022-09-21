#pragma once

#include <cstdint>
#include <optional>

#include "UEngine.hpp"

namespace sdk {
class UGameEngine : public UEngine {
public:
    static std::optional<uintptr_t> get_tick_address();
};
}