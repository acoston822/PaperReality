#pragma once

#include "NativeSettings.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace Paperboat::NativeSettings::Portrait {
// Positions and bounds stay in the game's coordinate system. Pixel dimensions
// belong to the loaded resource and can be larger (or differ between parts).
struct Layer {
    int x, y, z, width, height;
    Image image;
};

inline Image Assemble(std::vector<Layer> layers) {
    if (layers.empty())
        return { };
    int64_t left = std::numeric_limits<int64_t>::max(), top = left;
    int64_t right = std::numeric_limits<int64_t>::min(), bottom = right;
    double scale = 1;
    for (const auto& layer : layers) {
        const auto& image = layer.image;
        if (layer.width <= 0 || layer.height <= 0 || image.width <= 0 || image.height <= 0)
            return { };
        const uint64_t count = uint64_t(image.width) * image.height;
        if (count > 4096 * 4096 || image.rgba.size() < count * 4)
            return { };
        left = std::min(left, int64_t(layer.x));
        top = std::min(top, int64_t(layer.y));
        right = std::max(right, int64_t(layer.x) + layer.width);
        bottom = std::max(bottom, int64_t(layer.y) + layer.height);
        scale = std::max({ scale, double(image.width) / layer.width, double(image.height) / layer.height });
    }
    const int64_t width = right - left, height = bottom - top;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return { };
    // Bound temporary memory for unusually large packs. The actual 44pt Retina
    // thumbnail is produced only after composition, never per component.
    scale = std::min(scale, 2048. / std::max(width, height));
    Image result;
    result.width = std::max(1L, std::lround(width * scale));
    result.height = std::max(1L, std::lround(height * scale));
    result.rgba.resize(size_t(result.width) * result.height * 4);
    std::stable_sort(layers.begin(), layers.end(), [](const Layer& a, const Layer& b) { return a.z < b.z; });
    for (const auto& layer : layers) {
        const int x0 = std::clamp<long>(std::lround((layer.x - left) * scale), 0, result.width);
        const int y0 = std::clamp<long>(std::lround((layer.y - top) * scale), 0, result.height);
        const int x1 = std::clamp<long>(std::lround((layer.x - left + layer.width) * scale), 0, result.width);
        const int y1 = std::clamp<long>(std::lround((layer.y - top + layer.height) * scale), 0, result.height);
        for (int y = y0; y < y1; ++y) {
            const int sy = int(int64_t(y - y0) * layer.image.height / (y1 - y0));
            for (int x = x0; x < x1; ++x) {
                const int sx = int(int64_t(x - x0) * layer.image.width / (x1 - x0));
                const auto* src = &layer.image.rgba[(size_t(sy) * layer.image.width + sx) * 4];
                auto* dst = &result.rgba[(size_t(y) * result.width + x) * 4];
                const uint32_t sa = src[3], da = dst[3];
                const uint32_t alpha = sa * 255 + da * (255 - sa);
                if (!sa)
                    continue;
                // Straight-alpha source-over preserves antialiased HD edges,
                // including when one translucent component overlaps another.
                for (int channel = 0; channel < 3; ++channel)
                    dst[channel] = (src[channel] * sa * 255 + dst[channel] * da * (255 - sa) + alpha / 2) / alpha;
                dst[3] = (alpha + 127) / 255;
            }
        }
    }
    return result;
}
} // namespace Paperboat::NativeSettings::Portrait
