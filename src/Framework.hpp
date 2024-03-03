#pragma once

#include <array>
#include <unordered_set>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <imgui.h>

#include <utility/Address.hpp>
#include <sdk/Math.hpp>
#include <utility/Patch.hpp>

#include <sdk/threading/ThreadWorker.hpp>
#include <mods/vr/d3d12/CommandContext.hpp>

class Mods;

#include "hooks/D3D11Hook.hpp"
#include "hooks/D3D12Hook.hpp"
#include "hooks/WindowsMessageHook.hpp"
#include "hooks/XInputHook.hpp"
#include "hooks/DInputHook.hpp"

class UEVRSharedMemory {
public:
    static inline int MESSAGE_IDENTIFIER = *(int*)"VRMOD";

    enum Command {
        RELOAD_CONFIG = 0,
        CONFIG_SETUP_ACKNOWLEDGED = 1,
    };

public:
    UEVRSharedMemory();

    virtual ~UEVRSharedMemory() {
        UnmapViewOfFile(m_data);
        CloseHandle(m_memory);
    }

    #pragma pack(push, 1)
    struct Data {
        wchar_t path[MAX_PATH]{}; // Path to the game exe
        uint32_t pid{}; // Process ID of the game
        uint32_t main_thread_id{}; // Main thread ID of the game
        uint32_t command_thread_id{}; // Thread ID commands are sent to (via PostThreadMessage)
        bool signal_frontend_config_setup{false};
    };
    #pragma pack(pop)

    Data& data() {
        return *m_data;
    }

private:
    HANDLE m_memory{};
    Data* m_data{};
};

struct SidebarEntryInfo {
    SidebarEntryInfo(std::string_view label, bool advanced) : m_label(label), m_advanced_entry(advanced) {}

    std::string m_label{};
    bool m_advanced_entry{false};
};

// Global facilitator
class Framework {
private:
    void hook_monitor();
    void command_thread();

public:
    Framework(HMODULE framework_module);
    virtual ~Framework();

    auto get_framework_module() const { return m_framework_module; }

    bool is_valid() const { return m_valid; }

    bool is_dx11() const { return m_is_d3d11; }

    bool is_dx12() const { return m_is_d3d12; }

    const auto& get_mods() const { return m_mods; }

    const auto& get_mouse_delta() const { return m_mouse_delta; }
    const auto& get_keyboard_state() const { return m_last_keys; }

    Address get_module() const { return m_game_module; }

    bool is_ready() const { return m_initialized && m_game_data_initialized; }
    bool is_game_data_intialized() const { return m_game_data_initialized; }

    void run_imgui_frame(bool from_present);

    void on_frame_d3d11();
    void on_post_present_d3d11();
    void on_frame_d3d12();
    void on_post_present_d3d12();
    
    void on_reset(uint32_t w, uint32_t h);

    void activate_window();
    void set_mouse_to_center();
    void patch_set_cursor_pos();
    void remove_set_cursor_pos_patch();
    void post_message(UINT message, WPARAM w_param, LPARAM l_param);
    bool on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param);

    void on_frontend_command(UEVRSharedMemory::Command command);
    void on_direct_input_keys(const std::array<uint8_t, 256>& keys);

    static std::filesystem::path get_persistent_dir();
    static std::filesystem::path get_persistent_dir(const std::string& dir) {
        return get_persistent_dir() / dir;
    }

    void save_config();
    void reset_config();

    enum class RendererType : uint8_t {
        D3D11,
        D3D12
    };
    
    auto get_renderer_type() const { return m_renderer_type; }
    auto& get_d3d11_hook() const { return m_d3d11_hook; }
    auto& get_d3d12_hook() const { return m_d3d12_hook; }

    auto get_window() const { return m_wnd; }
    auto get_last_window_pos() const { return m_last_window_pos; } // Framework imgui window
    auto get_last_window_size() const { return m_last_window_size; } // Framework imgui window

    bool is_drawing_ui() const {
        return m_draw_ui;
    }

    bool is_drawing_anything() const;

    void set_draw_ui(bool state, bool should_save = true);

    auto& get_hook_monitor_mutex() {
        return m_hook_monitor_mutex;
    }

    void set_font_size(int size) { 
        if (m_font_size != size) {
            m_font_size = size;
            m_fonts_need_updating = true;
        }
    }

    auto get_font_size() const { return m_font_size; }

    int add_font(const std::filesystem::path& filepath, int size, const std::vector<ImWchar>& ranges = {});

    ImFont* get_font(int index) const {
        if (index >= 0 && index < m_additional_fonts.size()) {
            return m_additional_fonts[index].font;
        }
        
        return nullptr;
    }

    Vector2f get_d3d11_rt_size() const {
        return m_last_rt_size;
    }

    Vector2f get_d3d12_rt_size() const {
        return m_last_rt_size;
    }

    Vector2f get_rt_size() const {
        if (get_renderer_type() == RendererType::D3D11) {
            return get_d3d11_rt_size();
        }

        return get_d3d12_rt_size();
    }

    auto& get_frame_worker() {
        return m_frame_worker;
    }

    auto& get_shared_memory() {
        return m_uevr_shared_memory;
    }

    void enable_engine_thread() {
        m_has_engine_thread = true;
    }

    void increment_sidebar_page() {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = now - m_last_page_inc_time;
        if (delta < std::chrono::milliseconds(100)) {
            return;
        }

        ++m_sidebar_state.selected_entry;
        m_last_page_inc_time = now;
    }

    void decrement_sidebar_page() {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = now - m_last_page_dec_time;
        if (delta < std::chrono::milliseconds(100)) {
            return;
        }

        --m_sidebar_state.selected_entry;
        m_last_page_dec_time = now;
    }

    bool is_advanced_view_enabled() const;

    enum ImGuiThemes : int8_t {
        DEFAULT_DARK,
        ALTERNATIVE_DARK,
        DEFAULT_LIGHT,
        HIGH_CONTRAST,
    };
    
    ImGuiThemes get_imgui_theme_value() const;

