#include <vector>
#include <format>

#include <utility/String.hpp>
#include <utility/Module.hpp>

#include <sdkgenny/sdk.hpp>
#include <sdkgenny/class.hpp>
#include <sdkgenny/static_function.hpp>
#include <sdkgenny/pointer.hpp>
#include <sdkgenny/parameter.hpp>
#include <sdkgenny/variable.hpp>

#include <sdk/UObjectArray.hpp>
#include <sdk/UClass.hpp>
#include <sdk/UFunction.hpp>
#include <sdk/UEnum.hpp>
#include <sdk/FProperty.hpp>
#include <sdk/FBoolProperty.hpp>
#include <sdk/FObjectProperty.hpp>
#include <sdk/FStructProperty.hpp>

#include "Framework.hpp"

#include "SDKDumper.hpp"

namespace detail {
std::vector<sdk::UStruct*> get_all_structs() {
    std::vector<sdk::UStruct*> classes{};

    const auto objects = sdk::FUObjectArray::get();

    if (objects == nullptr) {
        return classes;
    }

    const auto struct_c = sdk::UStruct::static_class();
    const auto function_c = sdk::UFunction::static_class();
    const auto uenum_c = sdk::UEnum::static_class();

    for (auto i = 0; i < objects->get_object_count(); ++i) {
        const auto entry = objects->get_object(i);

        if (entry == nullptr) {
            continue;
        }

        const auto object = entry->object;

        if (object == nullptr) {
            continue;
        }

        const auto object_class = object->get_class();

        if (object_class == nullptr) {
            continue;
        }

        if (object_class->is_a(struct_c) && !object_class->is_a(function_c) && !object_class->is_a(uenum_c)) {
            classes.push_back(reinterpret_cast<sdk::UStruct*>(object));
        }
    }

    return classes;
}
}

void SDKDumper::dump() {
    auto dumper = std::make_unique<SDKDumper>();
    dumper->dump_internal();
}

void SDKDumper::dump_internal() {
    initialize_sdk();
    populate_sdk();
    write_sdk();
}

void SDKDumper::initialize_sdk() {
    m_sdk = std::make_unique<sdkgenny::Sdk>();
    m_sdk->include("cstdint");
    m_sdk->include("string_view");
    m_sdk->include("string");

    auto g = m_sdk->global_ns();

    g->type("int8_t")->size(1);
    g->type("int16_t")->size(2);
    g->type("int32_t")->size(4);
    g->type("int64_t")->size(8);
    g->type("uint8_t")->size(1);
    g->type("uint16_t")->size(2);
    g->type("uint32_t")->size(4);
    g->type("uint64_t")->size(8);
    g->type("float")->size(4);
    g->type("double")->size(8);
    g->type("bool")->size(1);
    g->type("char")->size(1);
    g->type("int")->size(4);
    g->type("void")->size(0);

    initialize_boilerplate_classes();
}

void SDKDumper::initialize_boilerplate_classes() {
    initialize_tarray();
    initialize_uobject();
    initialize_ustruct();
    initialize_uobject_array();
    initialize_fname();
}

void SDKDumper::initialize_tarray() {
    ExtraSdkClass tarray{};
    tarray.name = "TArray";
    tarray.header = 
R"(#pragma once

#include <cstdint>

template<typename T>
class TArray {
public:
    T* data;
    int32_t count;
    int32_t capacity;
};)";

    m_extra_classes.push_back(tarray);
}

