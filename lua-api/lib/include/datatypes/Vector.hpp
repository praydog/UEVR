#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <sol/sol.hpp>
#include <uevr/API.hpp>

namespace lua::datatypes {
    using Vector2f = glm::vec2;
    using Vector2d = glm::dvec2;
    using Vector3f = glm::vec3;
    using Vector3d = glm::dvec3;
    using Vector4f = glm::vec4;
    using Vector4d = glm::dvec4;

    void bind_vectors(sol::state_view& lua);
}