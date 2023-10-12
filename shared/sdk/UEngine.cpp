#include <spdlog/spdlog.h>
#include <bddisasm.h>
#include <bdshemu.h>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/Emulation.hpp>

#include "CVar.hpp"

#include "EngineModule.hpp"
#include "UGameViewportClient.hpp"
#include "UEngine.hpp"
#include "UGameEngine.hpp"

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

ULocalPlayer* UEngine::get_localplayer(int32_t index) {
    if (index < 0) {
        return nullptr;
    }

    const auto& game_instance = get_property<sdk::UObject*>(L"GameInstance");

    if (game_instance == nullptr) {
        return nullptr;
    }

    const auto& localplayers = game_instance->get_property<sdk::TArray<sdk::ULocalPlayer*>>(L"LocalPlayers");

    if (localplayers.count <= index) {
        return nullptr;
    }

    return localplayers.data[index];
}

APawn* UEngine::get_localpawn(int32_t index) {
    const auto player = (UObject*)get_localplayer(index);

    if (player == nullptr) {
        return nullptr;
    }

    const auto player_controller = (UObject*)player->get_property<sdk::APlayerController*>(L"PlayerController");

    if (player_controller == nullptr) {
        return nullptr;
    }

    return player_controller->get_property<sdk::APawn*>(L"AcknowledgedPawn");
}

