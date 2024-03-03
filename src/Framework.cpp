#include <chrono>
#include <filesystem>

#include <windows.h>
#include <ShlObj.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <imgui.h>
#include "uevr-imgui/font_robotomedium.hpp"
#include "uevr-imgui/imgui_impl_dx11.h"
#include "uevr-imgui/imgui_impl_dx12.h"
#include "uevr-imgui/imgui_impl_win32.h"

#include "utility/Module.hpp"
#include "utility/Patch.hpp"
#include "utility/Scan.hpp"
#include "utility/Thread.hpp"
#include "utility/String.hpp"
#include "utility/Input.hpp"

#include "WindowFilter.hpp"

#include "Mods.hpp"
#include "mods/PluginLoader.hpp"
#include "mods/VR.hpp"
#include "mods/ImGuiThemeHelpers.hpp"

#include "CommitHash.hpp"
#include "ExceptionHandler.hpp"
#include "LicenseStrings.hpp"
#include "mods/FrameworkConfig.hpp"
#include "Framework.hpp"

namespace fs = std::filesystem;
using namespace std::literals;

std::unique_ptr<Framework> g_framework{};

UEVRSharedMemory::UEVRSharedMemory() {
    spdlog::info("Shared memory constructor!");

    m_memory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Data), "UnrealVRMod");
    m_data = (Data*)MapViewOfFile(m_memory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Data));

    if (m_data != nullptr) {
        const auto p = *utility::get_module_pathw(utility::get_executable());
        std::memset(m_data->path, 0, sizeof(m_data->path));
        std::wcsncpy(m_data->path, p.c_str(), p.length());
        m_data->path[p.length()] = L'\0';
        m_data->pid = GetCurrentProcessId();
        spdlog::info("Mapped memory!");
    } else {
        spdlog::error("Failed to map memory!");
    }
}

void Framework::hook_monitor() {
    if (!m_hook_monitor_mutex.try_lock()) {
        // If this happens then we can assume execution is going as planned
        // so we can just reset the times so we dont break something
        m_last_present_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        m_last_chance_time = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        m_has_last_chance = true;
    } else {
        m_hook_monitor_mutex.unlock();
    }

    std::scoped_lock _{ m_hook_monitor_mutex };

    if (g_framework == nullptr) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    auto& d3d11 = get_d3d11_hook();
    auto& d3d12 = get_d3d12_hook();

    const auto renderer_type = get_renderer_type();

    if (d3d11 == nullptr || d3d12 == nullptr 
        || (renderer_type == Framework::RendererType::D3D11 && d3d11 != nullptr && !d3d11->is_inside_present()) 
        || (renderer_type == Framework::RendererType::D3D12 && d3d12 != nullptr && !d3d12->is_inside_present())) 
    {
        // check if present time is more than 5 seconds ago
        if (now - m_last_present_time >= std::chrono::seconds(5)) {
            if (m_has_last_chance) {
                // the purpose of this is to make sure that the game is not frozen
                // e.g. if we are debugging the game, so we don't rehook anything on accident
                m_has_last_chance = false;
                m_last_chance_time = now;

                spdlog::info("Last chance encountered for hooking");
            }

            if (!m_has_last_chance && now - m_last_chance_time > std::chrono::seconds(1)) {
                spdlog::info("Sending rehook request for D3D");

                // hook_d3d12 always gets called first.
                if (m_is_d3d11) {
                    hook_d3d11();
                } else {
                    hook_d3d12();
                }

                // so we don't immediately go and hook it again
                // add some additional time to it to give it some leeway
                m_last_present_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                m_last_message_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                m_last_chance_time = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                m_has_last_chance = true;
            }
        } else {
            m_last_chance_time = std::chrono::steady_clock::now();
            m_has_last_chance = true;
        }

        if (m_initialized && m_wnd != 0 && now - m_last_message_time > std::chrono::seconds(5)) {
            if (m_windows_message_hook != nullptr && m_windows_message_hook->is_hook_intact()) {
                spdlog::info("Windows message hook is still intact, ignoring...");
                m_last_message_time = now;
                m_last_sendmessage_time = now;
                m_sent_message = false;
                return;
            }

            // send dummy message to window to check if our hook is still intact
            if (!m_sent_message) {
                spdlog::info("Sending initial message hook test");

                auto proc = (WNDPROC)GetWindowLongPtr(m_wnd, GWLP_WNDPROC);

                if (proc != nullptr) {
                    const auto ret = CallWindowProc(proc, m_wnd, WM_NULL, 0, 0);

                    spdlog::info("Hook test message sent");
                }

                m_last_sendmessage_time = std::chrono::steady_clock::now();
                m_sent_message = true;
            } else if (now - m_last_sendmessage_time > std::chrono::seconds(1)) {
                spdlog::info("Sending reinitialization request for message hook");

                // if we don't get a message for 5 seconds, assume the hook is broken
                //m_initialized = false; // causes the hook to be re-initialized next frame
                m_message_hook_requested = true;
                m_last_message_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                m_last_present_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);

                m_sent_message = false;
            }
        } else {
            m_sent_message = false;
        }
    }
}

void Framework::command_thread() {
    m_uevr_shared_memory->data().command_thread_id = GetCurrentThreadId();

    MSG msg{};
    if (PeekMessageA(&msg, (HWND)-1, WM_USER, WM_USER, PM_REMOVE) == 0) {
        return;
    }

    if (msg.message != WM_USER) {
        return;
    }

    if (msg.wParam == UEVRSharedMemory::MESSAGE_IDENTIFIER) {
        on_frontend_command((UEVRSharedMemory::Command)msg.lParam);
    }
}

