#pragma once

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/matrix_interpolation.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_major_storage.hpp>
#include <glm/gtx/compatibility.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vector_angle.hpp>

using Vector2f = glm::vec2;
using Vector3f = glm::vec3;
using Vector4f = glm::vec4;
using Vector3d = glm::dvec3;
using Vector4d = glm::dvec4;
using Matrix3x3f = glm::mat3x3;
using Matrix3x4f = glm::mat3x4;
using Matrix4x4f = glm::mat4x4;
using Matrix4x4d = glm::dmat4x4;

namespace utility::math {
using namespace glm;

static vec3 euler_angles(const glm::mat4& rot);
static float fix_angle(float ang);
static void fix_angles(const glm::vec3& angles);
static float clamp_pitch(float ang);

static vec3 euler_angles_from_steamvr(const glm::mat4& rot) {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
    const auto m = rot *
        Matrix4x4f {
            1, 0, 0, 0,
            0, 0, 1, 0,
            0, 1, 0, 0,
            0, 0, 0, 1
        };
    glm::extractEulerAngleYXZ(rot, yaw, pitch, roll);

    return { pitch, -yaw, -roll };
}

static vec3 euler_angles_from_steamvr(const glm::quat& q) {
    const auto m = glm::mat4{q};
    return euler_angles_from_steamvr(m);
}

static vec3 euler_angles_from_ue4(const glm::quat q) {
    const auto m = glm::mat4{q};

    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
    glm::extractEulerAngleYZX(m, yaw, roll, pitch);

    return { pitch, yaw, roll };
}

static glm::quat glm_to_ue4(const glm::quat q) {
    return glm::quat{ q.w, -q.z, q.x, q.y };
}

static vec3 glm_to_ue4(const glm::vec3 v) {
    return vec3{ -v.z, v.x, v.y };
}

static float fix_angle(float ang) {
    auto angDeg = glm::degrees(ang);

    while (angDeg > 180.0f) {
        angDeg -= 360.0f;
    }

    while (angDeg < -180.0f) {
        angDeg += 360.0f;
    }

    return glm::radians(angDeg);
}

static void fix_angles(glm::vec3& angles) {
    angles[0] = fix_angle(angles[0]);
    angles[1] = fix_angle(angles[1]);
    angles[2] = fix_angle(angles[2]);
}

float clamp_pitch(float ang) {
    return std::clamp(ang, glm::radians(-89.0f), glm::radians(89.0f));
}

static glm::mat4 remove_y_component(const glm::mat4& mat) {
    // Remove y component and normalize so we have the facing direction
    const auto forward_dir = glm::normalize(Vector3f{ mat[2].x, 0.0f, mat[2].z });

    return glm::rowMajor4(glm::lookAtLH(Vector3f{}, Vector3f{ forward_dir }, Vector3f(0.0f, 1.0f, 0.0f)));
}

static quat to_quat(const vec3& v) {
    const auto mat = glm::rowMajor4(glm::lookAtLH(Vector3f{0.0f, 0.0f, 0.0f}, v, Vector3f{0.0f, 1.0f, 0.0f}));

    return glm::quat{mat};
}

static quat flatten(const quat& q) {
    const auto forward = glm::normalize(glm::quat{q} * Vector3f{ 0.0f, 0.0f, 1.0f });
    const auto flattened_forward = glm::normalize(Vector3f{forward.x, 0.0f, forward.z});
    return utility::math::to_quat(flattened_forward);
}

// Obtains the forward dir, and isolates the pitch component
static quat pitch_only(const quat& q) {
    const auto forward = glm::normalize(glm::quat{q} * Vector3f{ 0.0f, 0.0f, 1.0f });

    const auto pitch = glm::degrees(glm::asin(forward.y));
    return glm::quat(glm::radians(glm::vec3{ pitch, 0.0f, 0.0f }));
}
}