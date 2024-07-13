#pragma once

#include <chrono>
#include <iostream>
#include <memory>

#include <sol/sol.hpp>
#include <uevr/API.hpp>

#include <vector>

#include "ScriptContext.hpp"

namespace uevr {
class ScriptState {
public:
    enum class GarbageCollectionHandler : uint32_t {
        UEVR_MANAGED = 0,
        LUA_MANAGED = 1,
        LAST
    };

    enum class GarbageCollectionType : uint32_t {
        STEP = 0,
        FULL = 1,
        LAST
    };

    enum class GarbageCollectionMode : uint32_t {
        GENERATIONAL = 0,
        INCREMENTAL = 1,
        LAST
    };

    struct GarbageCollectionData {
        GarbageCollectionHandler gc_handler{GarbageCollectionHandler::UEVR_MANAGED};
        GarbageCollectionType gc_type{GarbageCollectionType::FULL};
        GarbageCollectionMode gc_mode{GarbageCollectionMode::GENERATIONAL};
        std::chrono::microseconds gc_budget{1000};

        uint32_t gc_minor_multiplier{1};
        uint32_t gc_major_multiplier{100};
    };

    ScriptState(const GarbageCollectionData& gc_data, UEVR_PluginInitializeParam* param, bool is_main_state);
    ~ScriptState();

    void run_script(const std::string& p);
    sol::protected_function_result handle_protected_result(sol::protected_function_result result); // because protected_functions don't throw

    void gc_data_changed(GarbageCollectionData data);
    void on_frame();
    void on_draw_ui();
    void on_script_reset();

    auto& context() {
        return m_context;
    }

    auto& lua() { return m_lua; }

private:
    std::shared_ptr<sol::state> m_lua_impl{std::make_shared<sol::state>()};
    sol::state& m_lua{*m_lua_impl.get()};
    std::shared_ptr<ScriptContext> m_context{nullptr};

    GarbageCollectionData m_gc_data{};
    bool m_is_main_state;
};
}