Framework::Framework(HMODULE framework_module)
    : m_framework_module{framework_module}
    , m_game_module{GetModuleHandle(0)},
    m_logger{spdlog::basic_logger_mt("UnrealVR", (get_persistent_dir() / "log.txt").string(), true)} 
{
    std::scoped_lock __{m_constructor_mutex};

    spdlog::set_default_logger(m_logger);
    spdlog::flush_on(spdlog::level::info);
    spdlog::info("UnrealVR entry");

    const auto module_size = *utility::get_module_size(m_game_module);

    spdlog::info("Game Module Addr: {:x}", (uintptr_t)m_game_module);
    spdlog::info("Game Module Size: {:x}", module_size);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    // Create the typedef for RtlGetVersion
    typedef LONG (*RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);

    const auto ntdll = GetModuleHandle("ntdll.dll");

    if (ntdll != nullptr) {
        // Manually get RtlGetVersion
        auto rtl_get_version = (RtlGetVersionFunc)GetProcAddress(ntdll, "RtlGetVersion");

        if (rtl_get_version != nullptr) {
            spdlog::info("Getting OS version information...");

            // Create an initial log that prints out the user's Windows OS version information
            // With the major and minor version numbers
            // Using RtlGetVersion()
            OSVERSIONINFOW os_version_info{};
            ZeroMemory(&os_version_info, sizeof(OSVERSIONINFOW));
            os_version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
            os_version_info.dwMajorVersion = 0;
            os_version_info.dwMinorVersion = 0;
            os_version_info.dwBuildNumber = 0;
            os_version_info.dwPlatformId = 0;

            if (rtl_get_version(&os_version_info) != 0) {
                spdlog::info("RtlGetVersion() failed");
            } else {
                // Log the Windows version information
                spdlog::info("OS Version Information");
                spdlog::info("\tMajor Version: {}", os_version_info.dwMajorVersion);
                spdlog::info("\tMinor Version: {}", os_version_info.dwMinorVersion);
                spdlog::info("\tBuild Number: {}", os_version_info.dwBuildNumber);
                spdlog::info("\tPlatform Id: {}", os_version_info.dwPlatformId);

                spdlog::info("Disclaimer: Framework does not send this information to the developers or any other third party.");
                spdlog::info("This information is only used to help with the development of Framework.");
            }
        } else {
            spdlog::info("RtlGetVersion() not found");
        }
    } else {
        spdlog::info("ntdll.dll not found");
    }

    // Hooking D3D12 initially because we need to retrieve the command queue before the first frame then switch to D3D11 if it failed later
    // on
    // addendum: now we don't need to do that, we just grab the command queue offset from the swapchain we create
    /*if (!hook_d3d12()) {
        spdlog::error("Failed to hook D3D12 for initial test.");
    }*/

    std::scoped_lock _{m_hook_monitor_mutex};
    PluginLoader::get()->early_init();

    m_last_present_time = std::chrono::steady_clock::time_point{}; // Instantly send the first message
    m_last_message_time = std::chrono::steady_clock::time_point{}; // Instantly send the first message
    m_last_chance_time = std::chrono::steady_clock::time_point{}; // Instantly send the first message
    m_has_last_chance = false;

    m_uevr_shared_memory = std::make_unique<UEVRSharedMemory>();
    m_command_thread = std::make_unique<std::jthread>([this](std::stop_token s) {
        spdlog::info("Command thread entry");

        {
            std::scoped_lock _{m_constructor_mutex};

            while (g_framework == nullptr) {
                std::this_thread::yield();
            }
        }

        while (!s.stop_requested() && !m_terminating) {
            this->command_thread();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });

    m_d3d_monitor_thread = std::make_unique<std::jthread>([this](std::stop_token s) {
        spdlog::info("D3D monitor thread entry");

        {
            std::scoped_lock _{m_constructor_mutex};

            while (g_framework == nullptr) {
                std::this_thread::yield();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));

        while (!s.stop_requested() && !m_terminating) {
            this->hook_monitor();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    spdlog::info("Leaving Framework constructor");
}

bool Framework::hook_d3d11() {
    //if (m_d3d11_hook == nullptr) {
        m_d3d11_hook.reset();
        m_d3d11_hook = std::make_unique<D3D11Hook>();
        m_d3d11_hook->on_present([this](D3D11Hook& hook) { on_frame_d3d11(); });
        m_d3d11_hook->on_post_present([this](D3D11Hook& hook) { on_post_present_d3d11(); });
        m_d3d11_hook->on_resize_buffers([this](D3D11Hook& hook, uint32_t w, uint32_t h) { on_reset(w, h); });
    //}

    // Making sure D3D12 is not hooked
    if (!m_is_d3d12) {
        if (m_d3d11_hook->hook()) {
            spdlog::info("Hooked DirectX 11");
            m_valid = true;
            m_is_d3d11 = true;
            return true;
        }
        // We make sure to no unhook any unwanted hooks if D3D11 didn't get hooked properly
        if (m_d3d11_hook->unhook()) {
            spdlog::info("D3D11 unhooked!");
        } else {
            spdlog::info("Cannot unhook D3D11, this might crash.");
        }

        m_valid = false;
        m_is_d3d11 = false;
        return false;
    }

    return false;
}

bool Framework::hook_d3d12() {
    // windows 7?
    if (LoadLibraryA("d3d12.dll") == nullptr) {
        spdlog::info("d3d12.dll not found, user is probably running Windows 7.");
        spdlog::info("Falling back to hooking D3D11.");

        m_is_d3d12 = false;
        return hook_d3d11();
    }

    //if (m_d3d12_hook == nullptr) {
        m_d3d12_hook.reset();
        m_d3d12_hook = std::make_unique<D3D12Hook>();
        m_d3d12_hook->on_present([this](D3D12Hook& hook) { on_frame_d3d12(); });
        m_d3d12_hook->on_post_present([this](D3D12Hook& hook) { on_post_present_d3d12(); });
        m_d3d12_hook->on_resize_buffers([this](D3D12Hook& hook, uint32_t w, uint32_t h) { on_reset(w, h); });
        m_d3d12_hook->on_resize_target([this](D3D12Hook& hook, uint32_t w, uint32_t h) { on_reset(w, h); });
    //}
    //m_d3d12_hook->on_create_swap_chain([this](D3D12Hook& hook) { m_d3d12.command_queue = m_d3d12_hook->get_command_queue(); });

    // Making sure D3D11 is not hooked
    if (!m_is_d3d11) {
        if (m_d3d12_hook->hook()) {
            spdlog::info("Hooked DirectX 12");
            m_valid = true;
            m_is_d3d12 = true;
            return true;
        }
        // We make sure to no unhook any unwanted hooks if D3D12 didn't get hooked properly
        if (m_d3d12_hook->unhook()) {
            spdlog::info("D3D12 Unhooked!");
        } else {
            spdlog::info("Cannot unhook D3D12, this might crash.");
        }

        m_valid = false;
        m_is_d3d12 = false;

        // Try to hook d3d11 instead
        return hook_d3d11();
    }

    return false;
}

Framework::~Framework() {
    spdlog::info("Framework shutting down...");

    m_terminating = true;
    m_d3d_monitor_thread->request_stop();
    m_command_thread->request_stop();
    if (m_d3d_monitor_thread->joinable()) {
        m_d3d_monitor_thread->join();
    }

    if (m_command_thread->joinable()) {
        m_command_thread->join();
    }

    if (m_is_d3d11) {
        deinit_d3d11();
    }

    if (m_is_d3d12) {
        deinit_d3d12();
    }

    ImGui_ImplWin32_Shutdown();

    if (m_initialized) {
        ImGui::DestroyContext();
    }
}

void Framework::run_imgui_frame(bool from_present) {
    std::scoped_lock _{ m_imgui_mtx };

    m_has_frame = false;

    if (!m_initialized) {
        return;
    }

    const bool is_init_ok = m_error.empty() && m_game_data_initialized;

    consume_input();
    update_fonts();
    
    ImGui_ImplWin32_NewFrame();

    // from_present is so we don't accidentally
    // run script/game code within the present thread.
    if (is_init_ok && !from_present) {
        // Run mod frame callbacks.
        m_mods->on_pre_imgui_frame();
    }

    ImGui::NewFrame();

    if (!from_present) {
        call_on_frame();
    }

    draw_ui();
    m_last_draw_ui = m_draw_ui;

    ImGui::EndFrame();
    ImGui::Render();

    m_has_frame = true;
}

// D3D11 Draw funciton
void Framework::on_frame_d3d11() {
    std::scoped_lock _{ m_imgui_mtx };

    spdlog::debug("on_frame (D3D11)");

    m_renderer_type = RendererType::D3D11;

    if (!m_initialized) {
        if (!initialize()) {
            spdlog::error("Failed to initialize Framework on DirectX 11");
            return;
        }

        spdlog::info("Framework initialized");
        m_initialized = true;
        return;
    }

    if (m_message_hook_requested) {
        initialize_windows_message_hook();
        initialize_xinput_hook();
        initialize_dinput_hook();
    }

    auto device = m_d3d11_hook->get_device();
    
    if (device == nullptr) {
        spdlog::error("D3D11 device was null when it shouldn't be, returning...");
        m_initialized = false;
        return;
    }

    bool is_init_ok = first_frame_initialize();

    //if (!m_has_frame) {
        //if (!is_init_ok) {
            //update_fonts();
            invalidate_device_objects();

            ImGui_ImplDX11_NewFrame();
            // hooks don't run until after initialization, so we just render the imgui window while initalizing.
            if (!m_has_engine_thread) {
                run_imgui_frame(false);
            }
   /*     } else {   
            return;
        }
    } else {
        invalidate_device_objects();
        ImGui_ImplDX11_NewFrame();
    }*/

    if (is_init_ok) {
        m_mods->on_present();
    }

    ComPtr<ID3D11DeviceContext> context{};
    float clear_color[]{0.0f, 0.0f, 0.0f, 0.0f};

    m_d3d11_hook->get_device()->GetImmediateContext(&context);
    context->ClearRenderTargetView(m_d3d11.blank_rt_rtv.Get(), clear_color);
    context->ClearRenderTargetView(m_d3d11.rt_rtv.Get(), clear_color);
    context->OMSetRenderTargets(1, m_d3d11.rt_rtv.GetAddressOf(), NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    for (auto& mod : m_mods->get_mods()) {
        mod->on_post_render_vr_framework_dx11(context.Get(), m_d3d11.rt.Get(), m_d3d11.rt_rtv.Get());
    }

    // Set the back buffer to be the render target.
    context->OMSetRenderTargets(1, m_d3d11.bb_rtv.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (is_init_ok) {
        m_mods->on_post_frame();
    }
}

void Framework::on_post_present_d3d11() {
    if (!m_error.empty() || !m_initialized || !m_game_data_initialized) {
        if (m_last_present_time <= std::chrono::steady_clock::now()){
            m_last_present_time = std::chrono::steady_clock::now();
        }

        return;
    }

    for (auto& mod : m_mods->get_mods()) {
        mod->on_post_present();
    }

    if (m_last_present_time <= std::chrono::steady_clock::now()){
        m_last_present_time = std::chrono::steady_clock::now();
    }
}

// D3D12 Draw funciton
void Framework::on_frame_d3d12() {
    std::scoped_lock _{ m_imgui_mtx };

    m_renderer_type = RendererType::D3D12;

    auto command_queue = m_d3d12_hook->get_command_queue();
    //spdlog::debug("on_frame (D3D12)");
    
    if (!m_initialized) {
        if (!initialize()) {
            spdlog::error("Failed to initialize Framework on DirectX 12");
            return;
        }

        spdlog::info("Framework initialized");
        m_initialized = true;
        return;
    }

    if (command_queue == nullptr) {
        spdlog::error("Null Command Queue");
        return;
    }

    if (m_message_hook_requested) {
        initialize_windows_message_hook();
        initialize_xinput_hook();
        initialize_dinput_hook();
    }

    auto device = m_d3d12_hook->get_device();

    if (device == nullptr) {
        spdlog::error("D3D12 Device was null when it shouldn't be, returning...");
        m_initialized = false;
        return;
    }

    bool is_init_ok = first_frame_initialize();

    //if (!m_has_frame) {
        //if (!is_init_ok) {
            //update_fonts();

            ImGui::GetIO().BackendRendererUserData = m_d3d12.imgui_backend_datas[0];
            const auto prev_cleanup = m_wants_device_object_cleanup;
            invalidate_device_objects();
            ImGui_ImplDX12_NewFrame();

            ImGui::GetIO().BackendRendererUserData = m_d3d12.imgui_backend_datas[1];
            m_wants_device_object_cleanup = prev_cleanup;
            invalidate_device_objects();
            ImGui_ImplDX12_NewFrame();
            // hooks don't run until after initialization, so we just render the imgui window while initalizing.
            if (!m_has_engine_thread) {
                run_imgui_frame(false);
            }
    /*    } else {   
            return;
        }
    } else {
        invalidate_device_objects();
        ImGui_ImplDX12_NewFrame();
    }*/

    if (is_init_ok) {
        m_mods->on_present();
    }

    if (m_d3d12.cmd_ctxs.empty()) {
        return;
    }

    auto& cmd_ctx = m_d3d12.cmd_ctxs[m_d3d12.cmd_ctx_index++ % m_d3d12.cmd_ctxs.size()];

    if (cmd_ctx == nullptr) {
        return;
    }

    cmd_ctx->wait(INFINITE);
    {
        std::scoped_lock _{ cmd_ctx->mtx };
        cmd_ctx->has_commands = true;

        // Draw to our render target.
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_d3d12.get_rt(D3D12::RTV::IMGUI).Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);

        float clear_color[]{0.0f, 0.0f, 0.0f, 0.0f};
        D3D12_CPU_DESCRIPTOR_HANDLE rts[1]{};
        cmd_ctx->cmd_list->ClearRenderTargetView(m_d3d12.get_cpu_rtv(device, D3D12::RTV::IMGUI), clear_color, 0, nullptr);
        rts[0] = m_d3d12.get_cpu_rtv(device, D3D12::RTV::IMGUI);
        cmd_ctx->cmd_list->OMSetRenderTargets(1, rts, FALSE, NULL);
        cmd_ctx->cmd_list->SetDescriptorHeaps(1, m_d3d12.srv_desc_heap.GetAddressOf());

        ImGui::GetIO().BackendRendererUserData = m_d3d12.imgui_backend_datas[1];
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_ctx->cmd_list.Get());

        for (auto& mod : m_mods->get_mods()) {
            rts[0] = m_d3d12.get_cpu_rtv(device, D3D12::RTV::IMGUI);
            mod->on_post_render_vr_framework_dx12(cmd_ctx->cmd_list.Get(), m_d3d12.get_rt(D3D12::RTV::IMGUI).Get(), &rts[0]);
        }
        
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);

        // Draw to the back buffer.
        auto swapchain = m_d3d12_hook->get_swap_chain();
        auto bb_index = swapchain->GetCurrentBackBufferIndex();
        barrier.Transition.pResource = m_d3d12.rts[bb_index].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);
        rts[0] = m_d3d12.get_cpu_rtv(device, (D3D12::RTV)bb_index);
        cmd_ctx->cmd_list->OMSetRenderTargets(1, rts, FALSE, NULL);
        cmd_ctx->cmd_list->SetDescriptorHeaps(1, m_d3d12.srv_desc_heap.GetAddressOf());

        ImGui::GetIO().BackendRendererUserData = m_d3d12.imgui_backend_datas[0];
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_ctx->cmd_list.Get());

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);

        cmd_ctx->execute();
    }

    if (is_init_ok) {
        m_mods->on_post_frame();
    }
}

