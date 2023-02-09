#pragma once

#include <cstdint>
#include <optional>

namespace sdk {
class UGameViewportClient {
public:
    static std::optional<uintptr_t> get_draw_function();
};
}