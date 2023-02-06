#include <string_view>
#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>

#include "EngineModule.hpp"

#include "DynamicRHI.hpp"

namespace sdk {
FDynamicRHI* FDynamicRHI::get() {
    static FDynamicRHI** instance = []() -> FDynamicRHI** {
        SPDLOG_INFO("Searching for FDynamicRHI instance...");

        // Scans for a string reference inside InitNullRHI
        const auto module = sdk::get_ue_module(L"RHI");
        const auto str = utility::scan_string(module, L"NullDrvFailure");

        if (!str) {
            SPDLOG_ERROR("Failed to find FDynamicRHI instance (no string)");
            return nullptr;
        }

        const auto ref = utility::scan_displacement_reference(module, *str);

        if (!ref) {
            SPDLOG_ERROR("Failed to find FDynamicRHI instance (no string reference)");
            return nullptr;
        }

        const auto resolved = utility::resolve_instruction(*ref);

        if (!resolved) {
            SPDLOG_ERROR("Failed to find FDynamicRHI instance (cannot resolve instruction)");
            return nullptr;
        }

        auto ip = (uint8_t*)resolved->addr;

        for (auto i = 0; i < 100; ++i) {
            const auto ix = utility::decode_one(ip);

            if (!ix) {
                SPDLOG_ERROR("Failed to find FDynamicRHI instance (failed to decode instruction @ {:x})", (uintptr_t)ip);
                return nullptr;
            }

            if (std::string_view{ix->Mnemonic}.starts_with("MOV") && 
                ix->Operands[0].Type == ND_OP_MEM &&
                ix->Operands[1].Type == ND_OP_REG && 
                ix->Operands[1].Info.Register.Reg == NDR_RAX &&
                ix->Operands[1].Size == sizeof(void*)) 
            {
                const auto result = utility::resolve_displacement((uintptr_t)ip);
                if (result) {
                    SPDLOG_INFO("Found FDynamicRHI instance at {:x} (referenced at {:x})", *result, (uintptr_t)ip);
                    return (FDynamicRHI**)*result;
                }

                SPDLOG_ERROR("Failed to find FDynamicRHI instance @ {:x}", (uintptr_t)ip);
            }

            ip += ix->Length;
        }

        return nullptr;
    }();
    
    if (instance == nullptr) {
        return nullptr;
    }

    return *instance;
}

std::unique_ptr<FTexture2DRHIRef>
FDynamicRHI::create_texture_2d(
    uintptr_t command_list,
    uint32_t w,
    uint32_t h,
    uint8_t format,
    uint32_t mips,
    uint32_t samples,
    ETextureCreateFlags flags,
    FCreateInfo& create_info) 
{
    return nullptr; // TODO
}
}