/*
This file (API.hpp) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

extern "C" {
    #include "API.h"
}

#include <optional>
#include <filesystem>
#include <string>
#include <mutex>
#include <array>
#include <vector>
#include <cassert>
#include <string_view>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace uevr {
class API {
private:
    static inline std::unique_ptr<API> s_instance{};

public:
    // ALWAYS call initialize first in uevr_plugin_initialize
    static auto& initialize(const UEVR_PluginInitializeParam* param) {
        if (param == nullptr) {
            throw std::runtime_error("param is null");
        }

        if (s_instance != nullptr) {
            return s_instance;
        }

        s_instance = std::make_unique<API>(param);
        return s_instance;
    }

    // only call this AFTER calling initialize
    static auto& get() {
        if (s_instance == nullptr) {
            throw std::runtime_error("API not initialized");
        }

        return s_instance;
    }

public:
    API(const UEVR_PluginInitializeParam* param) 
        : m_param{param},
        m_sdk{param->sdk}
    {
    }

    virtual ~API() {

    }

    inline const UEVR_PluginInitializeParam* param() const {
        return m_param;
    }

    inline const UEVR_SDKData* sdk() const {
        return m_sdk;
    }

    std::filesystem::path get_persistent_dir(std::optional<std::wstring> file = std::nullopt) {
        static const auto fn = param()->functions->get_persistent_dir;
        const auto size = fn(nullptr, 0);
        if (size == 0) {
            return std::filesystem::path{};
        }

        std::wstring result(size, L'\0');
        fn(result.data(), size + 1);

        if (file.has_value()) {
            return std::filesystem::path{result} / file.value();
        }

        return result;
    }

    template <typename... Args> void log_error(const char* format, Args... args) { m_param->functions->log_error(format, args...); }
    template <typename... Args> void log_warn(const char* format, Args... args) { m_param->functions->log_warn(format, args...); }
    template <typename... Args> void log_info(const char* format, Args... args) { m_param->functions->log_info(format, args...); }

public:
    // C++ wrapper structs for the C structs
    struct UObject;
    struct UEngine;
    struct UGameEngine;
    struct UWorld;
    struct UStruct;
    struct UClass;
    struct UFunction;
    struct UScriptStruct;
    struct UStructOps;
    struct FField;
    struct FProperty;
    struct FFieldClass;
    struct FUObjectArray;
    struct FName;
    struct FConsoleManager;
    struct IConsoleObject;
    struct IConsoleVariable;
    struct IConsoleCommand;
    struct ConsoleObjectElement;
    struct UObjectHook;
    struct FRHITexture2D;
    struct IPooledRenderTarget;

    template<typename T>
    struct TArray;

    // dynamic_cast variant
    template<typename T>
    static T* dcast(UObject* obj) {
        if (obj == nullptr) {
            return nullptr;
        }

        if (obj->is_a(T::static_class())) {
            return static_cast<T*>(obj);
        }

        return nullptr;
    }

    template<typename T = UObject>
    T* find_uobject(std::wstring_view name) {
        static const auto fn = sdk()->uobject_array->find_uobject;
        return (T*)fn(name.data());
    }

    UEngine* get_engine() {
        static const auto fn = sdk()->functions->get_uengine;
        return (UEngine*)fn();
    }

    UObject* get_player_controller(int32_t index) {
        static const auto fn = sdk()->functions->get_player_controller;
        return (UObject*)fn(index);
    }

    UObject* get_local_pawn(int32_t index) {
        static const auto fn = sdk()->functions->get_local_pawn;
        return (UObject*)fn(index);
    }

    UObject* spawn_object(UClass* klass, UObject* outer) {
        static const auto fn = sdk()->functions->spawn_object;
        return (UObject*)fn(klass->to_handle(), outer->to_handle());
    }

    void execute_command(std::wstring_view command) {
        static const auto fn = sdk()->functions->execute_command;
        fn(command.data());
    }

    void execute_command_ex(UWorld* world, std::wstring_view command, void* output_device) {
        static const auto fn = sdk()->functions->execute_command_ex;
        fn((UEVR_UObjectHandle)world, command.data(), output_device);
    }

    FUObjectArray* get_uobject_array() {
        static const auto fn = sdk()->functions->get_uobject_array;
        return (FUObjectArray*)fn();
    }

    FConsoleManager* get_console_manager() {
        static const auto fn = sdk()->functions->get_console_manager;
        return (FConsoleManager*)fn();
    }

    struct FMalloc {
        static FMalloc* get() {
            static const auto fn = initialize()->get;
            return (FMalloc*)fn();
        }

        inline UEVR_FMallocHandle to_handle() { return (UEVR_FMallocHandle)this; }
        inline UEVR_FMallocHandle to_handle() const { return (UEVR_FMallocHandle)this; }

        using FMallocSizeT = uint32_t; // because of C89

        void* malloc(FMallocSizeT size, uint32_t alignment = 0) {
            static const auto fn = initialize()->malloc;
            return fn(to_handle(), size, alignment);
        }
        
        void* realloc(void* original, FMallocSizeT size, uint32_t alignment = 0) {
            static const auto fn = initialize()->realloc;
            return fn(to_handle(), original, size, alignment);
        }

        void free(void* original) {
            static const auto fn = initialize()->free;
            fn(to_handle(), original);
        }

    private:
        static inline const UEVR_FMallocFunctions* s_functions{nullptr};
        static inline const UEVR_FMallocFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->malloc;
            }

            return s_functions;
        }
    };

    struct FName {
        inline UEVR_FNameHandle to_handle() { return (UEVR_FNameHandle)this; }
        inline UEVR_FNameHandle to_handle() const { return (UEVR_FNameHandle)this; }

        enum class EFindName : uint32_t {
            Find,
            Add
        };

        FName() = default;
        FName(std::wstring_view name, EFindName find_type = EFindName::Add) {
            static const auto fn = initialize()->constructor;
            fn(to_handle(), name.data(), (uint32_t)find_type);
        }

        std::wstring to_string() const {
            static const auto fn = initialize()->to_string;
            const auto size = fn(to_handle(), nullptr, 0);
            if (size == 0) {
                return L"";
            }

            std::wstring result(size, L'\0');
            fn(to_handle(), result.data(), size + 1);
            return result;
        }

        int32_t comparison_index{};
        int32_t number{};

    private:
        static inline const UEVR_FNameFunctions* s_functions{nullptr};
        static inline const UEVR_FNameFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->fname;
            }

            return s_functions;
        }
    };
    
    struct UObject {
        inline UEVR_UObjectHandle to_handle() { return (UEVR_UObjectHandle)this; }
        inline UEVR_UObjectHandle to_handle() const { return (UEVR_UObjectHandle)this; }

        static UClass* static_class() {
            static auto result = API::get()->find_uobject<UClass>(L"Class /Script/CoreUObject.Object");
            return result;
        }

        inline UClass* get_class() const {
            static const auto fn = initialize()->get_class;
            return (UClass*)fn(to_handle());
        }

        inline UObject* get_outer() const {
            static const auto fn = initialize()->get_outer;
            return (UObject*)fn(to_handle());
        }

        inline bool is_a(UClass* cmp) const {
            static const auto fn = initialize()->is_a;
            return fn(to_handle(), cmp->to_handle());
        }

        void process_event(UFunction* function, void* params) {
            static const auto fn = initialize()->process_event;
            fn(to_handle(), function->to_handle(), params);
        }

        void call_function(std::wstring_view name, void* params) {
            static const auto fn = initialize()->call_function;
            fn(to_handle(), name.data(), params);
        }

        // Pointer that points to the address of the data within the object, not the data itself
        template<typename T>
        T* get_property_data(std::wstring_view name) const {
            static const auto fn = initialize()->get_property_data;
            return (T*)fn(to_handle(), name.data());
        }

        // Pointer that points to the address of the data within the object, not the data itself
        void* get_property_data(std::wstring_view name) const {
            return get_property_data<void*>(name);
        }

        // use this if you know for sure that the property exists
        // or you will cause an access violation
        template<typename T>
        T& get_property(std::wstring_view name) const {
            return *get_property_data<T>(name);
        }

        bool get_bool_property(std::wstring_view name) const {
            static const auto fn = initialize()->get_bool_property;
            return fn(to_handle(), name.data());
        }

        void set_bool_property(std::wstring_view name, bool value) {
            static const auto fn = initialize()->set_bool_property;
            fn(to_handle(), name.data(), value);
        }

        FName* get_fname() const {
            static const auto fn = initialize()->get_fname;
            return (FName*)fn(to_handle());
        }

        std::wstring get_full_name() const {
            const auto c = get_class();

            if (c == nullptr) {
                return L"";
            }

            auto obj_name = get_fname()->to_string();

            for (auto outer = this->get_outer(); outer != nullptr && outer != this; outer = outer->get_outer()) {
                obj_name = outer->get_fname()->to_string() + L'.' + obj_name;
            }

            return c->get_fname()->to_string() + L' ' + obj_name;
        }

        // dynamic_cast variant
        template<typename T>
        T* dcast() {
            if (this->is_a(T::static_class())) {
                return static_cast<T*>(this);
            }

            return nullptr;
        }

    private:
        static inline const UEVR_UObjectFunctions* s_functions{nullptr};
        inline static const UEVR_UObjectFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->uobject;
            }

            return s_functions;
        }
    };

    struct UStruct : public UObject {
        inline UEVR_UStructHandle to_handle() { return (UEVR_UStructHandle)this; }
        inline UEVR_UStructHandle to_handle() const { return (UEVR_UStructHandle)this; }

        static UClass* static_class() {
            static auto result = API::get()->find_uobject<UClass>(L"Class /Script/CoreUObject.Struct");
            return result;
        }

        UStruct* get_super_struct() const {
            static const auto fn = initialize()->get_super_struct;
            return (UStruct*)fn(to_handle());
        }

        UStruct* get_super() const {
            return get_super_struct();
        }
        
        UFunction* find_function(std::wstring_view name) const {
            static const auto fn = initialize()->find_function;
            return (UFunction*)fn(to_handle(), name.data());
        }

        // Not an array, it's a linked list. Meant to call ->get_next() until nullptr
        FField* get_child_properties() const {
            static const auto fn = initialize()->get_child_properties;
            return (FField*)fn(to_handle());
        }

    private:
        static inline const UEVR_UStructFunctions* s_functions{nullptr};
        inline static const UEVR_UStructFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->ustruct;
            }

            return s_functions;
        }
    };

    struct UClass : public UStruct {
        inline UEVR_UClassHandle to_handle() { return (UEVR_UClassHandle)this; }
        inline UEVR_UClassHandle to_handle() const { return (UEVR_UClassHandle)this; }

        static UClass* static_class() {
            static auto result = API::get()->find_uobject<UClass>(L"Class /Script/CoreUObject.Class");
            return result;
        }

        UObject* get_class_default_object() const {
            static const auto fn = initialize()->get_class_default_object;
            return (UObject*)fn(to_handle());
        }

        std::vector<UObject*> get_objects_matching(bool allow_default = false) const {
            static auto activate_fn = API::get()->sdk()->uobject_hook->activate;
            static auto fn = API::get()->sdk()->uobject_hook->get_objects_by_class;
            activate_fn();
            std::vector<UObject*> result{};
            const auto size = fn(to_handle(), nullptr, 0, allow_default);
            if (size == 0) {
                return result;
            }

            result.resize(size);

            fn(to_handle(), (UEVR_UObjectHandle*)result.data(), size, allow_default);
            return result;
        }

        UObject* get_first_object_matching(bool allow_default = false) const {
            static auto activate_fn = API::get()->sdk()->uobject_hook->activate;
            static auto fn = API::get()->sdk()->uobject_hook->get_first_object_by_class;
            activate_fn();
            return (UObject*)fn(to_handle(), allow_default);
        }

        template<typename T>
        std::vector<T*> get_objects_matching(bool allow_default = false) const {
            std::vector<UObject*> objects = get_objects_matching(allow_default);

            return *reinterpret_cast<std::vector<T*>*>(&objects);
        }

        template<typename T>
        T* get_first_object_matching(bool allow_default = false) const {
            return (T*)get_first_object_matching(allow_default);
        }

    private:
        static inline const UEVR_UClassFunctions* s_functions{nullptr};
        inline static const UEVR_UClassFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->uclass;
            }

            return s_functions;
        }
    };

    struct UFunction : public UStruct {
        inline UEVR_UFunctionHandle to_handle() { return (UEVR_UFunctionHandle)this; }
        inline UEVR_UFunctionHandle to_handle() const { return (UEVR_UFunctionHandle)this; }

        static UClass* static_class() {
            static auto result = API::get()->find_uobject<UClass>(L"Class /Script/CoreUObject.Function");
            return result;
        }

        void call(UObject* obj, void* params) {
            if (obj == nullptr) {
                return;
            }

            obj->process_event(this, params);
        }

        void* get_native_function() const {
            static const auto fn = initialize()->get_native_function;
            return fn(to_handle());
        }

    private:
        static inline const UEVR_UFunctionFunctions* s_functions{nullptr};
        inline static const UEVR_UFunctionFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->ufunction;
            }

            return s_functions;
        }
    };

    struct UScriptStruct : public UStruct {
        inline UEVR_UScriptStructHandle to_handle() { return (UEVR_UScriptStructHandle)this; }
        inline UEVR_UScriptStructHandle to_handle() const { return (UEVR_UScriptStructHandle)this; }

        static UClass* static_class() {
            static auto result = API::get()->find_uobject<UClass>(L"Class /Script/CoreUObject.ScriptStruct");
            return result;
        }

        struct StructOps {
            virtual ~StructOps() {};

            int32_t size;
            int32_t alignment;
        };

        StructOps* get_struct_ops() const {
            static const auto fn = initialize()->get_struct_ops;
            return (StructOps*)fn(to_handle());
        }

        int32_t get_struct_size() const {
            static const auto fn = initialize()->get_struct_size;
            return fn(to_handle());
        }

    private:
        static inline const UEVR_UScriptStructFunctions* s_functions{nullptr};
        inline static const UEVR_UScriptStructFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->uscriptstruct;
            }

            return s_functions;
        }
    };

    // Wrapper class for UField AND FField
    struct FField {
        inline UEVR_FFieldHandle to_handle() { return (UEVR_FFieldHandle)this; }
        inline UEVR_FFieldHandle to_handle() const { return (UEVR_FFieldHandle)this; }

        inline FField* get_next() const {
            static const auto fn = initialize()->get_next;
            return (FField*)fn(to_handle());
        }
        
        FName* get_fname() const {
            static const auto fn = initialize()->get_fname;
            return (FName*)fn(to_handle());
        }

        FFieldClass* get_class() const {
            static const auto fn = initialize()->get_class;
            return (FFieldClass*)fn(to_handle());
        }

    private:
        static inline const UEVR_FFieldFunctions* s_functions{nullptr};
        static inline const UEVR_FFieldFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->ffield;
            }

            return s_functions;
        }
    };

    // Wrapper class for FProperty AND UProperty
    struct FProperty : public FField {
        inline UEVR_FPropertyHandle to_handle() { return (UEVR_FPropertyHandle)this; }
        inline UEVR_FPropertyHandle to_handle() const { return (UEVR_FPropertyHandle)this; }

        int32_t get_offset() const {
            static const auto fn = initialize()->get_offset;
            return fn(to_handle());
        }

    private:
        static inline const UEVR_FPropertyFunctions* s_functions{nullptr};
        static inline const UEVR_FPropertyFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->fproperty;
            }

            return s_functions;
        }
    };

    struct FFieldClass {
        inline UEVR_FFieldClassHandle to_handle() { return (UEVR_FFieldClassHandle)this; }
        inline UEVR_FFieldClassHandle to_handle() const { return (UEVR_FFieldClassHandle)this; }

        FName* get_fname() const {
            static const auto fn = initialize()->get_fname;
            return (FName*)fn(to_handle());
        }

        std::wstring get_name() const {
            return get_fname()->to_string();
        }

    private:
        static inline const UEVR_FFieldClassFunctions* s_functions{nullptr};
        static inline const UEVR_FFieldClassFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->ffield_class;
            }

            return s_functions;
        }
    };

    struct ConsoleObjectElement {
        wchar_t* key;
        int32_t unk[2];
        IConsoleObject* value;
        int32_t unk2[2];
    };

    struct FConsoleManager {
        inline UEVR_FConsoleManagerHandle to_handle() { return (UEVR_FConsoleManagerHandle)this; }
        inline UEVR_FConsoleManagerHandle to_handle() const { return (UEVR_FConsoleManagerHandle)this; }

        TArray<ConsoleObjectElement>& get_console_objects() {
            static const auto fn = initialize()->get_console_objects;
            return *(TArray<ConsoleObjectElement>*)fn(to_handle());
        }

        IConsoleObject* find_object(std::wstring_view name) {
            static const auto fn = initialize()->find_object;
            return (IConsoleObject*)fn(to_handle(), name.data());
        }

        IConsoleVariable* find_variable(std::wstring_view name) {
            static const auto fn = initialize()->find_variable;
            return (IConsoleVariable*)fn(to_handle(), name.data());
        }

        IConsoleCommand* find_command(std::wstring_view name) {
            static const auto fn = initialize()->find_command;
            return (IConsoleCommand*)fn(to_handle(), name.data());
        }

    private:
        static inline const UEVR_ConsoleFunctions* s_functions{nullptr};
        static inline const UEVR_ConsoleFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->console;
            }

            return s_functions;
        }
    };

    struct IConsoleObject {
        inline UEVR_IConsoleObjectHandle to_handle() { return (UEVR_IConsoleObjectHandle)this; }
        inline UEVR_IConsoleObjectHandle to_handle() const { return (UEVR_IConsoleObjectHandle)this; }

        IConsoleCommand* as_command() {
            static const auto fn = initialize()->as_command;
            return (IConsoleCommand*)fn(to_handle());
        }

    private:
        static inline const UEVR_ConsoleFunctions* s_functions{nullptr};
        static inline const UEVR_ConsoleFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->console;
            }

            return s_functions;
        }
    };

    struct IConsoleVariable : public IConsoleObject {
        inline UEVR_IConsoleVariableHandle to_handle() { return (UEVR_IConsoleVariableHandle)this; }
        inline UEVR_IConsoleVariableHandle to_handle() const { return (UEVR_IConsoleVariableHandle)this; }

        void set(std::wstring_view value) {
            static const auto fn = initialize()->variable_set;
            fn(to_handle(), value.data());
        }

        void set_ex(std::wstring_view value, uint32_t flags = 0x80000000) {
            static const auto fn = initialize()->variable_set_ex;
            fn(to_handle(), value.data(), flags);
        }

        void set(float value) {
            set(std::to_wstring(value));
        }

        void set(int value) {
            set(std::to_wstring(value));
        }

        int get_int() const {
            static const auto fn = initialize()->variable_get_int;
            return fn(to_handle());
        }

        float get_float() const {
            static const auto fn = initialize()->variable_get_float;
            return fn(to_handle());
        }

    private:
        static inline const UEVR_ConsoleFunctions* s_functions{nullptr};
        static inline const UEVR_ConsoleFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->console;
            }

            return s_functions;
        }
    };

    struct IConsoleCommand : public IConsoleObject {
        inline UEVR_IConsoleCommandHandle to_handle() { return (UEVR_IConsoleCommandHandle)this; }
        inline UEVR_IConsoleCommandHandle to_handle() const { return (UEVR_IConsoleCommandHandle)this; }

        void execute(std::wstring_view args) {
            static const auto fn = initialize()->command_execute;
            fn(to_handle(), args.data());
        }
    
    private:
        static inline const UEVR_ConsoleFunctions* s_functions{nullptr};
        static inline const UEVR_ConsoleFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->console;
            }

            return s_functions;
        }
    };

    // TODO
    struct UEngine : public UObject {
        static UEngine* get() {
            return API::get()->get_engine();
        }
    };

    struct UGameEngine : public UEngine {

    };

    struct UWorld : public UObject {

    };

    struct FUObjectArray {
        inline UEVR_UObjectArrayHandle to_handle() { return (UEVR_UObjectArrayHandle)this; }
        inline UEVR_UObjectArrayHandle to_handle() const { return (UEVR_UObjectArrayHandle)this; }

        static FUObjectArray* get() {
            return API::get()->get_uobject_array();
        }

        static bool is_chunked() {
            static const auto fn = initialize()->is_chunked;
            return fn();
        }

        static bool is_inlined() {
            static const auto fn = initialize()->is_inlined;
            return fn();
        }

        static size_t get_objects_offset() {
            static const auto fn = initialize()->get_objects_offset;
            return (size_t)fn();
        }

        static size_t get_item_distance() {
            static const auto fn = initialize()->get_item_distance;
            return (size_t)fn();
        }

        int32_t get_object_count() const {
            static const auto fn = initialize()->get_object_count;
            return fn(to_handle());
        }

        void* get_objects_ptr() const {
            static const auto fn = initialize()->get_objects_ptr;
            return fn(to_handle());
        }

        UObject* get_object(int32_t index) const {
            static const auto fn = initialize()->get_object;
            return (UObject*)fn(to_handle(), index);
        }

        // Generally the same structure most of the time, not too much to worry about
        // Not that this would generally be used raw - instead prefer get_object
        struct FUObjectItem {
            API::UObject* object;
            int32_t flags;
            int32_t cluster_index;
            int32_t serial_number;
        };

        FUObjectItem* get_item(int32_t index) const {
            static const auto fn = initialize()->get_item;
            return (FUObjectItem*)fn(to_handle(), index);
        }

    private:
        static inline const UEVR_UObjectArrayFunctions* s_functions{nullptr};
        static inline const UEVR_UObjectArrayFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->uobject_array;
            }

            return s_functions;
        }
    };

    // One of the very few non-opaque structs
    // because these have never changed, if they do its because of bespoke code
    template <typename T>
    struct TArray {
        T* data;
        int32_t count;
        int32_t capacity;

        ~TArray() {
            if (data != nullptr) {
                FMalloc::get()->free(data);
                data = nullptr;
            }
        }

        T* begin() {
            return data;
        }

        T* end() {
            if (data == nullptr) {
                return nullptr;
            }

            return data + count;
        }

        T* begin() const {
            return data;
        }

        T* end() const {
            if (data == nullptr) {
                return nullptr;
            }

            return data + count;
        }

        bool empty() const {
            return count == 0 || data == nullptr;
        }
    };

    struct FRHITexture2D {
        inline UEVR_FRHITexture2DHandle to_handle() { return (UEVR_FRHITexture2DHandle)this; }
        inline UEVR_FRHITexture2DHandle to_handle() const { return (UEVR_FRHITexture2DHandle)this; }

        void* get_native_resource() const {
            static const auto fn = initialize()->get_native_resource;
            return fn(to_handle());
        }

    private:
        static inline const UEVR_FRHITexture2DFunctions* s_functions{nullptr};
        static inline const UEVR_FRHITexture2DFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->frhitexture2d;
            }

            return s_functions;
        }
    };

public:
    // UEVR specific stuff
    struct UObjectHook {
        struct MotionControllerState;

        static void activate() {
            static const auto fn = initialize()->activate;
            fn();
        }

        static bool exists(UObject* obj) {
            static const auto fn = initialize()->exists;
            return fn(obj->to_handle());
        }

        static std::vector<UObject*> get_objects_by_class(UClass* c, bool allow_default = false) {
            if (c == nullptr) {
                return {};
            }

            return c->get_objects_matching(allow_default);
        }

        static UObject* get_first_object_by_class(UClass* c, bool allow_default = false) {
            if (c == nullptr) {
                return nullptr;
            }

            return c->get_first_object_matching(allow_default);
        }

        // Must be a USceneComponent
        // Also, do NOT keep the pointer around, it will be invalidated at any time
        // Call it every time you need it
        static MotionControllerState* get_or_add_motion_controller_state(UObject* obj) {
            static const auto fn = initialize()->get_or_add_motion_controller_state;
            return (MotionControllerState*)fn(obj->to_handle());
        }

        static MotionControllerState* get_motion_controller_state(UObject* obj) {
            static const auto fn = initialize()->get_motion_controller_state;
            return (MotionControllerState*)fn(obj->to_handle());
        }

        struct MotionControllerState {
            inline UEVR_UObjectHookMotionControllerStateHandle to_handle() { return (UEVR_UObjectHookMotionControllerStateHandle)this; }
            inline UEVR_UObjectHookMotionControllerStateHandle to_handle() const { return (UEVR_UObjectHookMotionControllerStateHandle)this; }

            void set_rotation_offset(const UEVR_Quaternionf* offset) {
                static const auto fn = initialize()->set_rotation_offset;
                fn(to_handle(), offset);
            }

            void set_location_offset(const UEVR_Vector3f* offset) {
                static const auto fn = initialize()->set_location_offset;
                fn(to_handle(), offset);
            }

            void set_hand(uint32_t hand) {
                static const auto fn = initialize()->set_hand;
                fn(to_handle(), hand);
            }

            void set_permanent(bool permanent) {
                static const auto fn = initialize()->set_permanent;
                fn(to_handle(), permanent);
            }

        private:
            static inline const UEVR_UObjectHookMotionControllerStateFunctions* s_functions{nullptr};
            static inline const UEVR_UObjectHookMotionControllerStateFunctions* initialize() {
                if (s_functions == nullptr) {
                    s_functions = API::get()->sdk()->uobject_hook->mc_state;
                }

                return s_functions;
            }
        };

    private:
        static inline const UEVR_UObjectHookFunctions* s_functions{nullptr};
        static inline const UEVR_UObjectHookFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->uobject_hook;
            }

            return s_functions;
        }
    };

    struct RenderTargetPoolHook {
        static void activate() {
            static const auto fn = initialize()->activate;
            fn();
        }

        static IPooledRenderTarget* get_render_target(const wchar_t* name) {
            static const auto fn = initialize()->get_render_target;
            return (IPooledRenderTarget*)fn(name);
        }

    private:
        static inline const UEVR_FRenderTargetPoolHookFunctions* s_functions{nullptr};
        static inline const UEVR_FRenderTargetPoolHookFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->render_target_pool_hook;
            }

            return s_functions;
        }
    };

    struct StereoHook {
        static FRHITexture2D* get_scene_render_target() {
            static const auto fn = initialize()->get_scene_render_target;
            return (FRHITexture2D*)fn();
        }

        static FRHITexture2D* get_ui_render_target() {
            static const auto fn = initialize()->get_ui_render_target;
            return (FRHITexture2D*)fn();
        }

    private:
        static inline const UEVR_FFakeStereoRenderingHookFunctions* s_functions{nullptr};
        static inline const UEVR_FFakeStereoRenderingHookFunctions* initialize() {
            if (s_functions == nullptr) {
                s_functions = API::get()->sdk()->stereo_hook;
            }

            return s_functions;
        }
    };

private:
    const UEVR_PluginInitializeParam* m_param;
    const UEVR_SDKData* m_sdk;
};
}