// Lua API to expose UEVR functionality to Lua scripts via UE4SS

#include <windows.h>
#include "ScriptContext.hpp"

// Main exported function that takes in the lua_State*
extern "C" __declspec(dllexport) int luaopen_LuaVR(lua_State* L) {
    luaL_checkversion(L);

    ScriptContext::log("Initializing LuaVR...");

    auto script_context = ScriptContext::reinitialize(L);

    if (!script_context->valid()) {
        ScriptContext::log("LuaVR failed to initialize! Make sure to inject VR first!");
        return 0;
    }

    ScriptContext::log("LuaVR initialized!");

    return script_context->setup_bindings();
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