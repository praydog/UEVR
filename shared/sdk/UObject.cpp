#include <spdlog/spdlog.h>

#include <utility/String.hpp>

#include "UObjectArray.hpp"

#include "UClass.hpp"
#include "FProperty.hpp"
#include "UProperty.hpp"
#include "FBoolProperty.hpp"
#include "UObject.hpp"

namespace sdk {
UClass* UObject::static_class() {
    static auto result = (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Object");
    return result;
}

bool UObject::is_a(UClass* other) const {
    return get_class()->is_a((UStruct*)other);
}

void* UObject::get_property_data(std::wstring_view name) const {
    const auto klass = get_class();

    if (klass == nullptr) {
        return nullptr;
    }

    const auto prop = klass->find_property(name);

    if (prop == nullptr) {
        return nullptr;
    }

    const auto offset = prop->get_offset();

    return (void*)((uintptr_t)this + offset);
}

void* UObject::get_uproperty_data(std::wstring_view name) const {
    const auto klass = get_class();

    if (klass == nullptr) {
        return nullptr;
    }

    const auto prop = klass->find_uproperty(name);

    if (prop == nullptr) {
        return nullptr;
    }

    const auto offset = prop->get_offset();

    return (void*)((uintptr_t)this + offset);
}

nlohmann::json UObject::to_json(const std::vector<std::wstring>& properties) const {
    nlohmann::json j;

    const auto klass = get_class();

    if (klass == nullptr) {
        return j;
    }

    std::vector<sdk::FProperty*> wanted_properties{};
    
    // If "properties" is empty, get all properties
    if (properties.empty()) {
        for (auto prop = klass->get_child_properties(); prop != nullptr; prop = prop->get_next()) {
            const auto prop_c = prop->get_class();

            if (prop_c == nullptr) {
                continue;
            }

            const auto prop_c_name = prop_c->get_name().to_string();

            // Currently supported properties.
            switch (utility::hash(utility::narrow(prop_c_name))) {
            case "BoolProperty"_fnv:
            case "IntProperty"_fnv:
            case "FloatProperty"_fnv:
            case "DoubleProperty"_fnv:
                wanted_properties.push_back((sdk::FProperty*)prop);
                break;
            default:
                break;
            };
        }
    } else {
        for (const auto& property : properties) {
            const auto prop = klass->find_property(property);

            if (prop == nullptr) {
                continue;
            }

            wanted_properties.push_back(prop);
        }
    }

    j["properties"] = nlohmann::json{};
    auto& j_properties = j["properties"];

    for (const auto& prop : wanted_properties) {
        const auto prop_c = prop->get_class();

        if (prop_c == nullptr) {
            continue;
        }

        const auto prop_c_name = prop_c->get_name().to_string();
        const auto prop_field_name = utility::narrow(prop->get_field_name().to_string());

        // Currently supported properties.
        switch (utility::hash(utility::narrow(prop_c_name))) {
        case "BoolProperty"_fnv:
        {
            const auto bp = (sdk::FBoolProperty*)prop;
            j_properties[prop_field_name] = bp->get_value_from_object((void*)this);
            break;
        }
        case "IntProperty"_fnv:
            j_properties[prop_field_name] = *prop->get_data<int32_t>(this);
            break;
        case "FloatProperty"_fnv:
            j_properties[prop_field_name] = *prop->get_data<float>(this);
            break;
        case "DoubleProperty"_fnv:
            j_properties[prop_field_name] = *prop->get_data<double>(this);
            break;
        default:
            break;
        };
    }

    return j;
}

void UObject::from_json(const nlohmann::json& j) {
    const auto klass = get_class();

    if (klass == nullptr) {
        return;
    }

    if (!j.contains("properties")) {
        return;
    }

    const auto& j_properties = j["properties"];

    for (const auto& [key, value] : j_properties.items()) {
        const auto prop = klass->find_property(utility::widen(key));

        if (prop == nullptr) {
            continue;
        }

        const auto prop_c = prop->get_class();

        if (prop_c == nullptr) {
            continue;
        }

        const auto prop_c_name = utility::narrow(prop_c->get_name().to_string());

        // Currently supported properties.
        switch (utility::hash(prop_c_name)) {
        case "BoolProperty"_fnv:
        {
            if (!value.is_boolean()) {
                continue;
            }

            const auto bp = (sdk::FBoolProperty*)prop;
            bp->set_value_in_object((void*)this, value.get<bool>());
            break;
        }
        case "IntProperty"_fnv:
        {
            if (!value.is_number_integer()) {
                continue;
            }

            *prop->get_data<int32_t>(this) = value.get<int32_t>();
            break;
        }
        case "FloatProperty"_fnv:
        {
            if (!value.is_number_float()) {
                continue;
            }

            *prop->get_data<float>(this) = value.get<float>();
            break;
        }
        case "DoubleProperty"_fnv:
        {
            if (!value.is_number_float()) {
                continue;
            }

            *prop->get_data<double>(this) = value.get<double>();
            break;
        }
        default:
            SPDLOG_ERROR("[UObject::from_json] Unsupported property type: {}", prop_c_name);
            break;
        };
    }
}
}