#pragma once

namespace Paperboat {
// strength: 0 = light, 1 = medium, 2 = strong. Only does anything on iOS.
#if defined(__IOS__)
void HapticTap(int strength);
#else
inline void HapticTap(int) {
}
#endif
} // namespace Paperboat
