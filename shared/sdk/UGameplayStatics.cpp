#include "UObjectArray.hpp"

#include "ScriptVector.hpp"
#include "ScriptTransform.hpp"
#include "UFunction.hpp"
#include "FProperty.hpp"
#include "UGameplayStatics.hpp"

namespace sdk {
UGameplayStatics* UGameplayStatics::get() {
    return static_class()->get_class_default_object<UGameplayStatics>();
}

sdk::UClass* UGameplayStatics::static_class() {
    return sdk::find_uobject<sdk::UClass>(L"Class /Script/Engine.GameplayStatics");
}

sdk::APlayerController* UGameplayStatics::get_player_controller(UObject* world_context, int32_t index) {
    static const auto func = static_class()->find_function(L"GetPlayerController");

    struct {
        UObject* world_context{};
        int32_t index{};
        APlayerController* result{};
    } params;

    params.world_context = world_context;
    params.index = index;

    this->process_event(func, &params);
    return params.result;
}

AActor* UGameplayStatics::begin_deferred_actor_spawn_from_class(UObject* world_context, UClass* actor_class, const glm::vec3& location, uint32_t collision_method, AActor* owner) {
    static const auto func = static_class()->find_function(L"BeginDeferredActorSpawnFromClass");

    if (func == nullptr) {
        return nullptr;
    }
    
    const auto fvector = sdk::ScriptVector::static_struct();
    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    /*struct {
        UObject* world_context{};
        UClass* actor_class{};
        struct {
            double rotation[4]{0.0, 0.0, 0.0, 1.0};
            glm::vec<4, double> translation{};
            glm::vec<4, double> scale{1.0, 1.0, 1.0, 1.0};
        } transform;
        uint32_t spawn_collision_handling_override{};
        AActor* owner{};
        AActor* result{};
    } params_ue5{};

    struct {
        UObject* world_context{};
        UClass* actor_class{};
        struct {
            float rotation[4]{0.0f, 0.0f, 0.0f, 1.0f};
            glm::vec4 translation{};
            glm::vec4 scale{1.0f, 1.0f, 1.0f, 1.0f};
        } transform;
        uint32_t spawn_collision_handling_override{};
        AActor* owner{};
        AActor* result{};
    } params_ue4{};

    if (is_ue5) {
        params_ue5.world_context = world_context;
        params_ue5.actor_class = actor_class;
        params_ue5.transform.translation = glm::vec<4, double>{location, 1.0};
        params_ue5.spawn_collision_handling_override = collision_method;
        params_ue5.owner = owner;
    } else {
        params_ue4.world_context = world_context;
        params_ue4.actor_class = actor_class;
        params_ue4.transform.translation = glm::vec4{location, 1.0};
        params_ue4.spawn_collision_handling_override = collision_method;
        params_ue4.owner = owner;
    }

    if (is_ue5) {
        this->process_event(func, &params_ue5);
        return params_ue5.result;
    }

    this->process_event(func, &params_ue4);
    return params_ue4.result;*/

    std::vector<uint8_t> params{};
    params.insert(params.end(), (uint8_t*)&world_context, (uint8_t*)&world_context + sizeof(void*));
    params.insert(params.end(), (uint8_t*)&actor_class, (uint8_t*)&actor_class + sizeof(void*));

    static const auto spawn_transform_offset = func->find_property(L"SpawnTransform")->get_offset();

    // Pad until we reach the spawn transform
    if (params.size() < spawn_transform_offset) {
        params.insert(params.end(), spawn_transform_offset - params.size(), 0);
    }

    const auto transform = sdk::ScriptTransform::create_dynamic_struct(location, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});
    params.insert(params.end(), transform.begin(), transform.end());
    params.insert(params.end(), (uint8_t*)&collision_method, (uint8_t*)&collision_method + sizeof(uint32_t));

    // Align up to 8 bytes if necessary
    if (params.size() % sizeof(void*) != 0) {
        params.insert(params.end(), sizeof(void*) - (params.size() % sizeof(void*)), 0);
    }

    params.insert(params.end(), (uint8_t*)&owner, (uint8_t*)&owner + sizeof(void*));

    const auto result_offset = params.size();
    params.insert(params.end(), sizeof(void*), 0);

    this->process_event(func, params.data());

    return *(AActor**)(params.data() + result_offset);
}

AActor* UGameplayStatics::finish_spawning_actor(AActor* actor, const glm::vec3& location) {
    static const auto func = static_class()->find_function(L"FinishSpawningActor");

    if (func == nullptr) {
        return nullptr;
    }

    const auto fvector = sdk::ScriptVector::static_struct();
    const auto is_ue5 = fvector->get_struct_size() == sizeof(glm::vec<3, double>);

    std::vector<uint8_t> params{};
    params.insert(params.end(), (uint8_t*)&actor, (uint8_t*)&actor + sizeof(void*));

    static const auto spawn_transform_offset = func->find_property(L"SpawnTransform")->get_offset();

    // Pad until we reach the spawn transform
    if (params.size() < spawn_transform_offset) {
        params.insert(params.end(), spawn_transform_offset - params.size(), 0);
    }

    const auto transform = sdk::ScriptTransform::create_dynamic_struct(location, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});
    params.insert(params.end(), transform.begin(), transform.end());

    // Align up to 8 bytes if necessary
    if (params.size() % sizeof(void*) != 0) {
        params.insert(params.end(), sizeof(void*) - (params.size() % sizeof(void*)), 0);
    }
    
    const auto result_offset = params.size();
    params.insert(params.end(), sizeof(void*), 0);

    this->process_event(func, params.data()); 
    return *(AActor**)(params.data() + result_offset);
}

UObject* UGameplayStatics::spawn_object(UClass* uclass, UObject* outer) {
    static const auto func = static_class()->find_function(L"SpawnObject");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        UClass* uclass{};
        UObject* outer{};
        UObject* result{};
    } params{};

    params.uclass = uclass;
    params.outer = outer;

    this->process_event(func, &params);

    return params.result;
}
}