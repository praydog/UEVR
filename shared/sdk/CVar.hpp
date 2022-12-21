#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include <windows.h>

namespace sdk {
// In some games, likely due to obfuscation, the cvar description is missing
// so we must do an alternative scan for the cvar name itself, which is a bit tougher
// because the cvar name is usually referenced in multiple places, whereas
// the description is only referenced once, in the cvar registration function
std::optional<uintptr_t> find_alternate_cvar_ref(std::wstring_view str, uint32_t known_default, HMODULE module);

std::optional<uintptr_t> resolve_cvar_from_address(uintptr_t start, std::wstring_view str, bool stop_at_first_mov = false);
std::optional<uintptr_t> find_cvar_by_description(std::wstring_view str, std::wstring_view cvar_name, uint32_t known_default, HMODULE module, bool stop_at_first_mov = false);

std::optional<uintptr_t> find_cvar(std::wstring_view module, std::wstring_view name, bool stop_at_first_mov = false);

namespace rendering {
std::optional<uintptr_t> get_one_frame_thread_lag_cvar();
}

namespace vr {
std::optional<uintptr_t> get_enable_stereo_emulation_cvar();
std::optional<uintptr_t> get_slate_draw_to_vr_render_target_real_cvar();
std::optional<uintptr_t> get_slate_draw_to_vr_render_target_cvar();
std::optional<uintptr_t> get_slate_draw_to_vr_render_target_usage_location();
}
}