void SDKDumper::initialize_uobject() {
    const auto uobject = sdk::UObject::static_class();
    const auto ufunction = sdk::UFunction::static_class();
    const auto uclass = sdk::UClass::static_class();

    if (uobject == nullptr || ufunction == nullptr || uclass == nullptr) {
        return;
    }

    auto s = get_or_generate_struct(uobject);

    if (s == nullptr) {
        return;
    }

    auto g = m_sdk->global_ns();

    // ::ProcessEvent
    auto process_event = s->virtual_function("ProcessEvent");
    process_event->vtable_index(sdk::UObjectBase::get_process_event_index());
    process_event->returns(g->type("void"));
    process_event->param("func")->type(get_or_generate_struct(ufunction)->ptr());
    process_event->param("params")->type(g->type("void")->ptr());

    // Fields
    auto class_private = s->variable("ClassPrivate")
                          ->type(get_or_generate_struct(uclass)->ptr())
                          ->offset(sdk::UObjectBase::get_class_private_offset());

    auto outer_private = s->variable("OuterPrivate")
                            ->type(s->ptr())
                            ->offset(sdk::UObjectBase::get_outer_private_offset());

    auto object_flags = s->variable("ObjectFlags")
                           ->type(g->type("uint32_t"))
                           ->offset(sdk::UObjectBase::get_object_flags_offset());

    auto internal_index = s->variable("InternalIndex")
                             ->type(g->type("uint32_t"))
                             ->offset(sdk::UObjectBase::get_internal_index_offset());
    
    auto name_var = s->variable("Name")
                    ->type(g->struct_("FName"))
                    ->offset(sdk::UObjectBase::get_fname_offset());

    // Functions
    // ::get_full_name
    auto get_full_name = s->function("get_full_name");
    get_full_name->returns(g->type("std::wstring"));
    get_full_name->procedure(
R"(if (ClassPrivate == nullptr) { return L"null"; }
auto obj_name = Name.ToString();
for (auto outer = OuterPrivate; outer != nullptr && outer != this; outer = outer->OuterPrivate) {
    obj_name = outer->Name.ToString() + L'.' + obj_name;
}
return ClassPrivate->Name.ToString() + L' ' + obj_name;)"
    );
}

void SDKDumper::initialize_ustruct() {

}

