#pragma once

#include "UObject.hpp"
#include "UWorld.hpp"

namespace sdk {
class UWorld;

class UEngine : public sdk::UObject {
public:
    static UEngine** get_lvalue();
    static UEngine* get();

    UWorld* get_world();

public:
    void initialize_hmd_device();
    static std::optional<uintptr_t> get_initialize_hmd_device_address();
    static std::optional<uintptr_t> get_emulatestereo_string_ref_address();
    static std::optional<uintptr_t> get_stereo_rendering_device_offset();
};
}