#pragma once

#include <cstdint>

namespace Paperboat {
// AirPlay / external display: while a second screen is connected, the game image (without the
// touch controls or menus) is shown on it and the phone keeps the normal screen. Only does anything on iOS.
#if defined(__IOS__)
// Starts watching for screens connecting and disconnecting. Call once after the window exists.
void IosExternalDisplayInit();
// Published by UIKit; zero when no external output is attached.
uint32_t IosExternalDisplayRefreshRate();
#else
inline void IosExternalDisplayInit() {
}
#endif
} // namespace Paperboat