void SDKDumper::initialize_uobject_array() {
    const auto objs = sdk::FUObjectArray::get();

    if (objs == nullptr) {
        return;
    }

    const auto module_within = utility::get_module_within((uintptr_t)objs);

    if (!module_within.has_value()) {
        return;
    }

    const auto module_path_str = utility::get_module_pathw(*module_within);

    if (!module_path_str.has_value()) {
        return;
    }

    std::filesystem::path module_path{module_path_str.value()};

    // Now strip the module name from the path
    const auto module_name = module_path.filename();
    const auto offset = (uintptr_t)objs - (uintptr_t)module_within.value();

    const auto uobject = sdk::UObject::static_class();

    if (uobject == nullptr) {
        return;
    }

    auto g = m_sdk->global_ns();

    auto uobject_array = g->class_("FUObjectArray");
    auto uobject_item = g->struct_("FUObjectItem");
    uobject_item->variable("object")->type(get_or_generate_struct(uobject)->ptr())->offset(0);
    uobject_item->variable("flags")->type(g->type("int32_t"))->offset(sizeof(void*) * 1);
    uobject_item->variable("cluster_index")->type(g->type("int32_t"))->offset((sizeof(void*) * 1) + (sizeof(int32_t) * 1));
    uobject_item->variable("serial_number")->type(g->type("int32_t"))->offset((sizeof(void*) * 1) + (sizeof(int32_t) * 2));


    // ::get
    auto getter = uobject_array->static_function("get");

    getter->returns(uobject_array->ptr());
    getter->procedure(std::format("static const auto module = GetModuleHandleA(\"{}\");", module_name.string()) + "\n" +
                      std::format("static const auto offset = 0x{:x};", offset) + "\n" +
                      std::format("return (FUObjectArray*)((uintptr_t)module + offset);")
    );

    // ::is_chunked
    auto is_chunked = uobject_array->static_function("is_chunked");
    is_chunked->returns(g->type("bool"));
    is_chunked->procedure(std::format("return {};",
        objs->is_chunked() ? "true" : "false"
    ));

    // ::is_inlined
    auto is_inlined = uobject_array->static_function("is_inlined");
    is_inlined->returns(g->type("bool"));
    is_inlined->procedure(std::format("return {};",
        objs->is_inlined() ? "true" : "false"
    ));

    // ::get_objects_offset
    auto get_objects_offset = uobject_array->static_function("get_objects_offset");
    get_objects_offset->returns(g->type("size_t"));
    get_objects_offset->procedure(std::format("return 0x{:x};", objs->get_objects_offset()));

    // ::get_item_distance
    auto get_item_distance = uobject_array->static_function("get_item_distance");
    get_item_distance->returns(g->type("size_t"));
    get_item_distance->procedure(std::format("return 0x{:x};", objs->get_item_distance()));

    // ::get_object_count
    auto get_object_count = uobject_array->function("get_object_count");
    get_object_count->returns(g->type("int32_t"));

    if (objs->is_inlined()) {
        const auto offs = sdk::FUObjectArray::get_objects_offset() + (sdk::FUObjectArray::MAX_INLINED_CHUNKS * sizeof(void*));
        get_object_count->procedure(std::format("return *(int32_t*)((uintptr_t)this + 0x{:x});", offs));
    } else if (objs->is_chunked()) {
        const auto offs = sdk::FUObjectArray::get_objects_offset() + sizeof(void*) + sizeof(void*) + 0x4;
        get_object_count->procedure(std::format("return *(int32_t*)((uintptr_t)this + 0x{:x});", offs));
    } else {
        get_object_count->procedure(std::format("return *(int32_t*)((uintptr_t)this + 0x{:x});", sdk::FUObjectArray::get_objects_offset() + 0x8));
    }

    // ::get_object
    auto get_object = uobject_array->function("get_object");
    get_object->returns(uobject_item->ptr());
    get_object->param("index")->type(g->type("int32_t"));
    
    if (objs->is_inlined()) {
        get_object->procedure(
            std::format("const auto chunk_index = index / {};",
                sdk::FUObjectArray::OBJECTS_PER_CHUNK_INLINED
            ) + "\n" +
            std::format("const auto chunk_offset = index % {};",
                sdk::FUObjectArray::OBJECTS_PER_CHUNK_INLINED
            ) + "\n" +
            std::format("const auto chunk = *(void**)((uintptr_t)get_objects_ptr() + (chunk_index * sizeof(void*)));") + "\n" +
            std::format("if (chunk == nullptr) {{ return nullptr; }}") + "\n" +
            std::format("return (FUObjectItem*)((uintptr_t)chunk + (chunk_offset * {}));",
                objs->get_item_distance()
            )
        );
    } else if (objs->is_chunked()) {
        get_object->procedure(
            std::format("const auto chunk_index = index / {};",
                sdk::FUObjectArray::OBJECTS_PER_CHUNK
            ) + "\n" +
            std::format("const auto chunk_offset = index % {};",
                sdk::FUObjectArray::OBJECTS_PER_CHUNK
            ) + "\n" +
            std::format("const auto chunk = *(void**)((uintptr_t)get_objects_ptr() + (chunk_index * sizeof(void*)));") + "\n" +
            std::format("if (chunk == nullptr) {{ return nullptr; }}") + "\n" +
            std::format("return (FUObjectItem*)((uintptr_t)chunk + (chunk_offset * {}));",
                objs->get_item_distance()
            )
        );
    } else {
        get_object->procedure(
            std::format("return (FUObjectItem*)((uintptr_t)get_objects_ptr() + (index * {}));",
                objs->get_item_distance()
            )
        );
    }

    // ::get_objects_ptr
    auto get_objects_ptr = uobject_array->function("get_objects_ptr");
    get_objects_ptr->returns(g->type("void")->ptr());

    if (objs->is_inlined()) {
        get_objects_ptr->procedure(
            std::format("return (void*)((uintptr_t)this + {});",
                sdk::FUObjectArray::get_objects_offset()
            )
        );
    } else {
        get_objects_ptr->procedure(
            std::format("return *(void**)((uintptr_t)this + {});",
                sdk::FUObjectArray::get_objects_offset()
            )
        );
    }

    // ::find_uobject
    auto uobject_sdkgenny = get_or_generate_struct(uobject);
    auto find_uobject = uobject_array->function("find_uobject");
    find_uobject->returns(uobject_sdkgenny->ptr());
    find_uobject->param("name")->type(g->type("std::wstring_view"));

    find_uobject->procedure(
R"(for (int32_t i = 0; i < get_object_count(); ++i) {
    const auto item = get_object(i); // FUObjectItem*
    if (item == nullptr) { continue; }
    const auto object = item->object; // UObject*
    if (object == nullptr) { continue; }
    if (object->get_full_name() == name) { return object; }
}
return nullptr;)"
    );

    m_sdk->include("Windows.h");
}

