#include "port/ui/NativeControllerNavigation.h"
#include <cassert>
#include <limits>
using namespace Paperboat::NativeSettings;
int main() {
    ControllerRepeat input;
    // A held while opening a menu/dialog cannot activate its first action.
    assert(input.Update(PadAccept, 1) == 0);
    assert(input.Update(PadAccept, 2) == 0);
    assert(input.Update(0, 3) == 0);
    assert(input.Update(PadAccept, 3.01) == PadAccept);
    assert(input.Update(PadAccept, 4) == 0);
    input.Reset();
    assert(input.Update(PadAccept | PadDown, 5) == 0);
    assert(input.Update(0, 6) == 0);
    assert(input.Update(PadDown, 7) == PadDown);
    assert(input.Update(PadDown, 7.34) == 0);
    assert(input.Update(PadDown, 7.36) == PadDown);
    assert(input.Update(PadDown, 7.40) == 0);
    assert(input.Update(PadDown, 7.46) == PadDown);
    // A long blocked frame produces one repeat, never a catch-up burst.
    assert(input.Update(PadDown, 100) == PadDown);
    assert(input.Update(PadDown, 100) == 0);
    assert(input.Update(PadUp, 100.01) == PadUp);
    assert(input.Update(PadUp | PadDown, 100.02) == 0);
    assert(input.Update(PadLeft | PadRight, 100.03) == 0);
    assert(input.Update(0, 101) == 0);
    double now = 102;
    for (auto key : { PadAccept, PadBack, PadMenu, PadSearch, PadDelete, PadPrevious, PadNext }) {
        assert(input.Update(key, now) == key);
        assert(input.Update(key, now + 1) == 0);
        assert(input.Update(0, now + 2) == 0);
        now += 3;
    }
    // Invalid/backwards clocks and lifecycle resets disarm all held buttons.
    assert(input.Update(PadBack, 1) == 0);
    assert(input.Update(0, 2) == 0);
    assert(input.Update(PadBack, 3) == PadBack);
    assert(input.Update(PadBack, std::numeric_limits<double>::infinity()) == 0);
    assert(input.Update(PadAccept, 4) == 0);
    assert(input.Update(0, 5) == 0);
    assert(input.Update(PadAccept, 6) == PadAccept);
}
