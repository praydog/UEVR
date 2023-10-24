#include <spdlog/spdlog.h>

#include <bdshemu.h>

#include <utility/Scan.hpp>
#include <utility/String.hpp>
#include <utility/Emulation.hpp>

#include <tracy/Tracy.hpp>

#include "EngineModule.hpp"

#include "FName.hpp"

namespace sdk {
/*
4.27:
+ 0x8 NumNetGUIDsPending
+ 0x7 SkeletalMeshComponentClothTick
+ 0x7 DefaultModulationPlugin
+ 0x4 Oculus Quest2
+ 0x4 FSequencerPlayerAnimSequence
+ 0x4 EndPhysicsTick
+ 0x4 StartPhysicsTick
+ 0x4 TickAnimationSharing
+ 0x4 LakeCollisionComponent
+ 0x4 SkeletalMeshComponentEndPhysicsTick
+ 0x4 Behavior
+ 0x4 FSlateMouseEventsMetaData
+ 0x4 FSlateCursorMetaData
+ 0x4 SoundExporterWAV
+ 0x4 FReflectionMetaData
+ 0x4 GameDefaultMap
+ 0x4 Test FName
+ 0x4 WaterBodyCollision
+ 0x4 WidgetTree
+ 0x4 FTagMetaData
+ 0x4 FSlateToolTipMetaData
+ 0x4 ParticleSystemManager
+ 0x4 Plugins
+ 0x4 FNavigationMetaData
+ 0x4 FSceneViewExtensionContext
*/

/*
4.14:
0x8 bIsPlaying
0x8 FLandscapeUniformShaderParameters
0x8 STAT_ColorList
0x7 OPUS
0x4 LightComponent
0x4 FPostProcessMaterialNode
0x4 SoundExporterWAV
0x4 Component
0x4 STextBlock
0x4 FLightPropagationVolumeSettings
0x4 CraneCameraMount
0x4 Priority
0x4 FTagMetaData
0x4 FNavigationMetaData
*/

/*
Present in both (and 5.0.3+):
+ 0x4 FTagMetaData
+ 0x4 FNavigationMetaData
*/
namespace detail {
std::optional<FName::ConstructorFn> get_constructor_from_candidate(std::wstring_view module_candidate, std::wstring_view str_candidate) try {
    ZoneScopedN("sdk::detail::get_constructor_from_candidate");
    SPDLOG_INFO("FName::get_constructor_from_candidate: str_candidate={}", utility::narrow(str_candidate.data()));

    const auto module = sdk::get_ue_module(module_candidate.data());

    if (module == nullptr) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to get module {}", utility::narrow(module_candidate.data()));
        return std::nullopt;
    }

    const auto str_data = utility::scan_string(module, str_candidate.data());

    if (!str_data) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to get string data for {}", utility::narrow(str_candidate.data()));
        return std::nullopt;
    }

    SPDLOG_INFO("FName::get_constructor_from_candidate: str_data={:x}", *str_data);

    const auto str_ref = utility::scan_displacement_reference(module, *str_data);

    if (!str_ref) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to get string reference for {}", utility::narrow(str_candidate.data()));
        return std::nullopt;
    }

    SPDLOG_INFO("FName::get_constructor_from_candidate: str_ref={:x}", *str_ref);

    const auto next_instr = *str_ref + 4;
    const auto call_instr = utility::scan_mnemonic(next_instr, 10, "CALL");

    if (!call_instr) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to get call instruction for {}", utility::narrow(str_candidate.data()));
        return std::nullopt;
    }

    SPDLOG_INFO("FName::get_constructor_from_candidate: call_instr={:x}", *call_instr);

    const auto decoded = utility::decode_one((uint8_t*)*call_instr);

    if (!decoded) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to decode call instruction for {}", utility::narrow(str_candidate.data()));
        return std::nullopt;
    }

    const auto displacement = utility::resolve_displacement(*call_instr);

    if (!displacement) {
        SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to resolve displacement for {}", utility::narrow(str_candidate.data()));
        return std::nullopt;
    }

    std::optional<FName::ConstructorFn> result{};

    if (decoded->BranchInfo.IsIndirect) {
        SPDLOG_INFO("FName::get_constructor_from_candidate: indirect call");
        if (*displacement == 0) {
            SPDLOG_ERROR("FName::get_constructor_from_candidate: indirect call with null displacement");
            return std::nullopt;
        }

        result = *(FName::ConstructorFn*)*displacement;
    } else {
        SPDLOG_INFO("FName::get_constructor_from_candidate: direct call");
        result = (FName::ConstructorFn)*displacement;
    }

    SPDLOG_INFO("FName::get_constructor_from_candidate: result={:x}", (uintptr_t)*result);
    return result;
} catch(...) {
    SPDLOG_ERROR("FName::get_constructor_from_candidate: Failed to get constructor from candidate due to exception");
    return std::nullopt;
}
}