void SDKDumper::initialize_fname() {
    auto g = m_sdk->global_ns();
    auto fname = g->struct_("FName");

    fname->size(sizeof(sdk::FName));
    fname->variable("Index")->type(g->type("int32_t"))->offset(offsetof(sdk::FName, a1));
    fname->variable("Number")->type(g->type("int32_t"))->offset(offsetof(sdk::FName, a2));

    // ::to_string
    auto to_string = fname->function("ToString")->returns(g->type("std::wstring"));

    const auto to_string_addr = sdk::FName::get_to_string();

    if (!to_string_addr.has_value()) {
        to_string->procedure("return L\"\";");
    } else {
        const auto module_within = utility::get_module_within((uintptr_t)to_string_addr.value());

        if (!module_within) {
            to_string->procedure("return L\"\";");
            return;
        }

        const auto module_path_str = utility::get_module_pathw(*module_within);

        if (!module_path_str.has_value()) {
            to_string->procedure("return L\"\";");
            return;
        }

        std::filesystem::path module_path{module_path_str.value()};
        const auto module_name = module_path.filename();
        const auto offset = (uintptr_t)to_string_addr.value() - (uintptr_t)module_within.value();

        to_string->procedure(std::format("using ToStringFn = TArray<wchar_t>* (*)(const FName*, TArray<wchar_t>*);") + "\n" +
                             std::format("static const auto module = GetModuleHandleA(\"{}\");", module_name.string()) + "\n" +
                             std::format("static const auto offset = 0x{:x};", offset) + "\n" +
                             std::format("static const auto fn = (ToStringFn)((uintptr_t)module + offset);") + "\n" +
                             // We are using a static array because GMalloc
                             // is not implemented in this SDK dump yet
                             // The function SHOULD handle it correctly on subsequent calls
                             // and delete/resize the array as needed (hopefully)
                             std::format("static TArray<wchar_t> arr{{}};") + "\n" +
                             std::format("fn(this, &arr);") + "\n" +
                             std::format("return std::wstring(arr.data, arr.count);")
        );
    }
}

void SDKDumper::populate_sdk() {
    const auto structs = detail::get_all_structs();

    for (const auto ustruct : structs) {
        get_or_generate_struct(ustruct);
    }
}

void SDKDumper::write_sdk() {
    const auto persistent_dir = Framework::get_persistent_dir("sdkdump");
    std::filesystem::create_directories(persistent_dir);
    
    for (const auto& extra_class : m_extra_classes) {
        const auto header_path = persistent_dir / (extra_class.name + ".hpp");
        std::ofstream header{header_path};

        header << extra_class.header;

        if (extra_class.source.has_value()) {
            const auto source_path = persistent_dir / (extra_class.name + ".cpp");
            std::ofstream source{source_path};

            source << extra_class.source.value();
        }
    }

    m_sdk->generate(persistent_dir);
}

