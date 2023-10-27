#pragma once

#include "UClass.hpp"

#include "Math.hpp"

namespace sdk {
class APlayerController;
class AActor;

class UGameplayStatics : public UObject {
public:
    static UGameplayStatics* get();
    static sdk::UClass* static_class();

    APlayerController* get_player_controller(UObject* world_context, int32_t index);

    AActor* begin_deferred_actor_spawn_from_class(UObject* world_context, UClass* actor_class, const glm::vec3& location, uint32_t collision_method = 1, AActor* owner = nullptr);
    AActor* finish_spawning_actor(AActor* actor, const glm::vec3& location);

    AActor* spawn_actor(UObject* world_context, UClass* actor_class, const glm::vec3& location, uint32_t collision_method = 1, AActor* owner = nullptr) {
        auto result = begin_deferred_actor_spawn_from_class(world_context, actor_class, location, collision_method, owner);

        if (result != nullptr) {
            return finish_spawning_actor(result, location);
        }

        return nullptr;
    }

    UObject* spawn_object(UClass* uclass, UObject* outer);

protected:
};
}