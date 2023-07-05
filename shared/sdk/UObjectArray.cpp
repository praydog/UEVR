#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include "EngineModule.hpp"

#include "FName.hpp"
#include "UObjectArray.hpp"

namespace sdk {
FUObjectArray* FUObjectArray::get() {
    static auto result = []() -> FUObjectArray* {
        SPDLOG_INFO("[FUObjectArray::get] Searching for FUObjectArray...");

        const auto engine = sdk::get_ue_module(L"Engine");

        if (engine == nullptr) {
            return nullptr;
        }

        const auto object_base_init_fn = utility::find_function_with_string_refs(engine, L"gc.MaxObjectsNotConsideredByGC", 
                                                                                         L"/Script/Engine.GarbageCollectionSettings");

        if (!object_base_init_fn) {
            SPDLOG_ERROR("[FUObjectArray::get] Failed to find object base init function");
            return nullptr;
        }

        SPDLOG_INFO("[FUObjectArray::get] Found object base init function at 0x{:x}", *object_base_init_fn);

        // Now, at this point we're in a bit of a predicament
        // There's a function that takes a reference to the GUObjectArray as a parameter within this function
        // but in some circumstances, it can become inlined
        FUObjectArray* result{nullptr};

        utility::exhaustive_decode((uint8_t*)*object_base_init_fn, 1024, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            if (result != nullptr) {
                return utility::ExhaustionResult::BREAK;
            }

            if (std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            const auto displacement = utility::resolve_displacement(ip);

            if (!displacement) {
                return utility::ExhaustionResult::CONTINUE;
            }

            if (IsBadReadPtr((void*)*displacement, 0x30)) {
                return utility::ExhaustionResult::CONTINUE;
            }

            // Now, we need to analyze this "structure" to see if it's actually a GUObjectArray
            // We need to make sure the integer values look integer-like, and if casting them
            // to 8 bytes, they must point to bad memory
            // also, the actual pointer values should be pointing to good memory
            int32_t& potential_obj_first_gc_index = *(int32_t*)(*displacement + 0x0);
            int32_t& potential_obj_last_non_gc_index = *(int32_t*)(*displacement + 0x4);
            int32_t& potential_obj_max_objects_not_consid_by_gc = *(int32_t*)(*displacement + 0x8);
            bool& potential_obj_is_open_for_disregard_for_gc = *(bool*)(*displacement + 0xC);

            // array of structures, unknown size
            void*& potential_obj_obj_objects = *(void**)(*displacement + 0x10);
            int32_t& potential_obj_obj_max = *(int32_t*)(*displacement + 0x18);
            int32_t& potential_obj_obj_count = *(int32_t*)(*displacement + 0x1C);

            // FChunkedFixedUObjectArray
            // this is a bit tricky and not necessarily needed to identify the GUObjectArray
            // however, it will be necessary to determine how to traverse the GUObjectArray
            void*& potential_pre_allocated_objects = *(void**)(*displacement + 0x18);
            int32_t& potential_chunked_obj_max = *(int32_t*)(*displacement + 0x20);
            int32_t& potential_chunked_obj_count = *(int32_t*)(*displacement + 0x24);
            int32_t& potential_max_chunks = *(int32_t*)(*displacement + 0x28);
            int32_t& potential_num_chunks = *(int32_t*)(*displacement + 0x2C);

            if (potential_obj_first_gc_index < 0 || potential_obj_last_non_gc_index < 0 || potential_obj_max_objects_not_consid_by_gc < 0) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to negative values", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            if (!IsBadReadPtr(*(void**)&potential_obj_first_gc_index, sizeof(void*))) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to valid pointer", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            if (!IsBadReadPtr(*(void**)&potential_obj_max_objects_not_consid_by_gc, sizeof(void*))) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to valid pointer", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            if (*(uint8_t*)&potential_obj_is_open_for_disregard_for_gc > 1) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid boolean", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            if (potential_obj_obj_objects == nullptr || IsBadReadPtr(potential_obj_obj_objects, sizeof(void*))) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid pointer", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }
            
            // At this point we've found it, check if it's a chunked array or not, and set a static variable
            if (potential_max_chunks > 0 && potential_num_chunks > 0 && potential_max_chunks < 1000 && potential_num_chunks <= potential_max_chunks) {
                try {
                    SPDLOG_INFO("max_chunks: {}, num_chunks: {}", potential_max_chunks, potential_num_chunks);

                    bool any_failed = false;

                    for (auto i = 0; i < potential_num_chunks; ++i) {
                        auto potential_chunked_obj = *(void**)((uintptr_t)potential_obj_obj_objects + (i * sizeof(void*)));

                        if (potential_chunked_obj == nullptr || IsBadReadPtr(potential_chunked_obj, sizeof(void*))) {
                            SPDLOG_INFO("GUObjectArray failed to pass the chunked test");
                            any_failed = true;
                            break;
                        }
                    }

                    if (!any_failed) {
                        SPDLOG_INFO("GUObjectArray appears to be chunked");
                        s_is_chunked = true;
                    } else {
                        SPDLOG_INFO("GUObjectArray appears to not be chunked (max_chunks: {}, num_chunks: {})", potential_max_chunks, potential_num_chunks);
                    }
                } catch(...) {
                    SPDLOG_ERROR("Failed to check if GUObjectArray is chunked");
                }
            } else {
                SPDLOG_INFO("GUObjectArray appears to not be chunked (max_chunks: {}, num_chunks: {})", potential_max_chunks, potential_num_chunks);
            }

            SPDLOG_INFO("Found GUObjectArray at 0x{:x}", *displacement);
            result = (FUObjectArray*)*displacement;
            return utility::ExhaustionResult::BREAK;
        });

        if (result == nullptr) {
            SPDLOG_ERROR("[FUObjectArray::get] Failed to find GUObjectArray");
            return nullptr;
        }

        // do an initial first pass as a test
        SPDLOG_INFO("[FUObjectArray::get] {} objects", result->get_object_count());

        for (auto i = 0; i < result->get_object_count(); ++i) {
            auto item = result->get_object(i);
            if (item == nullptr) {
                continue;
            }
            
            auto obj = *(void**)item;

            if (obj == nullptr || IsBadReadPtr(obj, sizeof(void*))) {
                continue;
            }

            FName* fname = (FName*)((uintptr_t)obj + 0x18);
            try {
                const auto name = fname->to_string();

                SPDLOG_INFO("{} {}", i, utility::narrow(name));
            } catch(...) {
                SPDLOG_ERROR("Failed to get name {}", i);
            }
        }

        return result;
    }();

    return result;
}
}