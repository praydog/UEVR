#include <utility/Module.hpp>

#include "EngineModule.hpp"

namespace sdk {
HMODULE get_ue_module(const std::wstring& name) {
    const auto current_executable = utility::get_executable();
    const auto exe_name = utility::get_module_path(current_executable);

    if (exe_name && exe_name->ends_with("UnrealEditor.exe")) {
        const auto mod = utility::find_partial_module(L"UnrealEditor-" + name + L".dll");

        if (mod != nullptr) {
            return mod;
        }
    }

    if (exe_name && exe_name->ends_with("UE4Editor.exe")) {
        const auto mod = utility::find_partial_module(L"UE4Editor-" + name + L".dll");

        if (mod != nullptr) {
            return mod;
        }
    }

    const auto partial_module = utility::find_partial_module(L"-" + name + L"-Win64-Shipping.dll");

    if (partial_module != nullptr) {
        return partial_module;
    }

    return current_executable;
}
}