#include <spdlog/spdlog.h>
#include <bddisasm.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>

#include "CVar.hpp"

#include "EngineModule.hpp"
#include "UEngine.hpp"

namespace sdk {
UEngine** UEngine::get_lvalue() {
    static auto engine = []() -> UEngine** {
        SPDLOG_INFO("Attempting to locate GEngine...");

        const auto module = sdk::get_ue_module(L"Engine");
        const auto calibrate_tilt_fn = utility::find_function_from_string_ref(module, L"CALIBRATEMOTION");

        if (!calibrate_tilt_fn) {
            SPDLOG_ERROR("Failed to find CalibrateTilt function!");
            return (UEngine**)nullptr;
        }

        SPDLOG_INFO("CalibrateTilt function: {:x}", (uintptr_t)*calibrate_tilt_fn);

        // Use bddisasm to find the first ptr mov into a register
        uint8_t* ip = (uint8_t*)*calibrate_tilt_fn;

        for (auto i = 0; i < 50; ++i) {
            INSTRUX ix{};
            const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

            if (!ND_SUCCESS(status)) {
                SPDLOG_INFO("Decoding failed with error {:x}!", (uint32_t)status);
                break;
            }

            if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM && ix.Operands[1].Info.Memory.IsRipRel) {
                const auto offset = ix.Operands[1].Info.Memory.Disp;
                const auto result = (UEngine**)((uint8_t*)ip + ix.Length + offset);

                SPDLOG_INFO("Found GEngine at {:x}", (uintptr_t)result);
                return result;
            }

            ip += ix.Length;
        }

        SPDLOG_ERROR("Failed to find GEngine!");
        return nullptr;
    }();

    return engine;
}

UEngine* UEngine::get() {
    auto engine = get_lvalue();

    if (engine == nullptr) {
        return nullptr;
    }

    return *engine;
}

std::optional<uintptr_t> UEngine::get_emulatestereo_string_ref_address() {
    static const auto addr = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Searching for correct string reference to \"emulatestereo\"...");

        const auto mod = sdk::get_ue_module(L"Engine");
        const auto size = utility::get_module_size(mod);
        const auto end = (uintptr_t)mod + *size;

        // String pooling is disabled so we need to do this.
        bool found = false;
        for (auto str = utility::scan_string(mod, L"emulatestereo"); str; str = utility::scan_string(*str + 1, (end - (*str + 1)) - 0x1000, L"emulatestereo")) {
            SPDLOG_INFO("On string at {:x}", (uintptr_t)*str);

            for (auto ref = utility::scan_displacement_reference(mod, *str); ref; ref = utility::scan_displacement_reference(*ref + 1, (end - (*ref + 1)) - 0x1000, *str)) {
                SPDLOG_INFO("On reference at {:x}", (uintptr_t)*ref);

                // InitializeHMDDevice has always been guaranteed to be a virtual function
                // so we can just use find_virtual_function_start.
                // The other function that "emulatestereo" is in is not a virtual function.
                const auto func = utility::find_virtual_function_start(*ref);

                if (func) {
                    return ref;
                }
            }
        }

        SPDLOG_ERROR("Failed to find emulatestereo string reference!");
        return std::nullopt;
    }();

    return addr;
}

std::optional<uintptr_t> UEngine::get_initialize_hmd_device_address() {
    static const auto addr = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Searching for InitializeHMDDevice function...");

        // For older UE versions.
        auto fallback_scan = []() -> std::optional<uintptr_t> {
            SPDLOG_INFO("Performing fallback scan for InitializeHMDDevice function...");

            const auto emulate_stereo_ref = get_emulatestereo_string_ref_address();

            if (emulate_stereo_ref) {
                const auto func = utility::find_virtual_function_start(*emulate_stereo_ref);

                if (func) {
                    SPDLOG_INFO("Found InitializeHMDDevice function at {:x}", *func);
                    return func;
                }
            }

            SPDLOG_ERROR("Failed to find InitializeHMDDevice function!");
            return std::nullopt;
        };

        const auto enable_stereo_emulation_cvar = vr::get_enable_stereo_emulation_cvar();

        if (!enable_stereo_emulation_cvar) {
            SPDLOG_ERROR("Failed to locate r.EnableStereoEmulation cvar, cannot inject stereo rendering device at runtime.");
            return fallback_scan();
        }

        const auto module_within = utility::get_module_within((uintptr_t)enable_stereo_emulation_cvar->address());

        if (!module_within) {
            SPDLOG_ERROR("Failed to find module containing r.EnableStereoEmulation cvar!");
            return fallback_scan();
        }

        SPDLOG_INFO("Module containing r.EnableStereoEmulation cvar: {:x}", (uintptr_t)*module_within);

        std::optional<uintptr_t> enable_stereo_emulation_cvar_ref{};

        for (auto i = 0; i < 3; ++i) {
            enable_stereo_emulation_cvar_ref = utility::scan_displacement_reference(*module_within, enable_stereo_emulation_cvar->address() - (i * sizeof(void*)));

            if (enable_stereo_emulation_cvar_ref) {
                break;
            }
        }

        if (!enable_stereo_emulation_cvar_ref) {
            SPDLOG_ERROR("Failed to find r.EnableStereoEmulation cvar reference!");
            return fallback_scan();
        }

        SPDLOG_INFO("Found r.EnableStereoEmulation cvar reference at {:x}", (uintptr_t)*enable_stereo_emulation_cvar_ref);

        const auto result = utility::find_virtual_function_start(*enable_stereo_emulation_cvar_ref);

        if (result) {
            SPDLOG_INFO("Found InitializeHMDDevice at {:x}", (uintptr_t)*result);
        }

        return result;
    }();

    if (!addr) {
        SPDLOG_ERROR("Failed to locate InitializeHMDDevice function, cannot inject stereo rendering device at runtime.");
        return std::nullopt;
    }

    return addr;
}

void UEngine::initialize_hmd_device() {
    const auto addr = get_initialize_hmd_device_address();

    if (!addr) {
        return;
    }

    auto fn = (void(*)(UEngine*))*addr;
    fn(this);
}
}