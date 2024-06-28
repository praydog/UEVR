#include <format>
#include <string>
#include <cstdint>

#include <utility/String.hpp>

#include <datatypes/Vector.hpp>
#include <datatypes/StructObject.hpp>
#include <ScriptUtility.hpp>

namespace lua::utility {
sol::object call_function(sol::this_state s, uevr::API::UObject* self, uevr::API::UFunction* fn, sol::variadic_args args);

uevr::API::UScriptStruct* get_vector_struct() {
    static auto vector_struct = []() {
        const auto modern_class = uevr::API::get()->find_uobject<uevr::API::UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Vector");
        const auto old_class = modern_class == nullptr ? uevr::API::get()->find_uobject<uevr::API::UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Object.Vector") : nullptr;

        return modern_class != nullptr ? modern_class : old_class;
    }();

    return vector_struct;
}

bool is_ue5() {
    static auto cached_result = []() {
        const auto c = get_vector_struct();

        if (c == nullptr) {
            return false;
        }

        return c->get_struct_size() == sizeof(glm::dvec3);
    }();
    
    return cached_result;
}

sol::object prop_to_object(sol::this_state s, void* self, uevr::API::FProperty* desc, bool is_self_temporary) {
    const auto propc = desc->get_class();

    if (propc == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    const auto name_hash = ::utility::hash(propc->get_fname()->to_string());
    const auto offset = desc->get_offset();

    switch (name_hash) {
    case L"BoolProperty"_fnv:
    {
        const auto fbp = (uevr::API::FBoolProperty*)desc;
        return sol::make_object(s, fbp->get_value_from_object(self));
    }
    case L"FloatProperty"_fnv:
        return sol::make_object(s, *(float*)((uintptr_t)self + offset));
    case L"DoubleProperty"_fnv:
        return sol::make_object(s, *(double*)((uintptr_t)self + offset));
    case L"ByteProperty"_fnv:
        return sol::make_object(s, *(uint8_t*)((uintptr_t)self + offset));
    case L"Int8Property"_fnv:
        return sol::make_object(s, *(int8_t*)((uintptr_t)self + offset));
    case L"Int16Property"_fnv:
        return sol::make_object(s, *(int16_t*)((uintptr_t)self + offset));
    case L"UInt16Property"_fnv:
        return sol::make_object(s, *(uint16_t*)((uintptr_t)self + offset));
    case L"IntProperty"_fnv:
        return sol::make_object(s, *(int32_t*)((uintptr_t)self + offset));
    case L"UIntProperty"_fnv:
    case L"UInt32Property"_fnv:
        return sol::make_object(s, *(uint32_t*)((uintptr_t)self + offset));
    case L"UInt64Property"_fnv:
        return sol::make_object(s, *(uint64_t*)((uintptr_t)self + offset));
    case L"Int64Property"_fnv:
        return sol::make_object(s, *(int64_t*)((uintptr_t)self + offset));
    case L"EnumProperty"_fnv:
    {
        const auto ep = (uevr::API::FEnumProperty*)desc;
        const auto np = ep->get_underlying_prop();

        if (np == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto np_c = np->get_class();

        if (np_c == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto np_name_hash = ::utility::hash(np_c->get_fname()->to_string());

        switch (np_name_hash) {
        case L"FloatProperty"_fnv:
            return sol::make_object(s, *(float*)((uintptr_t)self + offset));
        case L"DoubleProperty"_fnv:
            return sol::make_object(s, *(double*)((uintptr_t)self + offset));
        case L"ByteProperty"_fnv:
            return sol::make_object(s, *(uint8_t*)((uintptr_t)self + offset));
        case L"Int8Property"_fnv:
            return sol::make_object(s, *(int8_t*)((uintptr_t)self + offset));
        case L"Int16Property"_fnv:
            return sol::make_object(s, *(int16_t*)((uintptr_t)self + offset));
        case L"UInt16Property"_fnv:
            return sol::make_object(s, *(uint16_t*)((uintptr_t)self + offset));
        case L"IntProperty"_fnv:
            return sol::make_object(s, *(int32_t*)((uintptr_t)self + offset));
        case L"UIntProperty"_fnv:
        case L"UInt32Property"_fnv:
            return sol::make_object(s, *(uint32_t*)((uintptr_t)self + offset));
        case L"UInt64Property"_fnv:
            return sol::make_object(s, *(uint64_t*)((uintptr_t)self + offset));
        case L"Int64Property"_fnv:
            return sol::make_object(s, *(int64_t*)((uintptr_t)self + offset));
        };

        return sol::make_object(s, sol::lua_nil);
    }
    case L"NameProperty"_fnv:
        return sol::make_object(s, *(uevr::API::FName*)((uintptr_t)self + offset));
    case L"ObjectProperty"_fnv:
        if (*(uevr::API::UObject**)((uintptr_t)self + offset) == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        return sol::make_object(s, *(uevr::API::UObject**)((uintptr_t)self + offset));
    case L"ClassProperty"_fnv:
        if (*(uevr::API::UClass**)((uintptr_t)self + offset) == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        return sol::make_object(s, *(uevr::API::UClass**)((uintptr_t)self + offset));
    case L"StructProperty"_fnv:
    {
        const auto struct_data = (void*)((uintptr_t)self + offset);
        const auto struct_desc = ((uevr::API::FStructProperty*)desc)->get_struct();

        if (struct_desc == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        /*const auto struct_name_hash = utility::hash(struct_desc->get_fname()->to_string());

        switch (struct_name_hash) {
        case L"Vector"_fnv:
            if (is_ue5()) {
                return sol::make_object(s, (lua::datatypes::Vector3f*)struct_data);
            }

            return sol::make_object(s, (lua::datatypes::Vector3f*)struct_data);
        };*/

        if (struct_desc == get_vector_struct()) {
            if (is_ue5()) {
                if (is_self_temporary) {
                    return sol::make_object(s, *(lua::datatypes::Vector3d*)struct_data);
                } else {
                    return sol::make_object(s, (lua::datatypes::Vector3d*)struct_data);
                }
            }

            if (is_self_temporary) {
                return sol::make_object(s, *(lua::datatypes::Vector3f*)struct_data);
            } else {
                return sol::make_object(s, (lua::datatypes::Vector3f*)struct_data);
            }
        }

        auto struct_object = lua::datatypes::StructObject{struct_data, struct_desc};

        return sol::make_object(s, struct_object);
    }
    case L"ArrayProperty"_fnv:
    {
        const auto inner_prop = ((uevr::API::FArrayProperty*)desc)->get_inner();

        if (inner_prop == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto inner_c = inner_prop->get_class();

        if (inner_c == nullptr) {
            return sol::make_object(s, sol::lua_nil);
        }

        const auto inner_name_hash = ::utility::hash(inner_c->get_fname()->to_string());

        switch (inner_name_hash) {
        case "ObjectProperty"_fnv:
        {
            const auto& arr = *(uevr::API::TArray<uevr::API::UObject*>*)((uintptr_t)self + offset);

            if (arr.data == nullptr || arr.count == 0) {
                return sol::make_object(s, sol::lua_nil);
            }

            auto lua_arr = std::vector<uevr::API::UObject*>{};

            for (size_t i = 0; i < arr.count; ++i) {
                lua_arr.push_back(arr.data[i]);
            }

            return sol::make_object(s, lua_arr);
        }
        // TODO: Add support for other types
        };

        return sol::make_object(s, sol::lua_nil);
    }
    };

    return sol::make_object(s, sol::lua_nil);
}

sol::object prop_to_object(sol::this_state s, void* self, uevr::API::UStruct* c, const std::wstring& name) {
    const auto desc = c->find_property(name.c_str());

    if (desc == nullptr) {
        if (auto fn = c->find_function(name.c_str()); fn != nullptr) {
            /*return sol::make_object(s, [self, s, fn](sol::variadic_args args) {
                return call_function(s, self, fn, args);
            });*/
            return sol::make_object(s, fn);
        }

        return sol::make_object(s, sol::lua_nil);
    }

    return prop_to_object(s, self, desc);
}

sol::object prop_to_object(sol::this_state s, uevr::API::UObject* self, const std::wstring& name) {
    const auto c = self->get_class();

    if (c == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    return prop_to_object(s, self, c, name);
}

void set_property(sol::this_state s, void* self, uevr::API::UStruct* c, const std::wstring& name, sol::object value) {
    const auto desc = c->find_property(name.c_str());

    if (desc == nullptr) {
        throw sol::error(std::format("[set_property] Property '{}' not found", ::utility::narrow(name)));
    }

    const auto propc = desc->get_class();

    if (propc == nullptr) {
        throw sol::error(std::format("[set_property] Property '{}' has no class", ::utility::narrow(name)));
    }

    const auto name_hash = ::utility::hash(propc->get_fname()->to_string());
    const auto offset = desc->get_offset();

    switch (name_hash) {
    case L"BoolProperty"_fnv:
    {
        const auto fbp = (uevr::API::FBoolProperty*)desc;
        fbp->set_value_in_object(self, value.as<bool>());
        return;
    }
    case L"FloatProperty"_fnv:
        *(float*)((uintptr_t)self + offset) = value.as<float>();
        return;
    case L"DoubleProperty"_fnv:
        *(double*)((uintptr_t)self + offset) = value.as<double>();
        return;
    case L"ByteProperty"_fnv:
        *(uint8_t*)((uintptr_t)self + offset) = value.as<uint8_t>();
        return;
    case L"Int8Property"_fnv:
        *(int8_t*)((uintptr_t)self + offset) = value.as<int8_t>();
        return;
    case L"Int16Property"_fnv:
        *(int16_t*)((uintptr_t)self + offset) = value.as<int16_t>();
        return;
    case L"UInt16Property"_fnv:
        *(uint16_t*)((uintptr_t)self + offset) = value.as<uint16_t>();
        return;
    case L"IntProperty"_fnv:
        *(int32_t*)((uintptr_t)self + offset) = value.as<int32_t>();
        return;
    case L"UIntProperty"_fnv:
    case L"UInt32Property"_fnv:
        *(uint32_t*)((uintptr_t)self + offset) = value.as<uint32_t>();
        return;
    case L"UInt64Property"_fnv:
        *(uint64_t*)((uintptr_t)self + offset) = value.as<uint64_t>();
        return;
    case L"Int64Property"_fnv:
        *(int64_t*)((uintptr_t)self + offset) = value.as<int64_t>();
        return;
    case L"EnumProperty"_fnv:
    {
        const auto ep = (uevr::API::FEnumProperty*)desc;
        const auto np = ep->get_underlying_prop();

        if (np == nullptr) {
            throw sol::error("Enum property has no underlying property");
        }

        const auto np_c = np->get_class();

        if (np_c == nullptr) {
            throw sol::error("Enum property's underlying property has no class");
        }

        const auto np_name_hash = ::utility::hash(np_c->get_fname()->to_string());

        switch (np_name_hash) {
        case L"FloatProperty"_fnv:
            *(float*)((uintptr_t)self + offset) = value.as<float>();
            return;
        case L"DoubleProperty"_fnv:
            *(double*)((uintptr_t)self + offset) = value.as<double>();
            return;
        case L"ByteProperty"_fnv:
            *(uint8_t*)((uintptr_t)self + offset) = value.as<uint8_t>();
            return;
        case L"Int8Property"_fnv:
            *(int8_t*)((uintptr_t)self + offset) = value.as<int8_t>();
            return;
        case L"Int16Property"_fnv:
            *(int16_t*)((uintptr_t)self + offset) = value.as<int16_t>();
            return;
        case L"UInt16Property"_fnv:
            *(uint16_t*)((uintptr_t)self + offset) = value.as<uint16_t>();
            return;
        case L"IntProperty"_fnv:
            *(int32_t*)((uintptr_t)self + offset) = value.as<int32_t>();
            return;
        case L"UIntProperty"_fnv:
        case L"UInt32Property"_fnv:
            *(uint32_t*)((uintptr_t)self + offset) = value.as<uint32_t>();
            return;
        case L"UInt64Property"_fnv:
            *(uint64_t*)((uintptr_t)self + offset) = value.as<uint64_t>();
            return;
        case L"Int64Property"_fnv:
            *(int64_t*)((uintptr_t)self + offset) = value.as<int64_t>();
            return;
        };

        throw sol::error("Could not set enum property");
    }
    case L"NameProperty"_fnv:
        //return sol::make_object(s, self.get_property<uevr::API::FName>(name));
        throw sol::error("Setting FName properties is not supported (yet)");
    case L"ObjectProperty"_fnv:
        *(uevr::API::UObject**)((uintptr_t)self + offset) = value.as<uevr::API::UObject*>();
        return;
    case L"ClassProperty"_fnv:
        *(uevr::API::UClass**)((uintptr_t)self + offset) = value.as<uevr::API::UClass*>();
        return;
    case L"ArrayProperty"_fnv:
        throw sol::error("Setting array properties is not supported (yet)");
    };

    // NONE
}

void set_property(sol::this_state s, uevr::API::UObject* self, const std::wstring& name, sol::object value) {
    const auto c = self->get_class();

    if (c == nullptr) {
        throw sol::error("[set_property] Object has no class");
    }

    set_property(s, self, c, name, value);
}

sol::object call_function(sol::this_state s, uevr::API::UObject* self, uevr::API::UFunction* fn, sol::variadic_args args) {
    const auto fn_args = fn->get_child_properties();

    if (fn_args == nullptr) {
        fn->call(self, nullptr);
        return sol::make_object(s, sol::lua_nil);
    }

    std::vector<uint8_t> params{};
    size_t args_index{0};

    const auto ps = fn->get_properties_size();
    const auto ma = fn->get_min_alignment();

    if (ma > 1) {
        params.resize(((ps + ma - 1) / ma) * ma);
    } else {
        params.resize(ps);
    }

    params.resize(fn->get_properties_size());

    uevr::API::FProperty* return_prop{nullptr};
    bool ret_is_bool{false};

    //std::vector<uint8_t> dynamic_data{};
    std::vector<std::wstring> dynamic_strings{};

    for (auto arg_desc = fn_args; arg_desc != nullptr; arg_desc = arg_desc->get_next()) {
        const auto arg_c = arg_desc->get_class();

        if (arg_c == nullptr) {
            continue;
        }

        const auto arg_c_name = arg_c->get_fname()->to_string();

        if (!arg_c_name.contains(L"Property")) {
            continue;
        }

        const auto prop_desc = (uevr::API::FProperty*)arg_desc;

        if (!prop_desc->is_param()) {
            continue;
        }

        if (prop_desc->is_return_param()) {
            return_prop = prop_desc;

            if (arg_c_name == L"BoolProperty") {
                ret_is_bool = true;
            }

            continue;
        }

        const auto arg_hash = ::utility::hash(arg_c_name);
        const auto offset = prop_desc->get_offset();

        switch (arg_hash) {
        case L"BoolProperty"_fnv:
        {
            const bool arg = args[args_index++].as<bool>();
            const auto fbp = (uevr::API::FBoolProperty*)prop_desc;
            fbp->set_value_in_object(params.data(), arg);
            continue;
        }
        case L"FloatProperty"_fnv:
        {
            const float arg = args[args_index++].as<float>();
            *(float*)&params[offset] = arg;
            continue;
        }
        case L"DoubleProperty"_fnv:
        {
            const double arg = args[args_index++].as<double>();
            *(double*)&params[offset] = arg;
            continue;
        }
        case L"IntProperty"_fnv:
        case L"UIntProperty"_fnv:
        case L"UInt32Property"_fnv:
        {
            const int32_t arg = args[args_index++].as<int32_t>();
            *(int32_t*)&params[offset] = arg;
            continue;
        }
        case L"NameProperty"_fnv:
        {
            const auto arg_obj = args[args_index++];

            if (arg_obj.is<std::string>()) {
                const auto arg = ::utility::widen(arg_obj.as<std::string>());
                *(uevr::API::FName*)&params[offset] = uevr::API::FName{arg};
            } else if (arg_obj.is<std::wstring>()) {
                const auto arg = arg_obj.as<std::wstring>();
                *(uevr::API::FName*)&params[offset] = uevr::API::FName{arg};
            } else if (arg_obj.is<uevr::API::FName>()) {
                const auto arg = arg_obj.as<uevr::API::FName>();
                *(uevr::API::FName*)&params[offset] = arg;
            } else {
                throw sol::error("Invalid argument type for FName");
            }

            continue;
        }
        case L"ObjectProperty"_fnv:
        {
            const auto arg = args[args_index++].as<uevr::API::UObject*>();
            *(uevr::API::UObject**)&params[offset] = arg;
            continue;
        }
        case L"ClassProperty"_fnv:
        {
            const auto arg = args[args_index++].as<uevr::API::UClass*>();
            *(uevr::API::UClass**)&params[offset] = arg;
            continue;
        }
        case L"StructProperty"_fnv:
        {
            const auto arg_obj = args[args_index++];
            const auto struct_desc = ((uevr::API::FStructProperty*)prop_desc)->get_struct();

            if (struct_desc == nullptr) {
                throw sol::error("Struct property has no struct");
            }

            if (arg_obj.is<lua::datatypes::StructObject>()) {
                const auto arg = arg_obj.as<lua::datatypes::StructObject>();

                if (arg.desc != struct_desc) {
                    if (arg.desc != nullptr) {
                        throw sol::error(std::format("Invalid argument type for function '{}', expected '{}', got '{}'", ::utility::narrow(fn->get_fname()->to_string()), ::utility::narrow(struct_desc->get_fname()->to_string()), ::utility::narrow(arg.desc->get_fname()->to_string())));
                    } else {
                        throw sol::error(std::format("Invalid argument type for function '{}', expected '{}', got 'nil'", ::utility::narrow(fn->get_fname()->to_string()), ::utility::narrow(struct_desc->get_fname()->to_string())));
                    }
                }

                memcpy(&params[offset], arg.object, struct_desc->get_struct_size());
            } else if (struct_desc == get_vector_struct()) {
                if (arg_obj.is<lua::datatypes::Vector3f>()) {
                    const auto arg = arg_obj.as<lua::datatypes::Vector3f>();

                    if (is_ue5()) {
                        *(lua::datatypes::Vector3d*)&params[offset] = arg;
                    } else {
                        *(lua::datatypes::Vector3f*)&params[offset] = arg;
                    }
                } else if (arg_obj.is<lua::datatypes::Vector3d>()) {
                    const auto arg = arg_obj.as<lua::datatypes::Vector3d>();

                    if (is_ue5()) {
                        *(lua::datatypes::Vector3d*)&params[offset] = arg;
                    } else {
                        *(lua::datatypes::Vector3f*)&params[offset] = arg;
                    }
                } else {
                    throw sol::error("Invalid argument type for FVector");
                }
            } else {
                throw sol::error("Invalid argument type for struct property");
            }

            continue;
        }
        case L"ArrayProperty"_fnv:
            // TODO
            throw sol::error("Array properties are not supported (yet)");
            continue;
        case L"StrProperty"_fnv:
        {
            const auto arg_obj = args[args_index++];
            using FString = uevr::API::TArray<wchar_t>;

            auto& fstr = *(FString*)&params[offset];

            if (arg_obj.is<std::wstring>()) {
                dynamic_strings.push_back(arg_obj.as<std::wstring>());

                fstr.count = dynamic_strings.back().size() + 1;
                fstr.data = (wchar_t*)dynamic_strings.back().c_str();
            } else if (arg_obj.is<std::string>()) {
                dynamic_strings.push_back(::utility::widen(arg_obj.as<std::string>()));

                fstr.count = dynamic_strings.back().size() + 1;
                fstr.data = (wchar_t*)dynamic_strings.back().c_str();
            } else if (arg_obj.is<wchar_t*>()) {
                dynamic_strings.push_back(arg_obj.as<wchar_t*>());

                fstr.count = dynamic_strings.back().size() + 1;
            } else {
                throw sol::error("Invalid argument type for FString");
            }
            continue;
        }
        default:
            //spdlog::warn("Unknown property type when calling '{}': {}", utility::narrow(fn->get_fname()->to_string()), utility::narrow(arg_c_name));
            uevr::API::get()->log_warn(std::format("Unknown property type when calling '{}': {}", ::utility::narrow(fn->get_fname()->to_string()), ::utility::narrow(arg_c_name)).c_str());
        };
    }

    fn->call(self, params.data());

    if (return_prop != nullptr) {
        if (ret_is_bool) {
            return sol::make_object(s, ((uevr::API::FBoolProperty*)return_prop)->get_value_from_object(params.data()));
        }

        return prop_to_object(s, params.data(), return_prop, true);
    }

    return sol::make_object(s, sol::lua_nil);
}

sol::object call_function(sol::this_state s, uevr::API::UObject* self, const std::wstring& name, sol::variadic_args args) {
    const auto c = self->get_class();

    if (c == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    const auto fn = c->find_function(name.c_str());

    if (fn == nullptr) {
        return sol::make_object(s, sol::lua_nil);
    }

    return call_function(s, self, fn, args);
}
}