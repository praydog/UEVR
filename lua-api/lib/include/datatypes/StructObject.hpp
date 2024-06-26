#pragma once

#include <uevr/API.hpp>
#include <sol/sol.hpp>

namespace lua::datatypes {
    struct StructObject {
        StructObject(void* obj, uevr::API::UStruct* def) : object{ obj }, desc{ def } {}

        void* object{ nullptr };
        uevr::API::UStruct* desc{ nullptr };
    };

    void bind_struct_object(sol::state_view& lua);
}