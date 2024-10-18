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
}