#pragma once

namespace Paperboat {
// AirPlay / external display: while a second screen is connected and this is enabled, the game image (without the
// touch controls or menus) is shown on it and the phone keeps the normal screen. Only does anything on iOS.
#if defined(__IOS__)
// Starts watching for screens connecting and disconnecting. Call once after the window exists.
void IosExternalDisplayInit(bool enabled);
// The setting changed.
void IosExternalDisplaySetEnabled(bool enabled);
#else
inline void IosExternalDisplayInit(bool) {
}
inline void IosExternalDisplaySetEnabled(bool) {
}
#endif
} // namespace Paperboat
