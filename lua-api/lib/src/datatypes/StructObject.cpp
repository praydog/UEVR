#include <utility/String.hpp>

#include <ScriptUtility.hpp>
#include <datatypes/StructObject.hpp>

namespace lua::datatypes {
    void StructObject::construct(uevr::API::UStruct* def) {
        // TODO: Call constructor? Not important for now
        if (def->is_a(uevr::API::UScriptStruct::static_class())) {
            auto script_struct = static_cast<uevr::API::UScriptStruct*>(def);

            created_object.resize(script_struct->get_struct_size());
            memset(created_object.data(), 0, created_object.size());
        } else {
            created_object.resize(def->get_properties_size());
        }

        object = created_object.data();
        desc = def;
    }

    StructObject::StructObject(uevr::API::UStruct* def) {
        if (def == nullptr) {
            throw sol::error("Cannot create a StructObject from a nullptr UStruct");
        }

        construct(def);
    }

    StructObject::StructObject(uevr::API::UObject* obj) {
        if (obj == nullptr) {
            throw sol::error("Cannot create a StructObject from a nullptr UObject");
        }

        if (!obj->is_a(uevr::API::UStruct::static_class())) {
            throw sol::error("Cannot create a StructObject from a UObject that is not a UStruct");
        }

        construct(static_cast<uevr::API::UStruct*>(obj));
    }

    StructObject::~StructObject() {

    }

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
            },
            sol::meta_function::construct, sol::constructors<StructObject(uevr::API::UStruct*), StructObject(uevr::API::UObject*)>()
        );
    }
}