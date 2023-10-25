#pragma once

#include <sdk/Math.hpp>

#include "UObject.hpp"
#include "UActorComponent.hpp"

namespace sdk {
class USceneComponent : public UActorComponent {
public:
    static UClass* static_class();

public:
    void set_world_rotation(const glm::vec3& rotation, bool sweep = false, bool teleport = false);
    void add_world_rotation(const glm::vec3& rotation, bool sweep = false, bool teleport = false);
    void set_world_location(const glm::vec3& location, bool sweep = false, bool teleport = false);
};
}