#pragma once

#include <vector>

#include "Math.hpp"

#include "UClass.hpp"
#include "TArray.hpp"

namespace sdk {
class UCameraComponent;
class USceneComponent;
class UActorComponent;
class AActor;
class UWorld;

class UPrimitiveComponent;

class AActor : public UObject {
public:
    static UClass* static_class();

    // it takes a hit result as out param but we dont care.
    bool set_actor_location(const glm::vec3& location, bool sweep, bool teleport);
    glm::vec3 get_actor_location();

    bool set_actor_rotation(const glm::vec3& rotation, bool teleport);

    USceneComponent* get_component_by_class(UClass* uclass);
    UCameraComponent* get_camera_component();

    UActorComponent* add_component_by_class(UClass* uclass, bool deferred = false);
    UActorComponent* add_component_by_class_ex(UClass* uclass, bool deferred = false);
    void finish_add_component(sdk::UObject* component);
    void finish_add_component_ex(sdk::UObject* component);

    std::vector<UActorComponent*> get_components_by_class(UClass* uclass);
    std::vector<UActorComponent*> get_all_components();

    void destroy_component(UActorComponent* component);
    void destroy_actor();

    USceneComponent* get_root_component();
    void set_root_component(USceneComponent* component);

    UObject* get_level();
    UWorld* get_world();

    TArray<AActor*> get_overlapping_actors(UClass* uclass = nullptr);
    TArray<UPrimitiveComponent*> get_overlapping_components();

protected:
};
}