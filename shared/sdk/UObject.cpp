#include "UObjectArray.hpp"

#include "UClass.hpp"
#include "FProperty.hpp"
#include "UProperty.hpp"
#include "UObject.hpp"

namespace sdk {
UClass* UObject::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Object");
}

bool UObject::is_a(UClass* other) const {
    return get_class()->is_a((UStruct*)other);
}

void* UObject::get_property_data(std::wstring_view name) const {
    const auto klass = get_class();

    if (klass == nullptr) {
        return nullptr;
    }

    const auto prop = klass->find_property(name);

    if (prop == nullptr) {
        return nullptr;
    }

    const auto offset = prop->get_offset();

    return (void*)((uintptr_t)this + offset);
}

void* UObject::get_uproperty_data(std::wstring_view name) const {
    const auto klass = get_class();

    if (klass == nullptr) {
        return nullptr;
    }

    const auto prop = klass->find_uproperty(name);

    if (prop == nullptr) {
        return nullptr;
    }

    const auto offset = prop->get_offset();

    return (void*)((uintptr_t)this + offset);
}
}