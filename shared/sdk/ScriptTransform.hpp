#pragma once

#include <vector>

#include "Math.hpp"
#include "UClass.hpp"

namespace sdk {
class ScriptTransform : public UObject {
public:
    static UScriptStruct* static_struct();

    static std::vector<uint8_t> create_dynamic_struct(const glm::vec3& location, const glm::vec4& rotation, const glm::vec3& scale);

protected:
};
}