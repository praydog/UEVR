#pragma once

namespace sdk {
class AActor;
class UWorld;

class UActorComponent : public UObject {
public:
    static UClass* static_class();

    AActor* get_owner();
    void destroy_component();
    void register_component_with_world(UWorld* world);
};
}