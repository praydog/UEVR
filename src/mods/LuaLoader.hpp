#pragma once

#include <deque>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "ScriptContext.hpp"
#include "ScriptState.hpp"

#include "Mod.hpp"

using namespace uevr;

class LuaLoader : public Mod {
public:
    static std::shared_ptr<LuaLoader>& get();

    std::string_view get_name() const override { return "LuaLoader"; }
    bool is_advanced_mod() const override { return true; }
    std::optional<std::string> on_initialize_d3d_thread() override;
    void on_draw_ui() override;
    void on_frame() override;

    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;


    const auto& get_state() {
        return m_main_state;
    }

    const auto& get_state(int index) { 
        return m_states[index];
    }

private:
    ScriptState::GarbageCollectionData make_gc_data() const {
        ScriptState::GarbageCollectionData data{};

        data.gc_handler = (decltype(ScriptState::GarbageCollectionData::gc_handler))m_gc_handler->value();
        data.gc_type = (decltype(ScriptState::GarbageCollectionData::gc_type))m_gc_type->value();
        data.gc_mode = (decltype(ScriptState::GarbageCollectionData::gc_mode))m_gc_mode->value();
        data.gc_budget = std::chrono::microseconds{(uint32_t)m_gc_budget->value()};
        data.gc_minor_multiplier = (uint32_t)m_gc_minor_multiplier->value();
        data.gc_major_multiplier = (uint32_t)m_gc_major_multiplier->value();

        return data;
    }

    std::shared_ptr<ScriptState> m_main_state{};
    std::vector<std::shared_ptr<ScriptState>> m_states{};
    std::recursive_mutex m_access_mutex{};

    // A list of Lua files that have been explicitly loaded either through the user manually loading the script, or
    // because the script was in the autorun directory.
    std::vector<std::string> m_loaded_scripts{};
    std::vector<std::string> m_known_scripts{};
    std::unordered_map<std::string, bool> m_loaded_scripts_map{};
    std::vector<lua_State*> m_states_to_delete{};
    std::string m_last_script_error{};
    std::shared_mutex m_script_error_mutex{};
    std::chrono::system_clock::time_point m_last_script_error_time{};

    bool m_console_spawned{false};
    bool m_needs_first_reset{true};

    const ModToggle::Ptr m_log_to_disk{ ModToggle::create(generate_name("LogToDisk"), false) };

    const ModCombo::Ptr m_gc_handler { 
        ModCombo::create(generate_name("GarbageCollectionHandler"),
        {
            "Managed by UEVR",
            "Managed by Lua"
        }, (int)ScriptState::GarbageCollectionHandler::UEVR_MANAGED)
    };

    const ModCombo::Ptr m_gc_type {
        ModCombo::create(generate_name("GarbageCollectionType"),
        {
            "Step",
            "Full",
        }, (int)ScriptState::GarbageCollectionType::STEP)
    };

    const ModCombo::Ptr m_gc_mode {
        ModCombo::create(generate_name("GarbageCollectionMode"),
        {
            "Generational",
            "Incremental (Mark & Sweep)",
        }, (int)ScriptState::GarbageCollectionMode::GENERATIONAL)
    };

    // Garbage collection budget in microseconds.
    const ModSlider::Ptr m_gc_budget {
        ModSlider::create(generate_name("GarbageCollectionBudget"), 0.0f, 2000.0f, 1000.0f)
    };

    const ModSlider::Ptr m_gc_minor_multiplier {
        ModSlider::create(generate_name("GarbageCollectionMinorMultiplier"), 1.0f, 200.0f, 1.0f)
    };

    const ModSlider::Ptr m_gc_major_multiplier {
        ModSlider::create(generate_name("GarbageCollectionMajorMultiplier"), 1.0f, 1000.0f, 100.0f)
    };

    ValueList m_options{
        *m_log_to_disk,
        *m_gc_handler,
        *m_gc_type,
        *m_gc_mode,
        *m_gc_budget,
        *m_gc_minor_multiplier,
        *m_gc_major_multiplier
    };

    // Resets the ScriptState and runs autorun scripts again.
    void reset_scripts();
};