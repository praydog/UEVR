#include <spdlog/spdlog.h>
#include <bddisasm.h>

#include <utility/Scan.hpp>

#include "EngineModule.hpp"
#include "UEngine.hpp"

namespace sdk {
UEngine* UEngine::get() {
    static auto engine = []() -> UEngine** {
        spdlog::info("Attempting to locate GEngine...");

        const auto module = sdk::get_ue_module(L"Engine");
        const auto calibrate_tilt_fn = utility::find_function_from_string_ref(module, L"CALIBRATEMOTION");

        if (!calibrate_tilt_fn) {
            spdlog::error("Failed to find CalibrateTilt function!");
            return (UEngine**)nullptr;
        }

        spdlog::info("CalibrateTilt function: {:x}", (uintptr_t)*calibrate_tilt_fn);

        // Use bddisasm to find the first ptr mov into a register
        uint8_t* ip = (uint8_t*)*calibrate_tilt_fn;

        for (auto i = 0; i < 50; ++i) {
            INSTRUX ix{};
            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                spdlog::info("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM && ix.Operands[1].Info.Memory.IsRipRel) {
                const auto offset = ix.Operands[1].Info.Memory.Disp;
                const auto result = (UEngine**)((uint8_t*)ip + ix.Length + offset);

                spdlog::info("Found GEngine at {:x}", (uintptr_t)result);
                return result;
            }

            ip += ix.Length;
        }

        spdlog::error("Failed to find GEngine!");
        return nullptr;
    }();

    if (engine == nullptr) {
        return nullptr;
    }

    return *engine;
}

void UEngine::initialize_hmd_device() {

}
}