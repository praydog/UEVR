#pragma once

namespace sdk {
class UEngine {
public:
    static UEngine* get();

public:
    void initialize_hmd_device();
};
}