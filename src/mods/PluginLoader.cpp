#include <filesystem>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <openvr.h>

#include "Framework.hpp"
#include "uevr/API.h"

#include <utility/String.hpp>
#include <utility/Module.hpp>

#include <sdk/UEngine.hpp>
#include <sdk/CVar.hpp>
#include <sdk/UObjectArray.hpp>
#include <sdk/UObject.hpp>
#include <sdk/FField.hpp>
#include <sdk/FProperty.hpp>
#include <sdk/UFunction.hpp>
#include <sdk/UGameplayStatics.hpp>
#include <sdk/APlayerController.hpp>
#include <sdk/USceneComponent.hpp>

#include "pluginloader/FFakeStereoRenderingFunctions.hpp"
#include "pluginloader/FRenderTargetPoolHook.hpp"
#include "pluginloader/FRHITexture2DFunctions.hpp"

#include "UObjectHook.hpp"
#include "VR.hpp"

#include "PluginLoader.hpp"

UEVR_PluginVersion g_plugin_version{
    UEVR_PLUGIN_VERSION_MAJOR, UEVR_PLUGIN_VERSION_MINOR, UEVR_PLUGIN_VERSION_PATCH};

namespace uevr {
UEVR_RendererData g_renderer_data{
    UEVR_RENDERER_D3D12, nullptr, nullptr, nullptr
};
}

namespace uevr {
void log_error(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::error("[Plugin] {}", str);
}
void log_warn(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::warn("[Plugin] {}", str);
}
void log_info(const char* format, ...) {
    va_list args{};
    va_start(args, format);
    auto str = utility::format_string(format, args);
    va_end(args);
    spdlog::info("[Plugin] {}", str);
}
bool is_drawing_ui() {
    return g_framework->is_drawing_ui();
}
bool remove_callback(void* cb) {
    return PluginLoader::get()->remove_callback(cb);
}

unsigned int get_persistent_dir(wchar_t* buffer, unsigned int buffer_size) {
    const auto path = g_framework->get_persistent_dir().wstring();
    if (buffer == nullptr || buffer_size == 0) {
        return (unsigned int)path.size();
    }

    const auto size = std::min<size_t>(path.size(), (size_t)buffer_size - 1);
    memcpy(buffer, path.c_str(), size * sizeof(wchar_t));
    buffer[size] = L'\0';

    return (unsigned int)size;
}
}

namespace uevr {
bool on_present(UEVR_OnPresentCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_present(cb);
}

bool on_device_reset(UEVR_OnDeviceResetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_device_reset(cb);
}

bool on_message(UEVR_OnMessageCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_message(cb);
}

bool on_xinput_get_state(UEVR_OnXInputGetStateCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_xinput_get_state(cb);
}

bool on_xinput_set_state(UEVR_OnXInputSetStateCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_xinput_set_state(cb);
}

bool on_post_render_vr_framework_dx11(UEVR_OnPostRenderVRFrameworkDX11Cb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_render_vr_framework_dx11(cb);
}

bool on_post_render_vr_framework_dx12(UEVR_OnPostRenderVRFrameworkDX12Cb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_render_vr_framework_dx12(cb);
}
}

UEVR_PluginCallbacks g_plugin_callbacks {
    uevr::on_present,
    uevr::on_device_reset,
    uevr::on_message,
    uevr::on_xinput_get_state,
    uevr::on_xinput_set_state,
    uevr::on_post_render_vr_framework_dx11,
    uevr::on_post_render_vr_framework_dx12
};

UEVR_PluginFunctions g_plugin_functions {
    uevr::log_error,
    uevr::log_warn,
    uevr::log_info,
    uevr::is_drawing_ui,
    uevr::remove_callback,
    uevr::get_persistent_dir
};

#define GET_ENGINE_WORLD_RETNULL() \
    auto engine = sdk::UEngine::get(); \
    if (engine == nullptr) { \
        return nullptr; \
    } \
    auto world = engine->get_world(); \
    if (world == nullptr) { \
        return nullptr; \
    }

UEVR_SDKFunctions g_sdk_functions {
    []() -> UEVR_UEngineHandle {
        return (UEVR_UEngineHandle)sdk::UEngine::get();
    },
    [](const char* module_name, const char* name, int value) -> void {
        static std::unordered_map<std::string, sdk::TConsoleVariableData<int>**> cvars{};

        auto set_cvar = [](sdk::TConsoleVariableData<int>** cvar, int value) {
            if (cvar != nullptr && *cvar != nullptr) {
                (*cvar)->set(value);
            }
        };

        if (!cvars.contains(name)) {
            const auto cvar = sdk::find_cvar_data(utility::widen(module_name), utility::widen(name));

            if (cvar) {
                cvars[name] = (sdk::TConsoleVariableData<int>**)cvar->address();
                set_cvar((sdk::TConsoleVariableData<int>**)cvar->address(), value);
            }
        } else {
            set_cvar(cvars[name], value);
        }
    },
    // get_uobject_array
    []() -> UEVR_UObjectArrayHandle {
        return (UEVR_UObjectArrayHandle)sdk::FUObjectArray::get();
    },
    // get_player_controller
    [](int index) -> UEVR_UObjectHandle {
        GET_ENGINE_WORLD_RETNULL();
        const auto ugameplay_statics = sdk::UGameplayStatics::get();
        if (ugameplay_statics == nullptr) {
            return nullptr;
        }

        return (UEVR_UObjectHandle)ugameplay_statics->get_player_controller(world, index);
    },
    // get_local_pawn
    [](int index) -> UEVR_UObjectHandle {
        GET_ENGINE_WORLD_RETNULL();
        const auto ugameplay_statics = sdk::UGameplayStatics::get();
        if (ugameplay_statics == nullptr) {
            return nullptr;
        }

        const auto pc = ugameplay_statics->get_player_controller(world, index);
        if (pc == nullptr) {
            return nullptr;
        }

        return (UEVR_UObjectHandle)pc->get_acknowledged_pawn();
    },
    // spawn_object
    [](UEVR_UClassHandle klass, UEVR_UObjectHandle outer) -> UEVR_UObjectHandle {
        if (klass == nullptr) {
            return nullptr;
        }

        const auto ugs = sdk::UGameplayStatics::get();
        if (ugs == nullptr) {
            return nullptr;
        }

        return (UEVR_UObjectHandle)ugs->spawn_object((sdk::UClass*)klass, (sdk::UObject*)outer);
    },
    // execute_command
    [](const wchar_t* command) -> void {
        if (command == nullptr) {
            return;
        }

        sdk::UEngine::get()->exec(command);
    },
    // execute_command_ex
    [](UEVR_UObjectHandle world, const wchar_t* command, void* output_device) -> void {
        if (command == nullptr) {
            return;
        }

        sdk::UEngine::get()->exec((sdk::UWorld*)world, command, output_device);
    },
    // get_console_manager
    []() -> UEVR_FConsoleManagerHandle {
        return (UEVR_FConsoleManagerHandle)sdk::FConsoleManager::get();
    },
};

