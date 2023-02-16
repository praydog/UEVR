#pragma once

#include <thread>

#include <safetyhook/InlineHook.hpp>
#include <Xinput.h>

class XInputHook {
public:
    XInputHook();

private:
    static uint32_t get_state_hook_1_4(uint32_t user_index, XINPUT_STATE* state);
    static uint32_t set_state_hook_1_4(uint32_t user_index, XINPUT_VIBRATION* vibration);

    static uint32_t get_state_hook_1_3(uint32_t user_index, XINPUT_STATE* state);
    static uint32_t set_state_hook_1_3(uint32_t user_index, XINPUT_VIBRATION* vibration);

    safetyhook::InlineHook m_xinput_1_4_get_state_hook;
    safetyhook::InlineHook m_xinput_1_4_set_state_hook;
    safetyhook::InlineHook m_xinput_1_3_get_state_hook;
    safetyhook::InlineHook m_xinput_1_3_set_state_hook;

    std::unique_ptr<std::jthread> m_hook_thread_1_4{};
    std::unique_ptr<std::jthread> m_hook_thread_1_3{};
};