void Framework::on_post_present_d3d12() {
    if (!m_error.empty() || !m_initialized || !m_game_data_initialized) {
        if (m_last_present_time <= std::chrono::steady_clock::now()){
            m_last_present_time = std::chrono::steady_clock::now();
        }

        return;
    }
    
    for (auto& mod : m_mods->get_mods()) {
        mod->on_post_present();
    }

    if (m_last_present_time <= std::chrono::steady_clock::now()){
        m_last_present_time = std::chrono::steady_clock::now();
    }
}

void Framework::on_reset(uint32_t w, uint32_t h) {
    std::scoped_lock _{ m_imgui_mtx };

    spdlog::info("Reset! {} {}", w, h);

    if (m_initialized) {
        // fixes text boxes not being able to receive input
        imgui::reset_keystates();
    }

    // Crashes if we don't release it at this point.
    if (m_is_d3d11) {
        deinit_d3d11();
        m_d3d11.rt_width = w;
        m_d3d11.rt_height = h;
    }

    if (m_is_d3d12) {
        deinit_d3d12();
        m_d3d12.rt_width = w;
        m_d3d12.rt_height = h;
    }

    if (m_game_data_initialized) {
        m_mods->on_device_reset();
    }

    m_has_frame = false;
    m_first_initialize = false;
    m_initialized = false;
}

void Framework::activate_window() {
    if (m_wnd == nullptr) {
        return;
    }

    AllowSetForegroundWindow(GetCurrentProcessId());
    SetForegroundWindow(m_wnd);
    //BringWindowToTop(m_wnd);
    //SetFocus(m_wnd);
    //SetActiveWindow(m_wnd);
}

void Framework::patch_set_cursor_pos() {
    std::scoped_lock _{ m_patch_mtx };

    if (m_set_cursor_pos_patch.get() == nullptr) {
        // Make SetCursorPos ret early
        const auto set_cursor_pos_addr = (uintptr_t)GetProcAddress(GetModuleHandleA("user32.dll"), "SetCursorPos");

        if (set_cursor_pos_addr != 0) {
            spdlog::info("Patching SetCursorPos");
            m_set_cursor_pos_patch = Patch::create(set_cursor_pos_addr, {0xC3});
        }
    }
}