private:
    void consume_input();
    void update_fonts();
    void invalidate_device_objects();

private:
    void draw_ui();
    void draw_about();

    bool hook_d3d11();
    bool hook_d3d12();

    bool initialize();
    bool initialize_windows_message_hook();
    bool initialize_xinput_hook();
    bool initialize_dinput_hook();

    bool first_frame_initialize();

    void call_on_frame();

    HMODULE m_framework_module{};

    bool m_first_frame{true};
    bool m_first_frame_d3d_initialize{true};
    bool m_is_d3d12{false};
    bool m_is_d3d11{false};
    bool m_valid{false};
    bool m_initialized{false};
    bool m_created_default_cfg{false};
    std::atomic<bool> m_terminating{false};
    std::atomic<bool> m_game_data_initialized{false};
    std::atomic<bool> m_mods_fully_initialized{false};
    
    // UI
    bool m_has_frame{false};
    bool m_wants_device_object_cleanup{false};
    bool m_draw_ui{true};
    bool m_last_draw_ui{m_draw_ui};
    bool m_is_ui_focused{false};
    bool m_cursor_state{false};
    bool m_cursor_state_changed{true};
    bool m_ui_option_transparent{true};
    bool m_ui_passthrough{false};
    
    ImVec2 m_last_window_pos{};
    ImVec2 m_last_window_size{};
    Vector2f m_last_rt_size{1920, 1080};

    ImGuiThemes m_current_theme;

    struct AdditionalFont {
        std::filesystem::path filepath{};
        int size{16};
        std::vector<ImWchar> ranges{};
        ImFont* font{};
    };

    bool m_fonts_need_updating{true};
    int m_font_size{16};
    std::vector<AdditionalFont> m_additional_fonts{};

    std::recursive_mutex m_input_mutex{};
    std::recursive_mutex m_config_mtx{};
    std::recursive_mutex m_imgui_mtx{};
    std::recursive_mutex m_patch_mtx{};

    HWND m_wnd{0};
    HMODULE m_game_module{0};

    float m_accumulated_mouse_delta[2]{};
    float m_mouse_delta[2]{};
    std::array<uint8_t, 256> m_last_keys{0};
    std::unique_ptr<D3D11Hook> m_d3d11_hook{};
    std::unique_ptr<D3D12Hook> m_d3d12_hook{};
    std::unique_ptr<WindowsMessageHook> m_windows_message_hook{};
    std::unique_ptr<XInputHook> m_xinput_hook{};
    std::unique_ptr<DInputHook> m_dinput_hook{};
    std::shared_ptr<spdlog::logger> m_logger{};
    std::unique_ptr<UEVRSharedMemory> m_uevr_shared_memory{};
    Patch::Ptr m_set_cursor_pos_patch{};

    std::unique_ptr<ThreadWorker<void>> m_frame_worker{ std::make_unique<ThreadWorker<void>>() };

    std::string m_error{""};

    // Game-specific stuff
    std::unique_ptr<Mods> m_mods;

    std::recursive_mutex m_hook_monitor_mutex{};
    std::recursive_mutex m_constructor_mutex{};
    std::unique_ptr<std::jthread> m_d3d_monitor_thread{};
    std::unique_ptr<std::jthread> m_command_thread{};
    std::chrono::steady_clock::time_point m_last_present_time{};
    std::chrono::steady_clock::time_point m_last_message_time{};
    std::chrono::steady_clock::time_point m_last_sendmessage_time{};
    std::chrono::steady_clock::time_point m_last_chance_time{};
    std::chrono::steady_clock::time_point m_last_page_dec_time{};
    std::chrono::steady_clock::time_point m_last_page_inc_time{};
    uint32_t m_frames_since_init{0};
    bool m_has_last_chance{true};
    bool m_first_initialize{true};

    bool m_sent_message{false};
    bool m_message_hook_requested{false};
    bool m_has_engine_thread{false};

    RendererType m_renderer_type{RendererType::D3D11};

    struct {
        int32_t selected_entry{0};
        bool initialized{false};

        std::vector<SidebarEntryInfo> entries{};
    } m_sidebar_state{};

    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private: // D3D misc
    void set_imgui_style() noexcept;

