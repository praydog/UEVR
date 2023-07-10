#include <shared_mutex>
#include <unordered_map>

#include <windows.h>
#include <spdlog/spdlog.h>

#include <utility/String.hpp>
#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "UObjectArray.hpp"
#include "UFunction.hpp"
#include "UProperty.hpp"
#include "FProperty.hpp"
#include "FField.hpp"
#include "FProperty.hpp"

#include "UClass.hpp"

namespace sdk {
UClass* UField::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Field");
}

UClass* UStruct::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Struct");
}

UClass* UClass::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.Class");
}

void UField::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UField] Bruteforcing offsets...");

    UStruct::update_offsets();
}

void UStruct::resolve_field_offsets(uint32_t child_search_start) {
    const auto matrix_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Matrix");
    const auto vector_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Vector");
    const auto float_property = sdk::find_uobject(L"Class /Script/CoreUObject.FloatProperty");
    const auto double_property = sdk::find_uobject(L"Class /Script/CoreUObject.DoubleProperty");

    if (matrix_scriptstruct == nullptr || vector_scriptstruct == nullptr) {
        SPDLOG_ERROR("[UStruct] Failed to find Matrix/Vector!");
        return;
    }

    bool found = false;

    for (auto i = child_search_start; i < 0x300; i += sizeof(void*)) try {
        // These can be either FField or UField (UProperty or FProperty)
        // quantum physics... brutal.
        const auto potential_field = *(sdk::FField**)((uintptr_t)vector_scriptstruct + i);

        if (potential_field == nullptr || IsBadReadPtr(potential_field, sizeof(void*)) || ((uintptr_t)potential_field & 1) != 0) {
            continue;
        }

        const auto vtable = *(void**)potential_field;

        if (vtable == nullptr || IsBadReadPtr(vtable, sizeof(void*)) || ((uintptr_t)vtable & 1) != 0) {
            continue;
        }

        const auto vfunc = *(void**)vtable;

        if (vfunc == nullptr || IsBadReadPtr(vfunc, sizeof(void*))) {
            continue;
        }

        // Now we need to walk the memory of the value to see if calling FName::ToString results in "X" or "Y" or "Z"
        // this means we found something that looks like the fields array, as well as resolving the offset to the name index.
        for (auto j = sizeof(void*); j < 0x100; ++j) try {
            const auto& possible_fname_a1 = *(FName*)((uintptr_t)potential_field + j);
            const auto possible_name_a1 = possible_fname_a1.to_string();

            if (possible_name_a1 == L"X" || possible_name_a1 == L"Y" || possible_name_a1 == L"Z") {
                SPDLOG_INFO("[UStruct] Found ChildProperties at offset 0x{:X}", i);
                SPDLOG_INFO("[UStruct] Found FField/UField Name at offset 0x{:X} ({})", j, utility::narrow(possible_name_a1));
                s_child_properties_offset = i;
                s_children_offset = i - sizeof(void*);
                FField::s_name_offset = j;

                bool found_ufield_class_offset = false;

                // For UE4
                if (float_property != nullptr) {
                    const auto class_offset = utility::scan_ptr((uintptr_t)potential_field, 0x50, (uintptr_t)float_property);

                    if (class_offset) {
                        FField::s_class_offset = *class_offset - (uintptr_t)potential_field;
                        found_ufield_class_offset = true;
                        SPDLOG_INFO("[UStruct] Found FField/UField Class at offset 0x{:X}", FField::s_class_offset);
                    }
                }

                // For UE5
                if (!found_ufield_class_offset && double_property != nullptr) {
                    const auto class_offset = utility::scan_ptr((uintptr_t)potential_field, 0x50, (uintptr_t)double_property);

                    if (class_offset) {
                        FField::s_class_offset = *class_offset - (uintptr_t)potential_field;
                        found_ufield_class_offset = true;
                        SPDLOG_INFO("[UStruct] Found FField/UField Class at offset 0x{:X}", FField::s_class_offset);
                    }
                }

                // Need to look for FFieldClass variant
                if (!found_ufield_class_offset) {
                    for (auto k = 0; k < 0x100; k += sizeof(void*)) try {
                        const auto potential_ffc = *(void**)((uintptr_t)potential_field + k);

                        if (potential_ffc == nullptr || IsBadReadPtr(potential_ffc, sizeof(void*)) || ((uintptr_t)potential_ffc & 1) != 0) {
                            continue;
                        }

                        // probe for a valid FName, FFieldClass should have one near the start of the struct.
                        for (auto l = 0; l < 0x100; l += sizeof(void*)) try {
                            const auto& possible_fname_ffc = *(FName*)((uintptr_t)potential_ffc + l);
                            const auto possible_name_ffc = possible_fname_ffc.to_string();

                            if (possible_name_ffc == L"FloatProperty" || possible_name_ffc == L"DoubleProperty") {
                                FField::s_class_offset = k;
                                FFieldClass::s_name_offset = l;
                                found_ufield_class_offset = true;
                                SPDLOG_INFO("[UStruct] Found FFieldClass offset 0x{:X}", FField::s_class_offset);
                                SPDLOG_INFO("[UStruct] Found FFieldClass::Name offset 0x{:X} ({})", FFieldClass::s_name_offset, utility::narrow(possible_name_ffc));
                                break;
                            }
                        } catch (...) {
                            continue;
                        }

                        if (found_ufield_class_offset) {
                            break;
                        }
                    } catch (...) {
                        continue;
                    }
                }

                // Now we need to locate the "Next" field of the UField struct.
                // We cant start at UObjectBase::get_class_size() because it can be an FField or UField.
                for (auto k = 0; k < 0x100; k += sizeof(void*)) try {
                    const auto potential_next_field = *(sdk::FField**)((uintptr_t)potential_field + k);

                    if (potential_next_field == nullptr || IsBadReadPtr(potential_next_field, sizeof(void*)) || ((uintptr_t)potential_next_field & 1) != 0) {
                        continue;
                    }

                    const auto potential_field_vtable = *(void**)potential_next_field;

                    if (potential_field_vtable == nullptr || IsBadReadPtr(potential_field_vtable, sizeof(void*)) || ((uintptr_t)potential_field_vtable & 1) != 0) {
                        continue;
                    }

                    const auto potential_field_vfunc = *(void**)potential_field_vtable;

                    if (potential_field_vfunc == nullptr || IsBadReadPtr(potential_field_vfunc, sizeof(void*))) {
                        continue;
                    }

                    const auto& potential_next_field_fname = potential_next_field->get_field_name();
                    const auto potential_next_field_name = potential_next_field_fname.to_string();

                    if (possible_name_a1 != potential_next_field_name &&
                        (potential_next_field_name == L"X" || potential_next_field_name == L"Y" || potential_next_field_name == L"Z")) 
                    {
                        SPDLOG_INFO("[UStruct] Found FField Next at offset 0x{:X} ({})", k, utility::narrow(potential_next_field_name));
                        FField::s_next_offset = k;

                        try {
                            const auto uvalue = ((UObject*)potential_field);

                            if (uvalue->is_a(sdk::UField::static_class())) {
                                UField::s_next_offset = k;
                                FField::s_uses_ufield_only = true; // for older versions of the engine
                                s_children_offset = s_child_properties_offset; // for older versions of the engine
                                SPDLOG_INFO("[UStruct] UField detected, setting UField::s_next_offset to 0x{:X}", k);
                            } else {
                                s_children_offset = s_child_properties_offset - sizeof(void*); // for newer versions of the engine
                            }
                        } catch(...) {
                            // dont care
                        }

                        const auto next_next = potential_next_field->get_next();

                        if (next_next != nullptr) try {
                            const auto& next_next_fname = next_next->get_field_name();
                            const auto next_next_name = next_next_fname.to_string();

                            SPDLOG_INFO("[UStruct] Next Next Name: {}", utility::narrow(next_next_name));

                            // now determine which ones are xyz and pass to bruteforce func
                            FProperty* x_prop{};
                            FProperty* y_prop{};
                            FProperty* z_prop{};

                            if (possible_name_a1 == L"X") {
                                x_prop = (FProperty*)potential_field;
                            } else if (possible_name_a1 == L"Y") {
                                y_prop = (FProperty*)potential_field;
                            } else if (possible_name_a1 == L"Z") {
                                z_prop = (FProperty*)potential_field;
                            }

                            if (potential_next_field_name == L"X") {
                                x_prop = (FProperty*)potential_next_field;
                            } else if (potential_next_field_name == L"Y") {
                                y_prop = (FProperty*)potential_next_field;
                            } else if (potential_next_field_name == L"Z") {
                                z_prop = (FProperty*)potential_next_field;
                            }

                            if (next_next_name == L"X") {
                                x_prop = (FProperty*)next_next;
                            } else if (next_next_name == L"Y") {
                                y_prop = (FProperty*)next_next;
                            } else if (next_next_name == L"Z") {
                                z_prop = (FProperty*)next_next;
                            }

                            FProperty::bruteforce_fproperty_offset(x_prop, y_prop, z_prop);
                        } catch(...) {
                            SPDLOG_ERROR("[UStruct] Failed to get Next Next Name!");
                        }

                        found = true;
                        break;
                    }
                } catch(...) {
                    continue;
                }
            }

            if (found) {
                break;
            }
        } catch(...) {
            continue;
        }

        if (found) {
            break;
        }
    } catch(...) {
        continue;
    }

    if (!found) {
        SPDLOG_ERROR("[UStruct] Failed to find ChildProperties and Name!");
    }
}

