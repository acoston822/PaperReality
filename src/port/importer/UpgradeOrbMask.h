#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PM64 {
// This effect consumes an intensity texture as both colour and coverage. A
// grayscale RGBA replacement can carry a non-zero alpha pedestal in black areas.
// Cap coverage by intensity, retaining RGB and any deliberately lower alpha.
// Validate the entire image before changing anything; coloured art is untouched.
inline bool NormalizeUpgradeOrbMaskAlpha(std::string_view path, uint8_t* rgba, size_t bytes) {
    constexpr std::string_view name = "effects/effect_gfx_motion_blur_flame/tex_0";
    if (path.starts_with("alt/")) {
        path.remove_prefix(4);
    }
    if (path != name || rgba == nullptr || bytes == 0 || bytes % 4 != 0) {
        return false;
    }
    for (size_t i = 0; i < bytes; i += 4) {
        if (rgba[i] != rgba[i + 1] || rgba[i] != rgba[i + 2]) {
            return false;
        }
    }
    bool changed = false;
    for (size_t i = 0; i < bytes; i += 4) {
        const uint8_t alpha = std::min(rgba[i + 3], rgba[i]);
        changed |= alpha != rgba[i + 3];
        rgba[i + 3] = alpha;
    }
    return changed;
}
} // namespace PM64
