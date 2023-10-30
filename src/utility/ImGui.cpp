#include <imgui.h>
#include <imgui_internal.h>

#include <glm/vec2.hpp>

#include "ImGui.hpp"

namespace imgui {
bool is_point_intersecting_any(float x, float y) {
    const ImGuiIO& io = ImGui::GetIO();
    const auto ctx = io.Ctx;

    if (ctx == nullptr) {
        return false;
    }

    for (int n = 0; n < ctx->Windows.Size; n++) {
        const auto window = ctx->Windows[n];
        const auto pos = glm::vec2{window->Pos.x, window->Pos.y};
        const auto size = glm::vec2{window->Size.x, window->Size.y};
        const auto mx = pos + size;

        if (window->WasActive && window->Active) {
            if (x >= window->Pos.x && x <= window->Pos.x + window->Size.x &&
                y >= window->Pos.y && y <= window->Pos.y + window->Size.y)
            {
                return true;
            }
        }
    }

    return false;
}
}