void UStruct::resolve_function_offsets(uint32_t child_search_start) try {
    // This object has a bunch of functions so we can use it to find the function offsets
    // and the UField Next offset for the games that have FField
    const auto gameplay_statics = (sdk::UClass*)sdk::find_uobject(L"Class /Script/Engine.GameplayStatics");

    if (gameplay_statics == nullptr) {
        SPDLOG_ERROR("[UStruct] Failed to find GameplayStatics!");
        return;
    }

    // We need to bruteforce the true Children offset if the game is using FField
    if (!FField::s_uses_ufield_only) {
        for (auto i = child_search_start; i < 0x100; i += sizeof(void*)) try {
            const auto potential_field = *(sdk::UField**)((uintptr_t)gameplay_statics + i);

            if (potential_field == nullptr || IsBadReadPtr(potential_field, sizeof(void*)) || ((uintptr_t)potential_field & 1) != 0) {
                continue;
            }

            const auto potential_field_vtable = *(void**)potential_field;

            if (potential_field_vtable == nullptr || IsBadReadPtr(potential_field_vtable, sizeof(void*)) || ((uintptr_t)potential_field_vtable & 1) != 0) {
                continue;
            }

            const auto potential_field_vtable_first = *(void**)potential_field_vtable;

            if (potential_field_vtable_first == nullptr || IsBadReadPtr(potential_field_vtable_first, sizeof(void*))) {
                continue;
            }

            if (i != UStruct::s_super_struct_offset && potential_field->is_a<UField>()) {
                UStruct::s_children_offset = i;
                SPDLOG_INFO("[UStruct] Found UField Children at offset 0x{:X}", i);
                break;
            }
        } catch(...) {
            continue;
        }

        // Now we need to find the UField::Next offset
        const auto first_ufield = gameplay_statics->get_children();

        if (first_ufield == nullptr || IsBadReadPtr(first_ufield, sizeof(void*)) || ((uintptr_t)first_ufield & 1) != 0) {
            SPDLOG_ERROR("[UStruct] Failed to find first UField!");
            return;
        }

        for (auto i = sdk::UObjectBase::get_class_size(); i < 0x100; i += sizeof(void*)) try {
            const auto potential_next_field = *(sdk::UField**)((uintptr_t)first_ufield + i);

            if (potential_next_field == nullptr || IsBadReadPtr(potential_next_field, sizeof(void*)) || ((uintptr_t)potential_next_field & 1) != 0) {
                continue;
            }

            const auto potential_next_field_vtable = *(void**)potential_next_field;

            if (potential_next_field_vtable == nullptr || IsBadReadPtr(potential_next_field_vtable, sizeof(void*)) || ((uintptr_t)potential_next_field_vtable & 1) != 0) {
                continue;
            }

            const auto potential_next_field_vtable_first = *(void**)potential_next_field_vtable;

            if (potential_next_field_vtable_first == nullptr || IsBadReadPtr(potential_next_field_vtable_first, sizeof(void*))) {
                continue;
            }

            if (potential_next_field->is_a<UField>()) {
                UField::s_next_offset = i;
                // now verify it by doing a linked list walk to make sure that at least a few functions are valid
                uint32_t func_count = 0;
                for (auto next = potential_next_field; next != nullptr; next = next->get_next()) {
                    if (next->is_a<UFunction>()) {
                        func_count++;
                    }
                }
                
                if (func_count < 5) {
                    continue;
                }

                SPDLOG_INFO("[UStruct] Found UField Next at offset 0x{:X}", i);
                break;
            }
        } catch(...) {
            continue;
        }
    }

    std::unordered_map<uint32_t, uint32_t> offsets_with_ptr_within_module{};

    for (auto next = gameplay_statics->get_children(); next != nullptr; next = next->get_next()) try {
        if (!next->is_a<UFunction>()) {
            continue;
        }
        
        for (auto i = UStruct::s_child_properties_offset + sizeof(void*); i < 0x200; i += sizeof(void*)) try {
            const auto possible_native_fn = *(void**)((uintptr_t)next + i);

            if (possible_native_fn == nullptr || IsBadReadPtr(possible_native_fn, sizeof(void*))) {
                continue;
            }

            if (utility::get_module_within(possible_native_fn)) {
                offsets_with_ptr_within_module[i]++;
            }
        } catch(...) {
            continue;
        }
    } catch(...) {
        continue;
    }

    // Check which offset has the most pointers to a module
    uint32_t most_ptrs = 0;
    uint32_t most_ptrs_offset = 0;

    for (const auto& [offset, ptr_count] : offsets_with_ptr_within_module) {
        if (ptr_count > most_ptrs) {
            most_ptrs = ptr_count;
            most_ptrs_offset = offset;
        }
    }

    if (most_ptrs > 0) {
        UFunction::s_native_function_offset = most_ptrs_offset;
        SPDLOG_INFO("[UStruct] Found native function offset at 0x{:X}", most_ptrs_offset);
    } else {
        SPDLOG_ERROR("[UStruct] Failed to find native function offset!");
    }

    UObjectBase::update_process_event_index();
} catch(...) {
    SPDLOG_ERROR("[UStruct::resolve_function_offsets] Failed to resolve function offsets! (unknown exception)");
}

