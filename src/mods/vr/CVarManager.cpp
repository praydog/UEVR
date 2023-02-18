#include <filesystem>

#include <utility/Config.hpp>
#include <utility/String.hpp>

#include <sdk/CVar.hpp>
#include <sdk/threading/GameThreadWorker.hpp>

#include "Framework.hpp"

#include "CVarManager.hpp"

constexpr std::string_view cvars_txt_name = "cvars.txt";

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
    }
}

void CVarManager::on_draw_ui() {
    ImGui::SetNextItemOpen(true, ImGuiCond_::ImGuiCond_Once);
    if (ImGui::TreeNode("CVars")) {
        for (auto& cvar : m_displayed_cvars) {
            cvar->draw_ui();
        }

        ImGui::TreePop();
    }
}

std::string CVarManager::CVar::get_key_name() {
    return std::format("{}_{}", utility::narrow(m_module), utility::narrow(m_name));
}

void CVarManager::CVar::load() {
    const auto cvars_txt = Framework::get_persistent_dir(cvars_txt_name.data());

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
        m_frozen_int_value = std::stoi(value.value());
        break;
    case Type::INT:
        m_frozen_int_value = std::stoi(value.value());
        break;
    case Type::FLOAT:
        m_frozen_float_value = std::stof(value.value());
        break;
    }
}

void CVarManager::CVar::save() {
    const auto cvars_txt = Framework::get_persistent_dir(cvars_txt_name.data());

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
}

void CVarManager::CVarStandard::load() {
    CVar::load();
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

    CVar::save();
}

void CVarManager::CVarStandard::freeze() {
    if (!m_frozen) {
        return;
    }

    // TODO: implement this
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
            GameThreadWorker::get().enqueue([cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
            });
        }
        break;
    }
    case Type::INT: {
        auto value = cvar->GetInt();

        if (ImGui::SliderInt(narrow_name.c_str(), &value, m_min_int_value, m_max_int_value)) {
            GameThreadWorker::get().enqueue([cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
            });
        }
        break;
    }
    case Type::FLOAT: {
        auto value = cvar->GetFloat();

        if (ImGui::SliderFloat(narrow_name.c_str(), &value, m_min_float_value, m_max_float_value)) {
            GameThreadWorker::get().enqueue([cvar, value]() {
                cvar->Set(std::to_wstring(value).c_str());
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

void CVarManager::CVarData::load() {
    CVar::load();
}

void CVarManager::CVarData::save() {
    CVar::save();
}

void CVarManager::CVarData::freeze() {
    if (!m_frozen) {
        return;
    }
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
        }
        break;
    }
    case Type::INT: {
        auto value = cvar_int->get();

        if (ImGui::SliderInt(narrow_name.c_str(), &value, m_min_int_value, m_max_int_value)) {
            cvar_int->set(value); // no need to run on game thread, direct access
        }
        break;
    }
    case Type::FLOAT: {
        auto value = cvar_float->get();

        if (ImGui::SliderFloat(narrow_name.c_str(), &value, m_min_float_value, m_max_float_value)) {
            cvar_float->set(value); // no need to run on game thread, direct access
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