#pragma once

#include <vector>
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <unordered_map>
#include <memory>

#include <imgui.h>
#include <utility/Config.hpp>

#include <sdk/Math.hpp>
#include <sdk/UGameEngine.hpp>
#include <sdk/FViewportInfo.hpp>

#include "Framework.hpp"

class IModValue {
public:
    using Ptr = std::unique_ptr<IModValue>;

    virtual ~IModValue() {};
    virtual bool draw(std::string_view name) = 0;
    virtual void draw_value(std::string_view name) = 0;
    virtual void config_load(const utility::Config& cfg, bool set_defaults) = 0;
    virtual void config_save(utility::Config& cfg) = 0;
    virtual void set(const std::string& value) = 0;
    virtual std::string get() const = 0;
    virtual std::string get_config_name() const = 0;
    virtual std::string_view get_config_name_view() const = 0;
};

// Convenience classes for imgui
template <typename T>
class ModValue : public IModValue {
public:
    using Ptr = std::unique_ptr<ModValue<T>>;

    static auto create(std::string_view config_name, T default_value = T{}, bool advanced_option = false) {
        return std::make_unique<ModValue<T>>(config_name, default_value, advanced_option);
    }

    ModValue(std::string_view config_name, T default_value, bool advanced_option = false) 
        : m_config_name{ config_name },
        m_value{ default_value }, 
        m_default_value{ default_value },
        m_advanced_option{ advanced_option }
    {
    }

    virtual ~ModValue() override {};

    virtual void config_load(const utility::Config& cfg, bool set_defaults) override {
        if (set_defaults) {
            m_value = m_default_value;
            return;
        }

        if constexpr (std::is_same_v<T, std::string>) {
            auto v = cfg.get(m_config_name);

            if (v) {
                m_value = *v;
            }
        } else {
            auto v = cfg.get<T>(m_config_name);

            if (v) {
                m_value = *v;
            }
        }
    };

    virtual void config_save(utility::Config& cfg) override {
        if constexpr (std::is_same_v<T, std::string>) {
            cfg.set(m_config_name, m_value);
        } else {
            cfg.set<T>(m_config_name, m_value);
        }
    };

    virtual std::string get() const override {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
            return m_value;
        } else if constexpr (std::is_same_v<T, bool>) {
            return m_value ? "true" : "false";
        } else {
            return std::to_string(m_value);
        }
    }

    virtual void set(const std::string& value) override {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
            m_value = value;
            return;
        }

        else if constexpr (std::is_same_v<T, bool>) {
            m_value = value == "true" || value == "1";
            return;
        }

        // Use the correct conversion function based on the type.
        else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                m_value = (T)std::stoul(value);
                return;
            }

            m_value = (T)std::stol(value);
            return;
        } else if constexpr (std::is_floating_point_v<T>) {
            m_value = (T)std::stod(value);
            return;
        }

        static_assert(std::is_same_v<T, void> == false, "Unsupported type for ModValue::set");
    }

    operator T&() {
        return m_value;
    }

    T& value() {
        return m_value;
    }

    T& default_value() {
        return m_default_value;
    }

    std::string get_config_name() const override {
        return m_config_name;
    }

    std::string_view get_config_name_view() const override {
        return m_config_name;
    }

    bool is_advanced_option() const {
        return m_advanced_option;
    }

    bool should_draw_option() const {
        return g_framework->is_advanced_view_enabled() || !this->m_advanced_option;
    }
    

    void context_menu_logic() {
        if (ImGui::BeginPopupContextItem()) {
            reset_to_default_value_logic();
            ImGui::EndPopup();
        }
    }

    void reset_to_default_value_logic() {
        if (ImGui::Button("Reset to default")) {
            m_value = m_default_value;
        }
    }

protected:
    T m_value{};
    const T m_default_value{};
    const std::string m_config_name{ "Default_ModValue" };
    const bool m_advanced_option{false};
};

class ModToggle : public ModValue<bool> {
public:
    using Ptr = std::unique_ptr<ModToggle>;

    ModToggle(std::string_view config_name, bool default_value, bool advanced_option = false) 
        : ModValue<bool>{ config_name, default_value, advanced_option } 
    { 
    }