void UStruct::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UStruct] Bruteforcing offsets...");

    const auto object_class = sdk::UObject::static_class();
    const auto field_class = sdk::UField::static_class();
    const auto struct_class = sdk::UStruct::static_class();
    const auto class_class = sdk::UClass::static_class();

    SPDLOG_INFO("[UStruct] UObject: 0x{:x}", (uintptr_t)object_class);
    SPDLOG_INFO("[UStruct] UField: 0x{:x}", (uintptr_t)field_class);
    SPDLOG_INFO("[UStruct] UStruct: 0x{:x}", (uintptr_t)struct_class);
    SPDLOG_INFO("[UStruct] UClass: 0x{:x}", (uintptr_t)class_class);

    bool found_super_struct = false;

    if (class_class != nullptr && struct_class != nullptr && field_class != nullptr) {
        for (auto i = sdk::UObjectBase::get_class_size(); i < 0x100; i += sizeof(void*)) try {
            const auto value = *(sdk::UStruct**)((uintptr_t)class_class + i);

            // SuperStruct heuristic
            if (value == struct_class && 
                *(sdk::UStruct**)((uintptr_t)struct_class + i) == field_class && 
                *(sdk::UStruct**)((uintptr_t)field_class + i) == object_class) 
            {
                s_super_struct_offset = i;
                found_super_struct = true;
                SPDLOG_INFO("[UStruct] Found SuperStruct at offset 0x{:X}", i);
                break;
            }
        } catch(...) {
            continue;
        }
    } else {
        SPDLOG_ERROR("[UStruct] Failed to find classes!");
    }

    const auto child_search_start = found_super_struct ? s_super_struct_offset + sizeof(void*) : sdk::UObjectBase::get_class_size();

    UStruct::resolve_field_offsets(child_search_start);
    UStruct::resolve_function_offsets(child_search_start);

    UField::update_offsets();
}

