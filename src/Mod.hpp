#pragma once

#include <vector>
#include <windows.h>
#include <algorithm>
#include <unordered_map>
#include <memory>

#include <imgui.h>
#include <utility/Config.hpp>

#include <sdk/Math.hpp>

class IModValue {
public:
    using Ptr = std::unique_ptr<IModValue>;

    virtual ~IModValue() {};
    virtual bool draw(std::string_view name) = 0;
    virtual void draw_value(std::string_view name) = 0;
    virtual void config_load(const utility::Config& cfg) = 0;
    virtual void config_save(utility::Config& cfg) = 0;
};

// Convenience classes for imgui
template <typename T>
class ModValue : public IModValue {
public:
    using Ptr = std::unique_ptr<ModValue<T>>;

    static auto create(std::string_view config_name, T default_value = T{}) {
        return std::make_unique<ModValue<T>>(config_name, default_value);
    }

    ModValue(std::string_view config_name, T default_value) 
        : m_config_name{ config_name },
        m_value{ default_value }, 
        m_default_value{ default_value }
    {
    }

    virtual ~ModValue() override {};

    virtual void config_load(const utility::Config& cfg) override {
        auto v = cfg.get<T>(m_config_name);

        if (v) {
            m_value = *v;
        }
    };

    virtual void config_save(utility::Config& cfg) override {
        cfg.set<T>(m_config_name, m_value);
    };

    operator T&() {
        return m_value;
    }

    T& value() {
        return m_value;
    }

    const T& default_value() const {
        return m_default_value;
    }

    const auto& get_config_name() const {
        return m_config_name;
    }

protected:
    T m_value{};
    T m_default_value{};
    std::string m_config_name{ "Default_ModValue" };
};

class ModToggle : public ModValue<bool> {
public:
    using Ptr = std::unique_ptr<ModToggle>;

    ModToggle(std::string_view config_name, bool default_value) 
        : ModValue<bool>{ config_name, default_value } 
    { 
    }

    static auto create(std::string_view config_name, bool default_value = false) {
        return std::make_unique<ModToggle>(config_name, default_value);
    }

    bool draw(std::string_view name) override {
        ImGui::PushID(this);
        auto ret = ImGui::Checkbox(name.data(), &m_value);
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        ImGui::Text("%s: %i", name.data(), m_value);
    }

    bool toggle() {
        return m_value = !m_value;
    }
};

class ModFloat : public ModValue<float> {
public:
    using Ptr = std::unique_ptr<ModFloat>;

    ModFloat(std::string_view config_name, float default_value) 
        : ModValue<float>{ config_name, default_value } { }

    static auto create(std::string_view config_name, float default_value = 0.0f) {
        return std::make_unique<ModFloat>(config_name, default_value);
    }

    bool draw(std::string_view name) override {
        ImGui::PushID(this);
        auto ret = ImGui::InputFloat(name.data(), &m_value);
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        ImGui::Text("%s: %f", name.data(), m_value);
    }
};

class ModSlider : public ModFloat {
public:
    using Ptr = std::unique_ptr<ModSlider>;

    static auto create(std::string_view config_name, float mn = 0.0f, float mx = 1.0f, float default_value = 0.0f) {
        return std::make_unique<ModSlider>(config_name, mn, mx, default_value);
    }

    ModSlider(std::string_view config_name, float mn = 0.0f, float mx = 1.0f, float default_value = 0.0f)
        : ModFloat{ config_name, default_value },
        m_range{ mn, mx }
    {
    }

    bool draw(std::string_view name) override {
        ImGui::PushID(this);
        auto ret = ImGui::SliderFloat(name.data(), &m_value, m_range.x, m_range.y);
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        ImGui::Text("%s: %f [%f, %f]", name.data(), m_value, m_range.x, m_range.y);
    }

    auto& range() {
        return m_range;
    }

protected:
    Vector2f m_range{ 0.0f, 1.0f };
};

class ModInt32 : public ModValue<int32_t> {
public:
    using Ptr = std::unique_ptr<ModInt32>;

    static auto create(std::string_view config_name, uint32_t default_value = 0) {
        return std::make_unique<ModInt32>(config_name, default_value);
    }

    ModInt32(std::string_view config_name, uint32_t default_value = 0)
        : ModValue{ config_name, static_cast<int>(default_value) }
    {
    }

    bool draw(std::string_view name) override {
        ImGui::PushID(this);
        auto ret = ImGui::InputInt(name.data(), &m_value);
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        ImGui::Text("%s: %i", name.data(), m_value);
    }
};

class ModCombo : public ModValue<int32_t> {
public:
    using Ptr = std::unique_ptr<ModCombo>;

    static auto create(std::string_view config_name, std::vector<std::string> options, int32_t default_value = 0) {
        return std::make_unique<ModCombo>(config_name, options, default_value);
    }

    ModCombo(std::string_view config_name, const std::vector<std::string>& options, int32_t default_value = 0)
        : ModValue{ config_name, default_value },
        m_options_stdstr{ options }
    {
        for (auto& o : m_options_stdstr) {
            m_options.push_back(o.c_str());
        }
    }

    bool draw(std::string_view name) override {
        // clamp m_value to valid range
        m_value = std::clamp<int32_t>(m_value, 0, static_cast<int32_t>(m_options.size()) - 1);

        ImGui::PushID(this);
        auto ret = ImGui::Combo(name.data(), &m_value, m_options.data(), static_cast<int32_t>(m_options.size()));
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        m_value = std::clamp<int32_t>(m_value, 0, static_cast<int32_t>(m_options.size()) - 1);

        ImGui::Text("%s: %s", name.data(), m_options[m_value]);
    }

    const auto& options() const {
        return m_options;
    }
    
protected:
    std::vector<const char*> m_options{};
    std::vector<std::string> m_options_stdstr{};
};

class Mod {
protected:
    using ValueList = std::vector<std::reference_wrapper<IModValue>>;

public:
    virtual ~Mod() {};
    virtual std::string_view get_name() const { return "UnknownMod"; };

    // can be used for ModValues, like Mod_ValueName
    virtual std::string generate_name(std::string_view name) { return std::string{ get_name() } + "_" + name.data(); }

    virtual std::optional<std::string> on_initialize() { return std::nullopt; };

    // This gets called after updating stuff like keyboard/mouse input to imgui
    // can be used to override these inputs e.g. with a custom input system
    // like VR controllers
    virtual void on_pre_imgui_frame() {};
    virtual void on_frame() {}; // BeginRendering, can be used for imgui
    virtual void on_present() {}; // actual present frame, CANNOT be used for imgui
    virtual void on_post_frame() {}; // after imgui rendering is done
    virtual void on_post_present() {}; // actually after present gets called
    virtual void on_draw_ui() {};
    virtual void on_device_reset() {};
    virtual bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) { return true; };

    virtual void on_config_load(const utility::Config& cfg) {};
    virtual void on_config_save(utility::Config& cfg) {};

    // game specific
    virtual void on_think() {};
};