void Framework::remove_set_cursor_pos_patch() {
    std::scoped_lock _{ m_patch_mtx };

    if (m_set_cursor_pos_patch.get() != nullptr) {
        spdlog::info("Removing SetCursorPos patch");
    }

    m_set_cursor_pos_patch.reset();
}

void Framework::set_mouse_to_center() {
    if (m_wnd == nullptr) {
        return;
    }

    RECT rect{};
    GetWindowRect(m_wnd, &rect);

    int x = (rect.left + rect.right) / 2;
    int y = (rect.top + rect.bottom) / 2;

    if (m_set_cursor_pos_patch != nullptr) {
        remove_set_cursor_pos_patch();
        SetCursorPos(x, y);
        patch_set_cursor_pos();
    } else {
        SetCursorPos(x, y);
    }
}

void Framework::post_message(UINT message, WPARAM w_param, LPARAM l_param) {
    if (m_wnd == nullptr) {
        return;
    }

    PostMessage(m_wnd, message, w_param, l_param);
}

bool Framework::on_message(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    m_last_message_time = std::chrono::steady_clock::now();

    if (!m_initialized) {
        return true;
    }

    // If we called is_filtered during a WM_GETTEXT message we would deadlock.
    if (message != WM_GETTEXT && !WindowFilter::get().is_filtered(wnd)) {
        m_uevr_shared_memory->data().main_thread_id = GetCurrentThreadId();
    }

    bool is_mouse_moving{false};
    switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (w_param >= 0 && w_param < 256) {
            m_last_keys[w_param] = true;
        }

        if (w_param == VK_INSERT ||
            w_param == FrameworkConfig::get()->get_menu_key()->value()) 
        {
            set_draw_ui(!m_draw_ui, true);
            return false;
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (w_param >= 0 && w_param < 256) {
            m_last_keys[w_param] = false;
        }
        break;
    case WM_KILLFOCUS:
        std::fill(std::begin(m_last_keys), std::end(m_last_keys), false);
        break;
    case WM_SETFOCUS:
        std::fill(std::begin(m_last_keys), std::end(m_last_keys), false);
        break;
    case WM_ACTIVATE:
        if (LOWORD(w_param) == WA_INACTIVE) {
            return false;
        }
        break;
    case WM_INPUT: {
        // RIM_INPUT means the window has focus
        /*if (GET_RAWINPUT_CODE_WPARAM(w_param) == RIM_INPUT) {
            uint32_t size = sizeof(RAWINPUT);
            RAWINPUT raw{};
            
            // obtain size
            GetRawInputData((HRAWINPUT)l_param, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

            auto result = GetRawInputData((HRAWINPUT)l_param, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

            if (raw.header.dwType == RIM_TYPEMOUSE) {
                m_accumulated_mouse_delta[0] += (float)raw.data.mouse.lLastX;
                m_accumulated_mouse_delta[1] += (float)raw.data.mouse.lLastY;

                // Allowing camera movement when the UI is hovered while not focused
                if (raw.data.mouse.lLastX || raw.data.mouse.lLastY) {
                    is_mouse_moving = true;
                }
            }
        }*/
    } break;

    default:
        break;
    }

    ImGui_ImplWin32_WndProcHandler(wnd, message, w_param, l_param);

    {
        // If the user is interacting with the UI we block the message from going to the game.
        const auto& io = ImGui::GetIO();
        if (m_draw_ui && !m_ui_passthrough) {
            // Fix of a bug that makes the input key down register but the key up will never register
            // when clicking on the ui while the game is not focused
            if (message == WM_INPUT && GET_RAWINPUT_CODE_WPARAM(w_param) == RIM_INPUTSINK){
                return false;
            }

            static std::unordered_set<UINT> forcefully_allowed_messages {
                WM_DEVICECHANGE, // Allows XInput devices to connect to UE
                WM_SHOWWINDOW,
                WM_ACTIVATE,
                WM_ACTIVATEAPP,
                WM_CLOSE,
                WM_DPICHANGED,
                WM_SIZING,
                WM_MOUSEACTIVATE
            };

            if (!forcefully_allowed_messages.contains(message)) {      
                if (m_is_ui_focused) {
                    if (io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput)
                        return false;
                } else {
                    if (!is_mouse_moving && (io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput))
                        return false;
                }
            }
        }
    }

    bool any_false = false;

    if (m_game_data_initialized) {
        for (auto& mod : m_mods->get_mods()) {
            if (!mod->on_message(wnd, message, w_param, l_param)) {
                any_false = true;
            }
        }
    }

    return !any_false;
}

void Framework::on_frontend_command(UEVRSharedMemory::Command command) {
    spdlog::info("Received frontend command: {}", (int)command);

    switch (command) {
    case UEVRSharedMemory::Command::RELOAD_CONFIG:
        m_frame_worker->enqueue([this]() {
            m_mods->reload_config();
        });

        break;
    case UEVRSharedMemory::Command::CONFIG_SETUP_ACKNOWLEDGED:
        m_uevr_shared_memory->data().signal_frontend_config_setup = false;
        break;
    default:
        spdlog::error("Unknown frontend command received: {}", (int)command);
        break;
    }
}

// this is unfortunate.
void Framework::on_direct_input_keys(const std::array<uint8_t, 256>& keys) {
    /*const auto menu_key = FrameworkConfig::get()->get_menu_key()->value();

    if (keys[menu_key] && m_last_keys[menu_key] == 0) {
        std::lock_guard _{m_input_mutex};

        set_draw_ui(!m_draw_ui);
    }

    m_last_keys = keys;*/
}

std::filesystem::path Framework::get_persistent_dir() {
    auto return_appdata_dir = []() -> std::filesystem::path {
        wchar_t app_data_path[MAX_PATH]{};
        SHGetSpecialFolderPathW(0, app_data_path, CSIDL_APPDATA, false);

        const auto exe_name = [&]() {
            const auto result = std::filesystem::path(*utility::get_module_pathw(utility::get_executable())).stem().string();
            const auto dir = std::filesystem::path(app_data_path) / "UnrealVRMod" / result;
            std::filesystem::create_directories(dir);

            return result;
        }();

        return std::filesystem::path(app_data_path) / "UnrealVRMod" / exe_name;
    };

    static const auto result = return_appdata_dir();

    return result;
}

void Framework::save_config() {
    std::scoped_lock _{m_config_mtx};

    spdlog::info("Saving config config.txt");

    utility::Config cfg{};

    for (auto& mod : m_mods->get_mods()) {
        mod->on_config_save(cfg);
    }

    if (!cfg.save(get_persistent_dir("config.txt").string())) {
        spdlog::info("Failed to save config");
        return;
    }

    spdlog::info("Saved config");

    if (auto& sm = g_framework->get_shared_memory(); sm) {
        sm->data().signal_frontend_config_setup = true;
        spdlog::info("Signaled frontend config setup");
    }
}

void Framework::reset_config() try {
    std::scoped_lock _{m_config_mtx};

    m_mods->reload_config(true);

    spdlog::info("Removed config");
} catch (const std::exception& e) {
    spdlog::error("Failed to reset config: {}", e.what());
}

bool Framework::is_drawing_anything() const {
    return m_draw_ui || FrameworkConfig::get()->is_always_show_cursor();
}

void Framework::set_draw_ui(bool state, bool should_save) {
    std::scoped_lock _{m_config_mtx};

    spdlog::info("Setting draw ui to {}", state);

    bool prev_state = m_draw_ui;
    m_draw_ui = state;

    if (m_game_data_initialized) {
        FrameworkConfig::get()->get_menu_open()->value() = state;
    }

    if (state != prev_state && should_save && m_game_data_initialized) {
        save_config();
    }
}

