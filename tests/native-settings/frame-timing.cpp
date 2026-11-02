#include "port/ui/NativeFrameTiming.h"
#include "port/ui/NativeRenderTiming.h"
#include <cassert>
#include <iostream>
#include <limits>

namespace N = Paperboat::NativeSettings;
int main() {
    for (const double hz : { 60., 120. }) {
        N::FrameTiming timing;
        std::optional<N::FrameTimingSnapshot> snapshot;
        for (int i = 0; i <= int(hz); ++i)
            if (auto sample = timing.Sample(10 + i / hz, 1 / hz))
                snapshot = sample;
        assert(snapshot);
        assert(std::abs(snapshot->updatesPerSecond - hz) < .01);
        assert(std::abs(snapshot->targetHz - hz) < .01);
        assert(std::abs(snapshot->averageMs - 1000 / hz) < .01);
        assert(std::abs(snapshot->longestMs - 1000 / hz) < .01);
        assert(snapshot->lateCallbacks == 0);
    }
    N::FrameTiming timing;
    assert(!timing.Sample(0, 1. / 120));
    const auto stalled = timing.Sample(.5, 1. / 120);
    assert(stalled && stalled->lateCallbacks == 1 && stalled->longestMs == 500);
    assert(stalled->updatesPerSecond == 2); // Count deliveries, not predicted display-link timestamps.
    assert(!timing.Sample(.5, 1. / 120));
    assert(!timing.Sample(std::numeric_limits<double>::quiet_NaN(), 1. / 120));
    assert(!timing.Sample(1, 0));
    timing.Reset();
    assert(!timing.Sample(100, 1. / 60)); // Background time must not count as a hitch.
    std::optional<N::FrameTimingSnapshot> clean;
    for (int i = 0; i <= 60; ++i)
        if (auto sample = timing.Sample(100 + i / 60., 1. / 60))
            clean = sample;
    assert(clean && clean->lateCallbacks == 0);
    N::RenderTiming render;
    render.Begin(1, 60, true);
    render.RecordUiWait(.012);
    render.RecordDrawableWait(.006);
    render.End(1.020);
    auto work = render.Take();
    assert(std::abs(work.renderPeakMs - 8) < 1e-9); // Exclude UIKit waiting, retain drawable waiting.
    assert(std::abs(work.drawablePeakMs - 6) < 1e-9);
    assert(work.gameTargetFps == 60 && work.metalPacing);
    work = render.Take();
    assert(work.renderPeakMs == 0 && work.drawablePeakMs == 0);
    assert(work.gameTargetFps == 60 && work.metalPacing);
    render.Begin(2, 120, false);
    render.Take(); // A UI sample during an active wait must not discard the frame.
    render.RecordUiWait(.010);
    render.RecordDrawableWait(.002);
    render.End(2.017);
    work = render.Take();
    assert(std::abs(work.renderPeakMs - 7) < 1e-9);
    assert(std::abs(work.drawablePeakMs - 2) < 1e-9);
    assert(work.gameTargetFps == 120 && !work.metalPacing);
    render.Begin(3, 30, true);
    render.RecordUiWait(.020);
    render.End(3.010); // Never report negative work.
    assert(render.Take().renderPeakMs == 0);
    render.Begin(4, 60, true);
    render.Reset(); // Backgrounding may interrupt a frame and its UI wait.
    render.End(100);
    render.RecordUiWait(.5);
    assert(render.Take().renderPeakMs == 0);
    const double invalid = std::numeric_limits<double>::quiet_NaN();
    render.Begin(invalid, 60, true);
    render.RecordDrawableWait(invalid);
    render.RecordDrawableWait(-1);
    render.End(101);
    work = render.Take();
    assert(work.renderPeakMs == 0 && work.drawablePeakMs == 0);
    render.Begin(102, 60, true);
    render.RecordUiWait(invalid);
    render.End(102.008);
    assert(std::abs(render.Take().renderPeakMs - 8) < 1e-9);
    std::cout << "PASS: UI cadence, stalls and reset; render work excludes UI waits, retains drawable waits, "
                 "preserves active samples and resets on backgrounding\n";
}