namespace uevr {
bool on_pre_engine_tick(UEVR_Engine_TickCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_engine_tick(cb);
}

bool on_post_engine_tick(UEVR_Engine_TickCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_engine_tick(cb);
}

bool on_pre_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_slate_draw_window_render_thread(cb);
}

bool on_post_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_slate_draw_window_render_thread(cb);
}

bool on_pre_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_calculate_stereo_view_offset(cb);
}

bool on_post_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_calculate_stereo_view_offset(cb);
}

bool on_pre_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_pre_viewport_client_draw(cb);
}

bool on_post_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    if (cb == nullptr) {
        return false;
    }

    return PluginLoader::get()->add_on_post_viewport_client_draw(cb);
}
}

UEVR_SDKCallbacks g_sdk_callbacks {
    uevr::on_pre_engine_tick,
    uevr::on_post_engine_tick,
    uevr::on_pre_slate_draw_window_render_thread,
    uevr::on_post_slate_draw_window_render_thread,
    uevr::on_pre_calculate_stereo_view_offset,
    uevr::on_post_calculate_stereo_view_offset,
    uevr::on_pre_viewport_client_draw,
    uevr::on_post_viewport_client_draw
};

#define UOBJECT(x) ((sdk::UObject*)x)

UEVR_UObjectFunctions g_uobject_functions {
    // get_class
    [](UEVR_UObjectHandle obj) {
        return (UEVR_UClassHandle)UOBJECT(obj)->get_class();
    },
    // get_outer
    [](UEVR_UObjectHandle obj) {
        return (UEVR_UObjectHandle)UOBJECT(obj)->get_outer();
    },
    // get_property_data
    [](UEVR_UObjectHandle obj, const wchar_t* name) {
        return (void*)UOBJECT(obj)->get_property_data(name);
    },
    // is_a
    [](UEVR_UObjectHandle obj, UEVR_UClassHandle cmp) {
        return UOBJECT(obj)->is_a((sdk::UClass*)cmp);
    },
    // process_event
    [](UEVR_UObjectHandle obj, UEVR_UFunctionHandle func, void* params) {
        UOBJECT(obj)->process_event((sdk::UFunction*)func, params);
    },
    // call_function
    [](UEVR_UObjectHandle obj, const wchar_t* name, void* params) {
        UOBJECT(obj)->call_function(name, params);
    },
    // get_fname
    [](UEVR_UObjectHandle obj) {
        return (UEVR_FNameHandle)&UOBJECT(obj)->get_fname();
    },
};

UEVR_UObjectArrayFunctions g_uobject_array_functions {
    // find_uobject
    [](const wchar_t* name) {
        return (UEVR_UObjectHandle)sdk::find_uobject(name);
    },
};

#define FFIELD(x) ((sdk::FField*)x)

UEVR_FFieldFunctions g_ffield_functions {
    // get_next
    [](UEVR_FFieldHandle field) {
        return (UEVR_FFieldHandle)FFIELD(field)->get_next();
    },
    // get_class
    [](UEVR_FFieldHandle field) {
        return (UEVR_FFieldClassHandle)FFIELD(field)->get_class();
    },
    // get_fname
    [](UEVR_FFieldHandle field) {
        return (UEVR_FNameHandle)&FFIELD(field)->get_field_name();
    },
};

#define FPROPERTY(x) ((sdk::FProperty*)x)

UEVR_FPropertyFunctions g_fproperty_functions {
    // get_offset
    [](UEVR_FPropertyHandle prop) -> int {
        return FPROPERTY(prop)->get_offset();
    },
};

#define USTRUCT(x) ((sdk::UStruct*)x)

UEVR_UStructFunctions g_ustruct_functions {
    // get_super_struct
    [](UEVR_UStructHandle strct) {
        return (UEVR_UStructHandle)USTRUCT(strct)->get_super_struct();
    },
    // get_child_properties
    [](UEVR_UStructHandle strct) {
        return (UEVR_FFieldHandle)USTRUCT(strct)->get_child_properties();
    },
    // find_function
    [](UEVR_UStructHandle strct, const wchar_t* name) {
        return (UEVR_UFunctionHandle)USTRUCT(strct)->find_function(name);
    },
};

#define UCLASS(x) ((sdk::UClass*)x)

UEVR_UClassFunctions g_uclass_functions {
    // get_class_default_object
    [](UEVR_UClassHandle klass) {
        return (UEVR_UObjectHandle)UCLASS(klass)->get_class_default_object();
    },
};

#define UFUNCTION(x) ((sdk::UFunction*)x)

UEVR_UFunctionFunctions g_ufunction_functions {
    // get_native_function
    [](UEVR_UFunctionHandle func) {
        return (void*)UFUNCTION(func)->get_native_function();
    },
};

namespace uevr {
namespace uobjecthook {
    void activate() {
        UObjectHook::get()->activate();
    }

    bool exists(UEVR_UObjectHandle obj) {
        return UObjectHook::get()->exists((sdk::UObject*)obj);
    }

    int get_objects_by_class(UEVR_UClassHandle klass, UEVR_UObjectHandle* out_objects, unsigned int max_objects, bool allow_default) {
        const auto objects = UObjectHook::get()->get_objects_by_class((sdk::UClass*)klass);

        if (objects.empty()) {
            return 0;
        }

        const auto default_object = ((sdk::UClass*)klass)->get_class_default_object();

        unsigned int i = 0;
        for (auto&& obj : objects) {
            if (!allow_default && obj == default_object) {
                continue;
            }

            if (i < max_objects && out_objects != nullptr) {
                out_objects[i++] = (UEVR_UObjectHandle)obj;
            } else {
                i++;
            }
        }

        return i;
    }

