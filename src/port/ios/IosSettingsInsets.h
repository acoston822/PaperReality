#pragma once

namespace Paperboat {
struct SettingsInsets {
    float left = 0, top = 0, right = 0, bottom = 0;
};
#if defined(__IOS__)
SettingsInsets IosSettingsSafeArea();
#else
inline SettingsInsets IosSettingsSafeArea() {
    return { };
}
#endif
}
