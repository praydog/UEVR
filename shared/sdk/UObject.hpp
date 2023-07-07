#pragma once

#include "UObjectBase.hpp"

namespace sdk {
class UClass;

class UObject : public UObjectBase {
public:
    static UClass* static_class();
};
}