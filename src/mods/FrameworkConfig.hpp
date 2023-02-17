#pragma once

#include "Mod.hpp"

class FrameworkConfig : public Mod {
public:
    static std::shared_ptr<FrameworkConfig>& get();

public:
    std::string_view get_name() const {
        return "FrameworkConfig";
    }

    std::optional<std::string> on_initialize() override;
    void on_draw_ui() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

    /*auto& get_menu_key() {
        return m_menu_key;
    }*/

    auto& get_menu_open() {
        return m_menu_open;
    }

    bool is_remember_menu_state() {
        return m_remember_menu_state->value();
    }

    bool is_enable_l3_r3_toggle() {
        return m_enable_l3_r3_toggle->value();
    }

    int32_t get_font_size() {
        return m_font_size->value();
    }

private:
    //ModKey::Ptr m_menu_key{ ModKey::create(generate_name("MenuKey"), DIK_INSERT) };
    ModToggle::Ptr m_menu_open{ ModToggle::create(generate_name("MenuOpen"), true) };
    ModToggle::Ptr m_remember_menu_state{ ModToggle::create(generate_name("RememberMenuState"), false) };
    ModToggle::Ptr m_enable_l3_r3_toggle{ ModToggle::create(generate_name("EnableL3R3Toggle"), true) };
    ModInt32::Ptr m_font_size{ModInt32::create(generate_name("FontSize"), 16)};

    ValueList m_options {
        *m_menu_open,
        *m_remember_menu_state,
        *m_enable_l3_r3_toggle,
        *m_font_size,
    };
};
