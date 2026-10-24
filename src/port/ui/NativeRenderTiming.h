#pragma once

#include <algorithm>
#include <cmath>

namespace Paperboat::NativeSettings {
struct RenderTimingSnapshot {
    double renderPeakMs = 0, drawablePeakMs = 0;
    int gameTargetFps = 0;
    bool metalPacing = true;
};
// Main-thread wall times, not CPU/GPU execution times. Exclude the cooperative
// UIKit wait from render work; drawable acquisition remains part of render work.
class RenderTiming {
public:
    void Reset() {
        *this = RenderTiming {};
    }
    void Begin(double now, int targetFps, bool paced) {
        active = std::isfinite(now);
        started = now;
        servicedWait = 0;
        snapshot.gameTargetFps = targetFps;
        snapshot.metalPacing = paced;
    }
    void RecordUiWait(double seconds) {
        if (active && std::isfinite(seconds) && seconds > 0)
            servicedWait += seconds;
    }
    void RecordDrawableWait(double seconds) {
        if (std::isfinite(seconds) && seconds >= 0)
            snapshot.drawablePeakMs = std::max(snapshot.drawablePeakMs, seconds * 1000);
    }
    void End(double now) {
        if (active && std::isfinite(now) && now >= started)
            snapshot.renderPeakMs = std::max(snapshot.renderPeakMs, std::max(0., now - started - servicedWait) * 1000);
        active = false;
    }
    RenderTimingSnapshot Take() {
        auto result = snapshot;
        snapshot.renderPeakMs = snapshot.drawablePeakMs = 0;
        return result;
    }

private:
    bool active = false;
    double started = 0, servicedWait = 0;
    RenderTimingSnapshot snapshot;
};
}
