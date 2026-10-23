#include "port/ui/NativeFrameTiming.h"
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
    std::cout << "PASS: UI callback cadence, 60/120 Hz timing, stalls, invalid samples and reset\n";
}
