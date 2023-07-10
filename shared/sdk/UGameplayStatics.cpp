#include "UObjectArray.hpp"

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
}