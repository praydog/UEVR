#pragma once

#include "ScriptPrerequisites.hpp"
#include <uevr/API.hpp>

namespace lua::utility {
    uevr::API::UScriptStruct* get_vector_struct();
    bool is_ue5();

    sol::object prop_to_object(sol::this_state s, void* self, uevr::API::FProperty* desc, bool is_self_temporary = false);
    sol::object prop_to_object(sol::this_state s, void* self, uevr::API::UStruct* desc, const std::wstring& name);
    sol::object prop_to_object(sol::this_state s, uevr::API::UObject* self, const std::wstring& name);

    void set_property(sol::this_state s, void* self, uevr::API::UStruct* c, uevr::API::FProperty* desc, sol::object value);
    void set_property(sol::this_state s, void* self, uevr::API::UStruct* c, const std::wstring& name, sol::object value);
    void set_property(sol::this_state s, uevr::API::UObject* self, const std::wstring& name, sol::object value);

    sol::object call_function(sol::this_state s, uevr::API::UObject* self, uevr::API::UFunction* fn, sol::variadic_args args);
    sol::object call_function(sol::this_state s, uevr::API::UObject* self, const std::wstring& name, sol::variadic_args args);

    template<typename T>
    inline T read_t_struct(void* self, uevr::API::UStruct* c, size_t offset) {
        size_t size = 0;

        if (c->is_a(uevr::API::UScriptStruct::static_class())) {
            auto script_struct = reinterpret_cast<uevr::API::UScriptStruct*>(c);

            size = script_struct->get_struct_size();
        } else {
            size = c->get_properties_size();
        }

        if (offset + sizeof(T) > size) {
            throw sol::error("Offset out of bounds");
        }

        return *(T*)((uintptr_t)self + offset);
    }

    template <typename T>
    inline void write_t_struct(void* self, uevr::API::UStruct* c, size_t offset, T value) {
        size_t size = 0;
        if (c->is_a(uevr::API::UScriptStruct::static_class())) {
            auto script_struct = reinterpret_cast<uevr::API::UScriptStruct*>(c);

            size = script_struct->get_struct_size();
        } else {
            size = c->get_properties_size();
        }

        if (offset + sizeof(T) > size) {
            throw sol::error("Offset out of bounds");
        }

        *(T*)((uintptr_t)self + offset) = value;
    }

    template <typename T>
    void write_t(uevr::API::UObject* self, size_t offset, T value) {
        write_t_struct<T>(self, self->get_class(), offset, value);
    }

    template<typename T>
    T read_t(uevr::API::UObject* self, size_t offset) {
        return read_t_struct<T>(self, self->get_class(), offset);
    }
}