    static auto create(std::string_view config_name, bool default_value = false, bool advanced_option = false) {
        return std::make_unique<ModToggle>(config_name, default_value, advanced_option);
    }
    
    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }
        
        ImGui::PushID(this);
        auto ret = ImGui::Checkbox(name.data(), &m_value);
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

        ImGui::Text("%s: %i", name.data(), m_value);
    }

    bool toggle() {
        return m_value = !m_value;
    }
};

class ModFloat : public ModValue<float> {
public:
    using Ptr = std::unique_ptr<ModFloat>;

    ModFloat(std::string_view config_name, float default_value, bool advanced_option = false) 
        : ModValue<float>{ config_name, default_value, advanced_option } { }

    static auto create(std::string_view config_name, float default_value = 0.0f, bool advanced_option = false) {
        return std::make_unique<ModFloat>(config_name, default_value, advanced_option);
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        ImGui::PushID(this);
        auto ret = ImGui::InputFloat(name.data(), &m_value);
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

        ImGui::Text("%s: %f", name.data(), m_value);
    }
};

class ModSlider : public ModFloat {
public:
    using Ptr = std::unique_ptr<ModSlider>;

    static auto create(std::string_view config_name, float mn = 0.0f, float mx = 1.0f, float default_value = 0.0f, bool advanced_option = false) {
        return std::make_unique<ModSlider>(config_name, mn, mx, default_value, advanced_option);
    }

    ModSlider(std::string_view config_name, float mn = 0.0f, float mx = 1.0f, float default_value = 0.0f, bool advanced_option = false)
        : ModFloat{ config_name, default_value, advanced_option },
        m_range{ mn, mx }
    {
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }


        ImGui::PushID(this);
        auto ret = ImGui::SliderFloat(name.data(), &m_value, m_range.x, m_range.y);
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

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

    static auto create(std::string_view config_name, int32_t default_value = 0, bool advanced_option = false) {
        return std::make_unique<ModInt32>(config_name, default_value, advanced_option);
    }

    ModInt32(std::string_view config_name, int32_t default_value = 0, bool advanced_option = false)
        : ModValue{ config_name, default_value, advanced_option }
    {
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        ImGui::PushID(this);
        auto ret = ImGui::InputInt(name.data(), &m_value);
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

        ImGui::Text("%s: %i", name.data(), m_value);
    }
};

class ModSliderInt32 : public ModInt32 {
public:
    using Ptr = std::unique_ptr<ModSliderInt32>;

    static auto create(std::string_view config_name, int32_t mn = -100, int32_t mx = 100, int32_t default_value = 0, bool advanced_option = false) {
        return std::make_unique<ModSliderInt32>(config_name, mn, mx, default_value, advanced_option);
    }

    ModSliderInt32(std::string_view config_name, int32_t mn = -100, int32_t mx = 100, int32_t default_value = 0, bool advanced_option = false)
        : ModInt32{ config_name, default_value, advanced_option },
        m_int_range{ mn, mx }
    {
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        ImGui::PushID(this);
        auto ret = ImGui::SliderInt(name.data(), &m_value, m_int_range.min, m_int_range.max);
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        ImGui::Text("%s: %i [%i, %i]", name.data(), m_value, m_int_range.min, m_int_range.max);
    }

    auto& range() {
        return m_int_range;
    }

protected:
    struct SliderIntRange {
        int min;
        int max;
    }m_int_range;
};

class ModCombo : public ModValue<int32_t> {
public:
    using Ptr = std::unique_ptr<ModCombo>;

    static auto create(std::string_view config_name, std::vector<std::string> options, int32_t default_value = 0, bool advanced_option = false) {
        return std::make_unique<ModCombo>(config_name, options, default_value, advanced_option);
    }

    ModCombo(std::string_view config_name, const std::vector<std::string>& options, int32_t default_value = 0, bool advanced_option = false)
        : ModValue{ config_name, default_value, advanced_option },
        m_options_stdstr{ options }
    {
        for (auto& o : m_options_stdstr) {
            m_options.push_back(o.c_str());
        }
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        // clamp m_value to valid range
        m_value = std::clamp<int32_t>(m_value, 0, static_cast<int32_t>(m_options.size()) - 1);

        ImGui::PushID(this);
        auto ret = ImGui::Combo(name.data(), &m_value, m_options.data(), static_cast<int32_t>(m_options.size()));
        context_menu_logic();
        ImGui::PopID();

        return ret;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

        m_value = std::clamp<int32_t>(m_value, 0, static_cast<int32_t>(m_options.size()) - 1);

        ImGui::Text("%s: %s", name.data(), m_options[m_value]);
    }

