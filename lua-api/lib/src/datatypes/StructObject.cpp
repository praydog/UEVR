#include <datatypes/StructObject.hpp>

namespace lua::datatypes {
    void bind_struct_object(sol::state_view& lua) {
        lua.new_usertype<StructObject>("StructObject",
            "get_address", [](StructObject& self) { return (uintptr_t)self.object; },
            "get_struct", [](StructObject& self) { return self.desc; }

        );
    }
}