    int get_objects_by_class_name(const wchar_t* class_name, UEVR_UObjectHandle* out_objects, unsigned int max_objects, bool allow_default) {
        const auto c = sdk::find_uobject<sdk::UClass>(class_name);

        if (c == nullptr) {
            return 0;
        }

        return get_objects_by_class((UEVR_UClassHandle)c, out_objects, max_objects, allow_default);
    }

    UEVR_UObjectHandle get_first_object_by_class(UEVR_UClassHandle klass, bool allow_default) {
        const auto objects = UObjectHook::get()->get_objects_by_class((sdk::UClass*)klass);

        if (objects.empty()) {
            return nullptr;
        }

        if (allow_default) {
            return (UEVR_UObjectHandle)*objects.begin();
        }

        const auto default_object = ((sdk::UClass*)klass)->get_class_default_object();

        for (auto&& obj : objects) {
            if (obj != default_object) {
                return (UEVR_UObjectHandle)obj;
            }
        }

        return (UEVR_UObjectHandle)nullptr;
    }

    UEVR_UObjectHandle get_first_object_by_class_name(const wchar_t* class_name, bool allow_default) {
        const auto c = sdk::find_uobject<sdk::UClass>(class_name);

        if (c == nullptr) {
            return nullptr;
        }

        return get_first_object_by_class((UEVR_UClassHandle)c, allow_default);
    }

    UEVR_UObjectHookMotionControllerStateHandle get_or_add_motion_controller_state(UEVR_UObjectHandle obj_handle) {
        const auto obj = (sdk::USceneComponent*)obj_handle;
        if (obj == nullptr || !obj->is_a(sdk::USceneComponent::static_class())) {
            return nullptr;
        }

        const auto result = UObjectHook::get()->get_or_add_motion_controller_state(obj);

        return (UEVR_UObjectHookMotionControllerStateHandle)result.get();
    }

    UEVR_UObjectHookMotionControllerStateHandle get_motion_controller_state(UEVR_UObjectHandle obj_handle) {
        const auto obj = (sdk::USceneComponent*)obj_handle;
        if (obj == nullptr || !obj->is_a(sdk::USceneComponent::static_class())) {
            return nullptr;
        }

        const auto result = UObjectHook::get()->get_motion_controller_state(obj);

        if (!result.has_value()) {
            return nullptr;
        }

        return (UEVR_UObjectHookMotionControllerStateHandle)result->get();
    }

namespace mc_state {
    void set_rotation_offset(UEVR_UObjectHookMotionControllerStateHandle state, const UEVR_Quaternionf* rotation) {
        if (state == nullptr) {
            return;
        }

        auto& s = *(UObjectHook::MotionControllerState*)state;
        s.rotation_offset.x = rotation->x;
        s.rotation_offset.y = rotation->y;
        s.rotation_offset.z = rotation->z;
        s.rotation_offset.w = rotation->w;
    }

    void set_location_offset(UEVR_UObjectHookMotionControllerStateHandle state, const UEVR_Vector3f* location) {
        if (state == nullptr) {
            return;
        }

        auto& s = *(UObjectHook::MotionControllerState*)state;
        s.location_offset.x = location->x;
        s.location_offset.y = location->y;
        s.location_offset.z = location->z;
    }

    void set_hand(UEVR_UObjectHookMotionControllerStateHandle state, unsigned int hand) {
        if (state == nullptr) {
            return;
        }

        if (hand > 1) {
            return;
        }

        auto& s = *(UObjectHook::MotionControllerState*)state;
        s.hand = (uint8_t)hand;
    }

    void set_permanent(UEVR_UObjectHookMotionControllerStateHandle state, bool permanent) {
        if (state == nullptr) {
            return;
        }

        auto& s = *(UObjectHook::MotionControllerState*)state;
        s.permanent = permanent;
    }
}
}
}

UEVR_UObjectHookMotionControllerStateFunctions g_mc_functions {
    uevr::uobjecthook::mc_state::set_rotation_offset,
    uevr::uobjecthook::mc_state::set_location_offset,
    uevr::uobjecthook::mc_state::set_hand,
    uevr::uobjecthook::mc_state::set_permanent
};

UEVR_UObjectHookFunctions g_uobjecthook_functions {
    uevr::uobjecthook::activate,
    uevr::uobjecthook::exists,
    uevr::uobjecthook::get_objects_by_class,
    uevr::uobjecthook::get_objects_by_class_name,
    uevr::uobjecthook::get_first_object_by_class,
    uevr::uobjecthook::get_first_object_by_class_name,
    uevr::uobjecthook::get_or_add_motion_controller_state,
    uevr::uobjecthook::get_motion_controller_state,
    &g_mc_functions
};

#define FFIELDCLASS(x) ((sdk::FFieldClass*)x)

UEVR_FFieldClassFunctions g_ffield_class_functions {
    // get_fname
    [](UEVR_FFieldClassHandle field) {
        return (UEVR_FNameHandle)&FFIELDCLASS(field)->get_name();
    },
};

#define FNAME(x) ((sdk::FName*)x)

UEVR_FNameFunctions g_fname_functions {
    // to_string
    [](UEVR_FNameHandle name, wchar_t* buffer, unsigned int buffer_size) -> unsigned int {
        const auto result = FNAME(name)->to_string();

        if (buffer == nullptr || buffer_size == 0) {
            return (unsigned int)result.size();
        }

        const auto size = std::min<size_t>(result.size(), (size_t)buffer_size - 1);

        memcpy(buffer, result.c_str(), size * sizeof(wchar_t));
        buffer[size] = L'\0';

        return (unsigned int)size;
    },
    // constructor
    [](UEVR_FNameHandle name, const wchar_t* str, unsigned int find_type) {
        auto& fname = *(sdk::FName*)name;
        fname = sdk::FName{str, (sdk::EFindName)find_type};
    }
};

