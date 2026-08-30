#pragma once

#include <algorithm>
#include <array>

#include <windows.h>

// Shared chroma-void geometry for the passthrough key feature. D3D11_RECT and D3D12_RECT are both
// RECT-layout, so one set of helpers serves both backends and their math cannot drift.
namespace vrmod::chroma {
constexpr LONG PAD_PX = 4;
// Black ring at the projection layer's outer edge so a cropped-FOV boundary blends black-on-black
// instead of flashing the key during reprojection.
constexpr LONG EDGE_GUARD_PX = 16;

// Border pad clamped so tiny render targets can never produce inverted rects.
// NB: (std::min) parenthesized -- windows.h min/max macros may be live in including TUs.
inline LONG clamped_pad(LONG w, LONG h) {
    const LONG limit = (std::min)(w / 4, h / 4);
    return (std::max)(0L, (std::min)(PAD_PX, limit));
}

// The four edge strips of a w*h surface, `pad` px thick.
inline std::array<RECT, 4> border_ring(LONG w, LONG h, LONG pad) {
    return {{
        {0, 0, w, pad},
        {0, h - pad, w, h},
        {0, pad, pad, h - pad},
        {w - pad, pad, w, h - pad},
    }};
}

// Interior key rect for one eye's void, inset `guard` px inside the SUBMITTED region (the
// view_bounds crop of the texture), not the texture edge -- otherwise a projection-override crop
// larger than the guard would discard the whole black ring. Guard is clamped against the region.
inline RECT guard_interior(const float bounds[4], LONG eye_w, LONG h, LONG guard, LONG x_off) {
    const LONG l = x_off + (LONG)(bounds[0] * (float)eye_w);
    const LONG r = x_off + (LONG)(bounds[1] * (float)eye_w);
    const LONG t = (LONG)(bounds[2] * (float)h);
    const LONG b = (LONG)(bounds[3] * (float)h);
    const LONG limit = (std::min)((r - l) / 4, (b - t) / 4);
    const LONG g = (std::max)(0L, (std::min)(guard, limit));
    return {l + g, t + g, r - g, b - g};
}
} // namespace vrmod::chroma
