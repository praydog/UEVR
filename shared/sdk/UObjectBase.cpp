#include <Windows.h>
#include <spdlog/spdlog.h>

#include "UClass.hpp"
#include "UObjectBase.hpp"

namespace sdk {
void UObjectBase::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UObjectBase] Bruteforcing offsets...");

    // Look for the first valid pointer in the object array.
    // The first valid pointer is ClassPrivate.
    for (auto i = sizeof(void*); i < 0x50; i += sizeof(void*)) {
        const auto value = *(void**)((uintptr_t)this + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto possible_vtable = *(void**)value;

        if (possible_vtable == nullptr || IsBadReadPtr(possible_vtable, sizeof(void*)) || ((uintptr_t)possible_vtable & 1) != 0) {
            continue;
        }

        const auto possible_vfunc = *(void**)possible_vtable;

        if (possible_vfunc == nullptr || IsBadReadPtr(possible_vfunc, sizeof(void*))) {
            continue;
        }

        SPDLOG_INFO("[UObjectBase] Found ClassPrivate at offset 0x{:X}", i);
        s_class_private_offset = i;
        s_fname_offset = s_class_private_offset + sizeof(void*);
        s_outer_private_offset = s_fname_offset + sizeof(void*);
        
        break;
    }
}

std::wstring UObjectBase::get_full_name() const {
    auto c = get_class();

    if (c == nullptr) {
        return L"null";
    }

    auto obj_name = get_fname().to_string();

    for (auto outer = this->get_outer(); outer != nullptr && outer != this; outer = outer->get_outer()) {
        obj_name = outer->get_fname().to_string() + L'.' + obj_name;
    }

    return c->get_fname().to_string() + L' ' + obj_name;
}
}