namespace uevr {
namespace console {
// get_console_objects
UEVR_TArrayHandle get_console_objects(UEVR_FConsoleManagerHandle mgr) {
    const auto console_manager = (sdk::FConsoleManager*)mgr;
    if (console_manager == nullptr) {
        return nullptr;
    }
    return (UEVR_TArrayHandle)&console_manager->get_console_objects();
}

UEVR_IConsoleObjectHandle find_object(UEVR_FConsoleManagerHandle mgr, const wchar_t* name) {
    const auto console_manager = (sdk::FConsoleManager*)mgr;
    if (console_manager == nullptr) {
        return nullptr;
    }
    return (UEVR_IConsoleObjectHandle)console_manager->find(name);
}

// Naive implementation, but it's fine for now
UEVR_IConsoleVariableHandle find_variable(UEVR_FConsoleManagerHandle mgr, const wchar_t* name) {
    return (UEVR_IConsoleVariableHandle)find_object(mgr, name);
}

UEVR_IConsoleCommandHandle find_command(UEVR_FConsoleManagerHandle mgr, const wchar_t* name) {
    auto obj = (sdk::IConsoleObject*)find_object(mgr, name);

    if (obj == nullptr) {
        return nullptr;
    }

    return (UEVR_IConsoleCommandHandle)obj->AsCommand();
}

UEVR_IConsoleCommandHandle as_commmand(UEVR_IConsoleObjectHandle obj) {
    if (obj == nullptr) {
        return nullptr;
    }

    return (UEVR_IConsoleCommandHandle)((sdk::IConsoleObject*)obj)->AsCommand();
}

void variable_set(UEVR_IConsoleVariableHandle var, const wchar_t* value) {
    if (var == nullptr) {
        return;
    }

    ((sdk::IConsoleVariable*)var)->Set(value);
}

void variable_set_ex(UEVR_IConsoleVariableHandle var, const wchar_t* value, unsigned int flags) {
    if (var == nullptr) {
        return;
    }

    ((sdk::IConsoleVariable*)var)->Set(value, flags);
}

int variable_get_int(UEVR_IConsoleVariableHandle var) {
    if (var == nullptr) {
        return 0;
    }

    return ((sdk::IConsoleVariable*)var)->GetInt();
}

float variable_get_float(UEVR_IConsoleVariableHandle var) {
    if (var == nullptr) {
        return 0.0f;
    }

    return ((sdk::IConsoleVariable*)var)->GetFloat();
}

void command_execute(UEVR_IConsoleCommandHandle cmd, const wchar_t* args) {
    if (cmd == nullptr) {
        return;
    }

    ((sdk::IConsoleCommand*)cmd)->Execute(args);
}
}
}

UEVR_ConsoleFunctions g_console_functions {
    uevr::console::get_console_objects,
    uevr::console::find_object,
    uevr::console::find_variable,
    uevr::console::find_command,
    uevr::console::as_commmand,
    uevr::console::variable_set,
    uevr::console::variable_set_ex,
    uevr::console::variable_get_int,
    uevr::console::variable_get_float,
    uevr::console::command_execute
};

namespace uevr {
namespace malloc {
UEVR_FMallocHandle get() {
    return (UEVR_FMallocHandle)sdk::FMalloc::get();
}

void* malloc(UEVR_FMallocHandle malloc, unsigned int size, uint32_t alignment) {
    return ((sdk::FMalloc*)malloc)->malloc(size, alignment);
}

void* realloc(UEVR_FMallocHandle malloc, void* original, unsigned int size, uint32_t alignment) {
    return ((sdk::FMalloc*)malloc)->realloc(original, size, alignment);
}

void free(UEVR_FMallocHandle malloc, void* original) {
    return ((sdk::FMalloc*)malloc)->free(original);
}
}
}

UEVR_FMallocFunctions g_malloc_functions {
    uevr::malloc::get,
    uevr::malloc::malloc,
    uevr::malloc::realloc,
    uevr::malloc::free
};

UEVR_SDKData g_sdk_data {
    &g_sdk_functions,
    &g_sdk_callbacks,
    &g_uobject_functions,
    &g_uobject_array_functions,
    &g_ffield_functions,
    &g_fproperty_functions,
    &g_ustruct_functions,
    &g_uclass_functions,
    &g_ufunction_functions,
    &g_uobjecthook_functions,
    &g_ffield_class_functions,
    &g_fname_functions,
    &g_console_functions,
    &g_malloc_functions,
    &uevr::render_target_pool_hook::functions,
    &uevr::stereo_hook::functions,
    &uevr::frhitexture2d::functions
};

