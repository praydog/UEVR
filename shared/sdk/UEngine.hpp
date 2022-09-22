#pragma once

namespace sdk {
class UEngine {
public:
    static UEngine** get_lvalue();
    static UEngine* get();

public:
    void initialize_hmd_device();
};
}