#include <filesystem>

#include <utility/Config.hpp>
#include <utility/String.hpp>

#include <sdk/CVar.hpp>
#include <sdk/threading/GameThreadWorker.hpp>

#include "Framework.hpp"

#include "CVarManager.hpp"

constexpr std::string_view cvars_standard_txt_name = "cvars_standard.txt";
constexpr std::string_view cvars_data_txt_name = "cvars_data.txt";

CVarManager::CVarManager() {
    m_displayed_cvars.insert(m_displayed_cvars.end(), s_default_standard_cvars.begin(), s_default_standard_cvars.end());
    m_displayed_cvars.insert(m_displayed_cvars.end(), s_default_data_cvars.begin(), s_default_data_cvars.end());

    // Sort first by name, then by bool/int/float type. Bools get displayed first.
    std::sort(m_displayed_cvars.begin(), m_displayed_cvars.end(), [](const auto& a, const auto& b) {
        return a->get_name() < b->get_name();
    });

    std::sort(m_displayed_cvars.begin(), m_displayed_cvars.end(), [](const auto& a, const auto& b) {
        return (int)a->get_type() < (int)b->get_type();
    });

    m_all_cvars.insert(m_all_cvars.end(), m_displayed_cvars.begin(), m_displayed_cvars.end());
}

CVarManager::~CVarManager() {
    /*for (auto& cvar : m_cvars) {
        cvar->save();
    }*/
}

void CVarManager::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    for (auto& cvar : m_all_cvars) {
        cvar->update();
        cvar->freeze();
    }
}