std::optional<FName::ConstructorFn> FName::get_constructor() {
    static auto result = []() -> std::optional<FName::ConstructorFn> {
        ZoneScopedN("sdk::FName::get_constructor static init");

        struct Candidate {
            std::wstring_view module;
            std::wstring_view str;
        };

        std::vector<Candidate> candidates {
            Candidate{L"SlateCore", L"FTagMetaData"},
            Candidate{L"SlateCore", L"FNavigationMetaData"}
        };

        const auto now = std::chrono::high_resolution_clock::now();

        for (const auto& candidate : candidates) {
            const auto constructor = detail::get_constructor_from_candidate(candidate.module, candidate.str);

            if (constructor) {
                const auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - now).count();
                SPDLOG_INFO("FName::get_constructor: Found constructor in {} ms", time_elapsed);
                return constructor;
            }
        }

        SPDLOG_ERROR("FName::get_constructor: Failed to find constructor");
        return std::nullopt;
    }();

    return result;
}

namespace detail {
// Alternative for ToString if a majority of the calls are inlined
// not all of them are though.
std::optional<FName::ToStringFn> inlined_find_to_string() try {
    ZoneScopedN("sdk::detail::inlined_find_to_string");
    SPDLOG_INFO("FName::get_to_string: inlined_find_to_string");

    const auto module = sdk::get_ue_module(L"AnimGraphRuntime");

    if (module == nullptr) {
        SPDLOG_ERROR("FName::get_to_string (inlined): Failed to get module");
        return std::nullopt;
    }

    std::vector<std::wstring> candidates {
        L" Bone: %s, Look At Location : %s, Target Location : %s)",
        L"Commandlet %s finished execution (result %d)"
    };

    for (auto candidate : candidates) {
        const auto str_data = utility::scan_string(module, candidate.data());

        if (!str_data) {
            SPDLOG_ERROR("FName::get_to_string (inlined): Failed to get string data");
            continue;
        }
        
        SPDLOG_INFO("FName::get_to_string (inlined): str_data={:x}", *str_data);

        const auto str_ref = utility::scan_displacement_reference(module, *str_data);

        if (!str_ref) {
            SPDLOG_ERROR("FName::get_to_string  (inlined): Failed to get string reference");
            continue;
        }

        SPDLOG_INFO("FName::get_to_string (inlined): str_ref={:x}", *str_ref);

        auto attempt_find_from_addr = [&](uintptr_t addr) -> std::optional<FName::ToStringFn> {
            auto instructions = utility::get_disassembly_behind(addr);

            if (instructions.empty()) {
                SPDLOG_ERROR("FName::get_to_string (inlined): Failed to get instructions");
                return std::nullopt;
            }

            std::reverse(instructions.begin(), instructions.end());

            for (auto& instr : instructions) {
                if (std::string_view{instr.instrux.Mnemonic}.starts_with("CALL")) {
                    const auto displacement = utility::resolve_displacement(instr.addr);

                    if (instr.instrux.BranchInfo.IsIndirect) {
                        if (!IsBadReadPtr((void*)*displacement, sizeof(void*))) {
                            return *(FName::ToStringFn*)*displacement;
                        }
                    } else {
                        return (FName::ToStringFn)*displacement;
                    }
                    
                    break;
                }
            }

            return std::nullopt;
        };

        auto result = attempt_find_from_addr(*str_ref);

        // very alternative scan, for when this isn't inlined...
        if (!result) {
            SPDLOG_INFO("Attempting to perform alternative scan to the alternative scan");

            const auto fn_begin = utility::find_function_start(*str_ref);

            if (fn_begin) {
                const auto fn_callsite = utility::scan_relative_reference_strict(module, *fn_begin, "E8");

                if (fn_callsite) {
                    result = attempt_find_from_addr(*fn_callsite);
                } else {
                    SPDLOG_ERROR("FName::get_to_string (inlined): Failed to find function callsite for alternative alternative");
                }
            } else {
                SPDLOG_ERROR("FName::get_to_string (inlined): Failed to find function begin for alternative alternative");
            }
        }

        if (result) {
            SPDLOG_INFO("FName::get_to_string (inlined alternative): result={:x}", (uintptr_t)*result);
        } else {
            SPDLOG_ERROR("FName::get_to_string (inlined alternative): Failed to find ToString function");
        }

        return result;
    }

    return std::nullopt;
} catch(...) {
    SPDLOG_ERROR("FName::get_to_string (inlined): Failed to find ToString function due to exception");
    return std::nullopt;
}

std::optional<FName::ToStringFn> standard_find_to_string() {
    ZoneScopedN("sdk::detail::standard_find_to_string");
    SPDLOG_INFO("FName::get_to_string: standard_find_to_string");

    const auto module = sdk::get_ue_module(L"Engine");

    if (module == nullptr) {
        SPDLOG_ERROR("FName::get_to_string: Failed to get module");
        return std::nullopt;
    }

    const auto str_data = utility::scan_string(module, L"TAutoWeakObjectPtr<%s%s>");

    if (!str_data) {
        SPDLOG_ERROR("FName::get_to_string: Failed to get string data");
        return std::nullopt;
    }
    
    SPDLOG_INFO("FName::get_to_string: str_data={:x}", *str_data);

    const auto str_ref = utility::scan_displacement_reference(module, *str_data);

    if (!str_ref) {
        SPDLOG_ERROR("FName::get_to_string: Failed to get string reference");
        return std::nullopt;
    }

    SPDLOG_INFO("FName::get_to_string: str_ref={:x}", *str_ref);

    const auto containing_func = utility::find_function_start(*str_ref);

    if (!containing_func) {
        SPDLOG_ERROR("FName::get_to_string: Failed to find containing function");
        return std::nullopt;
    }

    SPDLOG_INFO("FName::get_to_string: containing_func={:x}", *containing_func);

    std::optional<uintptr_t> last_direct_call{};
    std::optional<FName::ToStringFn> result{};

    // It's known that FName::ToString precedes a string reference to TAutoWeakObjectPtr<%s%s>.
    // So we must keep track of each direct call that precedes it
    // because there's an indirect call that also precedes it.
    // TODO: Possibly fix this for modular builds.
    utility::exhaustive_decode((uint8_t*)*containing_func, 100, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
        if (result) {
            return utility::ExhaustionResult::BREAK;
        }

        if (std::string_view{ix.Mnemonic}.starts_with("CALL") && !ix.BranchInfo.IsIndirect) {
            last_direct_call = utility::calculate_absolute((ip + ix.Length) - 4);
            SPDLOG_INFO("Found a direct call to {:x} at {:x}", *last_direct_call, ip);
        }

        if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
            return utility::ExhaustionResult::STEP_OVER;
        }

        const auto displacement = utility::resolve_displacement(ip);

        if (displacement && *displacement == str_data) {
            if (last_direct_call) {
                result = (FName::ToStringFn)*last_direct_call;
            }

            return utility::ExhaustionResult::BREAK;
        }

        return utility::ExhaustionResult::CONTINUE;
    });

    if (!result) {
        SPDLOG_ERROR("FName::get_to_string: Failed to find ToString function");
        return std::nullopt;
    }

    // Check the ToString function's code to see if it's accessing memory that's >= sizeof(FName)
    // because compiler optimizations can turn the function into UClass::GetName() which isn't inlined which calls GetFName().ToString()
    // if it is, we can get the first function it calls and use that instead.
    utility::ShemuContext ctx{module};
    ctx.ctx->Registers.RegRip = (uintptr_t)*result;
    ctx.ctx->Registers.RegRcx = 0x12345678; // magic number

    try {
        utility::emulate(module, (uintptr_t)*result, 100, ctx, [&](const utility::ShemuContextExtended& ctx) -> utility::ExhaustionResult {
            if (ctx.next.writes_to_memory || std::string_view{ctx.next.ix.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }
            
            const auto& nix = ctx.next.ix;

            if (nix.OperandsCount < 2) {
                return utility::ExhaustionResult::CONTINUE;
            }

            if (nix.Operands[1].Type == ND_OP_MEM && nix.Operands[1].Info.Memory.HasBase && 
                nix.Operands[1].Info.Memory.HasDisp && nix.Operands[1].Info.Memory.Disp >= 8) 
            {
                const auto reg_value = ((ND_UINT64*)&ctx.ctx->ctx->Registers)[nix.Operands[1].Info.Memory.Base];

                if (reg_value == 0x12345678) {
                    char text[ND_MIN_BUF_SIZE];
                    NdToText(&nix, 0, sizeof(text), text);
                    SPDLOG_INFO("{}", text);
                    SPDLOG_INFO("FName::get_to_string: Found function is not inlined (UClass::GetName)");

                    const auto next_call = utility::scan_mnemonic(ctx.ctx->ctx->Registers.RegRip, 20, "CALL");

                    if (next_call) {
                        const auto resolved_call = utility::resolve_displacement(*next_call);

                        if (resolved_call) {
                            *result = (FName::ToStringFn)utility::calculate_absolute(*next_call + 1);
                            SPDLOG_INFO("FName::get_to_string: Found function to use instead {:x}", (uintptr_t)*result);
                        } else {
                            SPDLOG_ERROR("FName::get_to_string: Failed to resolve displacement for next call");
                        }
                    }

                    return utility::ExhaustionResult::BREAK;
                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });
    } catch(...) {
        SPDLOG_ERROR("FName::get_to_string: Failed to emulate");
    }

    SPDLOG_INFO("FName::get_to_string: result={:x}", (uintptr_t)*result);

    return result;
}
}

std::optional<FName::ToStringFn> FName::get_to_string() {
    static auto result = []() -> std::optional<FName::ToStringFn> {
        ZoneScopedN("sdk::FName::get_to_string static init");
        SPDLOG_INFO("FName::get_to_string");

        const auto inlined_result = detail::inlined_find_to_string();

        if (inlined_result) {
            return inlined_result;
        }

        return detail::standard_find_to_string();
    }();

    return result;
}

FName::FName(std::wstring_view name, EFindName find_type) {
    const auto constructor = get_constructor();

    if (!constructor) {
        return;
    }

    const auto fn = *constructor;

    fn(this, name.data(), static_cast<uint32_t>(find_type));
}

std::wstring FName::to_string() const {
    static bool once = true;

    if (once) {
        SPDLOG_INFO("Calling FName::to_string for the first time");
        once = false;
    }

    const auto to_string = get_to_string();

    if (!to_string) {
        return L"";
    }

    const auto fn = *to_string;
    TArray<wchar_t> buffer{};
    /*wchar_t scratch[512]{};
    buffer.data = scratch;
    buffer.capacity = 512 * sizeof(wchar_t);*/

    fn(this, &buffer);

    if (buffer.data != nullptr && buffer.count > 0) {
        return buffer.data;
    }

    return std::wstring{};
}
}