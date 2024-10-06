#include <datatypes/Quaternion.hpp>

namespace lua::datatypes {
    void bind_quaternions(sol::state_view& lua) {
        #define BIND_QUATERNION_LIKE(name, datatype) \
            lua.new_usertype<name>(#name, \
                "x", &name::x, \
                "y", &name::y, \
                "z", &name::z, \
                "w", &name::w, \
                "X", &name::x, \
                "Y", &name::y, \
                "Z", &name::z, \
                "W", &name::w, \
                "length", [](name& v) { return glm::length(v); }, \
                "normalize", [](name& v) { v = glm::normalize(v); }, \
                "normalized", [](name& v) { return glm::normalize(v); }, \
                "conjugate", [](name& v) { return glm::conjugate(v); }, \
                "inverse", [](name& v) { return glm::inverse(v); }, \
                "dot", [](name& v1, name& v2) { return glm::dot(v1, v2); }, \
                "slerp", [](name& v1, name& v2, datatype t) { return glm::slerp(v1, v2, t); }, \
                sol::meta_function::addition, [](name& lhs, name& rhs) { return lhs + rhs; }, \
                sol::meta_function::subtraction, [](name& lhs, name& rhs) { return lhs - rhs; }, \
                sol::meta_function::multiplication, [](name& lhs, datatype scalar) { return lhs * scalar; }
        
        #define BIND_QUATERNION_LIKE_END() \
            );
        
        BIND_QUATERNION_LIKE(Quaternionf, float),
            sol::meta_function::construct, sol::constructors<Quaternionf(float, float, float, float)>()
        BIND_QUATERNION_LIKE_END();
        
        BIND_QUATERNION_LIKE(Quaterniond, double),
            sol::meta_function::construct, sol::constructors<Quaterniond(double, double, double, double)>()
        BIND_QUATERNION_LIKE_END();
    }
}