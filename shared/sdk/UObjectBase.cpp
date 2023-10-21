#include <unordered_set>

#include <Windows.h>
#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "UClass.hpp"
#include "UObjectArray.hpp"
#include "UObjectBase.hpp"
#include "UObject.hpp"
#include "EngineModule.hpp"

namespace sdk {
void UObjectBase::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UObjectBase] Bruteforcing offsets...");

    // Look for the first valid pointer in the object array.
    // The first valid pointer is ClassPrivate.
    for (auto i = sizeof(void*); i < 0x50; i += sizeof(void*)) {
        const auto value = *(void**)((uintptr_t)this + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto possible_vtable = *(void**)value;

        if (possible_vtable == nullptr || IsBadReadPtr(possible_vtable, sizeof(void*)) || ((uintptr_t)possible_vtable & 1) != 0) {
            continue;
        }

        const auto possible_vfunc = *(void**)possible_vtable;

        if (possible_vfunc == nullptr || IsBadReadPtr(possible_vfunc, sizeof(void*))) {
            continue;
        }

        SPDLOG_INFO("[UObjectBase] Found ClassPrivate at offset 0x{:X}", i);
        s_class_private_offset = i;
        s_fname_offset = s_class_private_offset + sizeof(void*);
        s_outer_private_offset = s_fname_offset + sizeof(void*);
        
        break;
    }
}

