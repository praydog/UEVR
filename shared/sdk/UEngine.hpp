#pragma once

namespace sdk {
class UEngine {
public:
    static UEngine** get_lvalue();
    static UEngine* get();

public:
    void initialize_hmd_device();
    static std::optional<uintptr_t> get_initialize_hmd_device_address();
    static std::optional<uintptr_t> get_emulatestereo_string_ref_address();
    static std::optional<uintptr_t> get_stereo_rendering_device_offset();
};
}