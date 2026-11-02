#include "port/importer/UpgradeOrbMask.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main() {
    constexpr const char* path = "alt/effects/effect_gfx_motion_blur_flame/tex_0";
    // Representative inputs are synthetic; no game or pack bytes are embedded.
    std::array<uint8_t, 24> pixels = {
        0,0,0,27, 32,32,32,70, 128,128,128,96,
        255,255,255,255, 0,0,0,0, 64,64,64,64
    };
    const auto before = pixels;
    require(PM64::NormalizeUpgradeOrbMaskAlpha(path, pixels.data(), pixels.size()), "pedestal should be corrected");
    require(pixels[3] == 0, "black background must be fully transparent");
    require(pixels[7] == 32, "coverage must not exceed intensity");
    require(pixels[11] == 96, "already lower alpha must be retained");
    require(pixels[15] == 255, "bright core must be retained");
    for (size_t i = 0; i < pixels.size(); i += 4) {
        require(!std::memcmp(pixels.data()+i, before.data()+i, 3), "HD RGB must not change");
    }
    require(!PM64::NormalizeUpgradeOrbMaskAlpha(path, pixels.data(), pixels.size()), "normalization must be idempotent");

    auto root = before;
    require(PM64::NormalizeUpgradeOrbMaskAlpha("effects/effect_gfx_motion_blur_flame/tex_0", root.data(), root.size()), "root replacement path supported");
    for (const char* other : {"alt/effects/effect_gfx_motion_blur_flame/tex_100", "alt/sprites/goombario", "alt/alt/effects/effect_gfx_motion_blur_flame/tex_0", "effects/effect_gfx_motion_blur_flame/tex_0/extra"}) {
        auto untouched = before;
        require(!PM64::NormalizeUpgradeOrbMaskAlpha(other, untouched.data(), untouched.size()), "other assets must be excluded");
        require(untouched == before, "other asset bytes changed");
    }
    auto coloured = before;
    coloured[coloured.size()-2] = 1;
    const auto colouredBefore = coloured;
    require(!PM64::NormalizeUpgradeOrbMaskAlpha(path, coloured.data(), coloured.size()), "coloured image must be excluded");
    require(coloured == colouredBefore, "validation must not partially modify an image");
    require(!PM64::NormalizeUpgradeOrbMaskAlpha(path, nullptr, 4), "null rejected");
    require(!PM64::NormalizeUpgradeOrbMaskAlpha(path, pixels.data(), 0), "empty rejected");
    auto truncated = before;
    require(!PM64::NormalizeUpgradeOrbMaskAlpha(path, truncated.data(), truncated.size()-1), "truncated pixels rejected");
    require(truncated == before, "truncated data changed");

    // Exhaustively verify every intensity/alpha pair in an HD-sized grayscale image.
    std::vector<uint8_t> ramp(256*256*4);
    for (int intensity = 0; intensity < 256; ++intensity) {
        for (int alpha = 0; alpha < 256; ++alpha) {
            const size_t p = (intensity*256 + alpha)*4;
            ramp[p] = ramp[p+1] = ramp[p+2] = intensity;
            ramp[p+3] = alpha;
        }
    }
    PM64::NormalizeUpgradeOrbMaskAlpha(path, ramp.data(), ramp.size());
    for (int intensity = 0; intensity < 256; ++intensity) {
        for (int alpha = 0; alpha < 256; ++alpha) {
            const size_t p = (intensity*256 + alpha)*4;
            require(ramp[p] == intensity && ramp[p+1] == intensity && ramp[p+2] == intensity, "HD RGB ramp changed");
            require(ramp[p+3] <= alpha && ramp[p+3] <= intensity, "coverage invariant failed");
            require(ramp[p+3] == (alpha < intensity ? alpha : intensity), "coverage result failed");
        }
    }
    puts("PASS: targeted HD coverage preserves RGB/lower alpha, removes black pedestal, and excludes other/invalid/coloured assets");
}