sdkgenny::Struct* SDKDumper::get_or_generate_struct(sdk::UStruct* ustruct) {
    auto g = m_sdk->global_ns();
    auto ns = get_or_generate_namespace_chain(ustruct);

    if (ns == nullptr) {
        return nullptr;
    }

    const auto struct_name = utility::narrow(ustruct->get_fname().to_string());

    if (auto existing = ns->find<sdkgenny::Struct>(struct_name); existing != nullptr) {
        return existing;
    }

    auto s = ns->struct_(utility::narrow(ustruct->get_fname().to_string()));

    generate_inheritance(s, ustruct);
    generate_properties(s, ustruct);
    generate_functions(s, ustruct);

    static const auto uscriptstruct_c = sdk::UScriptStruct::static_class();

    if (auto c = ustruct->get_class(); c != nullptr && c->is_a(uscriptstruct_c)) {
        auto uscriptstruct = (sdk::UScriptStruct*)ustruct;

        if (uscriptstruct->get_struct_size() > 0) {
            s->size(uscriptstruct->get_struct_size());
        }
    }

    if (ustruct->get_properties_size() > s->size() && ustruct->get_properties_size() > 0) {
        const auto ps = ustruct->get_properties_size();
        const auto ma = ustruct->get_min_alignment();

        if (ma > 1) {
            s->size(((ps + ma - 1) / ma) * ma);
        } else {
            s->size(ps);
        }
    }

    return s;
}

sdkgenny::Namespace* SDKDumper::get_or_generate_namespace_chain(sdk::UStruct* ustruct) {
    auto g = m_sdk->global_ns();

    std::vector<sdk::UObject*> outers{};

    for (auto outer = ustruct->get_outer(); outer != nullptr; outer = outer->get_outer()) {
        outers.push_back(outer);
    }

    sdkgenny::Namespace* current = g;

    static auto ustruct_c = sdk::UStruct::static_class();

    for (auto it = outers.rbegin(); it != outers.rend(); ++it) {
        const auto outer = *it;
        const auto name = utility::narrow(outer->get_fname().to_string());

        if (outer->is_a(ustruct_c)) {
            // uh... dont know how to handle this... yet
        } else {
            if (auto existing = current->find<sdkgenny::Namespace>(name); existing != nullptr) {
                current = existing;
            } else {
                current = current->namespace_(name);
            }
        }
    }

    return current;
}

void SDKDumper::generate_inheritance(sdkgenny::Struct* s, sdk::UStruct* ustruct) {
    auto super = ustruct->get_super_struct();

    if (super != nullptr && super != ustruct) {
        s->parent(get_or_generate_struct(super)); // get_or_generate_struct will generate the inheritance chain for us.
    }
}

void SDKDumper::generate_properties(sdkgenny::Struct* s, sdk::UStruct* ustruct) {
    for (auto prop = ustruct->get_child_properties(); prop != nullptr; prop = prop->get_next()) {
        auto c = prop->get_class();

        if (c == nullptr) {
            continue;
        }

        auto c_name = utility::narrow(c->get_name().to_string());

        // Janky way to check if it's a property
        if (!c_name.contains("Property")) {
            continue;
        }

        generate_property(s, (sdk::FProperty*)prop);
    }
}

