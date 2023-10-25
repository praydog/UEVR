#include "UObjectArray.hpp"
#include "UClass.hpp"
#include "AActor.hpp"

#include "UActorComponent.hpp"

namespace sdk {
UClass* UActorComponent::static_class() {
    return sdk::find_uobject<UClass>(L"Class /Script/Engine.ActorComponent");
}

AActor* UActorComponent::get_owner() {
    static const auto func = UActorComponent::static_class()->find_function(L"GetOwner");

    if (func == nullptr) {
        return nullptr;
    }

    struct {
        AActor* ReturnValue;
    } params;

    params.ReturnValue = nullptr;

    this->process_event(func, &params);

    return params.ReturnValue;
}

void UActorComponent::destroy_component() {
    auto owner = this->get_owner();

    if (owner != nullptr) {
        owner->destroy_component(this);
    }
}
}