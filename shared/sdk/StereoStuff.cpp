#include <spdlog/spdlog.h>
#include <windows.h>

#include <utility/Module.hpp>

#include "StereoStuff.hpp"

// The first pointer returned that we run into
// that has a vtable pointing into d3d11 or d3d12.dll
// is GetNativeResource.
void* FRHITexture::get_native_resource() const {
    static auto vtable_index = [](const FRHITexture* tex) -> size_t {
        spdlog::info("Attempting to get vtable index for FRHITexture::GetNativeResource");

        auto vtable = *(void* (***)(const FRHITexture*, void*))tex;

        if (vtable == 0) {
            spdlog::error("FRHITexture vtable is null");
            return 0;
        }

        spdlog::info(" Vtable: {:x}", (uintptr_t)vtable);

        // Start at 1 to skip over the destructor.
        for (auto i = 1; i < 15; ++i) {
            spdlog::info(" Examining index {}", i);
            const auto func = vtable[i];

            if (func == nullptr || IsBadReadPtr(func, 1)) {
                spdlog::error("Encountered bad function pointer at index {} before finding GetNativeResource", i);
                throw std::runtime_error("Encountered bad function pointer before finding GetNativeResource");
                return 0;
            }

            // Pass in dummy data in-case the function has an out parameter
            // so we don't cause a segfault.
            uint8_t filler_data[0x100]{};

            const auto potential_resource = func(tex, &filler_data);

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

            // create a lowercase copy of the module path
            auto module_path_lower = std::string(*module_path);
            std::transform(module_path_lower.begin(), module_path_lower.end(), module_path_lower.begin(), ::tolower);

            spdlog::info(" Function at {} returned a pointer with a vtable in {}", i, *module_path);

            if (module_path_lower.ends_with("d3d11.dll") || 
                module_path_lower.ends_with("d3d12.dll") || 
                module_path_lower.ends_with("d3d12core.dll") ||
                module_path_lower.ends_with("dxgi.dll")) 
            {
                spdlog::info(" Found vtable index {} for FRHITexture::GetNativeResource", i);
                return i;
            }
        }

        
        spdlog::error("Failed to find vtable index for FRHITexture::GetNativeResource");
        throw std::runtime_error("Could not find vtable index for FRHITexture::GetNativeResource");
        return 0;
    }(this);

    const auto vtable = *(void* (***)(const FRHITexture*))this;
    const auto func = vtable[vtable_index];

    return func(this);
}