sdkgenny::Variable* SDKDumper::generate_property(sdkgenny::Struct* s, sdk::FProperty* fprop) {
    auto g = m_sdk->global_ns();
    const auto c = fprop->get_class();
    const auto c_name = utility::narrow(c->get_name().to_string());
    const auto prop_name = utility::narrow(fprop->get_field_name().to_string());

    auto getter = s->function("get_" + prop_name);

    auto make_primitive_getter = [&](const std::string_view type_name) {
        getter->returns(g->type(type_name)->ref());
        getter->procedure(
            std::format("return *({}*)((uintptr_t)this + 0x{:x});", type_name, fprop->get_offset())
        );
    };

    switch (utility::hash(c_name)) {
    case "BoolProperty"_fnv:
    {
        const auto boolprop = (sdk::FBoolProperty*)fprop;
        // Create a getter and setter for the property
        const auto byte_offset = boolprop->get_byte_offset();
        const auto byte_mask = boolprop->get_byte_mask();

        getter->returns(g->type("bool"));
        getter->procedure(
            std::format("return (*(uint8_t*)((uintptr_t)this + 0x{:x} + {})) & {} != 0;", fprop->get_offset(), byte_offset, byte_mask)
        );

        auto setter = s->function("set_" + prop_name);
        setter->returns(g->type("void"));
        setter->param("value")->type(g->type("bool"));
        setter->procedure(
            std::format("const auto cur_value = *(uint8_t*)((uintptr_t)this + 0x{:x} + {});", fprop->get_offset(), byte_offset) + "\n" +
            std::format("*(uint8_t*)((uintptr_t)this + 0x{:x} + {}) = (cur_value & ~{}) | (value ? {} : 0);", fprop->get_offset(), byte_offset, byte_mask, byte_mask)
        );


        break;
    }
    case "FloatProperty"_fnv:
    {
        make_primitive_getter("float");
        break;
    }
    case "DoubleProperty"_fnv:
    {
        make_primitive_getter("double");
        break;
    }
    case "IntProperty"_fnv:
    {
        make_primitive_getter("int32_t");
        break;
    }
    case "ObjectProperty"_fnv:
    {
        const auto objprop = (sdk::FObjectProperty*)fprop;
        const auto objtype = objprop->get_property_class();

        if (objtype != nullptr) {
            auto obj_sdkgenny = get_or_generate_struct(objtype);
            auto obj_ns = get_or_generate_namespace_chain(objtype);

            std::string ns_chain{obj_ns != nullptr ? obj_ns->usable_name() : ""};

            if (obj_ns != nullptr) {
                for (auto ns = obj_ns->owner<sdkgenny::Namespace>(); ns != nullptr; ns = ns->owner<sdkgenny::Namespace>()) {
                    if (ns->usable_name().length() > 0) {
                        ns_chain = ns->usable_name() + "::" + ns_chain;
                    }
                }
            }

            getter->returns(obj_sdkgenny->ptr()->ref());
            getter->procedure(
                std::format("return *({}**)((uintptr_t)this + 0x{:x});", ns_chain + "::" + obj_sdkgenny->usable_name(), fprop->get_offset())
            );
        } else {
            getter->returns(g->type("void")->ptr()->ref());
            getter->procedure(
                std::format("return *(void**)((uintptr_t)this + 0x{:x});", fprop->get_offset())
            );
        }

        break;
    }
    default:
    {
        // Create just a getter for the property that returns a void* directly to the property inside the struct
        getter->returns(g->type("void")->ptr());
        getter->procedure(
            std::format("return (void*)((uintptr_t)this + 0x{:x});", fprop->get_offset())
        );

        break;
    }
    };

    return nullptr;
}