private: // D3D11 Init
    bool init_d3d11();
    void deinit_d3d11();

private: // D3D12 Init
    bool init_d3d12();
    void deinit_d3d12();

private: // D3D11 members
    struct D3D11 {
        ComPtr<ID3D11Texture2D> blank_rt{};
		ComPtr<ID3D11Texture2D> rt{};
        ComPtr<ID3D11RenderTargetView> blank_rt_rtv{};
		ComPtr<ID3D11RenderTargetView> rt_rtv{};
		ComPtr<ID3D11ShaderResourceView> rt_srv{};
        uint32_t rt_width{};
        uint32_t rt_height{};
		ComPtr<ID3D11RenderTargetView> bb_rtv{};
    } m_d3d11{};

public:
    auto& get_blank_rendertarget_d3d11() { return m_d3d11.blank_rt; }
    auto& get_rendertarget_d3d11() { return m_d3d11.rt; }
    auto get_rendertarget_width_d3d11() const { return m_d3d11.rt_width; }
    auto get_rendertarget_height_d3d11() const { return m_d3d11.rt_height; }

private: // D3D12 members
    struct D3D12 {
        std::vector<std::unique_ptr<d3d12::CommandContext>> cmd_ctxs{};
        uint32_t cmd_ctx_index{0};

        enum class RTV : int{
            BACKBUFFER_0,
            BACKBUFFER_1,
            BACKBUFFER_2,
            BACKBUFFER_3,
            IMGUI,
            BLANK,
            COUNT,
        };

        enum class SRV : int {
            IMGUI_FONT_BACKBUFFER,
            IMGUI_FONT_VR,
            IMGUI_VR,
            BLANK,
            COUNT
        };

        ComPtr<ID3D12DescriptorHeap> rtv_desc_heap{};
        ComPtr<ID3D12DescriptorHeap> srv_desc_heap{};
        ComPtr<ID3D12Resource> rts[(int)RTV::COUNT]{};

        auto& get_rt(RTV rtv) { return rts[(int)rtv]; }

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_rtv(ID3D12Device* device, RTV rtv) {
            return {rtv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                    (SIZE_T)rtv * (SIZE_T)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_srv(ID3D12Device* device, SRV srv) {
            return {srv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                    (SIZE_T)srv * (SIZE_T)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_srv(ID3D12Device* device, SRV srv) {
            return {srv_desc_heap->GetGPUDescriptorHandleForHeapStart().ptr +
                    (SIZE_T)srv * (SIZE_T)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        }

        uint32_t rt_width{};
        uint32_t rt_height{};

        std::array<void*, 2> imgui_backend_datas{};
    } m_d3d12{};

public:
    auto& get_blank_rendertarget_d3d12() { return m_d3d12.get_rt(D3D12::RTV::BLANK); }
    auto& get_rendertarget_d3d12() { return m_d3d12.get_rt(D3D12::RTV::IMGUI); }
    auto get_rendertarget_width_d3d12() { return m_d3d12.rt_width; }
    auto get_rendertarget_height_d3d12() { return m_d3d12.rt_height; }

private:
};

extern std::unique_ptr<Framework> g_framework;
