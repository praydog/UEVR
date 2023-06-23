#pragma once

#include <chrono>

#include <spdlog/spdlog.h>

#define SPDLOG_INFO_ONCE(...) {static bool once = true; if (once) { SPDLOG_INFO(__VA_ARGS__); once = false; }}
#define SPDLOG_WARN_ONCE(...) {static bool once = true; if (once) { SPDLOG_WARN(__VA_ARGS__); once = false; }}
#define SPDLOG_ERROR_ONCE(...) {static bool once = true; if (once) { SPDLOG_ERROR(__VA_ARGS__); once = false; }}

#define SPDLOG_INFO_EVERY_N_SEC(n, ...) {static auto last = std::chrono::steady_clock::now(); auto now = std::chrono::steady_clock::now(); if (now - last >= std::chrono::seconds(n)) { SPDLOG_INFO(__VA_ARGS__); last = now; }}
#define SPDLOG_WARNING_EVERY_N_SEC(n, ...) {static auto last = std::chrono::steady_clock::now(); auto now = std::chrono::steady_clock::now(); if (now - last >= std::chrono::seconds(n)) { SPDLOG_WARN(__VA_ARGS__); last = now; }}
#define SPDLOG_ERROR_EVERY_N_SEC(n, ...) {static auto last = std::chrono::steady_clock::now(); auto now = std::chrono::steady_clock::now(); if (now - last >= std::chrono::seconds(n)) { SPDLOG_ERROR(__VA_ARGS__); last = now; }}