    void config_load(const utility::Config& cfg, bool set_defaults) override {
        ModValue<int32_t>::config_load(cfg, set_defaults);

        if (m_value >= (int32_t)m_options.size()) {
            if (!m_options.empty()) {
                m_value = (int32_t)m_options.size() - 1;
            } else {
                m_value = 0;
            }
        }

        if (m_value < 0) {
            m_value = 0;
        }
    };

    const auto& options() const {
        return m_options;
    }
    
protected:
    std::vector<const char*> m_options{};
    std::vector<std::string> m_options_stdstr{};
};

class ModKey: public ModInt32 {
public:
    using Ptr = std::unique_ptr<ModKey>;

    static auto create(std::string_view config_name, int32_t default_value = UNBOUND_KEY, bool advanced_option = false) {
        return std::make_unique<ModKey>(config_name, default_value);
    }

    ModKey(std::string_view config_name, int32_t default_value = UNBOUND_KEY, bool advanced_option = false)
        : ModInt32{ config_name, default_value, advanced_option }
    {
    }

    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        if (name.empty()) {
            return false;
        }

        ImGui::PushID(this);
        ImGui::Button(name.data());
        context_menu_logic();

        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[0]) {
            m_waiting_for_new_key = true;
        }

        if (m_waiting_for_new_key) {
            const auto &keys = g_framework->get_keyboard_state();
            for (int32_t k{ 0 }; k < keys.size(); ++k) {
                if (k == VK_LBUTTON || k == VK_RBUTTON) {
                    continue;
                }

                if (keys[k]) {
                    m_value = is_erase_key(k) ? UNBOUND_KEY : k;
                    m_waiting_for_new_key = false;
                    break;
                }
            }

            ImGui::SameLine();
            ImGui::Text("Press any key...");
        }
        else {
            ImGui::SameLine();

            if (m_value >= 0 && m_value <= 255) {
                if (keycodes.contains(m_value)) {
                    ImGui::Text("%s", keycodes[m_value].c_str());
                }
                else {
                    ImGui::Text("%i (Unknown)", m_value);
                }
            }
            else {
                ImGui::Text("Not bound");
            }
        }

        ImGui::PopID();

        return true;
    }

    bool is_key_down() const {
        if (m_value < 0 || m_value > 255) {
            return false;
        }

        if (m_value == VK_LBUTTON || m_value == VK_RBUTTON) {
            return false;
        }

        return g_framework->get_keyboard_state()[(uint8_t)m_value] != 0;
    }

    bool is_key_down_once() {
        auto down = is_key_down();

        if (!m_was_key_down && down) {
            m_was_key_down = true;
            return true;
        }

        if (!down) {
            m_was_key_down = false;
        }

        return false;
    }

    bool is_erase_key(uint8_t k) const {
        switch (k) {
        case VK_ESCAPE:
        case VK_BACK:
            return true;

        default:
            return false;
        }
    }

    static constexpr int32_t UNBOUND_KEY{ -1 };
    static std::unordered_map<int, std::string> keycodes;

protected:
    bool m_was_key_down{ false };
    bool m_waiting_for_new_key{ false };
};

class ModString : public ModValue<std::string> {
public:
    using Ptr = std::unique_ptr<ModString>;

    static auto create(std::string_view config_name, std::string default_value = "", bool advanced_option = false) {
        return std::make_unique<ModString>(config_name, default_value, advanced_option);
    }

    ModString(std::string_view config_name, std::string default_value = "", bool advanced_option = false)
        : ModValue{ config_name, default_value, advanced_option }
    {
    }

    // No use for actually displaying this yet, so leaving them out for now
    bool draw(std::string_view name) override {
        if (!should_draw_option()) {
            return false;
        }

        // TODO

        return false;
    }

