#include <cstdint>
#include <filesystem>

#include "Framework.hpp"
#include "PluginLoader.hpp"
#include "LuaLoader.hpp"

#include <sdk/threading/GameThreadWorker.hpp>

#include <lstate.h> // weird include order because of sol
#include <lgc.h>

std::shared_ptr<LuaLoader>& LuaLoader::get() {
    static auto instance = std::make_shared<LuaLoader>();
    return instance;
}

std::optional<std::string> LuaLoader::on_initialize_d3d_thread() {
    // TODO?
    return Mod::on_initialize_d3d_thread();
}

void LuaLoader::on_config_load(const utility::Config& cfg, bool set_defaults) {
    std::scoped_lock _{m_access_mutex};

    for (IModValue& option : m_options) {
        option.config_load(cfg, set_defaults);
    }

    if (m_main_state != nullptr) {
        m_main_state->gc_data_changed(make_gc_data());
    }
}

void LuaLoader::on_config_save(utility::Config& cfg) {
    std::scoped_lock _{m_access_mutex};

    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }


    // TODO: Add config save callback to ScriptState
    if (m_main_state != nullptr) {
        //m_main_state->on_config_save();
    }
}

void LuaLoader::on_frame() {
    // Only run on the game thread
    // on_frame can sometimes run in the DXGI thread, this happens
    // before tick is hooked, which is where the game thread is.
    // once tick is hooked, on_frame will always run on the game thread.
    if (!GameThreadWorker::get().is_same_thread()) {
        return;
    }

    std::scoped_lock _{m_access_mutex};

    if (m_needs_first_reset) {
        spdlog::info("[LuaLoader] Initializing Lua state for the first time...");

        // Calling reset_scripts even though the scripts have never been set yet still works.
        reset_scripts();
        m_needs_first_reset = false;

        spdlog::info("[LuaLoader] Lua state initialized.");
    }

    for (auto state_to_delete : m_states_to_delete) {
        std::erase_if(m_states, [&](std::shared_ptr<ScriptState> state) { return state->lua().lua_state() == state_to_delete; });
    }

    m_states_to_delete.clear();

    if (m_main_state == nullptr) {
        return;
    }

    for (auto &state : m_states) {
        state->on_frame();
    }
}

