#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/Emulation.hpp>

#include <spdlog/spdlog.h>

#include <bdshemu.h>

#include "Utility.hpp"

namespace sdk {
bool is_vfunc_pattern(uintptr_t addr, std::string_view pattern) {
    if (utility::scan(addr, 100, pattern.data()).value_or(0) == addr) {
        return true;
    }

    // In some very rare cases, there can be obfuscation that makes the above scan fail.
    // One such form of obfuscation is multiple jmps and/or calls to reach the real function.
    // We can use emulation to find the real function.
    const auto module_within = utility::get_module_within(addr);

    if (!module_within) {
        SPDLOG_ERROR("Cannot perform emulation, module not found to create pseudo shellcode");
        return false;
    }

    SPDLOG_INFO("Performing emulation to find real function...");

    utility::ShemuContext ctx{ *module_within };

    ctx.ctx->Registers.RegRip = addr;
    bool prev_was_branch = true;

    do {
        prev_was_branch = ctx.ctx->InstructionsCount == 0 || ctx.ctx->Instruction.BranchInfo.IsBranch;

        if (prev_was_branch) {
            SPDLOG_INFO("Branch!");
        }

        SPDLOG_INFO("Emulating at {:x}", ctx.ctx->Registers.RegRip);

        if (prev_was_branch && !IsBadReadPtr((void*)ctx.ctx->Registers.RegRip, 4)) {
            if (utility::scan(ctx.ctx->Registers.RegRip, 100, pattern.data()).value_or(0) == ctx.ctx->Registers.RegRip) {
                SPDLOG_INFO("Encountered true vfunc at {:x}", ctx.ctx->Registers.RegRip);
                return true;
            }
        }
    } while(ctx.emulate() == SHEMU_SUCCESS && ctx.ctx->InstructionsCount < 50);

    SPDLOG_ERROR("Failed to find true vfunc at {:x}, reason {}", addr, ctx.status);
    return false;
};

VS_FIXEDFILEINFO get_file_version_info() {
    try {
        const auto full_path = utility::get_module_pathw(utility::get_executable());

        if (!full_path) {
            SPDLOG_ERROR("Failed to get executable path");
            return {};
        }

        const auto file_version_size = GetFileVersionInfoSizeW(full_path->c_str(), nullptr);

        if (file_version_size == 0) {
            SPDLOG_ERROR("Failed to get file version info size");
            return {};
        }

        std::vector<uint8_t> file_version_data(file_version_size);
        GetFileVersionInfoW(full_path->c_str(), 0, file_version_size, file_version_data.data());

        UINT size{};
        VS_FIXEDFILEINFO* fixed_file_info{};

        if (VerQueryValueA(file_version_data.data(), "\\", (LPVOID*)&fixed_file_info, &size) && fixed_file_info != nullptr) {
            SPDLOG_INFO("MS: {:x}, LS: {:x}", fixed_file_info->dwFileVersionMS, fixed_file_info->dwFileVersionLS);
            return *fixed_file_info;
        } else {
            SPDLOG_ERROR("Failed to get file version info");
        }
    } catch(...) {
        SPDLOG_ERROR("Failed to get file version info");
    }

    return {};
}

bool check_file_version(uint32_t ms, uint32_t ls) {
    try {
        const auto full_path = utility::get_module_pathw(utility::get_executable());

        if (!full_path) {
            SPDLOG_ERROR("Failed to get executable path");
            return false;
        }

        const auto file_version_size = GetFileVersionInfoSizeW(full_path->c_str(), nullptr);

        if (file_version_size == 0) {
            SPDLOG_ERROR("Failed to get file version info size");
            return false;
        }

        std::vector<uint8_t> file_version_data(file_version_size);
        GetFileVersionInfoW(full_path->c_str(), 0, file_version_size, file_version_data.data());

        UINT size{};
        VS_FIXEDFILEINFO* fixed_file_info{};

        if (VerQueryValueA(file_version_data.data(), "\\", (LPVOID*)&fixed_file_info, &size) && fixed_file_info != nullptr) {
            SPDLOG_INFO("MS: {:x}, LS: {:x}", fixed_file_info->dwFileVersionMS, fixed_file_info->dwFileVersionLS);

            if (fixed_file_info->dwFileVersionMS == ms && fixed_file_info->dwFileVersionLS == ls) {
                SPDLOG_INFO("Found matching executable");
                return true;
            } else {
                SPDLOG_INFO("File does not match requested version");
            }
        } else {
            SPDLOG_ERROR("Failed to get file version info");
        }
    } catch(...) {
        SPDLOG_ERROR("Failed to get file version info");
    }

    return false;
}
}