void Framework::consume_input() {
    m_mouse_delta[0] = m_accumulated_mouse_delta[0];
    m_mouse_delta[1] = m_accumulated_mouse_delta[1];

    m_accumulated_mouse_delta[0] = 0.0f;
    m_accumulated_mouse_delta[1] = 0.0f;
}

int Framework::add_font(const std::filesystem::path& filepath, int size, const std::vector<ImWchar>& ranges) {
    // Look for a font already matching this description.
    for (int i = 0; i < m_additional_fonts.size(); ++i) {
        const auto& font = m_additional_fonts[i];

        if (font.filepath == filepath && font.size == size && font.ranges == ranges) {
            return i;
        }
    }

    m_additional_fonts.emplace_back(Framework::AdditionalFont{filepath, size, ranges, (ImFont*)nullptr});
    m_fonts_need_updating = true;

    return m_additional_fonts.size() - 1;
}

void Framework::update_fonts() {
    if (!m_fonts_need_updating) {
        return;
    }

    m_fonts_need_updating = false;

    auto& fonts = ImGui::GetIO().Fonts;

    fonts->Clear();
    fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size, (float)m_font_size);

    for (auto& font : m_additional_fonts) {
        const ImWchar* ranges = nullptr;

        if (!font.ranges.empty()) {
            ranges = font.ranges.data();
        }

        if (fs::exists(font.filepath)) {
            font.font = fonts->AddFontFromFileTTF(font.filepath.string().c_str(), (float)font.size, nullptr, ranges);
        } else {
            font.font = fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size, (float)font.size, nullptr, ranges);
        }
    }

    fonts->Build();
    m_wants_device_object_cleanup = true;
}

void Framework::invalidate_device_objects() {
    if (!m_wants_device_object_cleanup) {
        return;
    }

    if (m_renderer_type == RendererType::D3D11) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    } else if (m_renderer_type == RendererType::D3D12) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
    }

    m_wants_device_object_cleanup = false;
}

void Framework::draw_ui() {
    std::lock_guard _{m_input_mutex};

    if (m_current_theme != get_imgui_theme_value()) {
        set_imgui_style();
        m_current_theme = get_imgui_theme_value();
    }

    ImGui::GetIO().MouseDrawCursor = m_draw_ui || FrameworkConfig::get()->is_always_show_cursor();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // causes bugs with the cursor

    if (!m_draw_ui) {
        // remove SetCursorPos patch
        if (!FrameworkConfig::get()->is_always_show_cursor()) {
            remove_set_cursor_pos_patch();
        } else {
            patch_set_cursor_pos();
        }

        m_is_ui_focused = false;
        if (m_last_draw_ui) {
            m_windows_message_hook->window_toggle_cursor(m_cursor_state);
        }
        //m_dinput_hook->acknowledge_input();
        // ImGui::GetIO().MouseDrawCursor = false;
        return;
    }
    
    // UI Specific code:
    m_is_ui_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

    if (m_ui_option_transparent) {
        auto& style = ImGui::GetStyle();
        if (m_is_ui_focused) {
            style.Alpha = 1.0f;
        } else {
            if (ImGui::IsWindowHovered(ImGuiFocusedFlags_AnyWindow)) {
                style.Alpha = 0.9f;
            } else {
                style.Alpha = 0.8f;
            }
        }
    } else {
        auto& style = ImGui::GetStyle();
        style.Alpha = 1.0f;
    }

    auto& io = ImGui::GetIO();

    if (io.WantCaptureKeyboard) {
        //m_dinput_hook->ignore_input();
    } else {
        //m_dinput_hook->acknowledge_input();
    }

    const auto is_vr_active = VR::get()->is_hmd_active();


    // Center the window
    const auto rt_size = get_rt_size();
    constexpr auto window_w = 700.0f;
    constexpr auto window_h = 700.0f;

    const auto centered_x = (rt_size.x / 2) - (window_w / 2);
    const auto centered_y = (rt_size.y / 2) - (window_h / 2);

    // Always re-center the UI upon open if VR is active
    if (is_vr_active && !m_last_draw_ui) {
        ImGui::SetNextWindowPos(ImVec2(centered_x, centered_y), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos(ImVec2(centered_x, centered_y), ImGuiCond_Once);
    }

    if (!m_last_draw_ui || m_cursor_state_changed) {
        m_cursor_state_changed = false;
    }
    
    static const auto UEVR_NAME = std::format("UEVR [rev. {:.8}][{} {}]", UEVR_COMMIT_HASH, UEVR_BUILD_DATE, UEVR_BUILD_TIME);

    ImGui::SetNextWindowSize(ImVec2(window_w, window_h), ImGuiCond_::ImGuiCond_Once);
    ImGui::Begin(UEVR_NAME.c_str(), &m_draw_ui);

    ImGui::BeginGroup();
    ImGui::Columns(2);
    ImGui::BeginGroup();

    ImGui::Checkbox("Transparency", &m_ui_option_transparent);
    ImGui::SameLine();
    ImGui::Text("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Makes the UI transparent when not focused.");
    }
    ImGui::Checkbox("Input Passthrough", &m_ui_passthrough);
    ImGui::SameLine();
    ImGui::Text("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Allows mouse and keyboard inputs to register to the game while the UI is focused.");
    }

    FrameworkConfig::get()->get_advanced_mode()->draw("Show Advanced Options");

    ImGui::SameLine();
    ImGui::Text("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show additional options for greater control over various settings.");
    }

    if (m_mods_fully_initialized) {
        if (ImGui::Button("Reset to Default Settings")) {
            reset_config();
        }
    }

    ImGui::EndGroup();
    ImGui::NextColumn();

    ImGui::BeginGroup();
    ImGui::Text("Keyboard Menu Key: Insert");
    ImGui::Text("Gamepad L3 + R3: Toggle Menu");
    ImGui::Text("Gamepad RT: Shortcuts");
    ImGui::Text("Gamepad LB/RB: Change Sidebar Page");

    ImGui::EndGroup();
    ImGui::EndGroup();

    ImGui::Columns(1);

    // Mods:
    auto& sidebar_entries = m_sidebar_state.entries;
    sidebar_entries.clear();
    sidebar_entries.emplace_back("About", false);

    if (ImGui::BeginTable("UEVRTable", 2, ImGuiTableFlags_::ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_::ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("UEVRLeftPaneColumn", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("UEVRRightPaneColumn", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); // Set to the first column

        ImGui::BeginChild("UEVRLeftPane", ImVec2(0, 0), true);
        auto dcs = [&](const char* label, int32_t page_value) -> bool {
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
            if (ImGui::Selectable(label, m_sidebar_state.selected_entry == page_value)) {
                m_sidebar_state.selected_entry = page_value;
                ImGui::PopStyleVar();
                return true;
            }
            ImGui::PopStyleVar();
            return false;
        };

        dcs("About", 0);

        if (m_error.empty() && m_game_data_initialized) {
            struct Info {
                size_t mn{};
                size_t mx{};
                std::shared_ptr<Mod> mod{};
                bool has_sidebar_entries{};
            };

            std::vector<Info> mod_sidebar_ranges{};

            const auto is_advanced_mode = is_advanced_view_enabled();

            for (auto& mod : m_mods->get_mods()) {
                if (mod->is_advanced_mod() && !is_advanced_mode) {
                    continue;
                }

                auto entries = mod->get_sidebar_entries();

                if (!entries.empty()) {
                    size_t displayed_entries = 0;
                    for (auto& entry : entries) {
                        if (entry.m_advanced_entry && !is_advanced_mode) {
                            continue;
                        }

                        sidebar_entries.emplace_back(entry.m_label.c_str(), entry.m_advanced_entry);
                        ++displayed_entries;
                    }

                    if (displayed_entries > 0) {
                        mod_sidebar_ranges.push_back(Info{sidebar_entries.size() - displayed_entries, sidebar_entries.size(), mod, true});
                    }
                } else {
                    mod_sidebar_ranges.push_back(Info{sidebar_entries.size(), sidebar_entries.size() + 1, mod, false});
                    sidebar_entries.emplace_back(mod->get_name().data(), mod->is_advanced_mod());
                }
            }

            for (size_t i = 1; i < sidebar_entries.size(); ++i) {
                if (is_advanced_mode || !sidebar_entries[i].m_advanced_entry) {
                    for (const auto& range : mod_sidebar_ranges) {
                        if (i == range.mn) {
                            // Set first entry as default ("Runtime" entry of VR mod)
                            if (range.has_sidebar_entries && !m_sidebar_state.initialized) {
                                if (range.mod->get_name() == "VR") {
                                    m_sidebar_state.selected_entry = i;
                                    m_sidebar_state.initialized = true;
                                    ImGui::SetWindowFocus("UEVRRightPane");
                                }
                            }

                            ImGui::Text(range.mod->get_name().data());
                        }
                    }

                    ImGui::PushID(i);
                    dcs(sidebar_entries[i].m_label.c_str(), i);
                    ImGui::PopID();
                }
            }

            bool wants_focus_right = false;

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) {
                decrement_sidebar_page();
                wants_focus_right = true;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) {
                increment_sidebar_page();
                wants_focus_right = true;
            }

            if (m_sidebar_state.selected_entry < 0) {
                m_sidebar_state.selected_entry = sidebar_entries.size() - 1;
            }

            m_sidebar_state.selected_entry = m_sidebar_state.selected_entry % sidebar_entries.size();

            ImGui::EndChild();
            ImGui::TableNextColumn(); // Move to the next column (right)

            if (wants_focus_right) {
                ImGui::SetNextWindowFocus();
            }

            ImGui::BeginChild("UEVRRightPane", ImVec2(0, 0), true, ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysUseWindowPadding);
            {
                ImGui::BeginGroup();

                if (m_sidebar_state.selected_entry > 0) {
                    // Find the mod that owns this entry
                    for (const auto& range : mod_sidebar_ranges) {
                        if (m_sidebar_state.selected_entry >= range.mn && m_sidebar_state.selected_entry < range.mx) {
                            if (wants_focus_right) {
                                ImGui::SetKeyboardFocusHere();
                            }

                            if (range.has_sidebar_entries) {
                                range.mod->on_draw_sidebar_entry(sidebar_entries[m_sidebar_state.selected_entry].m_label);
                            } else {
                                range.mod->on_draw_ui();
                            }

                            break;
                        }
                    }
                } else {
                    draw_about();
                }

                ImGui::EndGroup();
                ImGui::EndChild();
            }

            /*for (auto& mod : m_mods->get_mods()) {
                mod->on_draw_ui();
            }*/

            //m_mods->on_draw_ui();
        } else if (!m_game_data_initialized) {
            ImGui::EndChild();

            if (m_sidebar_state.selected_entry == 0) {
                ImGui::TableNextColumn();
                draw_about();
            }

            ImGui::TextWrapped("Framework is currently initializing...");
            ImGui::TextWrapped("This menu will close after initialization if you have the remember option enabled.");
        } else if (!m_error.empty()) {
            ImGui::EndChild();

            if (m_sidebar_state.selected_entry == 0) {
                ImGui::TableNextColumn();
                draw_about();
            }

            ImGui::TextWrapped("Framework error: %s", m_error.c_str());
        }

        ImGui::EndTable();
    }

    m_last_window_pos = ImGui::GetWindowPos();
    m_last_window_size = ImGui::GetWindowSize();

    ImGui::End();

    // save the menu state in config
    if (m_draw_ui != m_last_draw_ui) {
        if (m_draw_ui) {
            set_mouse_to_center();
            patch_set_cursor_pos();
        }

        m_draw_ui = m_last_draw_ui;
        set_draw_ui(!m_draw_ui, true);
    }

    // if we pressed the X button to close the menu.
    if (m_last_draw_ui && !m_draw_ui) {
        m_windows_message_hook->window_toggle_cursor(m_cursor_state);
    }
}

