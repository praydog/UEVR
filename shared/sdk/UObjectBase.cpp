#include <unordered_set>

#include <Windows.h>
#include <spdlog/spdlog.h>
#include <utility/Scan.hpp>

#include "UClass.hpp"
#include "UObjectArray.hpp"
#include "UObjectBase.hpp"
#include "UObject.hpp"

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

std::optional<uintptr_t> UObjectBase::get_destructor() {
    static auto result = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("[UObjectBase] Searching for destructor...");

        const auto uobject_class = sdk::UObject::static_class();

        if (uobject_class == nullptr) {
            SPDLOG_ERROR("[UObjectBase] Failed to find UObject class, cannot find destructor");
            return {};
        }

        const auto default_object = uobject_class->get_class_default_object();

        if (default_object == nullptr) {
            SPDLOG_ERROR("[UObjectBase] Failed to find default UObject, cannot find destructor");
            return {};
        }

        const auto uobject_vtable = *(void***)default_object;

        if (uobject_vtable == nullptr || IsBadReadPtr(uobject_vtable, sizeof(void*))) {
            SPDLOG_ERROR("[UObjectBase] Failed to find UObject vtable, cannot find destructor");
            return {};
        }

        const auto uobject_destructor = uobject_vtable[0];

        if (uobject_destructor == nullptr || IsBadReadPtr(uobject_destructor, sizeof(void*))) {
            SPDLOG_ERROR("[UObjectBase] Failed to find UObject destructor, cannot find destructor");
            return {};
        }

        // Now we need to exhaustively decode the destructor, looking for a call to the destructor of parent class
        std::optional<uintptr_t> out{};

        utility::exhaustive_decode((uint8_t*)uobject_destructor, 100, [&](utility::ExhaustionContext& ctx) -> utility::ExhaustionResult {
            if (out) {
                return utility::ExhaustionResult::BREAK;
            }

            if (ctx.instrux.BranchInfo.IsBranch) {
                return utility::ExhaustionResult::CONTINUE;
            }

            const auto disp = utility::resolve_displacement(ctx.addr);

            if (disp && *disp != (uintptr_t)uobject_vtable) {
                out = ctx.branch_start;
                return utility::ExhaustionResult::BREAK;
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        if (out) {
            SPDLOG_INFO("[UObjectBase] Found destructor at 0x{:X}", *out);
        } else {
            SPDLOG_ERROR("[UObjectBase] Failed to find destructor");
        }

        return out;
    }();

    return result;
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