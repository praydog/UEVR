#pragma once

#include <string_view>
#include <typeinfo>

struct _s_RTTICompleteObjectLocator;

namespace utility {
namespace rtti {
    _s_RTTICompleteObjectLocator* get_locator(const void* obj);
    std::type_info* get_type_info(const void* obj);
    bool derives_from(const void* obj, std::string_view type_name);
}
}