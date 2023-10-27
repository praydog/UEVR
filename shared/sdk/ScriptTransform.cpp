#include "UObjectArray.hpp"
#include "FProperty.hpp"

#include "ScriptVector.hpp"
#include "ScriptTransform.hpp"

namespace sdk {
sdk::UScriptStruct* ScriptTransform::static_struct() {
    static auto modern_class = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Transform");
    static auto old_class = modern_class == nullptr ? sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/CoreUObject.Object.Transform") : nullptr;

    return modern_class != nullptr ? modern_class : old_class;
}

std::vector<uint8_t> ScriptTransform::create_dynamic_struct(const glm::vec3& location, const glm::vec4& rotation, const glm::vec3& scale) {
    static const bool is_ue5 = ScriptVector::static_struct()->get_struct_size() == sizeof(glm::vec<3, double>);

    std::vector<uint8_t> out{};
    out.insert(out.end(), static_struct()->get_struct_size(), 0);

    // quaternion, vec3, vec3
    static const auto quat_offset = static_struct()->find_property(L"Rotation")->get_offset();
    static const auto loc_offset = static_struct()->find_property(L"Translation")->get_offset();
    static const auto scale_offset = static_struct()->find_property(L"Scale3D")->get_offset();

    if (is_ue5) {
        *(glm::vec<4, double>*)(out.data() + quat_offset) = rotation;
        *(glm::vec<3, double>*)(out.data() + loc_offset) = location;
        *(glm::vec<3, double>*)(out.data() + scale_offset) = scale;
    } else {
        *(glm::vec4*)(out.data() + quat_offset) = rotation;
        *(glm::vec3*)(out.data() + loc_offset) = location;
        *(glm::vec3*)(out.data() + scale_offset) = scale;
    }

    return out;
}
}