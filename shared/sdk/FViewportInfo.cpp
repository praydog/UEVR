#include <windows.h>

#include <spdlog/spdlog.h>

#include <bdshemu.h>

#include <utility/Emulation.hpp>
#include <utility/Module.hpp>
#include <utility/Scan.hpp>

#include "FViewportInfo.hpp"

namespace sdk {
IViewportRenderTargetProvider* FViewportInfo::get_rt_provider(FRHITexture2D* known_tex) {
    auto resolve_offset = [this](FRHITexture2D* known_tex) -> uint32_t {
        // cannot bruteforce!
        if (known_tex == nullptr) {
            return 0;
        }

        SPDLOG_INFO("Searching for FViewportInfo::GetRenderTargetProvider offset...");

        for (auto i = 0; i < 0x500; i += sizeof(void*)) {
            SPDLOG_INFO(" Examining offset 0x{:x}", i);

            const auto ptr = *(uintptr_t*)((uintptr_t)this + i);

            if (ptr == 0 || IsBadReadPtr((void*)ptr, sizeof(void*))) {
                SPDLOG_INFO("  Invalid pointer at offset 0x{:x}", i);
                continue;
            }

            const auto vtable = *(uintptr_t*)ptr;

            if (vtable == 0 || IsBadReadPtr((void*)vtable, sizeof(void*))) {
                SPDLOG_INFO("  Invalid vtable at offset 0x{:x}", i);
                continue;
            }

            const auto first_func = *(uintptr_t*)vtable;

            if (first_func == 0 || IsBadReadPtr((void*)first_func, sizeof(void*))) {
                SPDLOG_INFO("  Invalid first function at offset 0x{:x}", i);
                continue;
            }
            
            const auto module = utility::get_module_within(first_func);

            if (!module) {
                SPDLOG_INFO("  Invalid module at offset 0x{:x}", i);
                continue;
            }

            const auto rel = first_func - (uintptr_t)*module;
            SPDLOG_INFO("  About to emulate 0x{:x}", rel);
            
            utility::ShemuContext emu{*module};

            emu.ctx->MemThreshold = 100;
            emu.ctx->Registers.RegRcx = ptr;
            emu.ctx->Registers.RegRip = first_func;

            while(true) {
                if (emu.emulate() != SHEMU_SUCCESS || emu.ctx->InstructionsCount > 100) {
                    break;
                }

                // Disassemble the IP and skip over any call instructions and just make them return true.
                // Reason being, bdshemu cannot emulate Windows API calls, so we just make the relevant function return true.
                const auto ix = utility::decode_one((uint8_t*)emu.ctx->Registers.RegRip);

                if (!ix) {
                    break;
                }

                if (std::string_view{ix->Mnemonic}.starts_with("CALL")) {
                    SPDLOG_INFO("  Found call at 0x{:x}, skipping it", emu.ctx->Registers.RegRip);
                    emu.ctx->Registers.RegRip += ix->Length;
                    emu.ctx->Registers.RegRax = 1;
                    continue;
                }
            }

            SPDLOG_INFO("  Emu status: {}", emu.status);

            const auto rax = emu.ctx->Registers.RegRax;

            if (rax != 0 && !IsBadReadPtr((void*)rax, sizeof(void*) + 0x20)) {
                const auto resource = (sdk::FSlateResource*)rax;

                if (auto result = utility::scan_ptr((uintptr_t)resource + sizeof(void*), 0x20, (uintptr_t)known_tex)) {
                    FSlateResource::resource_offset = *result - (uintptr_t)resource;
                    SPDLOG_INFO("  Found FViewportInfo::GetRenderTargetProvider offset: 0x{:x}", i);
                    SPDLOG_INFO("  Found FSlateResource::Resource offset: 0x{:x}", FSlateResource::resource_offset);
                    return i;
                }
            } else {
                SPDLOG_INFO("  Invalid return value at offset 0x{:x} ({:x})", i, rax);
            }
        }
        
        SPDLOG_INFO("Failed to find FViewportInfo::GetRenderTargetProvider offset!");

        return 0x0; // PLACEHOLDER
    };

    static uint32_t offset = resolve_offset(known_tex);

    // Keep trying to resolve it until we get a valid offset.
    if (offset == 0) {
        offset = resolve_offset(known_tex);

        if (offset == 0) {
            SPDLOG_ERROR("Failed to resolve FViewportInfo::GetRenderTargetProvider offset!");
            return nullptr;
        }
    }

    return *(IViewportRenderTargetProvider**)((uintptr_t)this + offset);
}

FRHITexture2D*& FSlateResource::get_mutable_resource() {
    return *(FRHITexture2D**)((uintptr_t)this + resource_offset);
}
}