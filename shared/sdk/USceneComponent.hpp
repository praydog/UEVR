#pragma once

#include <sdk/Math.hpp>

#include "UObject.hpp"

namespace sdk {
class USceneComponent : public UObject {
public:
    static UClass* static_class();

public:
    void set_world_rotation(const glm::vec3& rotation, bool sweep = false, bool teleport = false);
    void add_world_rotation(const glm::vec3& rotation, bool sweep = false, bool teleport = false);
};
}