namespace uevr {
namespace vr {
bool is_runtime_ready() {
    return ::VR::get()->get_runtime()->ready();
}

bool is_openvr() {
    return ::VR::get()->get_runtime()->is_openvr();
}

bool is_openxr() {
    return ::VR::get()->get_runtime()->is_openxr();
}

bool is_hmd_active() {
    return ::VR::get()->is_hmd_active();
}

void get_standing_origin(UEVR_Vector3f* out_origin) {
    auto origin = ::VR::get()->get_standing_origin();
    out_origin->x = origin.x;
    out_origin->y = origin.y;
    out_origin->z = origin.z;
}

void get_rotation_offset(UEVR_Quaternionf* out_rotation) {
    auto rotation = ::VR::get()->get_rotation_offset();
    out_rotation->x = rotation.x;
    out_rotation->y = rotation.y;
    out_rotation->z = rotation.z;
    out_rotation->w = rotation.w;
}

void set_standing_origin(const UEVR_Vector3f* origin) {
    ::VR::get()->set_standing_origin({origin->x, origin->y, origin->z, 1.0f});
}

void set_rotation_offset(const UEVR_Quaternionf* rotation) {
    ::VR::get()->set_rotation_offset({rotation->w, rotation->x, rotation->y, rotation->z});
}

UEVR_TrackedDeviceIndex get_hmd_index() {
    return VR::get()->get_hmd_index();
}

UEVR_TrackedDeviceIndex get_left_controller_index() {
    return VR::get()->get_left_controller_index();
}

UEVR_TrackedDeviceIndex get_right_controller_index() {
    return VR::get()->get_right_controller_index();
}

void get_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_grip_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_grip_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_aim_pose(UEVR_TrackedDeviceIndex index, UEVR_Vector3f* out_position, UEVR_Quaternionf* out_rotation) {
    static_assert(sizeof(UEVR_Vector3f) == sizeof(glm::vec3));
    static_assert(sizeof(UEVR_Quaternionf) == sizeof(glm::quat));

    auto transform = ::VR::get()->get_aim_transform(index);

    out_position->x = transform[3].x;
    out_position->y = transform[3].y;
    out_position->z = transform[3].z;

    const auto rot = glm::quat{glm::extractMatrixRotation(transform)};
    out_rotation->x = rot.x;
    out_rotation->y = rot.y;
    out_rotation->z = rot.z;
    out_rotation->w = rot.w;
}

void get_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_grip_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_grip_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_aim_transform(UEVR_TrackedDeviceIndex index, UEVR_Matrix4x4f* out_transform) {
    static_assert(sizeof(UEVR_Matrix4x4f) == sizeof(glm::mat4), "UEVR_Matrix4x4f and glm::mat4 must be the same size");

    const auto transform = ::VR::get()->get_aim_transform(index);
    memcpy(out_transform, &transform, sizeof(UEVR_Matrix4x4f));
}

void get_eye_offset(UEVR_Eye eye, UEVR_Vector3f* out_offset) {
    const auto out = ::VR::get()->get_eye_offset((VRRuntime::Eye)eye);

    out_offset->x = out.x;
    out_offset->y = out.y;
    out_offset->z = out.z;
}

void get_ue_projection_matrix(UEVR_Eye eye, UEVR_Matrix4x4f* out_projection) {
    const auto& projection = ::VR::get()->get_runtime()->projections[eye];
    memcpy(out_projection, &projection, sizeof(UEVR_Matrix4x4f));
}

UEVR_InputSourceHandle get_left_joystick_source() {
    return (UEVR_InputSourceHandle)::VR::get()->get_left_joystick();
}

UEVR_InputSourceHandle get_right_joystick_source() {
    return (UEVR_InputSourceHandle)::VR::get()->get_right_joystick();
}

UEVR_ActionHandle get_action_handle(const char* action_path) {
    return (UEVR_ActionHandle)::VR::get()->get_action_handle(action_path);
}

bool is_action_active(UEVR_ActionHandle action_handle, UEVR_InputSourceHandle source) {
    return ::VR::get()->is_action_active((::vr::VRActionHandle_t)action_handle, (::vr::VRInputValueHandle_t)source);
}

bool is_action_active_any_joystick(UEVR_ActionHandle action_handle) {
    return ::VR::get()->is_action_active_any_joystick((::vr::VRActionHandle_t)action_handle);
}

void get_joystick_axis(UEVR_InputSourceHandle source, UEVR_Vector2f* out_axis) {
    const auto axis = ::VR::get()->get_joystick_axis((::vr::VRInputValueHandle_t)source);

    out_axis->x = axis.x;
    out_axis->y = axis.y;
}

void trigger_haptic_vibration(float seconds_from_now, float duration, float frequency, float amplitude, UEVR_InputSourceHandle source) {
    ::VR::get()->trigger_haptic_vibration(seconds_from_now, duration, frequency, amplitude, (::vr::VRInputValueHandle_t)source);
}

bool is_using_controllers() {
    return ::VR::get()->is_using_controllers();
}

bool is_decoupled_pitch_enabled() {
    return ::VR::get()->is_decoupled_pitch_enabled();
}

unsigned int get_movement_orientation() {
    return ::VR::get()->get_movement_orientation();
}

unsigned int get_lowest_xinput_index() {
    return VR::get()->get_lowest_xinput_index();
}

void recenter_view() {
    VR::get()->recenter_view();
}

void recenter_horizon() {
    VR::get()->recenter_horizon();
}

unsigned int get_aim_method() {
    return (unsigned int)VR::get()->get_aim_method();
}

void set_aim_method(unsigned int method) {
    VR::get()->set_aim_method((VR::AimMethod)method);
}

bool is_aim_allowed() {
    return VR::get()->is_aim_allowed();
}

void set_aim_allowed(bool allowed) {
    VR::get()->set_aim_allowed(allowed);
}
}
}

UEVR_VRData g_vr_data {
    uevr::vr::is_runtime_ready,
    uevr::vr::is_openvr,
    uevr::vr::is_openxr,
    uevr::vr::is_hmd_active,
    uevr::vr::get_standing_origin,
    uevr::vr::get_rotation_offset,
    uevr::vr::set_standing_origin,
    uevr::vr::set_rotation_offset,
    uevr::vr::get_hmd_index,
    uevr::vr::get_left_controller_index,
    uevr::vr::get_right_controller_index,
    uevr::vr::get_pose,
    uevr::vr::get_transform,
    uevr::vr::get_grip_pose,
    uevr::vr::get_aim_pose,
    uevr::vr::get_grip_transform,
    uevr::vr::get_aim_transform,
    uevr::vr::get_eye_offset,
    uevr::vr::get_ue_projection_matrix,
    uevr::vr::get_left_joystick_source,
    uevr::vr::get_right_joystick_source,
    uevr::vr::get_action_handle,
    uevr::vr::is_action_active,
    uevr::vr::is_action_active_any_joystick,
    uevr::vr::get_joystick_axis,
    uevr::vr::trigger_haptic_vibration,
    uevr::vr::is_using_controllers,
    uevr::vr::is_decoupled_pitch_enabled,
    uevr::vr::get_movement_orientation,
    uevr::vr::get_lowest_xinput_index,
    uevr::vr::recenter_view,
    uevr::vr::recenter_horizon,
    uevr::vr::get_aim_method,
    uevr::vr::set_aim_method,
    uevr::vr::is_aim_allowed,
    uevr::vr::set_aim_allowed
};


/*
DECLARE_UEVR_HANDLE(UEVR_IVRSystem);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperone);
DECLARE_UEVR_HANDLE(UEVR_IVRChaperoneSetup);
DECLARE_UEVR_HANDLE(UEVR_IVRCompositor);
DECLARE_UEVR_HANDLE(UEVR_IVROverlay);
DECLARE_UEVR_HANDLE(UEVR_IVROverlayView);
DECLARE_UEVR_HANDLE(UEVR_IVRHeadsetView);
DECLARE_UEVR_HANDLE(UEVR_IVRScreenshots);
DECLARE_UEVR_HANDLE(UEVR_IVRRenderModels);
DECLARE_UEVR_HANDLE(UEVR_IVRApplications);
DECLARE_UEVR_HANDLE(UEVR_IVRSettings);
DECLARE_UEVR_HANDLE(UEVR_IVRResources);
DECLARE_UEVR_HANDLE(UEVR_IVRExtendedDisplay);
DECLARE_UEVR_HANDLE(UEVR_IVRTrackedCamera);
DECLARE_UEVR_HANDLE(UEVR_IVRDriverManager);
DECLARE_UEVR_HANDLE(UEVR_IVRInput);
DECLARE_UEVR_HANDLE(UEVR_IVRIOBuffer);
DECLARE_UEVR_HANDLE(UEVR_IVRSpatialAnchors);
DECLARE_UEVR_HANDLE(UEVR_IVRNotifications);
DECLARE_UEVR_HANDLE(UEVR_IVRDebug);
*/

