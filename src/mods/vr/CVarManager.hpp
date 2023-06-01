#pragma once

#include <optional>
#include <memory>
#include <string>

#include <sdk/CVar.hpp>

#include "../../Mod.hpp"

// For UE cvars.
class CVarManager final : public ModComponent {
public:
    CVarManager();
    virtual ~CVarManager();

    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_draw_ui() override;
    void on_frame() override;
    void display_console();
    void on_config_load(const utility::Config& cfg, bool set_defaults) override;

    void dump_commands();

public:
    class CVar : public std::enable_shared_from_this<CVar> {
    public:
        enum class Type {
            BOOL,
            INT,
            FLOAT
        };

    public:
        CVar(const std::wstring& module, const std::wstring& name, Type type, int min_value, int max_value) 
            : m_module{module}
            , m_name{name}
            , m_type{type}
            , m_min_int_value{min_value}
            , m_max_int_value{max_value}
        {
        }

        CVar(const std::wstring& module, const std::wstring& name, Type type, float min_value, float max_value) 
            : m_module{module}
            , m_name{name}
            , m_type{type}
            , m_min_float_value{min_value}
            , m_max_float_value{max_value}
        {
        }

        virtual ~CVar() = default;

        virtual void load(bool set_defaults) = 0;
        virtual void save() = 0;
        virtual void freeze() = 0;
        virtual void update() = 0;
        virtual void draw_ui() = 0;

        void unfreeze() {
            m_frozen = false;
        }

        bool is_frozen() const {
            return m_frozen;
        }

        std::string get_key_name();

        const auto& get_name() {
            return m_name;
        }

        const auto& get_module() {
            return m_module;
        }

        auto get_type() const {
            return m_type;
        }

    protected:
        void load_internal(const std::string& filename, bool set_defaults);
        void save_internal(const std::string& filename);

        std::wstring m_module{};
        std::wstring m_name{};
        Type m_type{};

        union {
            int m_frozen_int_value{};
            float m_frozen_float_value;
        };

        union {
            int m_min_int_value{};
            float m_min_float_value;
        };

        union {
            int m_max_int_value{};
            float m_max_float_value;
        };

        bool m_frozen{false};
    };

    class CVarStandard : public CVar {
    public:
        CVarStandard(const std::wstring& module, const std::wstring& name, Type type, int min_value, int max_value) 
            : CVar{module, name, type, min_value, max_value}
        {
        }

        CVarStandard(const std::wstring& module, const std::wstring& name, Type type, float min_value, float max_value)
            : CVar{module, name, type, min_value, max_value}
        {
        }

        void load(bool set_defaults) override;
        void save() override;
        void freeze() override;
        void update() override;
        void draw_ui() override;

    protected:
        std::wstring m_frozen_value{};
        sdk::IConsoleVariable** m_cvar{nullptr};
    };

    class CVarData : public CVar {
    public:
        CVarData(const std::wstring& module, const std::wstring& name, Type type, int min_value, int max_value)
            : CVar{module, name, type, min_value, max_value}
        {
        }
        CVarData(const std::wstring& module, const std::wstring& name, Type type, float min_value, float max_value)
            : CVar{module, name, type, min_value, max_value}
        {
        }

        void load(bool set_defaults) override;
        void save() override;
        void freeze() override;
        void update() override;
        void draw_ui() override;

    protected:
        std::optional<sdk::ConsoleVariableDataWrapper> m_cvar_data;
    };

private:
    std::vector<std::shared_ptr<CVar>> m_displayed_cvars{};
    std::vector<std::shared_ptr<CVar>> m_all_cvars{}; // ones the user can manually add to cvars.txt'

    struct AutoComplete {
        sdk::IConsoleObject* object;
        std::string name;
        std::string current_value;
        std::string description;
    };

