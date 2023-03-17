#pragma once

#include <spdlog/spdlog.h>

#define SPDLOG_INFO_ONCE(...) {static bool once = true; if (once) { SPDLOG_INFO(__VA_ARGS__); once = false; }}
#define SPDLOG_WARN_ONCE(...) {static bool once = true; if (once) { SPDLOG_WARN(__VA_ARGS__); once = false; }}
#define SPDLOG_ERROR_ONCE(...) {static bool once = true; if (once) { SPDLOG_ERROR(__VA_ARGS__); once = false; }}