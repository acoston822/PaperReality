#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace Paperboat::NativeSettings {
enum ControllerKey : uint32_t {
    PadUp = 1 << 0,
    PadDown = 1 << 1,
    PadLeft = 1 << 2,
    PadRight = 1 << 3,
    PadAccept = 1 << 4,
    PadBack = 1 << 5,
    PadPrevious = 1 << 6,
    PadNext = 1 << 7,
    PadMenu = 1 << 8,
    PadSearch = 1 << 9,
    PadDelete = 1 << 10
};

// Only directions repeat. A held button must never confirm a newly opened
// dialog, bind a second control, or close the menu it just opened.
class ControllerRepeat {
    uint32_t previous = 0;
    uint32_t direction = 0;
    double next = 0, last = 0;
    bool armed = false;

public:
    void Reset() {
        previous = direction = 0;
        next = last = 0;
        armed = false;
    }
    uint32_t Update(uint32_t held, double now) {
        if (!std::isfinite(now)) {
            Reset();
            return 0;
        }
        if (now < last)
            Reset();
        last = now;
        if (!armed) {
            armed = held == 0;
            previous = held;
            return 0;
        }
        uint32_t pressed = held & ~previous;
        previous = held;
        const uint32_t axes = held & (PadUp | PadDown | PadLeft | PadRight);
        // Opposing/diagonal directions cannot generate a burst of changes.
        uint32_t current = 0;
        for (uint32_t key : { uint32_t(PadUp), uint32_t(PadDown), uint32_t(PadLeft), uint32_t(PadRight) })
            if (axes == key)
                current = key;
        pressed &= ~(PadUp | PadDown | PadLeft | PadRight);
        if (current != direction) {
            direction = current;
            next = now + .35;
            pressed |= current;
        } else if (current && now >= next) {
            pressed |= current;
            next = now + .09;
        }
        return pressed;
    }
};
} // namespace Paperboat::NativeSettings
