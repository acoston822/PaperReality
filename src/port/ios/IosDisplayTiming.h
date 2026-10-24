#pragma once

#include <algorithm>
#include <cstdint>

namespace Paperboat {
// External presentation shares the game render thread. Asking it to submit
// 120 interpolated frames to a 60 Hz AirPlay output can hold up game progress.
// A zero external rate means disconnected; never change the saved preference.
constexpr uint32_t IosPresentationFrameRate(uint32_t requested, uint32_t externalRefreshRate) {
    if (externalRefreshRate == 0)
        return requested;
    // The engine's original game rate is 30 Hz; interpolation cannot target less.
    return std::min(requested, std::max(30U, externalRefreshRate));
}
} // namespace Paperboat
