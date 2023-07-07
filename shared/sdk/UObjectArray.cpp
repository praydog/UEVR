#include <unordered_map>
#include <shared_mutex>

#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include "EngineModule.hpp"

#include "UObject.hpp"
#include "UClass.hpp"
#include "FName.hpp"
#include "UObjectArray.hpp"

namespace sdk {
UObjectBase* find_uobject(const std::wstring& full_name, bool cached) {
    static std::unordered_map<std::wstring, UObjectBase*> cache{};
    static std::shared_mutex cache_mutex{};

    if (cached) {
        std::shared_lock _{cache_mutex};

        if (auto it = cache.find(full_name); it != cache.end()) {
            return it->second;
        }
    }

    const auto objs = FUObjectArray::get();

    if (objs == nullptr) {
        return nullptr;
    }

    for (auto i = 0; i < objs->get_object_count(); ++i) {
        const auto item = objs->get_object(i);
        if (item == nullptr) {
            continue;
        }

        const auto object = item->object;

        if (object == nullptr) {
            continue;
        }

        if (object->get_full_name() == full_name) {
            std::unique_lock _{cache_mutex};
            cache[full_name] = object;

            return object;
        }
    }

    return nullptr;
}

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

            if (*displacement & 1 != 0) {
                SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to odd alignment", *displacement);
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

            // Verify that the first object in the list is valid
            if (potential_obj_obj_objects != nullptr && !IsBadReadPtr(potential_obj_obj_objects, sizeof(void*))) {
                const auto first_obj = *(void**)potential_obj_obj_objects;

                if (first_obj == nullptr || IsBadReadPtr(first_obj, sizeof(void*))) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid pointer @ first object", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }

                const auto first_vtable = *(void**)first_obj;

                if (first_vtable == nullptr || IsBadReadPtr(first_vtable, sizeof(void*))) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid pointer @ first vtable", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }

                const auto first_vfunc = *(void**)first_vtable;

                if (first_vfunc == nullptr || IsBadReadPtr(first_vfunc, sizeof(void*))) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid pointer @ first vfunc", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }
            }

            // Do an initial check to see if we're operating on <= 4.10,
            // which means the array is inlined and the size and chunk amount is stored at 0x1010 (512 * 8 + OBJECTS_OFFSET)
            auto check_inlined_array = [&]() -> bool {
                constexpr auto inlined_count_offs = OBJECTS_OFFSET + (MAX_INLINED_CHUNKS * sizeof(void*));

                if (IsBadReadPtr((void*)*displacement, inlined_count_offs + 8)) {
                    return false;
                }

                const auto count = *(int32_t*)(*displacement + inlined_count_offs);
                const auto chunk_count = *(int32_t*)(*displacement + inlined_count_offs + 4);

                if (count > 0 && chunk_count > 0 && chunk_count < MAX_INLINED_CHUNKS) {
                    SPDLOG_INFO("Checking potential GUObjectArray at 0x{:x} with inlined count of {} and chunk count of {}", *displacement, count, chunk_count);

                    // Now that this has passed, we need to verify that the pointers in the inlined array match up with these counts
                    for (int32_t i = 0; i < chunk_count; ++i) {
                        const auto potential_chunk = *(void**)(*displacement + OBJECTS_OFFSET + (i * sizeof(void*)));

                        if (potential_chunk == nullptr || IsBadReadPtr(potential_chunk, sizeof(void*))) {
                            return false;
                        }

                        const auto potential_obj = *(void**)potential_chunk;

                        if (potential_obj == nullptr || IsBadReadPtr(potential_obj, sizeof(void*))) {
                            return false;
                        }
                    }

                    // Now check the ones ahead of the chunk count and make sure they are nullptr
                    for (int32_t i = chunk_count; i < MAX_INLINED_CHUNKS; ++i) {
                        const auto potential_chunk = *(void**)(*displacement + OBJECTS_OFFSET + (i * sizeof(void*)));

                        if (potential_chunk != nullptr) {
                            return false;
                        }
                    }

                    return true;
                }

                return false;
            };

            s_is_inlined_array = check_inlined_array();

            if (s_is_inlined_array) {
                SPDLOG_INFO("GUObjectArray appears to be inlined (<= 4.10)");
            }

            if (!s_is_inlined_array) {
                // Seen one of these be -1 normally, so we can't just rule out < 0, it has to be < -1
                if (potential_obj_first_gc_index < -1 || potential_obj_last_non_gc_index < -1 || potential_obj_max_objects_not_consid_by_gc < -1) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to negative values", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }

                // Seems like this can fail on some, we'll just skip it and come up with a better heuristic
                /*if (!IsBadReadPtr(*(void**)&potential_obj_first_gc_index, sizeof(void*)) && (*(int64_t*)&potential_obj_first_gc_index & 1) == 0) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to valid pointer", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }

                if (!IsBadReadPtr(*(void**)&potential_obj_max_objects_not_consid_by_gc, sizeof(void*)) && (*(int64_t*)&potential_obj_max_objects_not_consid_by_gc & 1) == 0) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to valid pointer", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }*/

                if (*(uint8_t*)&potential_obj_is_open_for_disregard_for_gc > 1) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid boolean", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }

                if (potential_obj_obj_objects == nullptr || IsBadReadPtr(potential_obj_obj_objects, sizeof(void*))) {
                    SPDLOG_INFO("Skipping potential GUObjectArray at 0x{:x} due to invalid pointer", *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }
            }
            
            // At this point we've found it, check if it's a chunked array or not, and set a static variable
            if (!s_is_inlined_array && potential_max_chunks > 0 && potential_num_chunks > 0 && potential_max_chunks < 1000 && potential_num_chunks <= potential_max_chunks) {
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
            } else if (!s_is_inlined_array) {
                SPDLOG_INFO("GUObjectArray appears to not be chunked (max_chunks: {}, num_chunks: {})", potential_max_chunks, potential_num_chunks);
            }

            // Now comes the real test, we need to check the entire size of the array and make sure that the pointers are valid
            // Locate the distance between FUObjectItems
            const auto potential_result = (FUObjectArray*)*displacement;
            const auto first_item = potential_result->get_object(0);
            bool distance_determined = false;

            for (auto i = sizeof(void*); i < 0x50; i+= sizeof(void*)) try {
                const auto potential_next_item = (void*)((uintptr_t)first_item + i);
                const auto potential_obj = *(void**)potential_next_item;

                // Make sure the "object" is valid
                if (potential_obj == nullptr || IsBadReadPtr(potential_obj, sizeof(void*)) || ((uintptr_t)potential_obj & 1) != 0) {
                    continue;
                }

                // Now make sure the vtable is valid too
                if (*(void**)potential_obj == nullptr || IsBadReadPtr(*(void**)potential_obj, sizeof(void*)) || ((uintptr_t)*(void**)potential_obj & 1) != 0) {
                    continue;
                }

                // Make sure the first virtual function exists
                if (**(void***)potential_obj == nullptr || IsBadReadPtr(**(void***)potential_obj, sizeof(void*))) {
                    continue;
                }

                SPDLOG_INFO("[FUObjectArray::get] FUObjectItem distance: 0x{:x}", i);
                s_item_distance = i;
                distance_determined = true;
                break;
            } catch(...) {
                continue; // we can try again.
            }

            if (!distance_determined) {
                SPDLOG_ERROR("[FUObjectArray::get] Failed to determine FUObjectItem distance for {:x}, skipping", *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            // Now verify
            for (auto i = 0; i < potential_result->get_object_count(); ++i) try {
                const auto potential_obj = potential_result->get_object(i);

                if (potential_obj == nullptr || IsBadReadPtr(potential_obj, sizeof(void*))) {
                    SPDLOG_ERROR("[FUObjectArray::get] Failed to read potential object at index {} for {:x}, skipping", i, *displacement);
                    return utility::ExhaustionResult::CONTINUE;
                }
            } catch(...) {
                SPDLOG_ERROR("[FUObjectArray::get] Failed to read potential object at index {} for {:x}, skipping (exception)", i, *displacement);
                return utility::ExhaustionResult::CONTINUE;
            }

            SPDLOG_INFO("[FUObjectArray::get] Found GUObjectArray at 0x{:x}", *displacement);
            result = potential_result;
            return utility::ExhaustionResult::BREAK;
        });

        if (result == nullptr) {
            SPDLOG_ERROR("[FUObjectArray::get] Failed to find GUObjectArray");
            return nullptr;
        }

        // do an initial first pass as a test
        SPDLOG_INFO("[FUObjectArray::get] {} objects", result->get_object_count());

        try {
            if (auto item = result->get_object(0); item != nullptr) {
                if (item->object != nullptr) {
                    item->object->update_offsets();
                }
            }
        } catch(...) {
            SPDLOG_ERROR("[FUObjectArray::get] Failed to update offsets");
        }

        return result;
    }();

    // after init func so we dont deadlock
    static bool once = true;

    if (once) {
        once = false;

        if (result == nullptr) {
            return nullptr;
        }

        // Attempt to find the SuperStruct offset
        sdk::UStruct::update_offsets();

        for (auto i = 0; i < result->get_object_count(); ++i) {
            auto item = result->get_object(i);
            if (item == nullptr) {
                continue;
            }
            
            auto obj = *(sdk::UObjectBase**)item;

            if (obj == nullptr || IsBadReadPtr(obj, sizeof(void*))) {
                continue;
            }

            try {
                const auto name = obj->get_full_name();

                SPDLOG_INFO("{} {}", i, utility::narrow(name));
            } catch(...) {
                SPDLOG_ERROR("Failed to get name {}", i);
            }
        }
    };

    return result;
}
}