namespace uevr {
namespace openvr{
UEVR_IVRSystem get_vr_system() {
    return (UEVR_IVRSystem)VR::get()->get_openvr_runtime()->hmd;
}

UEVR_IVRChaperone get_vr_chaperone() {
    return (UEVR_IVRChaperone)::vr::VRChaperone();
}

UEVR_IVRChaperoneSetup get_vr_chaperone_setup() {
    return (UEVR_IVRChaperoneSetup)::vr::VRChaperoneSetup();
}

UEVR_IVRCompositor get_vr_compositor() {
    return (UEVR_IVRCompositor)::vr::VRCompositor();
}

UEVR_IVROverlay get_vr_overlay() {
    return (UEVR_IVROverlay)::vr::VROverlay();
}

UEVR_IVROverlayView get_vr_overlay_view() {
    return (UEVR_IVROverlayView)::vr::VROverlayView();
}

UEVR_IVRHeadsetView get_vr_headset_view() {
    return (UEVR_IVRHeadsetView)::vr::VRHeadsetView();
}

UEVR_IVRScreenshots get_vr_screenshots() {
    return (UEVR_IVRScreenshots)::vr::VRScreenshots();
}

UEVR_IVRRenderModels get_vr_render_models() {
    return (UEVR_IVRRenderModels)::vr::VRRenderModels();
}

UEVR_IVRApplications get_vr_applications() {
    return (UEVR_IVRApplications)::vr::VRApplications();
}

UEVR_IVRSettings get_vr_settings() {
    return (UEVR_IVRSettings)::vr::VRSettings();
}

UEVR_IVRResources get_vr_resources() {
    return (UEVR_IVRResources)::vr::VRResources();
}

UEVR_IVRExtendedDisplay get_vr_extended_display() {
    return (UEVR_IVRExtendedDisplay)::vr::VRExtendedDisplay();
}

UEVR_IVRTrackedCamera get_vr_tracked_camera() {
    return (UEVR_IVRTrackedCamera)::vr::VRTrackedCamera();
}

UEVR_IVRDriverManager get_vr_driver_manager() {
    return (UEVR_IVRDriverManager)::vr::VRDriverManager();
}

UEVR_IVRInput get_vr_input() {
    return (UEVR_IVRInput)::vr::VRInput();
}

UEVR_IVRIOBuffer get_vr_io_buffer() {
    return (UEVR_IVRIOBuffer)::vr::VRIOBuffer();
}

UEVR_IVRSpatialAnchors get_vr_spatial_anchors() {
    return (UEVR_IVRSpatialAnchors)::vr::VRSpatialAnchors();
}

UEVR_IVRNotifications get_vr_notifications() {
    return (UEVR_IVRNotifications)::vr::VRNotifications();
}

UEVR_IVRDebug get_vr_debug() {
    return (UEVR_IVRDebug)::vr::VRDebug();
}
}
}

UEVR_OpenVRData g_openvr_data {
    uevr::openvr::get_vr_system,
    uevr::openvr::get_vr_chaperone,
    uevr::openvr::get_vr_chaperone_setup,
    uevr::openvr::get_vr_compositor,
    uevr::openvr::get_vr_overlay,
    uevr::openvr::get_vr_overlay_view,
    uevr::openvr::get_vr_headset_view,
    uevr::openvr::get_vr_screenshots,
    uevr::openvr::get_vr_render_models,
    uevr::openvr::get_vr_applications,
    uevr::openvr::get_vr_settings,
    uevr::openvr::get_vr_resources,
    uevr::openvr::get_vr_extended_display,
    uevr::openvr::get_vr_tracked_camera,
    uevr::openvr::get_vr_driver_manager,
    uevr::openvr::get_vr_input,
    uevr::openvr::get_vr_io_buffer,
    uevr::openvr::get_vr_spatial_anchors,
    uevr::openvr::get_vr_notifications,
    uevr::openvr::get_vr_debug
};

namespace uevr {
namespace openxr {
UEVR_XrInstance get_xr_instance() {
    return (UEVR_XrInstance)VR::get()->get_openxr_runtime()->instance;
}

UEVR_XrSession get_xr_session() {
    return (UEVR_XrSession)VR::get()->get_openxr_runtime()->session;
}

UEVR_XrSpace get_stage_space() {
    return (UEVR_XrSpace)VR::get()->get_openxr_runtime()->stage_space;
}

UEVR_XrSpace get_view_space() {
    return (UEVR_XrSpace)VR::get()->get_openxr_runtime()->view_space;
}
}
}

UEVR_OpenXRData g_openxr_data {
    uevr::openxr::get_xr_instance,
    uevr::openxr::get_xr_session,
    uevr::openxr::get_stage_space,
    uevr::openxr::get_view_space
};

extern "C" __declspec(dllexport) UEVR_PluginInitializeParam g_plugin_initialize_param{
    nullptr, 
    &g_plugin_version, 
    &g_plugin_functions, 
    &g_plugin_callbacks,
    &uevr::g_renderer_data,
    &g_vr_data,
    &g_openvr_data,
    &g_openxr_data,
    &g_sdk_data
};

void verify_sdk_pointers() {
    auto verify = [](auto& g) {
        spdlog::info("Verifying...");

        for (auto i = 0; i < sizeof(g) / sizeof(void*); ++i) {
            if (((void**)&g)[i] == nullptr) {
                spdlog::error("SDK pointer is null at index {}", i);
            }
        }
    };

    spdlog::info("Verifying SDK pointers...");

    verify(g_sdk_data);
}

std::shared_ptr<PluginLoader>& PluginLoader::get() {
    static auto instance = std::make_shared<PluginLoader>();
    return instance;
}

