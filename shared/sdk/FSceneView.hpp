#pragma once

namespace sdk {
struct FSceneViewInitOptions;

struct FSceneView {
    void constructor(const FSceneViewInitOptions* initOptions);
    static std::optional<uintptr_t> get_constructor_address();
};
}