// dllmain
#include <windows.h>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "Framework.hpp"

void startup_thread(HMODULE poc_module) {
    // create spdlog sink
    spdlog::set_default_logger(spdlog::basic_logger_mt("poc", "poc.log"));
    spdlog::flush_on(spdlog::level::info);
    spdlog::info("POC Entry");

    g_framework = std::make_unique<Framework>(poc_module);
}

BOOL APIENTRY DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)startup_thread, handle, 0, nullptr);
    }

    return TRUE;
}