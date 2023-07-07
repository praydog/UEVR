#pragma once

#include "UObjectBase.hpp"

namespace sdk {
class UClass;

class UObject : public UObjectBase {
public:
    static UClass* static_class();

    template <typename T>
    bool is_a() const {
        return is_a(T::static_class());
    }

    bool is_a(UClass* cmp) const;
};
}