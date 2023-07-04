#pragma once

#include <optional>
#include <cstdint>
#include <string_view>

namespace sdk {
enum EFindName {
    Find,
    Add
};

struct FName {
    using ConstructorFn = void* (*)(FName*, const wchar_t*, uint32_t);
    static std::optional<ConstructorFn> get_constructor();

    FName(std::wstring_view name, EFindName find_type = EFindName::Add);

    int32_t a1{0};
    int32_t a2{0};
};
}