void UClass::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    SPDLOG_INFO("[UClass] Bruteforcing offsets...");

    const auto object_class = sdk::UObject::static_class();
    const auto field_class = sdk::UField::static_class();
    const auto struct_class = sdk::UStruct::static_class();

    if (object_class == nullptr || field_class == nullptr || struct_class == nullptr) {
        SPDLOG_ERROR("[UClass] Failed to find UObject/UStruct/UField!");
        return;
    }

    for (auto i = UObjectBase::get_class_size(); i < 0x300; i += sizeof(void*)) try {
        const auto value = *(sdk::UObject**)((uintptr_t)object_class + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto struct_value = *(sdk::UStruct**)((uintptr_t)struct_class + i);
        const auto field_value = *(sdk::UField**)((uintptr_t)field_class + i);

        if (struct_value == nullptr || field_value == nullptr) {
            continue;
        }

        if (value == object_class || struct_value == struct_class || field_value == field_class) {
            continue;
        }

        if (value->get_class() == object_class &&
            field_value->get_class() == field_class &&
            struct_value->get_class() == struct_class)
        {
            SPDLOG_INFO("[UClass] Found DefaultObject at offset 0x{:X}", i);
            s_default_object_offset = i;
            break;
        }
    } catch(...) {
        continue;
    }
}

