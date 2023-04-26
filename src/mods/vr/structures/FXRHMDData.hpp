// Generated with sdkgenny
#pragma once

#include <sdk/StereoStuff.hpp>

#include "Enums.hpp"
#include "FGuid.hpp"
#include "FName.hpp"

namespace ue4_27 {
#pragma pack(push, 1)
struct FXRHMDData {
    bool bValid; // 0x0
    char pad_1[0x3];
    ue::FName DeviceName; // 0x4
    char pad__[0x4];
    ue::FGuid ApplicationInstanceID; // 0xc + 4
    ue::ETrackingStatus TrackingStatus; // 0x1c + 4
    char pad_1d[0x3];
    glm::vec3 Position; // 0x20 + 4
    Quat<float> Rotation; // 0x30
}; // Size: 0x40
#pragma pack(pop)
static_assert(sizeof(FXRHMDData) == 0x40);
static_assert(offsetof(FXRHMDData, Position) == 0x24);
static_assert(offsetof(FXRHMDData, Rotation) == 0x30);
}