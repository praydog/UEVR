#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_double.hpp>

#include <sol/sol.hpp>
#include <uevr/API.hpp>

namespace lua::datatypes {
    using Quaternionf = glm::quat;
    using Quaterniond = glm::dquat;

    void bind_quaternions(sol::state_view& lua);
}