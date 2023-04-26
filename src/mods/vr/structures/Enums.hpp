// Generated with sdkgenny
#pragma once

#include <cstdint>

namespace ue {
enum class EControllerHand : uint8_t {
    Left = 0,
    Right = 1,
};

enum class ETrackingStatus : uint8_t {
    NotTracked = 0,
    InertialOnly = 1,
    Tracked = 2,
    ETrackingStatus_MAX = 3,
}; 
}