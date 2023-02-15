// Standalone DLL to stop UE games from loading the VR plugins
// They constantly try to do this at runtime
// and causes for example, OpenXR (on our end) to fail to create an instance
// among other things.
// This has to be ran before the frontend injects openxr_loader.dll
#include <iostream>
#include <string_view>

#include <Windows.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/Patch.hpp>
#include <utility/PointerHook.hpp>

void nullify_openxr(HMODULE game) {
    std::cout << "[VR Plugin Nullifier] Scanning for openxr_loader.dll" << std::endl;

    const auto openxr_string = utility::scan_string(game, "openxr_loader.dll");

    if (!openxr_string) {
        std::cout << "[VR Plugin Nullifier] openxr_loader.dll string not found" << std::endl;
        return;
    }

    // Change the .dll extension at the end of the string to .nul
    // This will cause the game to fail to load the plugin
    const auto openxr_string_view = std::string_view{(char*)*openxr_string};
    char* openxr_string_chars = (char*)*openxr_string;

    ProtectionOverride _{openxr_string_chars, openxr_string_view.size(), PAGE_EXECUTE_READWRITE};

    openxr_string_chars[openxr_string_view.size() - 3] = 'n';
    openxr_string_chars[openxr_string_view.size() - 2] = 'u';
    openxr_string_chars[openxr_string_view.size() - 1] = 'l';

    std::cout << "[VR Plugin Nullifier] openxr_loader.dll string patched" << std::endl;
}

void nullify_openvr(HMODULE game) {
    std::cout << "[VR Plugin Nullifier] Scanning for openvr_api.dll" << std::endl;

    const auto openvr_string = utility::scan_string(game, "openvr_api.dll");

    if (!openvr_string) {
        std::cout << "[VR Plugin Nullifier] openvr_api.dll string not found" << std::endl;
        return;
    }

    // Change the .dll extension at the end of the string to .nul
    // This will cause the game to fail to load the plugin
    const auto openvr_string_view = std::string_view{(char*)*openvr_string};
    char* openvr_string_chars = (char*)*openvr_string;

    ProtectionOverride _{openvr_string_chars, openvr_string_view.size(), PAGE_EXECUTE_READWRITE};

    openvr_string_chars[openvr_string_view.size() - 3] = 'n';
    openvr_string_chars[openvr_string_view.size() - 2] = 'u';
    openvr_string_chars[openvr_string_view.size() - 1] = 'l';

    std::cout << "[VR Plugin Nullifier] openvr_api.dll string patched" << std::endl;
}

extern "C" __declspec(dllexport) bool g_finished = false;

extern "C" __declspec(dllexport) void nullify() try {
    // Spawn debug console
    //AllocConsole();
    //freopen("CONOUT$", "w", stdout);

    printf("Hello\n");

    const auto game = utility::get_executable();

    nullify_openxr(game);
    nullify_openvr(game);

    g_finished = true;
} catch(...) {
    std::cout << "[VR Plugin Nullifier] Exception thrown" << std::endl;
    g_finished = true;
}

// dllmain
BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call, LPVOID reserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        break;
    }

    return TRUE;
}