/*
This file (API.hpp) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

extern "C" {
    #include "API.h"
}

#include <mutex>
#include <array>
#include <vector>
#include <cassert>
#include <string_view>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace uevr {
class API {
private:
    static inline std::unique_ptr<API> s_instance{};

public:
    // ALWAYS call initialize first in uevr_plugin_initialize
    static auto& initialize(const UEVR_PluginInitializeParam* param) {
        if (param == nullptr) {
            throw std::runtime_error("param is null");
        }

        if (s_instance != nullptr) {
            throw std::runtime_error("API already initialized");
        }

        s_instance = std::make_unique<API>(param);
        return s_instance;
    }

    // only call this AFTER calling initialize
    static auto& get() {
        if (s_instance == nullptr) {
            throw std::runtime_error("API not initialized");
        }

        return s_instance;
    }

public:
    API(const UEVR_PluginInitializeParam* param) 
        : m_param{param},
        m_sdk{param->sdk}
    {
    }

    virtual ~API() {

    }

    inline const auto param() const {
        return m_param;
    }

    inline const auto sdk() const {
        return m_sdk;
    }

    template <typename... Args> void log_error(const char* format, Args... args) { m_param->functions->log_error(format, args...); }
    template <typename... Args> void log_warn(const char* format, Args... args) { m_param->functions->log_warn(format, args...); }
    template <typename... Args> void log_info(const char* format, Args... args) { m_param->functions->log_info(format, args...); }

private:
    const UEVR_PluginInitializeParam* m_param;
    const UEVR_SDKData* m_sdk;
};
}