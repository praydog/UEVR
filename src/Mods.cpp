#include <spdlog/spdlog.h>

#include "mods/FrameworkConfig.hpp"
#include "mods/VR.hpp"
#include "mods/PluginLoader.hpp"
#include "Mods.hpp"

Mods::Mods() {
    m_mods.emplace_back(new FrameworkConfig());
    m_mods.emplace_back(VR::get());

    m_mods.emplace_back(PluginLoader::get());
}

std::optional<std::string> Mods::on_initialize() const {
    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_initialize()", mod->get_name().data());

        if (auto e = mod->on_initialize(); e != std::nullopt) {
            spdlog::info("{:s}::on_initialize() has failed: {:s}", mod->get_name().data(), *e);
            return e;
        }
    }

    utility::Config cfg{ "fw_config.txt" };

    for (auto& mod : m_mods) {
        spdlog::info("{:s}::on_config_load()", mod->get_name().data());
        mod->on_config_load(cfg);
    }

    return std::nullopt;
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
