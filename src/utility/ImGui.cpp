#include <imgui.h>
#include <imgui_internal.h>

#include "ImGui.hpp"

namespace imgui {
bool is_point_intersecting_any(float x, float y) {
    const ImGuiIO& io = ImGui::GetIO();
    const auto ctx = io.Ctx;

    if (ctx == nullptr) {
        return false;
    }

    for (int i = 0; i < ctx->Windows.Size; i++) {
        const auto window = ctx->Windows[i];

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