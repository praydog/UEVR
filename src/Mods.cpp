#include <spdlog/spdlog.h>

#include "Framework.hpp"

#include "mods/FrameworkConfig.hpp"
#include "mods/VR.hpp"
#include "mods/PluginLoader.hpp"
#include "Mods.hpp"

Mods::Mods() {
    m_mods.emplace_back(FrameworkConfig::get());
    m_mods.emplace_back(VR::get());

    m_mods.emplace_back(PluginLoader::get());
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

void Mods::reload_config() const {
    utility::Config cfg{ Framework::get_persistent_dir("config.txt").string() };

    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg);
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
