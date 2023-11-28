#pragma once

#include <optional>
#include <cstdint>
#include <string_view>

#include "TArray.hpp"

namespace sdk {
enum EFindName {
    Find,
    Add
};

struct FName {
    using ConstructorFn = void* (*)(FName*, const wchar_t*, uint32_t);
    static std::optional<ConstructorFn> get_constructor();

    using ToStringFn = TArray<wchar_t>* (*)(const FName*, TArray<wchar_t>*);
    static std::optional<ToStringFn> get_to_string();

    FName() 
    {
        
    }
    FName(std::wstring_view name, EFindName find_type = EFindName::Add);
    std::wstring to_string() const;

    int32_t a1{0};
    int32_t a2{0};
};
}