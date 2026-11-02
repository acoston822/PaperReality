#pragma once

#include "NativeSettings.h"
#include <algorithm>
#include <cmath>

namespace Paperboat::NativeSettings {
inline double ThemeLuminance(ThemeColor color) {
    auto linear = [](double channel) {
        return channel <= .04045 ? channel / 12.92 : std::pow((channel + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.red) + .7152 * linear(color.green) + .0722 * linear(color.blue);
}
inline double ThemeContrast(ThemeColor first, ThemeColor second) {
    const double a = ThemeLuminance(first), b = ThemeLuminance(second);
    return (std::max(a, b) + .05) / (std::min(a, b) + .05);
}
inline ThemeColor ThemeForeground(ThemeColor background) {
    return ThemeLuminance(background) > .179 ? ThemeColor { 0, 0, 0 } : ThemeColor { 1, 1, 1 };
}
// Keep the original palette on filled controls and the touch cursor. Text and
// thin accents may need a lighter/darker tint on the native page background.
inline ThemeColor AccessibleThemeColor(ThemeColor color, ThemeColor background) {
    if (ThemeContrast(color, background) >= 4.5)
        return color;
    const auto target = ThemeForeground(background);
    auto mix = [&](double amount) -> ThemeColor {
        return { float(color.red + (target.red - color.red) * amount),
                 float(color.green + (target.green - color.green) * amount),
                 float(color.blue + (target.blue - color.blue) * amount) };
    };
    double low = 0, high = 1;
    for (int i = 0; i < 20; ++i) {
        const double middle = (low + high) / 2;
        if (ThemeContrast(mix(middle), background) >= 4.5)
            high = middle;
        else
            low = middle;
    }
    return mix(high);
}
} // namespace Paperboat::NativeSettings
