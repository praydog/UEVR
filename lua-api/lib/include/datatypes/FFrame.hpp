#pragma once

#include <uevr/API.hpp>
#include <sol/sol.hpp>

namespace lua::datatypes {
    struct FFrame {
        void* vtable;

        bool bool1;
        bool bool2;

        void* node;
        void* object;
        void* code;
        void* locals;
    };

    //void bind_fframe(sol::state_view& lua);
}