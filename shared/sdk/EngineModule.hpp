#pragma once

#include <string>
#include <windows.h>

// Finds a module matching the given name
// in a release build, it would be "-{name}-Win64-Shipping.dll"
// in the UnrealEditor, it would be "UnrealEditor-{name}.dll"
// otherwise, if everything is statically linked, it will just return the executable
namespace sdk {
HMODULE get_ue_module(const std::wstring& name);
}