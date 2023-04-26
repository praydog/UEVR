#pragma once

#include <cstdint>

#include <sdk/Math.hpp>

#include "Enums.hpp"
#include "FName.hpp"

namespace ue5 {
struct FQuat {
    double x{};
    double y{};
    double z{};
    double w{};
};
}

namespace ue4_27 {
struct FQuat {
    float x{};
    float y{};
    float z{};
    float w{};
};

struct FXRDeviceId {
    ue::FName SystemName{};
    int32_t DeviceID{};
};

#pragma pack(push, 1)
struct FXRMotionControllerData {
    bool bValid; // 0x0
    char pad_1[0x3];
    ue::FName DeviceName; // 0x4
    char pad_c[0x10];
    uint8_t DeviceVisualType; // 0x1c
    ue::EControllerHand HandIndex; // 0x1d
    ue::ETrackingStatus TrackingStatus; // 0x1e
    char pad_1f[0x1];
    glm::vec3 GripPosition; // 0x20
    char pad_2c[0x4];
    FQuat GripRotation; // 0x30
    glm::vec3 AimPosition; // 0x40
    char pad_4c[0x4];
    FQuat AimRotation; // 0x50
    char pad_60[0x30];
    char pad_91[0xf];
}; // Size: 0xa0
#pragma pack(pop)
}