    void draw_value(std::string_view name) override {
        if (!should_draw_option()) {
            return;
        }

        ImGui::Text("%s: %s", name.data(), m_value.c_str());
    }

    void config_load(const utility::Config& cfg, bool set_defaults) override {
        if (set_defaults) {
            m_value = m_default_value;
            return;
        }

        auto v = cfg.get(m_config_name);

        if (v) {
            m_value = *v;
        }
    };
};

class ModComponent;

class Mod {
public:
    using ValueList = std::vector<std::reference_wrapper<IModValue>>;

    virtual ~Mod() {};
    virtual std::string_view get_name() const { return "UnknownMod"; };
    virtual bool is_advanced_mod() const { return false; };

    // can be used for ModValues, like Mod_ValueName
    virtual std::string generate_name(std::string_view name) { return std::string{ get_name() } + "_" + name.data(); }

    virtual std::optional<std::string> on_initialize() { return std::nullopt; };
    virtual std::optional<std::string> on_initialize_d3d_thread() { return std::nullopt; };

    virtual std::vector<SidebarEntryInfo> get_sidebar_entries() { return {}; };

    // This gets called after updating stuff like keyboard/mouse input to imgui
    // can be used to override these inputs e.g. with a custom input system
    // like VR controllers
    virtual void on_pre_imgui_frame() {};
    virtual void on_frame() {}; // BeginRendering, can be used for imgui
    virtual void on_present() {}; // actual present frame, CANNOT be used for imgui
    virtual void on_post_frame() {}; // after imgui rendering is done
    virtual void on_post_present() {}; // actually after present gets called
    virtual void on_draw_ui() {};
    virtual void on_draw_sidebar_entry(std::string_view in_entry) {};
    virtual void on_device_reset() {};
    virtual bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) { return true; };
    virtual void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {};
    virtual void on_xinput_set_state(uint32_t* retval, uint32_t user_index, XINPUT_VIBRATION* vibration) {};

    virtual void on_post_render_vr_framework_dx11(ID3D11DeviceContext* context, ID3D11Texture2D* tex, ID3D11RenderTargetView* rtv) {};
    virtual void on_post_render_vr_framework_dx12(ID3D12GraphicsCommandList* command_list, ID3D12Resource* tex, D3D12_CPU_DESCRIPTOR_HANDLE* rtv) {};
    
    virtual void on_config_load(const utility::Config& cfg, bool set_defaults);
    virtual void on_config_save(utility::Config& cfg);

    virtual IModValue* get_value(std::string_view name) const;

    // game specific
    virtual void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {};
    virtual void on_post_engine_tick(sdk::UGameEngine* engine, float delta) {};
    virtual void on_pre_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {};
    virtual void on_post_slate_draw_window(void* renderer, void* command_list, sdk::FViewportInfo* viewport_info) {};
    virtual void on_early_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                     const float world_to_meters, Vector3f* view_location, bool is_double) {};
    virtual void on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                     const float world_to_meters, Vector3f* view_location, bool is_double) {};
    virtual void on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                      const float world_to_meters, Vector3f* view_location, bool is_double) {};
    virtual void on_pre_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {};
    virtual void on_post_viewport_client_draw(void* viewport_client, void* viewport, void* canvas) {};

protected:
    ValueList m_options{};
    std::vector<ModComponent*> m_components{};
};

class ModComponent : public Mod {
public:
    // todo?
};

inline void Mod::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (auto& value : m_options) {
        value.get().config_load(cfg, set_defaults);
    }

    for (auto& component : m_components) {
        component->on_config_load(cfg, set_defaults);
    }
}

inline void Mod::on_config_save(utility::Config& cfg) {
    for (const auto& value : m_options) {
        value.get().config_save(cfg);
    }

    for (auto& component : m_components) {
        component->on_config_save(cfg);
    }
}

inline IModValue* Mod::get_value(std::string_view name) const {
    auto it = std::find_if(m_options.begin(), m_options.end(), [&name](const auto& v) {
        return v.get().get_config_name_view() == name;
    });

    if (it == m_options.end()) {
        for (auto& component : m_components) {
            auto value = component->get_value(name);

            if (value != nullptr) {
                return value;
            }
        }

        return nullptr;
    }

    return &it->get();
}
