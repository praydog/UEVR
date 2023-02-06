#include <unordered_set>

#include <spdlog/spdlog.h>
#include <windows.h>

#include <utility/Module.hpp>

#include "StereoStuff.hpp"

// The first pointer returned that we run into
// that has a vtable pointing into d3d11 or d3d12.dll
// is GetNativeResource.
void* FRHITexture::get_native_resource() const {
    static std::optional<uintptr_t> offset_to_resource{}; // this is some shit that happens on D3D12 on old UE versions
    static auto vtable_index = [](const FRHITexture* tex) -> size_t {
        SPDLOG_INFO("Attempting to get vtable index for FRHITexture::GetNativeResource");

        auto vtable = *(void* (***)(const FRHITexture*, void*))tex;

        if (vtable == 0) {
            SPDLOG_ERROR("FRHITexture vtable is null");
            return 0;
        }

        const auto vtable_module = utility::get_module_within(vtable);

        if (vtable_module) {
            SPDLOG_INFO(" Vtable: {:x} (rel {:x})", (uintptr_t)vtable, (uintptr_t)vtable - (uintptr_t)*vtable_module);
        } else {
            SPDLOG_INFO(" Vtable: {:x} (could not detect module)", (uintptr_t)vtable);
        }

        std::optional<int> first_index_that_wasnt_self{};
        std::unordered_set<uintptr_t> seen_vtables{};

        // Start at 1 to skip over the destructor.
        // Start at 2 to skip over some weird function that also calls the destructor.
        for (auto i = 2; i < 15; ++i) {
            SPDLOG_INFO(" Examining index {}", i);

            const auto addr_of_func = (uintptr_t)&vtable[i];
            const auto func = vtable[i];

            if (seen_vtables.contains(addr_of_func) || func == nullptr) {
                SPDLOG_INFO("Reached the end of the vtable, falling back to first index that wasn't self: {}", *first_index_that_wasnt_self);
                return *first_index_that_wasnt_self;
            }

            if (func == nullptr || IsBadReadPtr(func, 1)) {
                SPDLOG_ERROR("Encountered bad function pointer at index {} before finding GetNativeResource", i);
                throw std::runtime_error("Encountered bad function pointer before finding GetNativeResource");
                return 0;
            }

            // Pass in dummy data in-case the function has an out parameter
            // so we don't cause a segfault.
            uint8_t filler_data[0x100]{};

            void* potential_resource = nullptr;

            try {
                potential_resource = func(tex, &filler_data);
            } catch(...) {
                SPDLOG_ERROR("Encountered exception when calling function at index {}", i);
                continue;
            }

            if (potential_resource == nullptr || IsBadReadPtr(potential_resource, sizeof(void*))) {
                continue;
            }

            const auto potential_vtable = *(void**)potential_resource;

            if (potential_vtable == nullptr || IsBadReadPtr(potential_vtable, sizeof(void*))) {
                continue;
            }

            const auto module_within = utility::get_module_within(potential_vtable);

            if (!module_within) {
                continue;
            }

            const auto module_path = utility::get_module_path(*module_within);

            if (!module_path) {
                continue;
            }

            seen_vtables.insert((uintptr_t)potential_vtable);
            
            if (potential_vtable != vtable && !first_index_that_wasnt_self) {
                first_index_that_wasnt_self = i;
                SPDLOG_INFO(" First index that wasn't self: {}", i);
            }

            if (potential_vtable == vtable) {
                SPDLOG_INFO(" Index {} returned a pointer with the same vtable as the FRHITexture", i);
                continue;
            }

            // create a lowercase copy of the module path
            auto module_path_lower = std::string(*module_path);
            std::transform(module_path_lower.begin(), module_path_lower.end(), module_path_lower.begin(), ::tolower);

            SPDLOG_INFO(" Function at {} returned a pointer with a vtable in {}", i, *module_path);
            SPDLOG_INFO(" vtable: {:x} (rel {:x})", (uintptr_t)potential_vtable, (uintptr_t)potential_vtable - (uintptr_t)*module_within);

            auto is_vtable_d3d = [](std::string_view module_path_lower) {
                return module_path_lower.ends_with("d3d11.dll") || 
                module_path_lower.ends_with("d3d12.dll") || 
                module_path_lower.ends_with("d3d12core.dll") ||
                module_path_lower.ends_with("dxgi.dll") ||
                module_path_lower.ends_with("d3d12sdklayers.dll") ||
                module_path_lower.ends_with("d3d11_1sdklayers.dll") ||
                module_path_lower.ends_with("d3d11_2sdklayers.dll") ||
                module_path_lower.ends_with("d3d11_3sdklayers.dll") ||
                module_path_lower.ends_with("d3d11on12.dll");
            };

            // Standard path for semi-newer UE versions
            if (is_vtable_d3d(module_path_lower)) {
                SPDLOG_INFO(" Found vtable index {} for FRHITexture::GetNativeResource", i);
                return i;
            }

            const auto returned_object = potential_resource;

            // now... to check if this is running old UE
            // we need to actually scan the memory of the returned object itself for a pointer to a vtable
            for (uintptr_t offset = sizeof(void*); offset <= 0x30; offset += sizeof(void*)) {
                const auto potential_resource = *(void**)((uintptr_t)returned_object + offset);

                if (potential_resource == nullptr || IsBadReadPtr(potential_resource, sizeof(void*))) {
                    continue;
                }

                const auto potential_vtable = *(void***)potential_resource;

                if (potential_vtable == nullptr || IsBadReadPtr(potential_vtable, sizeof(void*))) {
                    continue;
                }

                const auto module_within = utility::get_module_within(potential_vtable);

                if (!module_within) {
                    continue;
                }

                const auto module_path = utility::get_module_path(*module_within);

                if (!module_path) {
                    continue;
                }

                // create a lowercase copy of the module path
                auto module_path_lower = std::string(*module_path);
                std::transform(module_path_lower.begin(), module_path_lower.end(), module_path_lower.begin(), ::tolower);

                SPDLOG_INFO(" Offset {} contains a pointer to a vtable in {}", offset, *module_path);
                SPDLOG_INFO(" vtable: {:x} (rel {:x})", (uintptr_t)potential_vtable, (uintptr_t)potential_vtable - (uintptr_t)*module_within);

                if (is_vtable_d3d(module_path_lower)) {
                    SPDLOG_INFO(" Found vtable index {} for FRHITexture::GetNativeResource", i);
                    SPDLOG_INFO(" Old UE detected, offset to resource within wrapper: {}", offset);
                    offset_to_resource = offset;
                    return i;
                }
            }
        }

        
        SPDLOG_ERROR("Failed to find vtable index for FRHITexture::GetNativeResource");
        throw std::runtime_error("Could not find vtable index for FRHITexture::GetNativeResource");
        return 0;
    }(this);

    const auto vtable = *(void* (***)(const FRHITexture*))this;
    const auto func = vtable[vtable_index];

    const auto result = func(this);

    if (result == nullptr) {
        return nullptr;
    }

    if (offset_to_resource) {
        return *(void**)((uintptr_t)result + *offset_to_resource);
    }

    return result;
}