void LuaLoader::on_draw_sidebar_entry(std::string_view in_entry) {
    if (in_entry == "Main") {
        if (ImGui::Button("Run script")) {
            OPENFILENAME ofn{};
            char file[260]{};

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_framework->get_window();
            ofn.lpstrFile = file;
            ofn.nMaxFile = sizeof(file);
            ofn.lpstrFilter = "Lua script files (*.lua)\0*.lua\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

            if (GetOpenFileName(&ofn) != FALSE) {
                std::scoped_lock _{ m_access_mutex };
                m_main_state->run_script(file);
                m_loaded_scripts.emplace_back(std::filesystem::path{file}.filename().string());
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset scripts")) {
            reset_scripts();
        }

        ImGui::SameLine();

        if (ImGui::Button("Spawn Debug Console")) {
            if (!m_console_spawned) {
                AllocConsole();
                freopen("CONIN$", "r", stdin);
                freopen("CONOUT$", "w", stdout);
                freopen("CONOUT$", "w", stderr);

                m_console_spawned = true;
            }
        }

        //Garbage collection currently only showing from main lua state, might rework to show total later?
        if (ImGui::TreeNode("Garbage Collection Stats")) {
            std::scoped_lock _{ m_access_mutex };

            auto g = G(m_main_state->lua().lua_state());
            const auto bytes_in_use = g->totalbytes + g->GCdebt;

            ImGui::Text("Megabytes in use: %.2f", (float)bytes_in_use / 1024.0f / 1024.0f);

            ImGui::TreePop();
        }

        if (m_gc_handler->draw("Garbage Collection Handler")) {
            std::scoped_lock _{ m_access_mutex };
            m_main_state->gc_data_changed(make_gc_data());
        }

        if (m_gc_mode->draw("Garbage Collection Mode")) {
            std::scoped_lock _{ m_access_mutex };
            m_main_state->gc_data_changed(make_gc_data());
        }

        if ((uint32_t)m_gc_mode->value() == (uint32_t)ScriptState::GarbageCollectionMode::GENERATIONAL) {
            if (m_gc_minor_multiplier->draw("Minor GC Multiplier")) {
                std::scoped_lock _{ m_access_mutex };
                m_main_state->gc_data_changed(make_gc_data());
            }

            if (m_gc_major_multiplier->draw("Major GC Multiplier")) {
                std::scoped_lock _{ m_access_mutex };
                m_main_state->gc_data_changed(make_gc_data());
            }
        }

        if (m_gc_handler->value() == (int32_t)ScriptState::GarbageCollectionHandler::UEVR_MANAGED) {
            if (m_gc_type->draw("Garbage Collection Type")) {
                std::scoped_lock _{ m_access_mutex };
                m_main_state->gc_data_changed(make_gc_data());
            }

            if ((uint32_t)m_gc_mode->value() != (uint32_t)ScriptState::GarbageCollectionMode::GENERATIONAL) {
                if (m_gc_budget->draw("Garbage Collection Budget")) {
                    std::scoped_lock _{ m_access_mutex };
                    m_main_state->gc_data_changed(make_gc_data());
                }
            }
        }

        m_log_to_disk->draw("Log Lua Errors to Disk");

        auto last_script_error = m_main_state != nullptr ? m_main_state->get_last_script_error() : std::nullopt;

        if (last_script_error.has_value() && !last_script_error->e.empty()) {
            const auto now = std::chrono::system_clock::now();
            const auto diff = now - last_script_error->t;
            const auto sec = std::chrono::duration<float>(diff).count();

            ImGui::TextWrapped("Last Error Time: %.2f seconds ago", sec);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::TextWrapped("Last Script Error: %s", last_script_error->e.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::TextWrapped("No Script Errors... yet!");
        }

        if (!m_known_scripts.empty()) {
            ImGui::Text("Known scripts:");

            for (auto&& name : m_known_scripts) {
                if (ImGui::Checkbox(name.data(), &m_loaded_scripts_map[name])) {
                    reset_scripts();
                    break;
                }
            }
        } else {
            ImGui::Text("No scripts loaded.");
        }

        ImGui::TreePop();
    }


    if (in_entry == "Script UI") {
        std::scoped_lock _{m_access_mutex};

        if (m_states.empty()) {
            return;
        }

        for (auto& state : m_states) {
            state->on_draw_ui();
        }

        ImGui::TreePop();
    }
}

void LuaLoader::reset_scripts() {
    spdlog::info("[LuaLoader] Resetting scripts...");

    std::scoped_lock _{ m_access_mutex };

    if (m_main_state != nullptr) {
        /*auto& mods = g_framework->get_mods()->get_mods();

        for (auto& mod : mods) {
            mod->on_lua_state_destroyed(m_main_state->lua());
        }*/

        m_main_state->on_script_reset();
    }

    m_main_state.reset();
    m_states.clear();

    spdlog::info("[LuaLoader] Destroyed all Lua states.");

    m_main_state = std::make_shared<ScriptState>(make_gc_data(), &g_plugin_initialize_param, true);
    m_states.insert(m_states.begin(), m_main_state);

    //callback functions for main lua state creation
    /*auto& mods = g_framework->get_mods()->get_mods();
    for (auto& mod : mods) {
        mod->on_lua_state_created(m_main_state->lua());
    }*/

    m_loaded_scripts.clear();
    m_known_scripts.clear();

    const auto autorun_path = Framework::get_persistent_dir() / "scripts";
    const auto global_autorun_path = Framework::get_persistent_dir()  / ".." / "UEVR" / "scripts";

    spdlog::info("[LuaLoader] Creating directories {}", autorun_path.string());
    std::filesystem::create_directories(autorun_path);
    spdlog::info("[LuaLoader] Loading scripts...");
    namespace fs = std::filesystem;

	auto load_scripts_from_dir = [this](std::filesystem::path path) {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            return;
        }

		for (auto&& entry : std::filesystem::directory_iterator{path}) {
			auto&& path = entry.path();

			if (path.has_extension() && path.extension() == ".lua") {
				if (!m_loaded_scripts_map.contains(path.filename().string())) {
					m_loaded_scripts_map.emplace(path.filename().string(), true);
				}

				if (m_loaded_scripts_map[path.filename().string()] == true) {
					m_main_state->run_script(path.string());
					m_loaded_scripts.emplace_back(path.filename().string());
				}

				m_known_scripts.emplace_back(path.filename().string());
			}
		}
	};

    load_scripts_from_dir(global_autorun_path);
    load_scripts_from_dir(autorun_path);
	
    std::sort(m_known_scripts.begin(), m_known_scripts.end());
    std::sort(m_loaded_scripts.begin(), m_loaded_scripts.end());
}

void LuaLoader::dispatch_event(std::string_view event_name, std::string_view event_data) {
    std::scoped_lock _{m_access_mutex};

    if (m_main_state == nullptr) {
        return;
    }

    m_main_state->dispatch_event(event_name, event_data);
}