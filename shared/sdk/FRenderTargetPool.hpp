#pragma once

#include <cstdint>
#include <optional>

namespace sdk {
class FRenderTargetPool {
public:
    static std::optional<uintptr_t> get_find_free_element_fn();

private:
};
}