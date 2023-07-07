#include <spdlog/spdlog.h>

#include "FField.hpp"

namespace sdk {
void FField::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[FField] Bruteforcing offsets...");
}
}