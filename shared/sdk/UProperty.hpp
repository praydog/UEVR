#pragma once

#include "UClass.hpp"

namespace sdk {
class UProperty : public UField {
public:
    static UClass* static_class();
    static void update_offsets();

private:
    static inline bool s_attempted_update_offsets{false};

    friend class UStruct;
};
}