#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include "EngineModule.hpp"

#include "FRenderTargetPool.hpp"

namespace sdk {
std::optional<uintptr_t> FRenderTargetPool::get_find_free_element_fn() {
    static auto result = []() -> std::optional<uintptr_t> {
        SPDLOG_INFO("Attempting to find FRenderTargetPool::FindFreeElement");

        std::vector<std::wstring> potential_modules {
            L"Renderer",
            L"RenderCore"
        };

        for (const auto& module_name : potential_modules) {
            SPDLOG_INFO("Scanning module {} for FRenderTargetPool::FindFreeElement", utility::narrow(module_name));
            const auto module = sdk::get_ue_module(module_name);

            if (module == nullptr) {
                SPDLOG_INFO("Failed to find module {}, skipping", utility::narrow(module_name));
                continue;
            }

            // Other strings can be used if this one falls flat later on
            const auto scene_depth_z_strs = utility::scan_strings(module, L"SceneDepthZ", true);

            if (scene_depth_z_strs.empty()) {
                SPDLOG_ERROR("Failed to find SceneDepthZ string");
                continue;
            }

            SPDLOG_INFO("{} SceneDepthZ strings found", scene_depth_z_strs.size());

            for (auto scene_depth_z_str : scene_depth_z_strs) {
                SPDLOG_INFO("Found SceneDepthZ string at {:x}", scene_depth_z_str);

                const auto scene_depth_z_ref = utility::scan_displacement_reference(module, scene_depth_z_str);

                if (!scene_depth_z_ref) {
                    SPDLOG_ERROR("Failed to find reference to SceneDepthZ string");
                    continue;
                }

                const auto scene_depth_z_resolved = utility::resolve_instruction(*scene_depth_z_ref);

                if (!scene_depth_z_resolved) {
                    SPDLOG_ERROR("Failed to find resolve instruction from SceneDepthZ reference");
                    continue;
                }

                const auto scene_depth_z_post_instruction = scene_depth_z_resolved->addr + scene_depth_z_resolved->instrux.Length;

                // Scan forward for a call instruction, which is the call to FRenderTargetPool::FindFreeElement
                // We're going to do a linear disassembly scan here, looking for any kind of call
                // In modular builds, it calls via an import table, so we can't just scan for E8 calls
                std::optional<uintptr_t> find_free_element{};

                utility::exhaustive_decode((uint8_t*)scene_depth_z_post_instruction, 20, [&](INSTRUX& instr, uintptr_t ip) {
                    if (find_free_element) {
                        return utility::ExhaustionResult::BREAK;
                    }

                    if (std::string_view{ instr.Mnemonic }.starts_with("CALL")) {
                        if (*(uint8_t*)ip == 0xE8) {
                            SPDLOG_INFO("Found E8 call at {}", ip);
                            find_free_element = utility::calculate_absolute(ip + 1);
                            return utility::ExhaustionResult::BREAK;
                        } else if (instr.Operands[0].Type == ND_OP_MEM) {
                            SPDLOG_INFO("Found pointer-based call at {}", ip);
                            const auto ptr = utility::resolve_displacement(ip);

                            if (ptr) {
                                find_free_element = *(uintptr_t*)*ptr;
                                return utility::ExhaustionResult::BREAK;
                            } else {
                                SPDLOG_ERROR("Failed to resolve displacement at {}", ip);
                            }
                        } else {
                            SPDLOG_ERROR("Found unknown call at {}", ip);
                        }

                        return utility::ExhaustionResult::STEP_OVER;
                    }

                    return utility::ExhaustionResult::CONTINUE;
                });

                if (!find_free_element) {
                    SPDLOG_ERROR("Failed to find call to FRenderTargetPool::FindFreeElement");
                    continue;
                }

                SPDLOG_INFO("Found FRenderTargetPool::FindFreeElement at {:x}", *find_free_element);

                return find_free_element;
            }
        }

        return std::nullopt;
    }();

    return result;
}
}