void CVarManager::on_draw_ui() {
    ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
    if (ImGui::TreeNode("CVars")) {
        ImGui::TextWrapped("Note: Any changes here will be frozen.");

        uint32_t frozen_cvars = 0;

        for (auto& cvar : m_all_cvars) {
            if (cvar->is_frozen()) {
                ++frozen_cvars;
            }
        }

        ImGui::TextWrapped("Frozen CVars: %i", frozen_cvars);

        if (ImGui::Button("Clear Frozen CVars")) {
            for (auto& cvar : m_all_cvars) {
                cvar->unfreeze();
            }

            const auto cvars_txt = Framework::get_persistent_dir(cvars_standard_txt_name.data());

            try {
                if (std::filesystem::exists(cvars_txt)) {
                    std::filesystem::remove(cvars_txt);
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to remove {}: {}", cvars_standard_txt_name.data(), e.what());
            }

            const auto cvars_data_txt = Framework::get_persistent_dir(cvars_data_txt_name.data());

            try {
                if (std::filesystem::exists(cvars_data_txt)) {
                    std::filesystem::remove(cvars_data_txt);
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to remove {}: {}", cvars_data_txt_name.data(), e.what());
            }
        }
        
        for (auto& cvar : m_displayed_cvars) {
            cvar->draw_ui();
        }

        ImGui::TreePop();
    }
}

void CVarManager::on_config_load(const utility::Config& cfg, bool set_defaults) {
    for (auto& cvar : m_all_cvars) {
        cvar->load(set_defaults);
    }

    // TODO: Add arbitrary cvars from the other configs the user can add.
}

std::string CVarManager::CVar::get_key_name() {
    return std::format("{}_{}", utility::narrow(m_module), utility::narrow(m_name));
}

void CVarManager::CVar::load_internal(const std::string& filename, bool set_defaults) try {
    spdlog::info("[CVarManager] Loading {}...", filename);

    const auto cvars_txt = Framework::get_persistent_dir(filename);

    if (!std::filesystem::exists(cvars_txt)) {
        return;
    }

    auto cfg = utility::Config{cvars_txt.string()};
    auto value = cfg.get(get_key_name());

    if (!value) {
        // No need to freeze.
        return;
    }

    m_frozen = true;
    
    switch (m_type) {
    case Type::BOOL:
        m_frozen_int_value = *cfg.get<int>(get_key_name());
        break;
    case Type::INT:
        m_frozen_int_value = *cfg.get<int>(get_key_name());
        break;
    case Type::FLOAT:
        m_frozen_float_value = *cfg.get<float>(get_key_name());
        break;
    }
} catch(const std::exception& e) {
    spdlog::error("Failed to load {}: {}", filename, e.what());
}

void CVarManager::CVar::save_internal(const std::string& filename) try {
    spdlog::info("[CVarManager] Saving {}...", filename);

    const auto cvars_txt = Framework::get_persistent_dir(filename);

    auto cfg = utility::Config{cvars_txt.string()};

    switch (m_type) {
    case Type::BOOL:
        cfg.set<bool>(get_key_name(), (bool)m_frozen_int_value);
        break;
    case Type::INT:
        cfg.set<int>(get_key_name(), m_frozen_int_value);
        break;
    case Type::FLOAT:
        cfg.set<float>(get_key_name(), m_frozen_float_value);
        break;
    };

    cfg.save(cvars_txt.string());
    m_frozen = true;
} catch (const std::exception& e) {
    spdlog::error("Failed to save {}: {}", filename, e.what());
}

void CVarManager::CVarStandard::load(bool set_defaults) {
    load_internal(cvars_standard_txt_name.data(), set_defaults);
}

void CVarManager::CVarStandard::save() {
    if (m_cvar == nullptr || *m_cvar == nullptr) {
        // CVar not found, don't save.
        return;
    }

    auto cvar = *m_cvar;

    switch (m_type) {
    case Type::BOOL:
        m_frozen_int_value = cvar->GetInt();
        break;
    case Type::INT:
        m_frozen_int_value = cvar->GetInt();
        break;
    case Type::FLOAT:
        m_frozen_float_value = cvar->GetFloat();
        break;
    default:
        break;
    }

    save_internal(cvars_standard_txt_name.data());
}

void CVarManager::CVarStandard::freeze() {
    if (!m_frozen) {
        return;
    }

    if (m_cvar == nullptr || *m_cvar == nullptr) {
        return;
    }

    switch(m_type) {
    case Type::BOOL:
        // Limiting the amount of times Set gets called with string conversions.
        if ((*m_cvar)->GetInt() != m_frozen_int_value) {
            (*m_cvar)->Set(std::to_wstring(m_frozen_int_value).c_str());
        }
        break;
    case Type::INT:
        if ((*m_cvar)->GetInt() != m_frozen_int_value) {
            (*m_cvar)->Set(std::to_wstring(m_frozen_int_value).c_str());
        }
        break;
    case Type::FLOAT:
        if ((*m_cvar)->GetFloat() != m_frozen_float_value) {
            (*m_cvar)->Set(std::to_wstring(m_frozen_float_value).c_str());
        }
        break;
    default:
        break;
    };
}

void CVarManager::CVarStandard::update() {
    if (m_cvar == nullptr) {
        m_cvar = sdk::find_cvar_cached(m_module, m_name);
    }
}

void CVarManager::CVarStandard::draw_ui() try {
    if (m_cvar == nullptr || *m_cvar == nullptr) {
        ImGui::TextWrapped("Failed to find cvar: %s", utility::narrow(m_name).c_str());
        return;
    }

    auto cvar = *m_cvar;
    const auto narrow_name = utility::narrow(m_name);
    
    switch (m_type) {
    case Type::BOOL: {
        auto value = (bool)cvar->GetInt();

        if (ImGui::Checkbox(narrow_name.c_str(), &value)) {
            GameThreadWorker::get().enqueue([sft = shared_from_this(), cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
                sft->save();
            });
        }
        break;
    }
    case Type::INT: {
        auto value = cvar->GetInt();

        if (ImGui::SliderInt(narrow_name.c_str(), &value, m_min_int_value, m_max_int_value)) {
            GameThreadWorker::get().enqueue([sft = shared_from_this(), cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
                sft->save();
            });
        }
        break;
    }
    case Type::FLOAT: {
        auto value = cvar->GetFloat();

        if (ImGui::SliderFloat(narrow_name.c_str(), &value, m_min_float_value, m_max_float_value)) {
            GameThreadWorker::get().enqueue([sft = shared_from_this(), cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
                sft->save();
            });
        }
        break;
    }
    default:
        ImGui::TextWrapped("Unimplemented cvar type: %s", utility::narrow(m_name).c_str());
        break;
    };
} catch(...) {
    ImGui::TextWrapped("Failed to read cvar: %s", utility::narrow(m_name).c_str());
}

void CVarManager::CVarData::load(bool set_defaults) {
    load_internal(cvars_data_txt_name.data(), set_defaults);
}

void CVarManager::CVarData::save() {
    if (!m_cvar_data) {
        return;
    }

    // Points to the same thing, just different data internally.
    auto cvar_int = m_cvar_data->get<int>();
    auto cvar_float = m_cvar_data->get<float>();

    if (cvar_int == nullptr) {
        return;
    }

    switch (m_type) {
    case Type::BOOL:
        m_frozen_int_value = cvar_int->get();
        break;
    case Type::INT:
        m_frozen_int_value = cvar_int->get();
        break;
    case Type::FLOAT:
        m_frozen_float_value = cvar_float->get();
        break;
    default:
        break;
    };

    save_internal(cvars_data_txt_name.data());
}

void CVarManager::CVarData::freeze() {
    if (!m_frozen) {
        return;
    }

    if (!m_cvar_data) {
        return;
    }

    // Points to the same thing, just different data internally.
    auto cvar_int = m_cvar_data->get<int>();
    auto cvar_float = m_cvar_data->get<float>();

    if (cvar_int == nullptr) {
        return;
    }

    switch (m_type) {
    case Type::BOOL:
        cvar_int->set(m_frozen_int_value);
        break;
    case Type::INT:
        cvar_int->set(m_frozen_int_value);
        break;
    case Type::FLOAT:
        cvar_float->set(m_frozen_float_value);
        break;
    default:
        break;
    };
}

void CVarManager::CVarData::update() {
    if (!m_cvar_data) {
        m_cvar_data = sdk::find_cvar_data_cached(m_module, m_name);
    }
}

void CVarManager::CVarData::draw_ui() try {
    if (!m_cvar_data) {
        ImGui::TextWrapped("Failed to find cvar data: %s", utility::narrow(m_name).c_str());
        return;
    }

    // Points to the same thing, just different data internally.
    auto cvar_int = m_cvar_data->get<int>();
    auto cvar_float = m_cvar_data->get<float>();

    if (cvar_int == nullptr) {
        ImGui::TextWrapped("Failed to read cvar data: %s", utility::narrow(m_name).c_str());
        return;
    }

    const auto narrow_name = utility::narrow(m_name);

    switch (m_type) {
    case Type::BOOL: {
        auto value = (bool)cvar_int->get();

        if (ImGui::Checkbox(narrow_name.c_str(), &value)) {
            cvar_int->set((int)value); // no need to run on game thread, direct access
            this->save();
        }
        break;
    }
    case Type::INT: {
        auto value = cvar_int->get();

        if (ImGui::SliderInt(narrow_name.c_str(), &value, m_min_int_value, m_max_int_value)) {
            cvar_int->set(value); // no need to run on game thread, direct access
            this->save();
        }
        break;
    }
    case Type::FLOAT: {
        auto value = cvar_float->get();

        if (ImGui::SliderFloat(narrow_name.c_str(), &value, m_min_float_value, m_max_float_value)) {
            cvar_float->set(value); // no need to run on game thread, direct access
            this->save();
        }
        break;
    }
    default:
        ImGui::TextWrapped("Unimplemented cvar type: %s", narrow_name.c_str());
        break;
    }
} catch (...) {
    ImGui::TextWrapped("Failed to read cvar data: %s", utility::narrow(m_name).c_str());
}