UClass* UScriptStruct::static_class() {
    return (UClass*)sdk::find_uobject(L"Class /Script/CoreUObject.ScriptStruct");
}

void UScriptStruct::update_offsets() {
    if (s_attempted_update_offsets) {
        return;
    }

    s_attempted_update_offsets = true;

    UStruct::update_offsets();

    SPDLOG_INFO("[UScriptStruct] Bruteforcing offsets...");

    const auto matrix_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Matrix");
    const auto vector_scriptstruct = sdk::find_uobject(L"ScriptStruct /Script/CoreUObject.Vector");

    if (matrix_scriptstruct == nullptr || vector_scriptstruct == nullptr) {
        SPDLOG_ERROR("[UScriptStruct] Failed to find Matrix/Vector!");
        return;
    }

    constexpr auto MAT4_SIZE_FLOAT = 4 * 4 * sizeof(float);
    constexpr auto MAT4_SIZE_DOUBLE = 4 * 4 * sizeof(double);
    constexpr auto VECTOR_SIZE_FLOAT = 3 * sizeof(float);
    constexpr auto VECTOR_SIZE_DOUBLE = 3 * sizeof(double);

    for (auto i = UStruct::s_super_struct_offset + sizeof(void*); i < 0x300; i += sizeof(void*)) try {
        const auto value = *(sdk::UScriptStruct::StructOps**)((uintptr_t)matrix_scriptstruct + i);

        if (value == nullptr || IsBadReadPtr(value, sizeof(void*)) || ((uintptr_t)value & 1) != 0) {
            continue;
        }

        const auto potential_vtable = *(void**)value;

        if (potential_vtable == nullptr || IsBadReadPtr(potential_vtable, sizeof(void*)) || ((uintptr_t)potential_vtable & 1) != 0) {
            continue;
        }

        const auto potential_vfunc = *(void**)potential_vtable;

        if (potential_vfunc == nullptr || IsBadReadPtr(potential_vfunc, sizeof(void*))) {
            continue;
        }

        const auto value2 = *(sdk::UScriptStruct::StructOps**)((uintptr_t)vector_scriptstruct + i);

        if (value2 == nullptr || IsBadReadPtr(value2, sizeof(void*)) || ((uintptr_t)value2 & 1) != 0) {
            continue;
        }

        if ((value->size == MAT4_SIZE_FLOAT && value2->size == VECTOR_SIZE_FLOAT) || 
            (value->size == MAT4_SIZE_DOUBLE && value2->size == VECTOR_SIZE_DOUBLE))
        {
            SPDLOG_INFO("[UScriptStruct] Found StructOps at offset 0x{:X}", i);
            s_struct_ops_offset = i;
            break;
        }
    } catch(...) {
        continue;
    }
}