    struct {
        std::array<char, 256> input_buffer{};
        std::vector<std::string> history{};
        std::string last_parsed_name{};
        std::string last_parsed_buffer{};
        //std::string last_autocomplete_string{};
        std::recursive_mutex autocomplete_mutex{};
        std::vector<AutoComplete> autocomplete{};
        size_t history_index{0};
    } m_console;
    
    bool m_wants_display_console{false};

    static inline std::vector<std::shared_ptr<CVarStandard>> s_default_standard_cvars {
        // Bools
        std::make_unique<CVarStandard>(L"Renderer", L"r.HZBOcclusion", CVar::Type::BOOL, 0, 1),
        std::make_unique<CVarStandard>(L"Renderer", L"r.SSGI.Enable", CVar::Type::BOOL, 0, 1),

        // Ints
        std::make_unique<CVarStandard>(L"Renderer", L"r.DefaultFeature.AntiAliasing", CVar::Type::INT, 0, 2),
        std::make_unique<CVarStandard>(L"Renderer", L"r.TemporalAA.Algorithm", CVar::Type::INT, 0, 1),
        std::make_unique<CVarStandard>(L"Renderer", L"r.TemporalAA.Upsampling", CVar::Type::INT, 0, 1),
        std::make_unique<CVarStandard>(L"Renderer", L"r.Upscale.Quality", CVar::Type::INT, 0, 5),
        std::make_unique<CVarStandard>(L"Renderer", L"r.LightCulling.Quality", CVar::Type::INT, 0, 2),
        std::make_unique<CVarStandard>(L"Renderer", L"r.SubsurfaceScattering", CVar::Type::INT, 0, 2),
        
        // Floats
        std::make_unique<CVarStandard>(L"Renderer", L"r.Upscale.Softness", CVar::Type::FLOAT, 0.0f, 1.0f),
        std::make_unique<CVarStandard>(L"Renderer", L"r.ScreenPercentage", CVar::Type::FLOAT, 10.0f, 150.0f),
    };

    static inline std::vector<std::shared_ptr<CVarData>> s_default_data_cvars {
        // Bools
        std::make_unique<CVarData>(L"Engine", L"r.OneFrameThreadLag", CVar::Type::BOOL, 0, 1),
        std::make_unique<CVarData>(L"Engine", L"r.AllowOcclusionQueries", CVar::Type::BOOL, 0, 1),
        std::make_unique<CVarData>(L"Engine", L"r.VolumetricCloud", CVar::Type::BOOL, 0, 1),

        // Ints
        std::make_unique<CVarData>(L"Engine", L"r.AmbientOcclusionLevels", CVar::Type::INT, 0, 4),
        std::make_unique<CVarData>(L"Engine", L"r.DepthOfFieldQuality", CVar::Type::INT, 0, 2),
        std::make_unique<CVarData>(L"Engine", L"r.MotionBlurQuality", CVar::Type::INT, 0, 4),
        std::make_unique<CVarData>(L"Engine", L"r.SceneColorFringeQuality", CVar::Type::INT, 0, 1),

        // Floats
        std::make_unique<CVarData>(L"Engine", L"r.MotionBlur.Max", CVar::Type::FLOAT, -1.0f, 100.0f),
        std::make_unique<CVarData>(L"Engine", L"r.SceneColorFringe.Max", CVar::Type::FLOAT, -1.0f, 100.0f),
        std::make_unique<CVarData>(L"Renderer", L"r.Color.Min", CVar::Type::FLOAT, -1.0f, 1.0f),
        std::make_unique<CVarData>(L"Renderer", L"r.Color.Mid", CVar::Type::FLOAT, 0.0f, 1.0f),
        std::make_unique<CVarData>(L"Renderer", L"r.Color.Max", CVar::Type::FLOAT, -1.0f, 2.0f),
        std::make_unique<CVarData>(L"Renderer", L"r.Tonemapper.Sharpen", CVar::Type::FLOAT, 0.0f, 10.0f),
        std::make_unique<CVarData>(L"Renderer", L"r.TonemapperGamma", CVar::Type::FLOAT, -5.0f, 5.0f),
    };
};