#include <utility/String.hpp>

#include <ScriptUtility.hpp>
#include <datatypes/StructObject.hpp>

namespace lua::datatypes {
    void bind_struct_object(sol::state_view& lua) {
        lua.new_usertype<StructObject>("StructObject",
            "get_address", [](StructObject& self) { return (uintptr_t)self.object; },
            "get_struct", [](StructObject& self) { return self.desc; },
            "get_property", [](sol::this_state s, StructObject* self, const std::wstring& name) -> sol::object {
                return lua::utility::prop_to_object(s, self->object, self->desc, name);
            },
            "set_property", [](sol::this_state s, StructObject* self, const std::wstring& name, sol::object value) {
                lua::utility::set_property(s, self->object, self->desc, name, value);
            },
            sol::meta_function::index, [](sol::this_state s, StructObject* self, sol::object index_obj) -> sol::object {
                if (!index_obj.is<std::string>()) {
                    return sol::make_object(s, sol::lua_nil);
                }

                const auto name = ::utility::widen(index_obj.as<std::string>());

                return lua::utility::prop_to_object(s, self->object, self->desc, name);
            },
            sol::meta_function::new_index, [](sol::this_state s, StructObject* self, sol::object index_obj, sol::object value) {
                if (!index_obj.is<std::string>()) {
                    return;
                }

                const auto name = ::utility::widen(index_obj.as<std::string>());
                lua::utility::set_property(s, self->object, self->desc, name, value);
            }
        );
    }
}