void UObjectBase::update_process_event_index() try {
    const auto object_class = (sdk::UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Object");

    if (object_class == nullptr) {
        SPDLOG_ERROR("[UObjectBase] Failed to find UObject class, cannot update ProcessEvent index");
        return;
    }

    const auto default_object_class = object_class->get_class_default_object();

    if (default_object_class == nullptr) {
        SPDLOG_ERROR("[UObjectBase] Failed to find default UObject, cannot update ProcessEvent index");
        return;
    }

    const auto vtable = *(void***)default_object_class;

    if (vtable == nullptr || IsBadReadPtr(vtable, sizeof(void*))) {
        SPDLOG_ERROR("[UObjectBase] Failed to find UObject vtable, cannot update ProcessEvent index");
        return;
    }

    // Walk the vtable, looking for the ProcessEvent function
    std::unordered_set<uintptr_t> seen_ips{};

    for (auto i = 0; i < 100; ++i) {
        const auto vfunc = vtable[i];

        if (vfunc == nullptr || IsBadReadPtr(vfunc, sizeof(void*))) {
            break; // reached the end of the vtable
        }

        // Disassemble the func now, looking for displacements
        // treat the displacement as a separate vtable
        // we will walk that other vtable's functions, and look for string references to "Script Call Stack:\n"
        // this is a dead giveaway that we have found the ProcessEvent function
        // there's also another string - but we won't worry about that one for now
        bool found = false;

        const wchar_t target_string[] = L"Script call stack:\n";
        const size_t target_length = sizeof(target_string) - sizeof(wchar_t);  // Do not count the null terminator

        utility::exhaustive_decode((uint8_t*)vfunc, 1000, [&](INSTRUX& ix, uintptr_t ip) -> utility::ExhaustionResult {
            try {
                if (found) {
                    return utility::ExhaustionResult::BREAK;
                }

                if (seen_ips.find(ip) != seen_ips.end()) {
                    return utility::ExhaustionResult::BREAK;
                }

                seen_ips.insert(ip);

                // No need to deal with this one below. We will still decode both branches.
                if (ix.BranchInfo.IsBranch) {
                    return utility::ExhaustionResult::CONTINUE;
                }

                const auto displacement = utility::resolve_displacement(ip);

                if (!displacement) {
                    return utility::ExhaustionResult::CONTINUE;
                }

                if (*displacement == 0x0 || IsBadReadPtr((void*)*displacement, sizeof(void*) * 10)) {
                    return utility::ExhaustionResult::CONTINUE;
                }

                const auto inner_vtable = (void**)*displacement;
                
                for (auto j = 0; j < 10; ++j) {
                    const auto inner_vfunc = inner_vtable[j];

                    if (inner_vfunc == nullptr || IsBadReadPtr(inner_vfunc, sizeof(void*))) {
                        break;
                    }

                    utility::exhaustive_decode((uint8_t*)inner_vfunc, 1000, [&](INSTRUX& ix2, uintptr_t ip2) -> utility::ExhaustionResult {
                        try {
                            if (found) {
                                return utility::ExhaustionResult::BREAK;
                            }

                            if (seen_ips.find(ip2) != seen_ips.end()) {
                                return utility::ExhaustionResult::BREAK;
                            }

                            seen_ips.insert(ip2);

                            const auto displacement2 = utility::resolve_displacement(ip2);
                            
                            if (!displacement2) {
                                return utility::ExhaustionResult::CONTINUE;
                            }

                            const auto potential_string = (wchar_t*)*displacement2;

                            if (IsBadReadPtr(potential_string, target_length)) {
                                return utility::ExhaustionResult::CONTINUE;
                            }

                            if (std::memcmp(potential_string, target_string, target_length) == 0) {
                                UObjectBase::s_process_event_index = i;
                                found = true;

                                SPDLOG_INFO("[UObjectBase] Found ProcessEvent index at {}", UObjectBase::s_process_event_index);
                                return utility::ExhaustionResult::BREAK;
                            }
                        } catch(...) {
                            // dont care
                        }

                        return utility::ExhaustionResult::CONTINUE;
                    });
                }
            } catch(...) {
                // dont care
            }

            return utility::ExhaustionResult::CONTINUE;
        });
    }
} catch(...) {
    SPDLOG_ERROR("[UObjectBase] Failed to update ProcessEvent index");
}

void UObjectBase::update_offsets_post_uobjectarray() {
    if (s_updated_post_uobjectarray) {
        return;
    }

    s_updated_post_uobjectarray = true;

    SPDLOG_INFO("[UObjectBase] Searching for destructor...");

    const auto uobject_class = sdk::UObject::static_class();

    if (uobject_class == nullptr) {
        SPDLOG_ERROR("[UObjectBase] Failed to find UObject class, cannot find destructor");
        return;
    }

    const auto default_object = uobject_class->get_class_default_object();

    if (default_object == nullptr) {
        SPDLOG_ERROR("[UObjectBase] Failed to find default UObject, cannot find destructor");
        return;
    }

    const auto uobject_vtable = *(void***)default_object;

    if (uobject_vtable == nullptr || IsBadReadPtr(uobject_vtable, sizeof(void*))) {
        SPDLOG_ERROR("[UObjectBase] Failed to find UObject vtable, cannot find destructor");
        return;
    }

    const auto uobject_destructor = uobject_vtable[0];

    if (uobject_destructor == nullptr || IsBadReadPtr(uobject_destructor, sizeof(void*))) {
        SPDLOG_ERROR("[UObjectBase] Failed to find UObject destructor, cannot find destructor");
        return;
    }

    // Now we need to exhaustively decode the destructor, looking for a call to the destructor of parent class
    utility::exhaustive_decode((uint8_t*)uobject_destructor, 100, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
        if (s_destructor && s_vtable) {
            return utility::ExhaustionResult::BREAK;
        }

        if (std::string_view{ctx.instrux.Mnemonic}.starts_with("CALL")) {
            std::optional<uintptr_t> call_ip{};

            // Check where the call lands. We do it like this because it could be indirect because of modular builds or compiler optimizations.
            // And exhaustive_decode automatically handles both cases (indirect and direct (far and near)))
            utility::exhaustive_decode((uint8_t*)ctx.addr, 1, [&](utility::ExhaustionContext& ctx2) -> utility::ExhaustionResult {
                if (ctx2.branch_start != ctx.addr) {
                    call_ip = ctx2.addr;
                    return utility::ExhaustionResult::BREAK;
                }

                return utility::ExhaustionResult::CONTINUE;
            });

            if (call_ip) {
                SPDLOG_INFO("[UObjectBase] Examining called function at 0x{:X}", *call_ip);

                // Go through the called function, look for a displacement. That's the vtable.
                utility::exhaustive_decode((uint8_t*)*call_ip, 100, [&](utility::ExhaustionContext& ctx2) -> utility::ExhaustionResult {
                    const auto disp = utility::resolve_displacement(ctx2.addr);

                    if (disp && *disp != (uintptr_t)uobject_vtable) {
                        s_destructor = ctx2.branch_start;
                        s_vtable = *disp;
                        return utility::ExhaustionResult::BREAK;
                    }

                    return utility::ExhaustionResult::CONTINUE;
                });
            }

            return utility::ExhaustionResult::STEP_OVER;
        } else if (!ctx.instrux.BranchInfo.IsBranch) {
            // Otherwise, check the displacements at the current callstack level.
            const auto disp = utility::resolve_displacement(ctx.addr);

            if (disp && *disp != (uintptr_t)uobject_vtable) {
                s_destructor = ctx.branch_start;
                s_vtable = *disp;
                return utility::ExhaustionResult::BREAK;
            }
        }

        return utility::ExhaustionResult::CONTINUE;
    });

    if (s_destructor) {
        SPDLOG_INFO("[UObjectBase] Found destructor at 0x{:X}", *s_destructor);
    } else {
        SPDLOG_ERROR("[UObjectBase] Failed to find destructor");
    }

    if (s_vtable) {
        SPDLOG_INFO("[UObjectBase] Found vtable at 0x{:X}", *s_vtable);
    } else {
        SPDLOG_ERROR("[UObjectBase] Failed to find vtable");
    }

    if (!s_destructor) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject because destructor is unknown");
        return;
    }

    if (!s_vtable) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject because vtable is unknown");
        return;
    }

    const auto core_uobject = sdk::get_ue_module(L"CoreUObject");
    auto vtable_references = utility::scan_displacement_references(core_uobject, *s_vtable);

    if (vtable_references.empty()) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject because vtable has no references 1");
        return;
    }

    // Filter out references that are within the destructor
    vtable_references.erase(std::remove_if(vtable_references.begin(), vtable_references.end(), [&](const auto& ref) {
        return ref >= *s_destructor && ref <= *s_destructor + 0x100;
    }), vtable_references.end());
    
    if (vtable_references.empty()) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject because vtable has no references 2");
        return;
    }

    if (vtable_references.empty()) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject because vtable has no references 3");
        return;
    }

    SPDLOG_INFO("[UObjectBase] Vtable references: {}", vtable_references.size());

    for (auto ref : vtable_references) {
        if (utility::find_mnemonic_in_path(ref + 4, 100, "CALL", false)) {
            SPDLOG_INFO("[UObjectBase] Vtable reference: 0x{:X}", ref);
        }
    }

    std::optional<uintptr_t> correct_ref{};

    for (auto ref : vtable_references) {
        if (!utility::find_mnemonic_in_path(ref + 4, 100, "CALL", false)) {
            continue;
        }

        // Ensure this one doesn't terminate way too early. Usually the constructor if it does.
        uint32_t num_instructions_until_ret = 0;

        utility::exhaustive_decode((uint8_t*)ref + 4, 100, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
            ++num_instructions_until_ret;

            if (std::string_view{ctx.instrux.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (num_instructions_until_ret < 15) {
            continue;
        }

        if (utility::find_string_reference_in_path(ref + 4, "UObject(FVTableHelper& Helper)", false) || 
            utility::find_string_reference_in_path(ref + 4, L"UObject(FVTableHelper& Helper)", false)) 
        {
            continue;
        }

        // Make sure there's a mov [reg+something], 0xFFFFFFFF somewhere really close by in the path
        utility::exhaustive_decode((uint8_t*)ref + 4, 10, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
            if (std::string_view{ctx.instrux.Mnemonic}.starts_with("CALL")) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            if (std::string_view{ctx.instrux.Mnemonic}.starts_with("MOV")) {
                for (auto i = 0; i < ctx.instrux.OperandsCount; ++i) {
                    const auto& op = ctx.instrux.Operands[i];

                    if (op.Type == ND_OP_IMM && op.Info.Immediate.Imm & 0xFFFFFFFF == 0xFFFFFFFF) {
                        correct_ref = ref + 4;
                        return utility::ExhaustionResult::BREAK;
                    }
                }
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (correct_ref) {
            break;
        }
    }

    if (!correct_ref) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject, unable to find correct reference");
        return;
    }

    SPDLOG_INFO("[UObjectBase] Examining reference at 0x{:X}", *correct_ref);

    utility::exhaustive_decode((uint8_t*)*correct_ref, 100, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
        if (s_add_object) {
            return utility::ExhaustionResult::BREAK;
        }

        // Examine each call. Check to see if anywhere in its path it calls EnterCriticalSection. This is the right one.
        if (std::string_view{ctx.instrux.Mnemonic}.starts_with("CALL")) {
            auto fn = utility::resolve_displacement(ctx.addr);

            if (!fn) {
                return utility::ExhaustionResult::STEP_OVER;
            }

            uint8_t* ip = (uint8_t*)ctx.addr;

            if (ctx.instrux.IsRipRelative && ip[0] == 0xFF && ip[1] == 0x15) {
                if (*fn != 0 && fn != ctx.addr && !IsBadReadPtr((void*)*fn, sizeof(void*))) {
                    const auto real_dest = *(uintptr_t*)*fn;

                    if (real_dest != 0 && real_dest != (uintptr_t)ip && !IsBadReadPtr((void*)real_dest, sizeof(void*))) {
                        fn = real_dest;
                    } else {
                        return utility::ExhaustionResult::STEP_OVER;
                    }
                } else {
                    return utility::ExhaustionResult::STEP_OVER;
                }
            }

            if (utility::find_string_reference_in_path(*fn, "ClassPrivate", false) || utility::find_string_reference_in_path(*fn, L"ClassPrivate", false)) {
                SPDLOG_INFO("[UObjectBase] Skipping callsite because it references ClassPrivate");
                return utility::ExhaustionResult::STEP_OVER;
            }

            // This is usually the right one, but based on compiler optimizations
            // it may not be.
            if (auto resolved = utility::find_pointer_in_path(*fn, &EnterCriticalSection, true); resolved.has_value()) {
                const auto module = utility::get_module_within(resolved->addr);

                if (module.has_value()) {
                    SPDLOG_INFO("[UObjectBase] Module: {}", *utility::get_module_path(*module));

                    if (module != sdk::get_ue_module(L"CoreUObject")) {
                        SPDLOG_INFO("[UObjectBase] Skipping callsite because EnterCriticalSection call is not in CoreUObject");
                        return utility::ExhaustionResult::STEP_OVER;
                    }
                }

                s_add_object = fn;
                SPDLOG_INFO("[UObjectBase] Found AddObject via EnterCriticalSection reference: 0x{:x}", *s_add_object);
                return utility::ExhaustionResult::BREAK;
            }

            return utility::ExhaustionResult::STEP_OVER;
        }

        return utility::ExhaustionResult::CONTINUE;
    });

    if (!s_add_object) {
        SPDLOG_ERROR("[UObjectBase] Failed to find AddObject");
        return;
    }

    SPDLOG_INFO("[UObjectBase] AddObject: 0x{:x}", *s_add_object);
}

std::optional<uintptr_t> UObjectBase::get_destructor() {
    update_offsets_post_uobjectarray();

    return s_destructor;
}

std::optional<uintptr_t> UObjectBase::get_vtable() {
    update_offsets_post_uobjectarray();

    return s_vtable;
}

std::optional<uintptr_t> UObjectBase::get_add_object() {
    update_offsets_post_uobjectarray();

    return s_add_object;
}

std::wstring UObjectBase::get_full_name() const {
    auto c = get_class();

    if (c == nullptr) {
        return L"null";
    }

    auto obj_name = get_fname().to_string();

    for (auto outer = this->get_outer(); outer != nullptr && outer != this; outer = outer->get_outer()) {
        obj_name = outer->get_fname().to_string() + L'.' + obj_name;
    }

    return c->get_fname().to_string() + L' ' + obj_name;
}

void UObjectBase::process_event(sdk::UFunction* func, void* params) {
    const auto vtable = *(void***)this;
    const auto vfunc = (ProcessEventFn)vtable[s_process_event_index];
    
    vfunc(this, func, params);
}
}