void SDKDumper::generate_functions(sdkgenny::Struct* s, sdk::UStruct* ustruct) {
    static const auto uclass_c = sdk::UClass::static_class();

    auto g = m_sdk->global_ns();

    // obviously first thing to do is generate the static_class!
    auto sc_func = s->static_function("static_class");

    auto uclass_sdkgenny = get_or_generate_struct(uclass_c);

    auto generate_full_name = [&](sdk::UStruct* target) {
        auto class_ns = get_or_generate_namespace_chain(target);
        std::string ns_chain{class_ns != nullptr ? class_ns->usable_name() : ""};

        if (class_ns != nullptr) {
            for (auto ns = class_ns->owner<sdkgenny::Namespace>(); ns != nullptr; ns = ns->owner<sdkgenny::Namespace>()) {
                if (ns->usable_name().length() > 0) {
                    ns_chain = ns->usable_name() + "::" + ns_chain;
                }
            }
        }

        return ns_chain + "::" + uclass_sdkgenny->usable_name();
    };

    const auto uclass_full_name = generate_full_name(uclass_c);

    sc_func->returns(uclass_sdkgenny->ptr());
    sc_func->procedure(
        std::format("static auto result = ({}*)FUObjectArray::get()->find_uobject(L\"{}\");", uclass_full_name, utility::narrow(ustruct->get_full_name())) + "\n" +
        std::format("return result;")
    );
    sc_func->depends_on(g->class_("FUObjectArray"));

    for (auto field = ustruct->get_children(); field != nullptr; field = field->get_next()) {
        if (field->get_class() == nullptr) {
            continue;
        }

        const auto field_class = field->get_class();
        const auto field_class_name = field_class->get_fname().to_string();

        if (field_class_name != L"Function") {
            continue;
        }

        const auto func = (sdk::UFunction*)field;
        auto func_sdkgenny = s->function(utility::narrow(func->get_fname().to_string()));

        func_sdkgenny->procedure("return;"); // empty for now.

        for (auto prop = func->get_child_properties(); prop != nullptr; prop = prop->get_next()) {
            if (prop->get_class() == nullptr) {
                continue;
            }

            const auto param_class = prop->get_class();
            const auto param_class_name = param_class->get_name().to_string();

            if (!param_class_name.contains(L"Property")) {
                continue;
            }

            const auto param = (sdk::FProperty*)prop;
            if (!param->is_param()) {
                continue;
            }

            const auto is_ref = param->is_reference_param();
            const auto is_ret = param->is_return_param();
            const auto is_out = param->is_out_param();

            auto param_sdkgenny = !is_ret ? func_sdkgenny->param(utility::narrow(param->get_field_name().to_string())) : nullptr;

            std::optional<std::string> builtin_type{};
            sdk::UStruct* param_ustruct{nullptr};
            bool is_ptr = false;

            switch (utility::hash(utility::narrow(param_class_name))) {
            case "BoolProperty"_fnv:
                builtin_type = "bool";
                break;
            case "FloatProperty"_fnv:
                builtin_type = "float";
                break;
            case "DoubleProperty"_fnv:
                builtin_type = "double";
                break;
            case "IntProperty"_fnv:
                builtin_type = "int32_t";
                break;
            case "ObjectProperty"_fnv:
                {
                    const auto obj_prop = (sdk::FObjectProperty*)param;
                    param_ustruct = obj_prop->get_property_class();
                    is_ptr = true;
                    break;
                }
            case "StructProperty"_fnv:
                {
                    const auto struct_prop = (sdk::FStructProperty*)param;
                    param_ustruct = struct_prop->get_struct();
                    //is_ptr = true;
                    break;
                }
            default:
                //uiltin_type = utility::narrow(param_class_name); // for testing for now.
                break;
            };

            if (builtin_type) {
                if (is_ret) {
                    func_sdkgenny->returns(g->type(*builtin_type));
                } else if (is_ref || is_out) {
                    param_sdkgenny->type(g->type(*builtin_type)->ref());
                } else {
                    param_sdkgenny->type(g->type(*builtin_type));
                }
            } else if (param_ustruct != nullptr) {
                if (is_ret) {
                    if (is_ptr) {
                        func_sdkgenny->returns(get_or_generate_struct(param_ustruct)->ptr());
                    } else {
                        func_sdkgenny->returns(get_or_generate_struct(param_ustruct));
                    }
                } else if (is_ref || is_out) {
                    if (is_ptr) {
                        param_sdkgenny->type(get_or_generate_struct(param_ustruct)->ptr()->ref());
                    } else {
                        param_sdkgenny->type(get_or_generate_struct(param_ustruct)->ref());
                    }
                } else {
                    if (is_ptr) {
                        param_sdkgenny->type(get_or_generate_struct(param_ustruct)->ptr());
                    } else {
                        param_sdkgenny->type(get_or_generate_struct(param_ustruct));
                    }
                }
            } else {
                if (is_ret) {
                    func_sdkgenny->returns(g->type("void*"));
                } else if (is_ref || is_out) {
                    param_sdkgenny->type(g->type("void*")->ref());
                } else {
                    param_sdkgenny->type(g->type("void*"));
                }
            }
        }
    }
}