UWorld* UEngine::get_world() {
    const auto localplayer = (sdk::UObject*)get_localplayer(0);

    if (localplayer == nullptr) {
        return nullptr;
    }

    const auto viewport_client = localplayer->get_property<sdk::UObject*>(L"ViewportClient");

    if (viewport_client == nullptr) {
        return nullptr;
    }

    return viewport_client->get_property<sdk::UWorld*>(L"World");
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

            for (auto ref = utility::scan_displacement_reference(mod, *str); ref; ref = utility::scan_displacement_reference(*ref + 4, (end - (*ref + 4)) - 0x1000, *str)) {
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

std::optional<uintptr_t> UEngine::get_stereo_rendering_device_offset() {
    static const auto offset = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Searching for StereoRenderingDevice offset...");

        const auto game_viewport_client_draw = UGameViewportClient::get_draw_function();

        // We use UGameViewportClient::Draw to find the first function call that takes the GEngine in RCX.
        // This function is GEngine->IsStereoscopic3D, which accesses the StereoRenderingDevice.
        // Through some simple analysis, we can find the offset of StereoRenderingDevice via this function.
        // The function is called near the very top of UGameViewportClient::Draw, so it should be reasonably easy to find.
        if (!game_viewport_client_draw) {
            SPDLOG_ERROR("Failed to find UGameViewportClient::Draw function!");
            return std::nullopt;
        }

        const auto engine = sdk::UEngine::get_lvalue();

        if (engine == nullptr || *engine == nullptr) {
            SPDLOG_ERROR("Failed to find UGameEngine!");
            return std::nullopt;
        }

        std::optional<uintptr_t> result{};
        bool is_next_call_isstereoscopic3d = false;

        // Linearly disassemble all instructions in UGameViewportClient::Draw.
        // When we hit a function that takes GEngine in RCX, we know that the next function call is IsStereoscopic3D.
        // We will then emulate that function to find accesses to any offsets within GEngine.
        utility::exhaustive_decode((uint8_t*)*game_viewport_client_draw, 50, [&](INSTRUX& ix, uintptr_t ex_ip) -> utility::ExhaustionResult {
            if (result) {
                return utility::ExhaustionResult::BREAK;
            }

            // mov rcx, addr
            if (const auto disp = utility::resolve_displacement(ex_ip); disp) {
                // the second expression catches UE dynamic/debug builds
                if (*disp == (uintptr_t)engine || 
                    (!IsBadReadPtr((void*)*disp, sizeof(void*)) && *(uintptr_t*)*disp == (uintptr_t)*engine)) 
                {
                    SPDLOG_INFO("Found GEngine in RCX at {:x}", ex_ip);
                    is_next_call_isstereoscopic3d = true;
                    return utility::ExhaustionResult::CONTINUE;
                }
            }

            if (ix.BranchInfo.IsBranch && !ix.BranchInfo.IsConditional && std::string_view{ix.Mnemonic}.starts_with("CALL")) {
                // Skip this call if it's not IsStereoscopic3D (GEngine in RCX right before the call).
                if (!is_next_call_isstereoscopic3d) {
                    return utility::ExhaustionResult::STEP_OVER;
                }

                is_next_call_isstereoscopic3d = false;

                // Emulate from this call now and find the first instruction that accesses the GEngine from any register
                // Use the offset it accesses as the offset to StereoRenderingDevice.
                const auto is_stereoscopic_3d_fn = utility::calculate_absolute(ex_ip + 1);
                auto emu = utility::ShemuContext{*utility::get_module_within(is_stereoscopic_3d_fn)};

                emu.ctx->Registers.RegRax = 0;
                emu.ctx->Registers.RegRip = (ND_UINT64)is_stereoscopic_3d_fn;
                emu.ctx->Registers.RegRcx = (ND_UINT64)*engine;
                emu.ctx->Registers.RegRdx = 0; // Null viewport on purpose
                emu.ctx->MemThreshold = 100;

                while(true) try {
                    if (emu.ctx->InstructionsCount > 200) {
                        break;
                    }

                    const auto ip = emu.ctx->Registers.RegRip;
                    const auto bytes = (uint8_t*)ip;
                    const auto decoded = utility::decode_one((uint8_t*)ip);

                    if (decoded) {
                        const auto is_call = std::string_view{decoded->Mnemonic}.starts_with("CALL");

                        if (decoded->MemoryAccess & ND_ACCESS_ANY_WRITE || is_call) {
                            SPDLOG_INFO("Skipping write to memory instruction at {:x} ({:x} bytes, landing at {:x})", ip, decoded->Length, ip + decoded->Length);
                            emu.ctx->Registers.RegRip += decoded->Length;
                            emu.ctx->Instruction = *decoded; // pseudo-emulate the instruction
                            ++emu.ctx->InstructionsCount;
                            continue;
                        } else if (emu.emulate() != SHEMU_SUCCESS) { // only emulate the non-memory write instructions
                            SPDLOG_INFO("Emulation failed at {:x} ({:x} bytes, landing at {:x})", ip, decoded->Length, ip + decoded->Length);
                            // instead of just adding it onto the RegRip, we need to use the ip we had previously from the decode
                            // because the emulator can move the instruction pointer after emulate() is called
                            emu.ctx->Registers.RegRip = ip + decoded->Length;
                            ++emu.ctx->InstructionsCount;
                            continue;
                        }

                        // Check if instruction is mov reg1, [reg2 + offset]
                        // Check if reg2 is GEngine and grab the offset
                        const auto& emu_ix = emu.ctx->Instruction;

                        if (emu_ix.Instruction == ND_INS_MOV && 
                            emu_ix.Operands[0].Type == ND_OP_REG && 
                            emu_ix.Operands[1].Type == ND_OP_MEM &&
                            emu_ix.Operands[1].Info.Memory.HasBase &&
                            emu_ix.Operands[1].Info.Memory.HasDisp) 
                        {
                            const auto reg1 = emu_ix.Operands[0].Info.Register.Reg;
                            const auto reg2 = emu_ix.Operands[1].Info.Memory.Base;
                            const auto offset = emu_ix.Operands[1].Info.Memory.Disp;

                            const auto regs = (uintptr_t*)&emu.ctx->Registers;
                            const auto reg_value = regs[reg2];

                            if (reg_value == (uintptr_t)*engine) {
                                SPDLOG_INFO("Found GEngine->StereoRenderingDevice access at {:x} (offset: {:x})", ip, offset);
                                result = offset;
                                break;
                            }
                        }
                    }
                } catch(...) {
                    SPDLOG_ERROR("Encountered exception while emulating IsStereoscopic3D!");
                    return utility::ExhaustionResult::BREAK;
                }

                return utility::ExhaustionResult::BREAK;
            }

            return utility::ExhaustionResult::CONTINUE;
        });

        return result;
    }();

    return offset;
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

        auto result = utility::find_virtual_function_start(*enable_stereo_emulation_cvar_ref);

        if (result) {
            const auto engine = UGameEngine::get();

            // Verify that the result lands within the vtable.
            if (engine != nullptr && !IsBadReadPtr(*(void**)engine, 200 * sizeof(void*))) {
                const auto vtable = *(uintptr_t*)engine;
                auto vtable_within = utility::scan_ptr(vtable, 200 * sizeof(void*), *result);

                if (!vtable_within) {
                    SPDLOG_ERROR("Found \"InitializeHMDDevice\" function at {:x}, but it is not within the vtable at {:x}!", *result, vtable);
                    result = std::nullopt;
                }
            }
        }

        if (!result) try {
            SPDLOG_ERROR("Failed to find InitializeHMDDevice function, falling back to vtable walk.");

            const auto engine = UGameEngine::get();

            if (engine != nullptr && !IsBadReadPtr(*(void**)engine, sizeof(void*))) {
                const auto vtable = *(uintptr_t*)engine;

                // Traverse the vtable and look for a function whose execution path falls within the reference to r.EnableStereoEmulation.
                // Luckily the function is a virtual, so we don't need to worry about some random function
                // calling it (like one of the engine startup functions). Since we're not actually emulating,
                // and only doing an extensive disassembly, we won't be following indirect calls, so we can
                // be sure that the function we find is the one we're looking for.
                result = utility::find_encapsulating_virtual_function(vtable, 200, *enable_stereo_emulation_cvar_ref);
                if (result) {
                    SPDLOG_INFO("Found \"InitializeHMDDevice\" function at {:x}, within the vtable at {:x}!", *result, vtable);
                }
            }
        } catch(...) {
            SPDLOG_ERROR("Exception occurred while attempting to find InitializeHMDDevice function.");
            result = std::nullopt;  
        }

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