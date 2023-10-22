#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <spdlog/spdlog.h>

#include "EngineModule.hpp"

#include "FMalloc.hpp"

namespace sdk {
// There are many different ways to find this because the pointer is used all over.
FMalloc* FMalloc::get() {
    static FMalloc** result = []() -> FMalloc** {
        SPDLOG_INFO("[FMalloc::get] Finding GMalloc...");

        const auto core_uobject = sdk::get_ue_module(L"CoreUObject");

        if (core_uobject == nullptr) {
            return nullptr;
        }

        auto object_base_init_fn = utility::find_function_with_string_refs(core_uobject, L"gc.MaxObjectsNotConsideredByGC", 
                                                                                         L"/Script/Engine.GarbageCollectionSettings");

        // Alternative scan for REALLY old versions of UE4
        // MaxObjectsNotConsideredByGC
        // SizeOfPermanentObjectPool
        if (!object_base_init_fn) {
            object_base_init_fn = utility::find_function_with_string_refs(core_uobject, L"MaxObjectsNotConsideredByGC", 
                                                                                        L"SizeOfPermanentObjectPool");
            
            if (object_base_init_fn) {
                SPDLOG_INFO("[FMalloc::get] Found object base init function for old UE4 versions");
            }
        }

        if (!object_base_init_fn) {
            SPDLOG_ERROR("[FMalloc::get] Failed to find object base init function");
            return nullptr;
        }

        FMalloc** result = nullptr;

        utility::exhaustive_decode((uint8_t*)*object_base_init_fn, 0x1000, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
            if (result != nullptr) {
                return utility::ExhaustionResult::BREAK;
            }

            if (ctx.instrux.BranchInfo.IsBranch) {
                return utility::ExhaustionResult::CONTINUE;
            }

            const auto disp = utility::resolve_displacement(ctx.addr);

            if (!disp) {
                return utility::ExhaustionResult::CONTINUE;
            }

            if (IsBadReadPtr((void*)*disp, sizeof(void*))) {
                return utility::ExhaustionResult::CONTINUE;
            }

            const auto obj = *(void**)*disp;

            if (IsBadReadPtr(obj, sizeof(void*))) {
                return utility::ExhaustionResult::CONTINUE;
            }

            const auto vtable = *(void***)obj;

            if (IsBadReadPtr(vtable, sizeof(void*) * 30)) {
                return utility::ExhaustionResult::CONTINUE;
            }

            if (IsBadReadPtr(vtable[0], sizeof(void*))) {
                return utility::ExhaustionResult::CONTINUE;
            }

            for (auto i = 0; i < 30; ++i) {
                const auto fn = (uintptr_t)vtable[i];

                if (IsBadReadPtr((void*)fn, sizeof(void*))) {
                    return utility::ExhaustionResult::CONTINUE;
                }

                // Look for really small functions.
                uint32_t distance_to_ret = 0;
                utility::exhaustive_decode((uint8_t*)fn, 100, [&](utility::ExhaustionContext& ctx2) -> utility::ExhaustionResult {
                    ++distance_to_ret;
                    return utility::ExhaustionResult::CONTINUE;
                });

                if (distance_to_ret > 10) {
                    continue;
                }

                const std::vector<std::wstring> allocator_names {
                    L"binned",
                    L"binned2",
                    L"tbbmalloc",
                    L"ansimalloc",
                    L"binnedmalloc",
                    L"binnedmalloc2",
                    L"binnedmalloc3",
                    L"mimalloc",
                    L"stompmalloc",
                };

                for (const auto& name : allocator_names) {
                    if (utility::find_string_reference_in_path(fn, name)) {
                        SPDLOG_INFO("[FMalloc::get] Found GMalloc at 0x{:x}", *disp);
                        result = (FMalloc**)*disp;
                        return utility::ExhaustionResult::BREAK;
                    }
                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (!result) {
            SPDLOG_ERROR("[FMalloc::get] Failed to find GMalloc");
            return nullptr;
        }

        return result;
    }();

    if (result == nullptr) {
        return nullptr;
    }

    return *result;
}
}