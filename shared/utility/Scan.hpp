#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <bddisasm.h>
#include <Windows.h>

namespace utility {
    std::optional<uintptr_t> scan(const std::string& module, const std::string& pattern);
    std::optional<uintptr_t> scan(const std::string& module, uintptr_t start, const std::string& pattern);
    std::optional<uintptr_t> scan(HMODULE module, const std::string& pattern);
    std::optional<uintptr_t> scan(uintptr_t start, size_t length, const std::string& pattern);
    std::optional<uintptr_t> scan_reverse(uintptr_t start, size_t length, const std::string& pattern);
    
    std::optional<uintptr_t> scan_data(HMODULE, const uint8_t* data, size_t size);
    std::optional<uintptr_t> scan_data(uintptr_t start, size_t length, const uint8_t* data, size_t size);
    std::optional<uintptr_t> scan_data_reverse(uintptr_t start, size_t length, const uint8_t* data, size_t size);
    std::optional<uintptr_t> scan_ptr(HMODULE module, uintptr_t ptr);
    std::optional<uintptr_t> scan_ptr(uintptr_t start, size_t length, uintptr_t ptr);
    std::optional<uintptr_t> scan_string(HMODULE module, const std::string& str);
    std::optional<uintptr_t> scan_string(HMODULE module, const std::wstring& str);

    std::optional<uintptr_t> scan_reference(HMODULE module, uintptr_t ptr, bool relative = true);
    std::optional<uintptr_t> scan_reference(uintptr_t start, size_t length, uintptr_t ptr, bool relative = true);
    std::optional<uintptr_t> scan_relative_reference_strict(HMODULE module, uintptr_t ptr, const std::string& preceded_by);
    std::optional<uintptr_t> scan_relative_reference_displacement(HMODULE module, uintptr_t ptr);

    std::optional<uintptr_t> scan_opcode(uintptr_t ip, size_t num_instructions, uint8_t opcode);
    std::optional<uintptr_t> scan_disasm(uintptr_t ip, size_t num_instructions, const std::string& pattern);
    std::optional<uintptr_t> scan_mnemonic(uintptr_t ip, size_t num_instructions, const std::string& mnemonic);

    uint32_t get_insn_size(uintptr_t ip);

    uintptr_t calculate_absolute(uintptr_t address, uint8_t custom_offset = 4);

    std::optional<INSTRUX> decode_one(uint8_t* ip);

    std::optional<uintptr_t> find_function_start(uintptr_t middle);
    std::optional<uintptr_t> find_function_from_string_ref(HMODULE module, std::wstring_view str);

    // Same as the previous, but it keeps going upwards until utility::scan_ptr returns something
    std::optional<uintptr_t> find_virtual_function_start(uintptr_t middle);
    std::optional<uintptr_t> find_virtual_function_from_string_ref(HMODULE module, std::wstring_view str);

    std::optional<uintptr_t> resolve_displacement(uintptr_t ip);

    struct Resolved {
        uintptr_t addr{};
        INSTRUX instrux{};
    };
    std::optional<Resolved> resolve_instruction(uintptr_t middle); // finds the start of the instruction given an address in the middle of the instruction 
}
