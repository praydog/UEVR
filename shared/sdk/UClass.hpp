#pragma once

#include "UObject.hpp"

namespace sdk {
class UClass;

class UField : public UObject {
public:
    static UClass* static_class();
};

class UStruct : public UField {
public:
    static UClass* static_class();
};

class UClass : public UStruct {
public:
    static UClass* static_class();
};
}