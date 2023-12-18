#pragma once

#include "Mod.hpp"

class FrameworkConfig : public Mod {
public:
    static std::shared_ptr<FrameworkConfig>& get();

public:
    std::string_view get_name() const {
        return "FrameworkConfig";
    }

    std::vector<SidebarEntryInfo> get_sidebar_entries() override { 
        return {
                    { "Main", false },
                    { "GUI/Themes", false }
        };
    }

    std::optional<std::string> on_initialize() override;
    void on_frame() override;
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;
    void on_draw_sidebar_entry(std::string_view in_entry) override;

    void draw_themes();
    void draw_main();

    auto& get_menu_key() {
        return m_menu_key;
    }

    auto& get_menu_open() {
        return m_menu_open;
    }

    bool is_remember_menu_state() {
        return m_remember_menu_state->value();
    }

    bool is_enable_l3_r3_toggle() {
        return m_enable_l3_r3_toggle->value();
    }

    bool is_always_show_cursor() const {
        return m_always_show_cursor->value();
    }

    bool is_advanced_mode() const {
        return m_advanced_mode->value();
    }

    void toggle_advanced_mode() {
        m_advanced_mode->toggle();
    }

    auto& get_advanced_mode() {
        return m_advanced_mode;
    }

    auto& get_imgui_theme_value() {
        return m_imgui_theme->value();
    }

    auto& get_imgui_theme() {
        return m_imgui_theme;
    }

    int32_t get_font_size() {
        return m_font_size->value();
    }

private:
    static const inline std::vector<std::string> s_imgui_themes {
        "Default Dark",
        "Alternative Dark",
        "Default Light",
        "High Contrast",
    };
    
    ModKey::Ptr m_menu_key{ ModKey::create(generate_name("MenuKey"), VK_INSERT) };
    ModToggle::Ptr m_menu_open{ ModToggle::create(generate_name("MenuOpen"), true) };
    ModToggle::Ptr m_remember_menu_state{ ModToggle::create(generate_name("RememberMenuState"), false) };
    ModToggle::Ptr m_enable_l3_r3_toggle{ ModToggle::create(generate_name("EnableL3R3Toggle"), true) };
    ModToggle::Ptr m_always_show_cursor{ ModToggle::create(generate_name("AlwaysShowCursor"), false) };
    ModToggle::Ptr m_advanced_mode{ ModToggle::create(generate_name("AdvancedMode"), false) };
    ModCombo::Ptr m_imgui_theme{ ModCombo::create(generate_name("ImGuiTheme"), s_imgui_themes, Framework::ImGuiThemes::DEFAULT_DARK) };
    ModKey::Ptr m_show_cursor_key{ ModKey::create(generate_name("ShowCursorKey")) };
    ModInt32::Ptr m_font_size{ModInt32::create(generate_name("FontSize"), 16)};

    ValueList m_options {
        *m_menu_key,
        *m_show_cursor_key,
        *m_menu_open,
        *m_remember_menu_state,
        *m_enable_l3_r3_toggle,
        *m_advanced_mode,
        *m_imgui_theme,
        *m_always_show_cursor,
        *m_font_size,
    };
};
