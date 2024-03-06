// Lua API to expose UEVR functionality to Lua scripts via UE4SS

#include <windows.h>
#include "lib/include/ScriptContext.hpp"

std::shared_ptr<uevr::ScriptContext> g_script_context{};

// Main exported function that takes in the lua_State*
extern "C" __declspec(dllexport) int luaopen_LuaVR(lua_State* L) {
    luaL_checkversion(L);

    uevr::ScriptContext::log("Initializing LuaVR...");

    g_script_context = uevr::ScriptContext::create(L);

    if (!g_script_context->valid()) {
        uevr::ScriptContext::log("LuaVR failed to initialize! Make sure to inject VR first!");
        return 0;
    }

    uevr::ScriptContext::log("LuaVR initialized!");

    return g_script_context->setup_bindings();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call, LPVOID reserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}