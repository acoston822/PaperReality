#include <fast/backends/FramePacer.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static void Near(double actual, double expected) {
    assert(std::abs(actual - expected) < 1e-7);
}

int main() {
    // Sleep overshoot varies every frame. It must not accumulate into the next
    // deadline, including at fractional intervals and high uptime timestamps.
    for (const unsigned fps : { 30U, 50U, 59U, 60U, 120U, 144U, 240U, 360U }) {
        for (const double epoch : { 10., 1000000. }) {
            Fast::FramePacer pacer;
            Near(pacer.NextDeadline(epoch, fps), epoch);
            double previous = epoch;
            for (int frame = 1; frame <= 10000; ++frame) {
                const double oversleep = .00001 * (frame % 7);
                const double work = .2 / fps;
                const double now = previous + oversleep + work;
                const double deadline = pacer.NextDeadline(now, fps);
                Near(deadline, epoch + static_cast<double>(frame) / fps);
                assert(deadline > now);
                previous = deadline;
            }
        }
    }
    Fast::FramePacer pacer;
    Near(pacer.NextDeadline(0, 60), 0);
    Near(pacer.NextDeadline(.001, 60), 1. / 60);
    // A missed deadline or long suspension restarts the cadence. The following
    // frame must wait a whole interval, rather than catching up in a burst.
    Near(pacer.NextDeadline(.080, 60), .080);
    Near(pacer.NextDeadline(.081, 60), .080 + 1. / 60);
    Near(pacer.NextDeadline(100, 60), 100);
    Near(pacer.NextDeadline(100.001, 60), 100 + 1. / 60);
    Near(pacer.NextDeadline(101, 120), 101);
    Near(pacer.NextDeadline(101.001, 120), 101 + 1. / 120);
    // An FPS change, explicit pacing-mode reset or clock discontinuity must
    // never retain a deadline from the previous session/rate.
    Near(pacer.NextDeadline(5, 120), 5);
    Near(pacer.NextDeadline(5.001, 120), 5 + 1. / 120);
    pacer.Reset();
    Near(pacer.NextDeadline(6, 120), 6);
    Near(pacer.NextDeadline(6.001, 120), 6 + 1. / 120);
    Near(pacer.NextDeadline(7, 0), 7);
    Near(pacer.NextDeadline(8, 60), 8);
    Near(pacer.NextDeadline(std::numeric_limits<double>::quiet_NaN(), 60), 0);
    Near(pacer.NextDeadline(9, 60), 9);
    std::cout << "PASS: absolute 30-360 FPS deadlines, fractional cadence, wake jitter, late frames, "
                 "suspension, rate/mode changes and invalid samples\n";
}
