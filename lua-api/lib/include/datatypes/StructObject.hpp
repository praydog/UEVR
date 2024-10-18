#pragma once

#include <uevr/API.hpp>
#include "ScriptPrerequisites.hpp"

namespace lua::datatypes {
    struct StructObject {
        StructObject(void* obj, uevr::API::UStruct* def) : object{ obj }, desc{ def } {}
        StructObject(uevr::API::UStruct* def); // Allocates a new structure given a definition
        StructObject(uevr::API::UObject* obj);
        ~StructObject();

        void construct(uevr::API::UStruct* def);

        void* object{ nullptr };
        uevr::API::UStruct* desc{ nullptr };

        std::vector<uint8_t> created_object{}; // Only used when the object is created by second constructor
    };

    void bind_struct_object(sol::state_view& lua);
}