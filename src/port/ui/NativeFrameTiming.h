#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Paperboat::NativeSettings {
struct FrameTimingSnapshot {
    double updatesPerSecond, targetHz, averageMs, longestMs;
    uint64_t lateCallbacks;
};
// Measures main-thread callback delivery, not GPU presentation. Feed monotonic
// wall-clock times so a blocked run loop is visible in the interval statistics.
class FrameTiming {
public:
    void Reset() {
        *this = FrameTiming {};
    }
    std::optional<FrameTimingSnapshot> Sample(double now, double expectedInterval) {
        if (!std::isfinite(now) || !std::isfinite(expectedInterval) || expectedInterval <= 0)
            return std::nullopt;
        if (!started) {
            started = true;
            previous = windowStart = now;
            expected = expectedInterval;
            return std::nullopt;
        }
        if (now <= previous)
            return std::nullopt;
        const double interval = now - previous;
        if (interval > expected * 1.5 + .0005)
            ++late;
        previous = now;
        expected = expectedInterval;
        longest = std::max(longest, interval);
        ++count;
        const double elapsed = now - windowStart;
        if (elapsed < .5)
            return std::nullopt;
        FrameTimingSnapshot result { count / elapsed, 1 / expected, elapsed * 1000 / count, longest * 1000, late };
        windowStart = now;
        count = 0;
        longest = 0;
        return result;
    }

private:
    bool started = false;
    double previous = 0, windowStart = 0, expected = 0, longest = 0;
    uint64_t count = 0, late = 0;
};
} // namespace Paperboat::NativeSettings