void PluginLoader::early_init() try {
    namespace fs = std::filesystem;

    std::scoped_lock _{m_mux};
    std::wstring module_path{};

    module_path.resize(1024, 0);
    module_path.resize(GetModuleFileNameW(nullptr, module_path.data(), module_path.size()));
    spdlog::info("[PluginLoader] Module path {}", utility::narrow(module_path));

    const auto plugin_path = Framework::get_persistent_dir() / "plugins";

    spdlog::info("[PluginLoader] Creating directories {}", plugin_path.string());

    if (!fs::create_directories(plugin_path) && !fs::exists(plugin_path)) {
        spdlog::error("[PluginLoader] Failed to create directory for plugins: {}", plugin_path.string());
    } else {
        spdlog::info("[PluginLoader] Created directory for plugins: {}", plugin_path.string());
    }

    spdlog::info("[PluginLoader] Loading plugins...");

    // Load all dlls in the plugins directory.
    for (auto&& entry : fs::directory_iterator{plugin_path}) {
        auto&& path = entry.path();

        if (path.has_extension() && path.extension() == ".dll") {
            auto module = LoadLibrary(path.string().c_str());

            if (module == nullptr) {
                spdlog::error("[PluginLoader] Failed to load {}", path.string());
                m_plugin_load_errors.emplace(path.stem().string(), "Failed to load");
                continue;
            }

            spdlog::info("[PluginLoader] Loaded {}", path.string());
            m_plugins.emplace(path.stem().string(), module);
        }
    }
} catch(const std::exception& e) {
    spdlog::error("[PluginLoader] Exception during early init {}", e.what());
} catch(...) {
    spdlog::error("[PluginLoader] Unknown exception during early init");
}

std::optional<std::string> PluginLoader::on_initialize_d3d_thread() {
    std::scoped_lock _{m_mux};

    // Call UEVR_plugin_required_version on any dlls that export it.
    g_plugin_initialize_param.uevr_module = g_framework->get_framework_module();
    uevr::g_renderer_data.renderer_type = (int)g_framework->get_renderer_type();
    
    if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D11) {
        auto& d3d11 = g_framework->get_d3d11_hook();

        uevr::g_renderer_data.device = d3d11->get_device();
        uevr::g_renderer_data.swapchain = d3d11->get_swap_chain();
    } else if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D12) {
        auto& d3d12 = g_framework->get_d3d12_hook();

        uevr::g_renderer_data.device = d3d12->get_device();
        uevr::g_renderer_data.swapchain = d3d12->get_swap_chain();
        uevr::g_renderer_data.command_queue = d3d12->get_command_queue();
    } else {
        spdlog::error("[PluginLoader] Unsupported renderer type {}", uevr::g_renderer_data.renderer_type);
        return "PluginLoader: Unsupported renderer type detected";
    }

    verify_sdk_pointers();

    for (auto it = m_plugins.begin(); it != m_plugins.end();) {
        auto name = it->first;
        auto mod = it->second;
        auto required_version_fn = (UEVR_PluginRequiredVersionFn)GetProcAddress(mod, "uevr_plugin_required_version");

        if (required_version_fn == nullptr) {
            spdlog::info("[PluginLoader] {} has no uevr_plugin_required_version function, skipping...", name);

            ++it;
            continue;
        }

        UEVR_PluginVersion required_version{};

        try {
            required_version_fn(&required_version);
        } catch(...) {
            spdlog::error("[PluginLoader] {} has an exception in uevr_plugin_required_version, skipping...", name);
            m_plugin_load_errors.emplace(name, "Exception occurred in uevr_plugin_required_version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        spdlog::info(
            "[PluginLoader] {} requires version {}.{}.{}", name, required_version.major, required_version.minor, required_version.patch);

        if (required_version.major != g_plugin_version.major) {
            spdlog::error("[PluginLoader] Plugin {} requires a different major version", name);
            m_plugin_load_errors.emplace(name, "Requires a different major version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        if (required_version.minor > g_plugin_version.minor) {
            spdlog::error("[PluginLoader] Plugin {} requires a newer minor version", name);
            m_plugin_load_errors.emplace(name, "Requires a newer minor version");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        if (required_version.patch > g_plugin_version.patch) {
            spdlog::warn("[PluginLoader] Plugin {} desires a newer patch version", name);
            m_plugin_load_warnings.emplace(name, "Desires a newer patch version");
        }

        ++it;
    }

    // Call UEVR_plugin_initialize on any dlls that export it.
    for (auto it = m_plugins.begin(); it != m_plugins.end();) {
        auto name = it->first;
        auto mod = it->second;
        auto init_fn = (UEVR_PluginInitializeFn)GetProcAddress(mod, "uevr_plugin_initialize");

        if (init_fn == nullptr) {
            ++it;
            continue;
        }

        spdlog::info("[PluginLoader] Initializing {}...", name);
        try {
            if (!init_fn(&g_plugin_initialize_param)) {
                spdlog::error("[PluginLoader] Failed to initialize {}", name);
                m_plugin_load_errors.emplace(name, "Failed to initialize");
                FreeLibrary(mod);
                it = m_plugins.erase(it);
                continue;
            }
        } catch(...) {
            spdlog::error("[PluginLoader] {} has an exception in uevr_plugin_initialize, skipping...", name);
            m_plugin_load_errors.emplace(name, "Exception occurred in uevr_plugin_initialize");
            FreeLibrary(mod);
            it = m_plugins.erase(it);
            continue;
        }

        ++it;
    }

    return std::nullopt;
}

void PluginLoader::attempt_unload_plugins() {
    std::unique_lock _{m_api_cb_mtx};

    for (auto& callbacks : m_plugin_callback_lists) {
        callbacks->clear();
    }

    for (auto& pair : m_plugins) {
        FreeLibrary(pair.second);
    }

    m_plugins.clear();
}

void PluginLoader::reload_plugins() {
    early_init();
    on_initialize_d3d_thread();
}

void PluginLoader::on_draw_ui() {
    std::scoped_lock _{m_mux};

    if (ImGui::Button("Attempt Unload Plugins")) {
        attempt_unload_plugins();
    }

    if (ImGui::Button("Reload Plugins")) {
        attempt_unload_plugins();
        reload_plugins();
    }

    if (!m_plugins.empty()) {
        ImGui::Text("Loaded plugins:");

        for (auto&& [name, _] : m_plugins) {
            ImGui::Text(name.c_str());
        }
    } else {
        ImGui::Text("No plugins loaded.");
    }

    if (!m_plugin_load_errors.empty()) {
        ImGui::Spacing();
        ImGui::Text("Errors:");
        for (auto&& [name, error] : m_plugin_load_errors) {
            ImGui::Text("%s - %s", name.c_str(), error.c_str());
        }
    }

    if (!m_plugin_load_warnings.empty()) {
        ImGui::Spacing();
        ImGui::Text("Warnings:");
        for (auto&& [name, warning] : m_plugin_load_warnings) {
            ImGui::Text("%s - %s", name.c_str(), warning.c_str());
        }
    }
}

void PluginLoader::on_present() {
    std::shared_lock _{m_api_cb_mtx};

    uevr::g_renderer_data.renderer_type = (int)g_framework->get_renderer_type();
    
    if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D11) {
        auto& d3d11 = g_framework->get_d3d11_hook();

        uevr::g_renderer_data.device = d3d11->get_device();
        uevr::g_renderer_data.swapchain = d3d11->get_swap_chain();
    } else if (uevr::g_renderer_data.renderer_type == UEVR_RENDERER_D3D12) {
        auto& d3d12 = g_framework->get_d3d12_hook();

        uevr::g_renderer_data.device = d3d12->get_device();
        uevr::g_renderer_data.swapchain = d3d12->get_swap_chain();
        uevr::g_renderer_data.command_queue = d3d12->get_command_queue();
    }

    for (auto&& cb : m_on_present_cbs) {
        try {
            cb();
        } catch(...) {
            spdlog::error("[PluginLoader] Exception occurred in on_present callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_device_reset() {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_device_reset_cbs) {
        try {
            cb();
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_device_reset callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_render_vr_framework_dx11(ID3D11DeviceContext* context, ID3D11Texture2D* tex, ID3D11RenderTargetView* rtv) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_render_vr_framework_dx11_cbs) {
        try {
            cb((void*)context, (void*)tex, (void*)rtv);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_render_vr_framework_dx11 callback; one of the plugins has an error.");
            continue;
        }
    }
}

void PluginLoader::on_post_render_vr_framework_dx12(ID3D12GraphicsCommandList* command_list, ID3D12Resource* tex, D3D12_CPU_DESCRIPTOR_HANDLE* rtv) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_render_vr_framework_dx12_cbs) {
        try {
            cb((void*)command_list, (void*)tex, (void*)rtv);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_render_vr_framework_dx12 callback; one of the plugins has an error.");
            continue;
        }
    }
}

bool PluginLoader::on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_message_cbs) {
        try {
            if (!cb(hwnd, msg, wparam, lparam)) {
                return false;
            }
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_message callback; one of the plugins has an error.");
            continue;
        }
    }

    return true;
}

