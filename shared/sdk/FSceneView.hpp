#pragma once

#include "Math.hpp"

namespace sdk {
struct FSceneViewFamily;
struct FSceneViewStateInterface;

template <typename T>
struct FSceneViewProjectionDataT {
    glm::vec<3, T> view_origin{};
    __declspec(align(16)) glm::mat<4, 4, T, glm::defaultp> view_rotation_matrix{};
    __declspec(align(16)) glm::mat<4, 4, T, glm::defaultp> projection_matrix{};

    int32_t view_rect[4];
    int32_t constrained_view_rect[4];
};

template <typename T>
struct FSceneViewInitOptionsT : public FSceneViewProjectionDataT<T> {
    FSceneViewFamily* family{nullptr};
    FSceneViewStateInterface* scene_view_state{nullptr};
    void* actor{nullptr};
    int32_t player_index{};

    void* view_element_drawer{};

    // FLinearColor
    float background_color[4]{};
    float overlay_color[4]{};
    float color_scale[4]{};

    // EStereoscopicPass
    uint32_t stereo_pass{};
};

using FSceneViewInitOptions = FSceneViewInitOptionsT<float>;
using FSceneViewInitOptionsUE4 = FSceneViewInitOptionsT<float>;
using FSceneViewInitOptionsUE5 = FSceneViewInitOptionsT<double>;

struct FSceneView {
    void constructor(const FSceneViewInitOptions* initOptions);
    static std::optional<uintptr_t> get_constructor_address();
};
}