void Framework::draw_about() {
    ImGui::Text("Author: praydog");
    ImGui::Text("Unreal Engine VR");
    ImGui::Text("https://github.com/praydog/UEVR");
    ImGui::Text("http://praydog.com");

    if (ImGui::CollapsingHeader("Licenses")) {
        ImGui::TreePush("Licenses");

        struct License {
            std::string name;
            std::string text;
        };

        static std::array<License, 10> licenses{
            License{ "glm", license::glm },
            License{ "imgui", license::imgui },
            License{ "safetyhook", license::safetyhook },
            License{ "spdlog", license::spdlog },
            License{ "json", license::json },
            License{ "bddisasm", utility::narrow(license::bddisasm) },
            License{ "directxtk", license::directxtk },
            License{ "directxtk12", license::directxtk },
            License{ "openvr", license::openvr },
            License{ "openxr", license::openxr }
        };

        for (const auto& license : licenses) {
            if (ImGui::CollapsingHeader(license.name.c_str())) {
                ImGui::TextWrapped(license.text.c_str());
            }
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
}

void Framework::set_imgui_style() noexcept {
    
    auto current_theme = get_imgui_theme_value();
    
    switch (current_theme) {
        case ImGuiThemes::DEFAULT_DARK:
            ImGuiThemeHelper::StyleColorsDefaultDark();
            break;
        case ImGuiThemes::ALTERNATIVE_DARK:
            ImGuiThemeHelper::StyleColorsAlternativeDark();
            break;
        case ImGuiThemes::DEFAULT_LIGHT:
            ImGuiThemeHelper::StyleColorsDefaultLight();
            break;
        case ImGuiThemes::HIGH_CONTRAST:
            ImGuiThemeHelper::StyleColorsHighContrast();
            break;
        default:
            ImGuiThemeHelper::StyleColorsDefaultDark();
            break;
    }
    
    // Font
    set_font_size(m_font_size);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
}

bool Framework::initialize() {
    if (m_initialized) {
        return true;
    }

    framework::setup_exception_handler();

    if (m_first_initialize) {
        m_frames_since_init = 0;
        m_first_initialize = false;
    }

    if (m_frames_since_init < 60) {
        m_frames_since_init++;
        return false;
    }

    if (m_is_d3d11) {
        spdlog::info("Attempting to initialize DirectX 11");

        if (!m_d3d11_hook->is_hooked()) {
            return false;
        }

        auto device = m_d3d11_hook->get_device();
        auto swap_chain = m_d3d11_hook->get_swap_chain();

        // Wait.
        if (device == nullptr || swap_chain == nullptr) {
            m_first_initialize = true;

            spdlog::info("Device or SwapChain null. DirectX 12 may be in use. Unhooking D3D11...");

            // We unhook D3D11
            if (m_d3d11_hook->unhook()) {
                spdlog::info("D3D11 unhooked!");
            } else {
                spdlog::error("Cannot unhook D3D11, this might crash.");
            }

            m_is_d3d11 = false;
            m_valid = false;

            // We hook D3D12
            if (!hook_d3d12()) {
                spdlog::error("Failed to hook D3D12 after unhooking D3D11.");
            }
            return false;
        }

        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        swap_chain->GetDesc(&swap_desc);

        m_wnd = swap_desc.OutputWindow;


        spdlog::info("Window Handle: {0:x}", (uintptr_t)m_wnd);
        spdlog::info("Initializing ImGui");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        set_imgui_style();

        static const auto imgui_ini = (get_persistent_dir() / "imgui.ini").string();
        ImGui::GetIO().IniFilename = imgui_ini.c_str();

        spdlog::info("Initializing ImGui Win32");

        if (!ImGui_ImplWin32_Init(m_wnd)) {
            spdlog::error("Failed to initialize ImGui.");
            return false;
        }

        spdlog::info("Creating render target");

        if (!init_d3d11()) {
            spdlog::error("Failed to init D3D11");
            return false;
        }
    } else if (m_is_d3d12) {
        spdlog::info("Attempting to initialize DirectX 12");

        if (!m_d3d12_hook->is_hooked()) {
            return false;
        }

        auto device = m_d3d12_hook->get_device();
        auto swap_chain = m_d3d12_hook->get_swap_chain();

        if (device == nullptr || swap_chain == nullptr) {
            m_first_initialize = true;

            spdlog::info("Device: {:x}", (uintptr_t)device);
            spdlog::info("SwapChain: {:x}", (uintptr_t)swap_chain);

            spdlog::info("Device or SwapChain null. DirectX 11 may be in use. Unhooking D3D12...");

            // We unhook D3D12
            if (m_d3d12_hook->unhook())
                spdlog::info("D3D12 unhooked!");
            else
                spdlog::error("Cannot unhook D3D12, this might crash.");

            m_valid = false;
            m_is_d3d12 = false;

            // We hook D3D11
            if (!hook_d3d11()) {
                spdlog::error("Failed to hook D3D11 after unhooking D3D12.");
            }
            return false;
        }

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        swap_chain->GetDesc(&swap_desc);

        m_wnd = swap_desc.OutputWindow;


        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        set_imgui_style();

        static const auto imgui_ini = (get_persistent_dir() / "imgui.ini").string();
        ImGui::GetIO().IniFilename = imgui_ini.c_str();
        
        if (!ImGui_ImplWin32_Init(m_wnd)) {
            spdlog::error("Failed to initialize ImGui ImplWin32.");
            return false;
        }

        if (!init_d3d12()) {
            spdlog::error("Failed to init D3D12.");
            return false;
        }
    } else {
        return false;
    }

    initialize_windows_message_hook();
    initialize_xinput_hook();
    initialize_dinput_hook();

    // TODO
    /*if (m_first_frame) {
        m_dinput_hook = std::make_unique<DInputHook>(m_wnd);
    } else {
        m_dinput_hook->set_window(m_wnd);
    }*/

    if (m_first_frame) {
        m_first_frame = false;

        spdlog::info("Starting game data initialization thread");

        // Game specific initialization stuff. Code that runs any D3D must not run here (like VR code).
        std::thread init_thread([this]() {
            try {
                //Framework::initialize_sdk(); // TODO
                m_mods = std::make_unique<Mods>();

                auto e = m_mods->on_initialize();

                if (e) {
                    if (e->empty()) {
                        m_error = "An unknown error has occurred.";
                    } else {
                        m_error = *e;
                    }

                    spdlog::error("Initialization of mods failed. Reason: {}", m_error);
                }
                
                // Allow the window to initially render so the user knows we are loading.
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                m_game_data_initialized = true;
            } catch(...) {
                m_error = "An exception has occurred during initialization.";
                m_game_data_initialized = true;
                spdlog::error("Initialization of mods failed. Reason: exception thrown.");
            }

            spdlog::info("Game data initialization thread finished");
        });

        init_thread.detach();
    }

    return true;
}

bool Framework::initialize_windows_message_hook() {
    if (m_wnd == 0) {
        return false;
    }

    if (m_first_frame || m_message_hook_requested || m_windows_message_hook == nullptr) {
        m_last_message_time = std::chrono::steady_clock::now();
        m_windows_message_hook.reset();
        m_windows_message_hook = std::make_unique<WindowsMessageHook>(m_wnd);
        m_windows_message_hook->on_message = [this](auto wnd, auto msg, auto w_param, auto l_param) {
            return on_message(wnd, msg, w_param, l_param);
        };

        m_message_hook_requested = false;
        return true;
    }

    m_message_hook_requested = false;
    return false;
}

bool Framework::initialize_xinput_hook() {
    if (m_first_frame || m_xinput_hook == nullptr) {
        m_xinput_hook.reset();
        m_xinput_hook = std::make_unique<XInputHook>();
    }

    return true;
}

bool Framework::initialize_dinput_hook() {
    if (m_first_frame || m_dinput_hook == nullptr) {
        m_dinput_hook.reset();
        m_dinput_hook = std::make_unique<DInputHook>();
    }

    return true;
}

// Ran on the first valid frame after pre-initialization of mods has taken place and hasn't failed
// This one allows mods to run any initialization code in the context of the D3D thread (like VR code)
// It also is the one that actually loads any config files
bool Framework::first_frame_initialize() {
    const bool is_init_ok = m_error.empty() && m_game_data_initialized;

    if (!is_init_ok || !m_first_frame_d3d_initialize) {
        return is_init_ok;
    }

    std::scoped_lock _{get_hook_monitor_mutex()};

    spdlog::info("Running first frame D3D initialization of mods...");

    m_first_frame_d3d_initialize = false;
    auto e = m_mods->on_initialize_d3d_thread();

    if (e) {
        if (e->empty()) {
            m_error = "An unknown error has occurred.";
        } else {
            m_error = *e;
        }

        spdlog::error("Initialization of mods failed. Reason: {}", m_error);
        m_game_data_initialized = false;
        m_mods_fully_initialized = false;
        return false;
    } else {
        // Do an initial config save to set the default values for the frontend
        save_config();
        m_mods_fully_initialized = true;
    }

    return true;
}

void Framework::call_on_frame() {
    const bool is_init_ok = m_error.empty() && m_game_data_initialized && m_mods_fully_initialized;

    if (is_init_ok) {
        // Run mod frame callbacks.
        m_frame_worker->execute();
        m_mods->on_frame();
    }
}

// DirectX 11 Initialization methods

bool Framework::init_d3d11() {
    deinit_d3d11();

    auto swapchain = m_d3d11_hook->get_swap_chain();
    auto device = m_d3d11_hook->get_device();

    // Get back buffer.
    spdlog::info("[D3D11] Creating RTV of back buffer...");

    ComPtr<ID3D11Texture2D> backbuffer{};

    if (FAILED(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)))) {
        spdlog::error("[D3D11] Failed to get back buffer!");
        return false;
    }

    // Create a render target view of the back buffer.
    if (FAILED(device->CreateRenderTargetView(backbuffer.Get(), nullptr, &m_d3d11.bb_rtv))) {
        spdlog::error("[D3D11] Failed to create back buffer render target view!");
        return false;
    }

    // Get backbuffer description.
    D3D11_TEXTURE2D_DESC backbuffer_desc{};

    backbuffer->GetDesc(&backbuffer_desc);
    backbuffer_desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    spdlog::info("[D3D11] Back buffer format is {}", backbuffer_desc.Format);

    spdlog::info("[D3D11] Creating render targets...");
    {
        // Create our blank render target.
        auto d3d11_rt_desc = backbuffer_desc;
        d3d11_rt_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // For VR

        if (FAILED(device->CreateTexture2D(&d3d11_rt_desc, nullptr, &m_d3d11.blank_rt))) {
            spdlog::error("[D3D11] Failed to create render target texture!");
            return false;
        }

        // Create our render target
        if (FAILED(device->CreateTexture2D(&d3d11_rt_desc, nullptr, &m_d3d11.rt))) {
            spdlog::error("[D3D11] Failed to create render target texture!");
            return false;
        }
    }

    // Create our blank render target view.
    spdlog::info("[D3D11] Creating rtvs...");

    if (FAILED(device->CreateRenderTargetView(m_d3d11.blank_rt.Get(), nullptr, &m_d3d11.blank_rt_rtv))) {
        spdlog::error("[D3D11] Failed to create render terget view!");
        return false;
    }


    // Create our render target view.
    if (FAILED(device->CreateRenderTargetView(m_d3d11.rt.Get(), nullptr, &m_d3d11.rt_rtv))) {
        spdlog::error("[D3D11] Failed to create render terget view!");
        return false;
    }

    // Create our render target shader resource view.
    spdlog::info("[D3D11] Creating srvs...");

    if (FAILED(device->CreateShaderResourceView(m_d3d11.rt.Get(), nullptr, &m_d3d11.rt_srv))) {
        spdlog::error("[D3D11] Failed to create shader resource view!");
        return false;
    }

    m_d3d11.rt_width = backbuffer_desc.Width;
    m_d3d11.rt_height = backbuffer_desc.Height;
    m_last_rt_size = {backbuffer_desc.Width, backbuffer_desc.Height};

    spdlog::info("[D3D11] Initializing ImGui D3D11...");

    ComPtr<ID3D11DeviceContext> context{};

    device->GetImmediateContext(&context);

    if (!ImGui_ImplDX11_Init(device, context.Get())) {
        spdlog::error("[D3D11] Failed to initialize ImGui.");
        return false;
    }

    return true;
}

