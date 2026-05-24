#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

#include "Framework.hpp"

#include "mods/FrameworkConfig.hpp"
#include "mods/VR.hpp"
#include "mods/PluginLoader.hpp"
#include "mods/LuaLoader.hpp"
#include "mods/UObjectHook.hpp"
#include "Mods.hpp"

namespace {

constexpr const char* UI_INVERT_ALPHA_KEY = "UI_InvertAlpha";
const std::string UI_INVERT_ALPHA_KEY_STRING{UI_INVERT_ALPHA_KEY};
constexpr float UI_INVERT_ALPHA_SLIDER_MIN = 0.01f;
constexpr float UI_INVERT_ALPHA_SLIDER_MAX = 0.99f;

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

std::optional<float> parse_float(std::string_view value) {
    const auto trimmed = trim(value);

    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::string buffer{trimmed};
    char* end_ptr{};
    const auto parsed_value = std::strtof(buffer.c_str(), &end_ptr);

    if (end_ptr == buffer.c_str()) {
        return std::nullopt;
    }

    while (*end_ptr != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end_ptr))) {
            return std::nullopt;
        }

        ++end_ptr;
    }

    return parsed_value;
}

bool migrate_ui_invert_alpha(utility::Config& cfg) {
    auto apply_value = [&](float new_value) {
        const auto clamped = std::clamp(new_value, UI_INVERT_ALPHA_SLIDER_MIN, UI_INVERT_ALPHA_SLIDER_MAX);
        cfg.set<float>(UI_INVERT_ALPHA_KEY_STRING, clamped);
        return true;
    };

    auto safe_get_bool = [&]() -> std::optional<bool> {
        try {
            if (auto value = cfg.get<bool>(UI_INVERT_ALPHA_KEY_STRING)) {
                return *value;
            }
        } catch (...) {
        }

        return std::nullopt;
    };

    if (auto legacy_bool = safe_get_bool()) {
        return apply_value(*legacy_bool ? UI_INVERT_ALPHA_SLIDER_MAX : UI_INVERT_ALPHA_SLIDER_MIN);
    }

    auto safe_get_float = [&]() -> std::optional<float> {
        try {
            if (auto value = cfg.get<float>(UI_INVERT_ALPHA_KEY_STRING)) {
                return *value;
            }
        } catch (...) {
        }

        return std::nullopt;
    };

    if (auto legacy_float = safe_get_float()) {
        if (*legacy_float < UI_INVERT_ALPHA_SLIDER_MIN || *legacy_float > UI_INVERT_ALPHA_SLIDER_MAX) {
            return apply_value(*legacy_float);
        }

        return false;
    }

    auto safe_get_string = [&]() -> std::optional<std::string> {
        try {
            return cfg.get<std::string>(UI_INVERT_ALPHA_KEY_STRING);
        } catch (...) {
            return std::nullopt;
        }
    };

    if (auto legacy_string = safe_get_string()) {
        const auto trimmed = trim(*legacy_string);

        if (!trimmed.empty()) {
            std::string lowered{trimmed};
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (lowered == "true" || lowered == "false") {
                return apply_value(lowered == "true" ? UI_INVERT_ALPHA_SLIDER_MAX : UI_INVERT_ALPHA_SLIDER_MIN);
            }

            if (lowered == "1" || lowered == "0") {
                return apply_value(lowered == "1" ? UI_INVERT_ALPHA_SLIDER_MAX : UI_INVERT_ALPHA_SLIDER_MIN);
            }

            if (auto parsed = parse_float(trimmed)) {
                return apply_value(*parsed);
            }
        }
    }

    return false;
}

} // namespace

Mods::Mods() {
    m_mods.emplace_back(FrameworkConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(UObjectHook::get());

    m_mods.emplace_back(PluginLoader::get());
    m_mods.emplace_back(LuaLoader::get());
}

std::optional<std::string> Mods::on_initialize() const {
    std::scoped_lock _{g_framework->get_hook_monitor_mutex()};

    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_initialize()", mod->get_name().data());

        if (auto e = mod->on_initialize(); e != std::nullopt) {
            spdlog::info("{:s}::on_initialize() has failed: {:s}", mod->get_name().data(), *e);
            return e;
        }
    }

    return std::nullopt;
}

std::optional<std::string> Mods::on_initialize_d3d_thread() const {
    std::scoped_lock _{g_framework->get_hook_monitor_mutex()};

    // once here to at least setup the values
    reload_config();

    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_initialize_d3d_thread()", mod->get_name().data());

        if (auto e = mod->on_initialize_d3d_thread(); e != std::nullopt) {
            spdlog::info("{:s}::on_initialize_d3d_thread() has failed: {:s}", mod->get_name().data(), *e);
            return e;
        }
    }

    reload_config();

    return std::nullopt;
}

void Mods::reload_config(bool set_defaults) const {
    const auto config_path = Framework::get_persistent_dir("config.txt");
    utility::Config cfg{ config_path.string() };

    const auto migrated_invert_alpha = !set_defaults && migrate_ui_invert_alpha(cfg);

    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg, set_defaults);
    }

    if (migrated_invert_alpha) {
        if (!cfg.save(config_path.string())) {
            spdlog::warn("Failed to persist migrated UI_InvertAlpha value");
        } else {
            spdlog::info("Persisted migrated UI_InvertAlpha value");
        }
    }
}

void Mods::on_pre_imgui_frame() const {
    for (auto& mod : m_mods) {
        mod->on_pre_imgui_frame();
    }
}

void Mods::on_frame() const {
    for (auto& mod : m_mods) {
        mod->on_frame();
    }
}

void Mods::on_present() const {
    for (auto& mod : m_mods) {
        mod->on_present();
    }
}

void Mods::on_post_frame() const {
    for (auto& mod : m_mods) {
        mod->on_post_frame();
    }
}

void Mods::on_draw_ui() const {
    for (auto& mod : m_mods) {
        mod->on_draw_ui();
    }
}

void Mods::on_device_reset() const {
    for (auto& mod : m_mods) {
        mod->on_device_reset();
    }
}
