// Install DebugView to view the OutputDebugString calls
#include <sstream>
#include <mutex>
#include <memory>

#include <Windows.h>

// only really necessary if you want to render to the screen
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

#include "rendering/d3d11.hpp"
#include "rendering/d3d12.hpp"

#include "uevr/Plugin.hpp"

using namespace uevr;

class ExamplePlugin : public uevr::Plugin {
public:
    ExamplePlugin() = default;

    void on_dllmain() override {}

    void on_initialize() override {
        ImGui::CreateContext();

        API::get()->log_error("%s %s", "Hello", "error");
        API::get()->log_warn("%s %s", "Hello", "warning");
        API::get()->log_info("%s %s", "Hello", "info");
    }

    void on_present() override {
        if (!m_initialized) {
            if (!initialize_imgui()) {
                OutputDebugString("Failed to initialize imgui");
                return;
            } else {
                OutputDebugString("Initialized imgui");
            }
        }

        const auto renderer_data = API::get()->param()->renderer;

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            internal_frame();

            ImGui::EndFrame();
            ImGui::Render();

            g_d3d11.render_imgui();
        } else if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            auto command_queue = (ID3D12CommandQueue*)renderer_data->command_queue;

            if (command_queue == nullptr ){
                return;
            }

            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            internal_frame();

            ImGui::EndFrame();
            ImGui::Render();

            g_d3d12.render_imgui();
        }
    }

    void on_device_reset() override {
        OutputDebugString("Example Device Reset");

        const auto renderer_data = API::get()->param()->renderer;

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            ImGui_ImplDX11_Shutdown();
            g_d3d11 = {};
        }

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            ImGui_ImplDX12_Shutdown();
            g_d3d12 = {};
        }

        m_initialized = false;
    }

    bool on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override { 
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

        return !ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard;
    }

    void on_engine_tick(UEVR_UGameEngineHandle engine, float delta) override {
    }

    void on_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override {
    }

private:
    bool initialize_imgui() {
        if (m_initialized) {
            return true;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::GetIO().IniFilename = "example_dll_ui.ini";

        const auto renderer_data = API::get()->param()->renderer;

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        auto swapchain = (IDXGISwapChain*)renderer_data->swapchain;
        swapchain->GetDesc(&swap_desc);

        m_wnd = swap_desc.OutputWindow;

        if (!ImGui_ImplWin32_Init(m_wnd)) {
            return false;
        }

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            if (!g_d3d11.initialize()) {
                return false;
            }
        } else if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            if (!g_d3d12.initialize()) {
                return false;
            }
        }

        m_initialized = true;
        return true;
    }

    void internal_frame() {
        if (ImGui::Begin("Super Cool Plugin")) {
            ImGui::Text("Hello from the super cool plugin!");
    #if defined(__clang__)
            ImGui::Text("Plugin Compiler: Clang");
    #elif defined(_MSC_VER)
            ImGui::Text("Plugin Compiler: Visual Studio");
    #elif defined(__GNUC__)
            ImGui::Text("Plugin Compiler: GCC");
    #else
            ImGui::Text("Plugin Compiler: Unknown");
    #endif
        }
    }

private:
    HWND m_wnd{};
    bool m_initialized{false};
};

std::unique_ptr<ExamplePlugin> g_plugin{new ExamplePlugin()};