void Framework::deinit_d3d11() {
    ImGui_ImplDX11_Shutdown();
    m_d3d11 = {};
}

// DirectX 12 Initialization methods

bool Framework::init_d3d12() {
    deinit_d3d12();
    
    auto device = m_d3d12_hook->get_device();

    spdlog::info("[D3D12] Creating command allocator...");

    m_d3d12.cmd_ctxs.clear();

    for (auto i = 0; i < 3; ++i) {
        auto& ctx = m_d3d12.cmd_ctxs.emplace_back(std::make_unique<d3d12::CommandContext>());

        if (!ctx->setup(L"Framework::m_d3d12.cmd_ctx")) {
            spdlog::error("[D3D12] Failed to create command context.");
            return false;
        }
    }

    spdlog::info("[D3D12] Creating RTV descriptor heap...");

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};

        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = (int)D3D12::RTV::COUNT; 
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_d3d12.rtv_desc_heap)))) {
            spdlog::error("[D3D12] Failed to create RTV descriptor heap.");
            return false;
        }

        m_d3d12.rtv_desc_heap->SetName(L"Framework::m_d3d12.rtv_desc_heap");
    }

    spdlog::info("[D3D12] Creating SRV descriptor heap...");

    { 
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = (int)D3D12::SRV::COUNT;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_d3d12.srv_desc_heap)))) {
            spdlog::error("[D3D12] Failed to create SRV descriptor heap.");
            return false;
        }

        m_d3d12.srv_desc_heap->SetName(L"Framework::m_d3d12.srv_desc_heap");
    }

    spdlog::info("[D3D12] Creating render targets...");

    {
        // Create back buffer rtvs.
        auto swapchain = m_d3d12_hook->get_swap_chain();

        for (auto i = 0; i <= (int)D3D12::RTV::BACKBUFFER_3; ++i) {
            if (SUCCEEDED(swapchain->GetBuffer(i, IID_PPV_ARGS(&m_d3d12.rts[i])))) {
                device->CreateRenderTargetView(m_d3d12.rts[i].Get(), nullptr, m_d3d12.get_cpu_rtv(device, (D3D12::RTV)i));
            } else {
                spdlog::error("[D3D12] Failed to get back buffer for rtv.");
            }
        }

        // Create our imgui and blank rts.
        auto& backbuffer = m_d3d12.get_rt(D3D12::RTV::BACKBUFFER_0);
        auto desc = backbuffer->GetDesc();

        spdlog::info("[D3D12] Back buffer format is {}", desc.Format);

        D3D12_HEAP_PROPERTIES props{};
        props.Type = D3D12_HEAP_TYPE_DEFAULT;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        auto d3d12_rt_desc = desc;
        d3d12_rt_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // For VR

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = d3d12_rt_desc.Format;

        if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &d3d12_rt_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
                IID_PPV_ARGS(&m_d3d12.get_rt(D3D12::RTV::IMGUI))))) {
            spdlog::error("[D3D12] Failed to create the imgui render target.");
            return false;
        }

        m_d3d12.get_rt(D3D12::RTV::IMGUI)->SetName(L"Framework::m_d3d12.rts[IMGUI]");

        if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &d3d12_rt_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
                IID_PPV_ARGS(&m_d3d12.get_rt(D3D12::RTV::BLANK))))) {
            spdlog::error("[D3D12] Failed to create the blank render target.");
            return false;
        }

        m_d3d12.get_rt(D3D12::RTV::BLANK)->SetName(L"Framework::m_d3d12.rts[BLANK]");

        // Create imgui and blank rtvs and srvs.
        device->CreateRenderTargetView(m_d3d12.get_rt(D3D12::RTV::IMGUI).Get(), nullptr, m_d3d12.get_cpu_rtv(device, D3D12::RTV::IMGUI));
        device->CreateRenderTargetView(m_d3d12.get_rt(D3D12::RTV::BLANK).Get(), nullptr, m_d3d12.get_cpu_rtv(device, D3D12::RTV::BLANK));
        device->CreateShaderResourceView(
            m_d3d12.get_rt(D3D12::RTV::IMGUI).Get(), nullptr, m_d3d12.get_cpu_srv(device, D3D12::SRV::IMGUI_VR));
        device->CreateShaderResourceView(m_d3d12.get_rt(D3D12::RTV::BLANK).Get(), nullptr, m_d3d12.get_cpu_srv(device, D3D12::SRV::BLANK));

        m_d3d12.rt_width = (uint32_t)desc.Width;
        m_d3d12.rt_height = (uint32_t)desc.Height;

        m_last_rt_size = {desc.Width, desc.Height};
    }

    spdlog::info("[D3D12] Initializing ImGui...");

    auto& bb = m_d3d12.get_rt(D3D12::RTV::BACKBUFFER_0);
    auto bb_desc = bb->GetDesc();

    if (!ImGui_ImplDX12_Init(device, 3, bb_desc.Format, m_d3d12.srv_desc_heap.Get(),
            m_d3d12.get_cpu_srv(device, D3D12::SRV::IMGUI_FONT_BACKBUFFER), m_d3d12.get_gpu_srv(device, D3D12::SRV::IMGUI_FONT_BACKBUFFER))) {
        spdlog::error("[D3D12] Failed to initialize ImGui.");
        return false;
    }

    m_d3d12.imgui_backend_datas[0] = ImGui::GetIO().BackendRendererUserData;

    ImGui::GetIO().BackendRendererUserData = nullptr;

    // Now initialize another one for the VR texture.
    auto& bb_vr = m_d3d12.get_rt(D3D12::RTV::IMGUI);
    auto bb_vr_desc = bb_vr->GetDesc();

    if (!ImGui_ImplDX12_Init(device, 3, bb_vr_desc.Format, m_d3d12.srv_desc_heap.Get(),
            m_d3d12.get_cpu_srv(device, D3D12::SRV::IMGUI_FONT_VR), m_d3d12.get_gpu_srv(device, D3D12::SRV::IMGUI_FONT_VR))) {
        spdlog::error("[D3D12] Failed to initialize ImGui.");
        return false;
    }

    m_d3d12.imgui_backend_datas[1] = ImGui::GetIO().BackendRendererUserData;

    return true;
}

void Framework::deinit_d3d12() {
    for (auto userdata : m_d3d12.imgui_backend_datas) {
        if (userdata != nullptr) {
            ImGui::GetIO().BackendRendererUserData = userdata;
            ImGui_ImplDX12_Shutdown();
        }
    }

    ImGui::GetIO().BackendRendererUserData = nullptr;
    m_d3d12 = {};
}

bool Framework::is_advanced_view_enabled() const {
    return FrameworkConfig::get()->is_advanced_mode();
}

Framework::ImGuiThemes Framework::get_imgui_theme_value() const {
    return static_cast<ImGuiThemes>(FrameworkConfig::get()->get_imgui_theme_value());
}