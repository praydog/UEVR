#pragma once

#include <safetyhook/InlineHook.hpp>
#include <Xinput.h>

class XInputHook {
public:
    XInputHook();

private:
    static uint32_t get_state_hook(uint32_t user_index, XINPUT_STATE* state);
    static uint32_t set_state_hook(uint32_t user_index, XINPUT_VIBRATION* vibration);

    std::unique_ptr<safetyhook::InlineHook> m_xinput_get_state_hook;
    std::unique_ptr<safetyhook::InlineHook> m_xinput_set_state_hook;
};