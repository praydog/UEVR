#include <windows.h>

#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/Patch.hpp>
#include <utility/Thread.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <sdk/UObjectArray.hpp>

#define SPAWN_CONSOLE

void startup_thread() {
    // Spawn a debug console
#ifdef SPAWN_CONSOLE
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif

    // Set up spdlog to sink to the console
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] [iostore-standalone] %v");
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_default_logger(spdlog::stdout_logger_mt("console"));

    SPDLOG_INFO("Test!");

    sdk::FName::get_constructor();
    sdk::FName::get_to_string();
    sdk::FUObjectArray::get();

    SPDLOG_INFO("Test finished!");
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason_for_call, LPVOID reserved) {
    switch (reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        //CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)startup_thread, nullptr, 0, nullptr);
        startup_thread();
        break;
    }

    return TRUE;
}