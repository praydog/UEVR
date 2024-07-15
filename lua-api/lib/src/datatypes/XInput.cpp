#include <windows.h>
#include <Xinput.h>

#include <datatypes/XInput.hpp>

namespace lua::datatypes {
    void bind_xinput(sol::state_view& lua) {
        lua.new_usertype<XINPUT_GAMEPAD>("XINPUT_GAMEPAD",
            "wButtons", &XINPUT_GAMEPAD::wButtons,
            "bLeftTrigger", &XINPUT_GAMEPAD::bLeftTrigger,
            "bRightTrigger", &XINPUT_GAMEPAD::bRightTrigger,
            "sThumbLX", &XINPUT_GAMEPAD::sThumbLX,
            "sThumbLY", &XINPUT_GAMEPAD::sThumbLY,
            "sThumbRX", &XINPUT_GAMEPAD::sThumbRX,
            "sThumbRY", &XINPUT_GAMEPAD::sThumbRY
        );

        lua.new_usertype<XINPUT_STATE>("XINPUT_STATE",
            "Gamepad", &XINPUT_STATE::Gamepad,
            "dwPacketNumber", &XINPUT_STATE::dwPacketNumber
        );

        lua.new_usertype<XINPUT_VIBRATION>("XINPUT_VIBRATION",
            "wLeftMotorSpeed", &XINPUT_VIBRATION::wLeftMotorSpeed,
            "wRightMotorSpeed", &XINPUT_VIBRATION::wRightMotorSpeed
        );

        lua["XINPUT_GAMEPAD_DPAD_UP"] = XINPUT_GAMEPAD_DPAD_UP;
        lua["XINPUT_GAMEPAD_DPAD_DOWN"] = XINPUT_GAMEPAD_DPAD_DOWN;
        lua["XINPUT_GAMEPAD_DPAD_LEFT"] = XINPUT_GAMEPAD_DPAD_LEFT;
        lua["XINPUT_GAMEPAD_DPAD_RIGHT"] = XINPUT_GAMEPAD_DPAD_RIGHT;
        lua["XINPUT_GAMEPAD_START"] = XINPUT_GAMEPAD_START;
        lua["XINPUT_GAMEPAD_BACK"] = XINPUT_GAMEPAD_BACK;
        lua["XINPUT_GAMEPAD_LEFT_THUMB"] = XINPUT_GAMEPAD_LEFT_THUMB;
        lua["XINPUT_GAMEPAD_RIGHT_THUMB"] = XINPUT_GAMEPAD_RIGHT_THUMB;
        lua["XINPUT_GAMEPAD_LEFT_SHOULDER"] = XINPUT_GAMEPAD_LEFT_SHOULDER;
        lua["XINPUT_GAMEPAD_RIGHT_SHOULDER"] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
        lua["XINPUT_GAMEPAD_A"] = XINPUT_GAMEPAD_A;
        lua["XINPUT_GAMEPAD_B"] = XINPUT_GAMEPAD_B;
        lua["XINPUT_GAMEPAD_X"] = XINPUT_GAMEPAD_X;
        lua["XINPUT_GAMEPAD_Y"] = XINPUT_GAMEPAD_Y;
    }
}