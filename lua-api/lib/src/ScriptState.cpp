#include <format>

#include <windows.h>

#include "uevr/API.hpp"
#include "ScriptState.hpp"

namespace api::ue {
void msg(const char* text) {
    MessageBoxA(GetForegroundWindow(), text, "LuaLoader Message", MB_ICONINFORMATION | MB_OK);
}
}

namespace uevr {
ScriptState::ScriptState(const ScriptState::GarbageCollectionData& gc_data, UEVR_PluginInitializeParam* param, bool is_main_state)
{
    if (param != nullptr) {
        uevr::API::initialize(param);
    }

    if (param != nullptr && param->functions != nullptr) {
        param->functions->log_info("Creating new ScriptState...");
    }
    
    m_is_main_state = is_main_state;
    m_lua.registry()["uevr_state"] = this;
    m_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::math, sol::lib::table, sol::lib::bit32,
        sol::lib::utf8, sol::lib::os, sol::lib::coroutine);

    // Disable garbage collection. We will manually do it at the end of each frame.
    gc_data_changed(gc_data);
    
    // Restrict os library
    auto os = m_lua["os"];
    os["remove"] = sol::nil;
    os["rename"] = sol::nil;
    os["execute"] = sol::nil;
    os["exit"] = sol::nil;
    os["setlocale"] = sol::nil;
    os["getenv"] = sol::nil;

    // TODO: Make this actually support multiple states
    // This stores a global reference to itself, meaning it doesn't support multiple states
    // We pass along the shared_ptr (impl) so the context can keep it alive if for some reason the state is destroyed before the context
    m_context = ScriptContext::create(m_lua_impl, param);

    if (!m_context->valid()) {
        if (param != nullptr && param->functions != nullptr) {
            param->functions->log_error("Failed to create new ScriptState!");
        }

        return;
    }

    const auto result = m_context->setup_bindings();

    if (result != 0) {
        const auto table = sol::stack::pop<sol::table>(m_lua);
        m_lua["uevr"] = table;
    }
}

ScriptState::~ScriptState() {

}

void ScriptState::run_script(const std::string& p) {
    uevr::API::get()->log_info(std::format("Running script {}...", p).c_str());

    std::string old_package_path = m_lua["package"]["path"];
    std::string old_cpath = m_lua["package"]["cpath"];

    try {
        auto path = std::filesystem::path(p);
        auto dir = path.parent_path();

        std::string package_path = m_lua["package"]["path"];
        std::string cpath = m_lua["package"]["cpath"];

        package_path = old_package_path + ";" + dir.string() + "/?.lua";
        package_path = package_path + ";" + dir.string() + "/?/init.lua";
        //package_path = package_path + ";" + dir.string() + "/?.dll";

        cpath = old_cpath + ";" + dir.string() + "/?.dll";

        m_lua["package"]["path"] = package_path;
        m_lua["package"]["cpath"] = cpath;
        m_lua.safe_script_file(p);
    } catch (const std::exception& e) {
        //LuaLoader::get()->spew_error(e.what());
        api::ue::msg(e.what());
    } catch (...) {
        //LuaLoader::get()->spew_error((std::stringstream{} << "Unknown error when running script " << p).str());
        api::ue::msg((std::stringstream{} << "Unknown error when running script " << p).str().c_str());
    }

    m_lua["package"]["path"] = old_package_path;
    m_lua["package"]["cpath"] = old_cpath;
}

void ScriptState::gc_data_changed(GarbageCollectionData data) {
    // Handler
    switch (data.gc_handler) {
    case ScriptState::GarbageCollectionHandler::UEVR_MANAGED:
        lua_gc(m_lua, LUA_GCSTOP);
        break;
    case ScriptState::GarbageCollectionHandler::LUA_MANAGED:
        lua_gc(m_lua, LUA_GCRESTART);
        break;
    default:
        lua_gc(m_lua, LUA_GCRESTART);
        data.gc_handler = ScriptState::GarbageCollectionHandler::LUA_MANAGED;
        break;
    }

    // Type 
    if (data.gc_type >= ScriptState::GarbageCollectionType::LAST) {
       data.gc_type = ScriptState::GarbageCollectionType::STEP;
    }

    // Mode
    if (data.gc_mode >= ScriptState::GarbageCollectionMode::LAST) {
        data.gc_mode = ScriptState::GarbageCollectionMode::GENERATIONAL;
    }

    switch (data.gc_mode) {
    case ScriptState::GarbageCollectionMode::GENERATIONAL:
        lua_gc(m_lua, LUA_GCGEN, data.gc_minor_multiplier, data.gc_major_multiplier);
        break;
    case ScriptState::GarbageCollectionMode::INCREMENTAL:
        lua_gc(m_lua, LUA_GCINC);
        break;
    default:
        lua_gc(m_lua, LUA_GCGEN, data.gc_minor_multiplier, data.gc_major_multiplier);
        data.gc_mode = ScriptState::GarbageCollectionMode::GENERATIONAL;
        break;
    }

    m_gc_data = data;
}

void ScriptState::on_script_reset() {
    if (m_context == nullptr) {
        return;
    }

    m_context->script_reset();
}

void ScriptState::on_frame() {
    if (m_context != nullptr) {
        m_context->frame();
    }

    if (m_gc_data.gc_handler != ScriptState::GarbageCollectionHandler::UEVR_MANAGED) {
        return;
    }

    auto _ = std::scoped_lock{m_context->get_mutex()};

    switch (m_gc_data.gc_type) {
        case ScriptState::GarbageCollectionType::FULL:
            lua_gc(m_lua, LUA_GCCOLLECT);
            break;
        case ScriptState::GarbageCollectionType::STEP: 
            {
                const auto now = std::chrono::high_resolution_clock::now();

                if (m_gc_data.gc_mode == ScriptState::GarbageCollectionMode::GENERATIONAL) {
                    lua_gc(m_lua, LUA_GCSTEP, 1);
                } else {
                    while (lua_gc(m_lua, LUA_GCSTEP, 1) == 0) {
                        if (std::chrono::high_resolution_clock::now() - now >= m_gc_data.gc_budget) {
                            break;
                        }
                    }
                }
            }
            break;
        default:
            lua_gc(m_lua, LUA_GCCOLLECT);
            break;
    };
}

void ScriptState::on_draw_ui() {
    if (m_context != nullptr) {
        m_context->draw_ui();
    }
}

void ScriptState::dispatch_event(std::string_view event_name, std::string_view event_data) {
    if (m_context != nullptr) {
        m_context->dispatch_event(event_name, event_data);
    }
}
}