FProperty* UStruct::find_property(std::wstring_view name) const {
    static std::shared_mutex prop_mtx{};
    static std::unordered_map<const sdk::UStruct*, std::unordered_map<std::wstring, sdk::FProperty*>> prop_cache{};

    {
        std::shared_lock _{ prop_mtx };

        if (auto it1 = prop_cache.find(this); it1 != prop_cache.end()) {
            if (auto it2 = it1->second.find(name.data()); it2 != it1->second.end()) {
                return it2->second;
            }
        }
    }

    for (auto super = this; super != nullptr; super = (UClass*)super->get_super_struct()) {
        for (auto child = super->get_child_properties(); child != nullptr; child = child->get_next()) {
            //SPDLOG_INFO("[UStruct] Checking child property {}", utility::narrow(child->get_field_name().to_string()));

            if (child->get_field_name().to_string() == name) {
                std::unique_lock _{ prop_mtx };
                prop_cache[this][name.data()] = (sdk::FProperty*)child;
                return (FProperty*)child;
            }
        }
    }

    return nullptr;
}

UProperty* UStruct::find_uproperty(std::wstring_view name) const {
    static std::shared_mutex prop_mtx{};
    static std::unordered_map<const sdk::UStruct*, std::unordered_map<std::wstring, sdk::UProperty*>> prop_cache{};

    {
        std::shared_lock _{ prop_mtx };

        if (auto it1 = prop_cache.find(this); it1 != prop_cache.end()) {
            if (auto it2 = it1->second.find(name.data()); it2 != it1->second.end()) {
                return it2->second;
            }
        }
    }

    for (auto super = this; super != nullptr; super = (UClass*)super->get_super_struct()) {
        for (auto child = super->get_children(); child != nullptr; child = child->get_next()) {
            //SPDLOG_INFO("[UStruct] Checking child UProperty {}", utility::narrow(child->get_fname().to_string()));

            if (child->get_fname().to_string() == name) {
                std::unique_lock _{ prop_mtx };
                prop_cache[this][name.data()] = (sdk::UProperty*)child;
                return (UProperty*)child;
            }
        }
    }

    return nullptr;
}

UFunction* UStruct::find_function(std::wstring_view name) const {
    static std::shared_mutex func_mtx{};
    static std::unordered_map<const sdk::UStruct*, std::unordered_map<std::wstring, sdk::UFunction*>> func_cache{};

    {
        std::shared_lock _{ func_mtx };

        if (auto it1 = func_cache.find(this); it1 != func_cache.end()) {
            if (auto it2 = it1->second.find(name.data()); it2 != it1->second.end()) {
                return it2->second;
            }
        }
    }

    for (auto super = this; super != nullptr; super = (UClass*)super->get_super_struct()) {
        for (auto child = super->get_children(); child != nullptr; child = child->get_next()) {
            //SPDLOG_INFO("[UStruct] Checking child UProperty {}", utility::narrow(child->get_fname().to_string()));

            if (child->get_fname().to_string() == name) {
                std::unique_lock _{ func_mtx };
                func_cache[this][name.data()] = (sdk::UFunction*)child;
                return (UFunction*)child;
            }
        }
    }

    return nullptr;
}
}