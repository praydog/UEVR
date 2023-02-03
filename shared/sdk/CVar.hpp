#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include <windows.h>

namespace sdk {
template <typename T>
struct TConsoleVariableData {
    void set(T value) {
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
};

struct IConsoleVariable : IConsoleObject {
    void Set(const wchar_t* in, uint32_t set_by_flags = 0x8000000);
    int32_t GetInt();
    float GetFloat();

private:
    void locate_vtable_indices();
    static inline std::optional<uint32_t> s_set_vtable_index{};
    static inline std::optional<uint32_t> s_get_int_vtable_index{};
    static inline std::optional<uint32_t> s_get_float_vtable_index{};
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

namespace rendering {
std::optional<ConsoleVariableDataWrapper> get_one_frame_thread_lag_cvar();
}

namespace vr {
std::optional<ConsoleVariableDataWrapper> get_enable_stereo_emulation_cvar();
std::optional<ConsoleVariableDataWrapper> get_slate_draw_to_vr_render_target_real_cvar();
std::optional<uintptr_t> get_slate_draw_to_vr_render_target_usage_location();
}
}