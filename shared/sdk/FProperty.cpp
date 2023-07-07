#include <spdlog/spdlog.h>

#include "FProperty.hpp"

namespace sdk {
void FProperty::bruteforce_fproperty_offset(FProperty* x_prop, FProperty* y_prop, FProperty* z_prop) {
    SPDLOG_INFO("[FProperty] Bruteforcing offset field via FVector heuristic...");

    if (x_prop == nullptr || y_prop == nullptr || z_prop == nullptr) {
        SPDLOG_ERROR("[FProperty] Failed to bruteforce offset field");
        return;
    }

    // It has to be at least ahead of the next offset
    bool found = false;
    for (auto i = FField::s_next_offset + sizeof(void*); i < 0x200; i += 4) try {
        const auto x_offset = *(uint32_t*)((uintptr_t)x_prop + i);
        const auto y_offset = *(uint32_t*)((uintptr_t)y_prop + i);
        const auto z_offset = *(uint32_t*)((uintptr_t)z_prop + i);

        // float for UE4, double for UE5
        if (x_offset == 0 && 
            (y_offset == sizeof(float) || y_offset == sizeof(double)) &&
            ((z_offset == sizeof(float) * 2) || (z_offset == sizeof(double) * 2)))
        {
            SPDLOG_INFO("[FProperty] Found offset field at offset 0x{:X}", i);
            s_offset_offset = i;
            found = true;
            break;
        }
    } catch(...) {
        continue;
    }

    if (!found) {
        SPDLOG_ERROR("[FProperty] Failed to bruteforce offset field");
    }
}
}