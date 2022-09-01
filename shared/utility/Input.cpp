#include <windows.h>

#include "Input.hpp"

namespace utility {
bool was_key_down(uint32_t key) {
    static bool key_states[256]{0};

    const auto current_key_state = GetAsyncKeyState(key) != 0;
    const auto previous_key_state = key_states[key];
    const auto was_down = current_key_state && !previous_key_state;

    key_states[key] = current_key_state;
    return was_down;
}
}