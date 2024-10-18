#pragma once

#include <sol/sol.hpp>

#include <lstate.h> // weird include order because of sol
#include <lgc.h>

#include <uevr/API.hpp>

namespace detail {
template<typename T>
concept UObjectBased = std::is_base_of_v<uevr::API::UObject, T>;

constexpr uintptr_t FAKE_OBJECT_ADDR = 12345;
}

// Object pooling for UObject based pointers using a specialization for sol_lua_push
template<detail::UObjectBased T>
int sol_lua_push(sol::types<T*>, lua_State* l, T* obj) {
    static const std::string TABLE_NAME = std::string{"_sol_lua_push_objects"} + "_" + T::internal_name().data();

    if (obj != nullptr) {
        auto sv = sol::state_view{l};
        sol::lua_table objects = sv[TABLE_NAME];

        if (sol::object lua_obj = objects[(uintptr_t)obj]; !lua_obj.is<sol::nil_t>()) {
            // renew the reference so it doesn't get collected
            // had to dig deep in the lua source to figure out this nonsense
            lua_obj.push();
            auto g = G(l);
            auto tv = s2v(l->top - 1);
            auto& gc = tv->value_.gc;
            resetbits(gc->marked, bitmask(BLACKBIT) | WHITEBITS); // "touches" the object, marking it gray. lowers the insane GC frequency on our weak table

            return 1;
        } else {
            if ((uintptr_t)obj != detail::FAKE_OBJECT_ADDR) {
                // Keep alive logic for UObjects? Does this exist???
                //printf("Pushing object %p (%s)\n", obj, T::internal_name().data());
            }

            int32_t backpedal = 0;

            if ((uintptr_t)obj != detail::FAKE_OBJECT_ADDR) {
                backpedal = sol::stack::push<sol::detail::as_pointer_tag<std::remove_pointer_t<T>>>(l, obj);
                auto ref = sol::stack::get<sol::object>(l, -backpedal);

                // keep a weak reference to the object for caching
                objects[(uintptr_t)obj] = ref;

                return backpedal;
            } else {
                backpedal = sol::stack::push<sol::detail::as_pointer_tag<std::remove_pointer_t<T>>>(l, obj);
            }

            return backpedal;
        }
    }

    return sol::stack::push(l, sol::nil);
}