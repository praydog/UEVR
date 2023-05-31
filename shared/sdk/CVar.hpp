#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <mutex>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <windows.h>

namespace sdk {
template <typename T>
struct TConsoleVariableData {
    static inline std::recursive_mutex s_mutex{};
    static inline std::unordered_map<TConsoleVariableData*, bool> s_valid_states{};

    void set(T value) {
        {
            std::scoped_lock _{s_mutex};

            if (!s_valid_states.contains(this)) {
                // What we're doing here is double checking that this cvar "data" does not actually point to a vtable or something
                // because if it does, we will unintentionally corrupt the memory
                const auto is_not_vtable = IsBadReadPtr(*(void**)this, sizeof(void*)) == TRUE;
                s_valid_states[this] = is_not_vtable
                                        || (*(uint32_t*)this <= 8 && *(uint32_t*)((uintptr_t)this + 4) <= 8);

                if (!s_valid_states[this]) {
                    spdlog::error("TConsoleVariableData::set: Prevented corruption of memory at {:x}", (uintptr_t)this);
                }
            }

            if (!s_valid_states[this]) {
                return;
            }
        }

        this->values[0] = value;
        this->values[1] = value;
    }

    T get(int index = 0) const {
        if (index >= 2 || index < 0) {
            throw std::out_of_range("index out of range");
        }

        return this->values[index];
    }

    T values[2];
};

// Dummy interface for IConsoleObject
// The functions will actually dynamically scan the vtable for the right index
struct IConsoleObject {
    virtual ~IConsoleObject() {}
    virtual wchar_t* GetHelp() const = 0;
};

struct IConsoleVariable : IConsoleObject {
    void Set(const wchar_t* in, uint32_t set_by_flags = 0x8000000);
    int32_t GetInt();
    float GetFloat();

private:
    struct VtableInfo {
        uint32_t set_vtable_index;
        uint32_t get_int_vtable_index;
        uint32_t get_float_vtable_index;
    };

    std::optional<VtableInfo> locate_vtable_indices();

    static inline std::recursive_mutex s_vtable_mutex{};
    static inline std::unordered_map<void*, VtableInfo> s_vtable_infos{};
};

struct FConsoleVariableBase : public IConsoleVariable {
    struct {
        wchar_t* data;
        uint32_t size;
        uint32_t capacity;
    } help_string;

    uint32_t flags;
    void* on_changed_callback;
};

template<typename T>
struct FConsoleVariable : public FConsoleVariableBase {
    TConsoleVariableData<T> data;
};

class ConsoleVariableDataWrapper {
public:
    template<typename T>
    TConsoleVariableData<T>* get() {
        if (this->m_cvar == nullptr || *this->m_cvar == nullptr) {
            return nullptr;
        }

        return *(TConsoleVariableData<T>**)this->m_cvar;
    }

    template<typename T>
    void set(T value) {
        if (this->m_cvar == nullptr || *this->m_cvar == nullptr) {
            return;
        }

        auto data = *(TConsoleVariableData<T>**)this->m_cvar;
        data->set(value);
    }

    ConsoleVariableDataWrapper(uintptr_t address)
        : m_cvar{ (void**)address }
    {

    }

    uintptr_t address() const {
        return (uintptr_t)m_cvar;
    }

private:
    void** m_cvar;
};

// In some games, likely due to obfuscation, the cvar description is missing
// so we must do an alternative scan for the cvar name itself, which is a bit tougher
// because the cvar name is usually referenced in multiple places, whereas
// the description is only referenced once, in the cvar registration function
std::optional<uintptr_t> find_alternate_cvar_ref(std::wstring_view str, uint32_t known_default, HMODULE module);

std::optional<uintptr_t> resolve_cvar_from_address(uintptr_t start, std::wstring_view str, bool stop_at_first_mov = false);
std::optional<uintptr_t> find_cvar_by_description(std::wstring_view str, std::wstring_view cvar_name, uint32_t known_default, HMODULE module, bool stop_at_first_mov = false);

std::optional<ConsoleVariableDataWrapper> find_cvar_data(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);
IConsoleVariable** find_cvar(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);

// Cached versions of the above functions
std::optional<ConsoleVariableDataWrapper> find_cvar_data_cached(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);
IConsoleVariable** find_cvar_cached(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);

// Cached setters
bool set_cvar_data_int(std::wstring_view module, std::wstring_view name, int value, bool stop_at_first_mov = false);
bool set_cvar_data_float(std::wstring_view module, std::wstring_view name, float value, bool stop_at_first_mov = false);
bool set_cvar_int(std::wstring_view module, std::wstring_view name, int value, bool stop_at_first_mov = false);
bool set_cvar_float(std::wstring_view module, std::wstring_view name, float value, bool stop_at_first_mov = false);

std::optional<int> get_cvar_int(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);
std::optional<float> get_cvar_float(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);

namespace rendering {
std::optional<ConsoleVariableDataWrapper> get_one_frame_thread_lag_cvar();
}

namespace vr {
std::optional<ConsoleVariableDataWrapper> get_enable_stereo_emulation_cvar();
std::optional<ConsoleVariableDataWrapper> get_slate_draw_to_vr_render_target_real_cvar();
std::optional<uintptr_t> get_slate_draw_to_vr_render_target_usage_location();
}
}