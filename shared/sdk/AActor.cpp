#include <vector>
#include "UObjectArray.hpp"

#include "AActor.hpp"

namespace sdk {
UClass* AActor::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.Actor");
}

bool AActor::set_actor_location(const glm::vec3& location, bool sweep, bool teleport) {
    static const auto func = static_class()->find_function(L"K2_SetActorLocation");
    static const auto fhitresult = sdk::find_uobject<UScriptStruct>(L"ScriptStruct /Script/Engine.HitResult");

    // Need to dynamically allocate the params because of unknown FHitResult size
    std::vector<uint8_t> params{};

    // add a vec3
    params.insert(params.end(), (uint8_t*)&location, (uint8_t*)&location + sizeof(glm::vec3));
    // add a bool
    params.insert(params.end(), (uint8_t*)&sweep, (uint8_t*)&sweep + sizeof(bool));
    // align    
    params.insert(params.end(), 3, 0);
    // add a FHitResult
    params.insert(params.end(), fhitresult->get_struct_size(), 0);
    // add a bool
    params.insert(params.end(), (uint8_t*)&teleport, (uint8_t*)&teleport + sizeof(bool));
    // add a bool
    bool ret = false;
    void* ret_ptr = &ret;
    params.insert(params.end(), (uint8_t*)&ret_ptr, (uint8_t*)&ret_ptr + sizeof(void*));

    this->process_event(func, params.data());

    return ret;
}

glm::vec3 AActor::get_actor_location() {
    static const auto func = static_class()->find_function(L"K2_GetActorLocation");

    struct {
        glm::vec3 result{};
    } params;

    this->process_event(func, &params);

    return params.result;
};
}