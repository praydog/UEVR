#include <utility/String.hpp>

#include <ScriptUtility.hpp>
#include <datatypes/StructObject.hpp>

namespace lua::datatypes {
    template<typename T>
    T read_t_structobject(StructObject& self, size_t offset) {
        return utility::read_t_struct<T>(self.object, self.desc, offset);
    }

    template<typename T>
    void write_t_structobject(StructObject& self, size_t offset, T value) {
        utility::write_t_struct<T>(self.object, self.desc, offset, value);
    }

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
            "write_qword", &write_t_structobject<uint64_t>,
            "write_dword", &write_t_structobject<uint32_t>,
            "write_word", &write_t_structobject<uint16_t>,
            "write_byte", &write_t_structobject<uint8_t>,
            "write_float", &write_t_structobject<float>,
            "write_double", &write_t_structobject<double>,
            "read_qword", &read_t_structobject<uint64_t>,
            "read_dword", &read_t_structobject<uint32_t>,
            "read_word", &read_t_structobject<uint16_t>,
            "read_byte", &read_t_structobject<uint8_t>,
            "read_float", &read_t_structobject<float>,
            "read_double", &read_t_structobject<double>,
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