void PluginLoader::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_xinput_get_state_cbs) {
        try {
            cb(retval, user_index, (void*)state);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_xinput_get_state callback; one of the plugins has an error.");
            continue;
        }
    }
}

void PluginLoader::on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_xinput_set_state_cbs) {
        try {
            cb(retval, user_index, (void*)vibration);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_xinput_set_state callback; one of the plugins has an error.");
            continue;
        }
    }
}


void PluginLoader::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_engine_tick_cbs) {
        try {
            cb((UEVR_UGameEngineHandle)engine, delta);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_engine_tick callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_engine_tick(sdk::UGameEngine* engine, float delta) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_engine_tick_cbs) {
        try {
            cb((UEVR_UGameEngineHandle)engine, delta);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_engine_tick callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_slate_draw_window_render_thread_cbs) {
        try {
            cb((UEVR_FSlateRHIRendererHandle)renderer, (UEVR_FViewportInfoHandle)viewport_info);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_slate_draw_window callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_slate_draw_window_render_thread_cbs) {
        try {
            cb((UEVR_FSlateRHIRendererHandle)renderer, (UEVR_FViewportInfoHandle)viewport_info);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_slate_draw_window callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                       const float world_to_meters, Vector3f* view_location, bool is_double)
{
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_calculate_stereo_view_offset_cbs) {
        try {
            cb( (UEVR_StereoRenderingDeviceHandle)stereo_device, view_index, world_to_meters, 
                (UEVR_Vector3f*)view_location, (UEVR_Rotatorf*)view_rotation, is_double);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_calculate_stereo_view_offset callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                        const float world_to_meters, Vector3f* view_location, bool is_double)
{
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_calculate_stereo_view_offset_cbs) {
        try {
            cb( (UEVR_StereoRenderingDeviceHandle)stereo_device, view_index, world_to_meters, 
                (UEVR_Vector3f*)view_location, (UEVR_Rotatorf*)view_rotation, is_double);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_calculate_stereo_view_offset callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_pre_viewport_client_draw_cbs) {
        try {
            cb((UEVR_UGameViewportClientHandle)viewport_client, (UEVR_FViewportHandle)viewport, (UEVR_FCanvasHandle)canvas);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_pre_viewport_client_draw callback; one of the plugins has an error.");
        }
    }
}

void PluginLoader::on_post_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {
    std::shared_lock _{m_api_cb_mtx};

    for (auto&& cb : m_on_post_viewport_client_draw_cbs) {
        try {
            cb((UEVR_UGameViewportClientHandle)viewport_client, (UEVR_FViewportHandle)viewport, (UEVR_FCanvasHandle)canvas);
        } catch(...) {
            spdlog::error("[APIProxy] Exception occurred in on_post_viewport_client_draw callback; one of the plugins has an error.");
        }
    }
}

bool PluginLoader::add_on_present(UEVR_OnPresentCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_present_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_device_reset(UEVR_OnDeviceResetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_device_reset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_message(UEVR_OnMessageCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_message_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_xinput_get_state(UEVR_OnXInputGetStateCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_xinput_get_state_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_xinput_set_state(UEVR_OnXInputSetStateCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_xinput_set_state_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_render_vr_framework_dx11(UEVR_OnPostRenderVRFrameworkDX11Cb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_render_vr_framework_dx11_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_render_vr_framework_dx12(UEVR_OnPostRenderVRFrameworkDX12Cb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_render_vr_framework_dx12_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_engine_tick(UEVR_Engine_TickCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_engine_tick_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_engine_tick(UEVR_Engine_TickCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_engine_tick_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_slate_draw_window_render_thread_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_slate_draw_window_render_thread(UEVR_Slate_DrawWindow_RenderThreadCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_slate_draw_window_render_thread_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_calculate_stereo_view_offset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_calculate_stereo_view_offset(UEVR_Stereo_CalculateStereoViewOffsetCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_calculate_stereo_view_offset_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_pre_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_pre_viewport_client_draw_cbs.push_back(cb);
    return true;
}

bool PluginLoader::add_on_post_viewport_client_draw(UEVR_ViewportClient_DrawCb cb) {
    std::unique_lock _{m_api_cb_mtx};

    m_on_post_viewport_client_draw_cbs.push_back(cb);
    return true;
}