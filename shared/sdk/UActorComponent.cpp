#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "EngineModule.hpp"

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

void UActorComponent::register_component_with_world(UWorld* world) {
    using Fn = void(*)(UActorComponent*, UWorld*, void*, void*);
    static auto fn = []() -> Fn {
        SPDLOG_INFO("Finding UActorComponent::RegisterComponentWithWorld");

        Fn result{};

        const auto module = sdk::get_ue_module(L"Engine");
        const auto str = utility::scan_string(module, L"PC_InputComponent0");

        if (!str) {
            SPDLOG_ERROR("Failed to find UActorComponent::RegisterComponentWithWorld (no string)");
            return result;
        }

        const auto ref = utility::scan_displacement_reference(module, *str);

        if (!ref) {
            SPDLOG_ERROR("Failed to find UActorComponent::RegisterComponentWithWorld (no reference)");
            return result;
        }

        const auto next_instr = *ref + 4;
        
        utility::exhaustive_decode((uint8_t*)next_instr, 300, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
            if (result != nullptr) {
                return utility::ExhaustionResult::BREAK;
            }

            if (ctx.instrux.IsRipRelative && std::string_view{ctx.instrux.Mnemonic}.starts_with("CALL")) {
                const auto called_fn = utility::resolve_displacement(ctx.addr);

                if (!called_fn) {
                    return utility::ExhaustionResult::STEP_OVER;
                }

                if (utility::find_string_reference_in_path(*called_fn, L"ActorComponent")) {
                    result = (Fn)*called_fn;
                    SPDLOG_INFO("Found UActorComponent::RegisterComponentWithWorld at {:x}", *called_fn);
                    return utility::ExhaustionResult::BREAK;
                }

                return utility::ExhaustionResult::STEP_OVER;
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        return result;
    }();

    fn(this, world, nullptr, nullptr);
}
}