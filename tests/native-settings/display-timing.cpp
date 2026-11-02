#include "port/ios/IosDisplayTiming.h"
#include <fast/backends/FramePacer.h>
#include <cassert>
#include <cmath>

int main() {
    using Paperboat::IosPresentationFrameRate;
    // Connecting AirPlay must not double the work for a TV that cannot present it.
    assert(IosPresentationFrameRate(120, 60) == 60);
    assert(IosPresentationFrameRate(120, 30) == 30);
    assert(IosPresentationFrameRate(240, 60) == 60);
    // Preserve lower user targets and genuinely faster external displays.
    assert(IosPresentationFrameRate(30, 60) == 30);
    assert(IosPresentationFrameRate(60, 120) == 60);
    assert(IosPresentationFrameRate(120, 120) == 120);
    assert(IosPresentationFrameRate(120, 50) == 50);
    // Disconnect restores the original phone target without changing preferences.
    assert(IosPresentationFrameRate(120, 0) == 120);
    assert(IosPresentationFrameRate(60, 0) == 60);
    assert(IosPresentationFrameRate(30, 0) == 30);
    assert(IosPresentationFrameRate(120, 24) == 30);
    Fast::FramePacer pacer;
    double now = 100;
    for (const unsigned external : { 0U, 60U, 30U, 0U }) {
        const auto fps = IosPresentationFrameRate(120, external);
        // Changing output resets the game's pacing; no stale wait/catch-up burst.
        assert(pacer.NextDeadline(now, fps) == now);
        assert(std::abs(pacer.NextDeadline(now + .001, fps) - (now + 1. / fps)) < 1e-10);
        now += .002;
    }
}
