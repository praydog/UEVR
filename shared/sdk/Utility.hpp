#pragma once

#include <string_view>

// Utility functions that don't seem necessary to put into kananlib
// because of the very specific use cases they're used for

namespace sdk {
// This just scans the functions for a specific pattern
// that may seem simple (and maybe dumb) at first glance
// but the function actually emulates the code to find the
// real function in some cases, as some obfuscated games
// will not work if you just plain linearly scan for the pattern
// so it performs emulation until the instruction pointer lands
// on a set of instructions that match the pattern
bool is_vfunc_pattern(uintptr_t addr, std::string_view pattern);

VS_FIXEDFILEINFO get_file_version_info();
bool check_file_version(uint32_t ms, uint32_t ls);
}