#include "IosSettingsInsets.h"
#if defined(__IOS__)
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <atomic>
#include <mutex>

namespace Paperboat {
SettingsInsets IosSettingsSafeArea() {
    // SDL's window coordinates are also used for ImGui's DisplaySize. Convert
    // UIKit points to those coordinates; don't assume a particular Retina scale.
    static SettingsInsets cached;
    static std::mutex cacheMutex;
    static std::atomic<bool> pending = false;
    SettingsInsets result;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        result = cached;
    }
    if (pending.exchange(true))
        return result;
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (window == nullptr)
        window = SDL_GetWindowFromID(1);
    if (window == nullptr) {
        pending = false;
        return result;
    }
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        pending = false;
        return result;
    }
    UIWindow* uiWindow = info.info.uikit.window;
    if (uiWindow == nil) {
        pending = false;
        return result;
    }
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    auto read = ^{
        const CGSize size = uiWindow.bounds.size;
        const UIEdgeInsets insets = uiWindow.safeAreaInsets;
        const float sx = size.width > 0 ? width / size.width : 1;
        const float sy = size.height > 0 ? height / size.height : 1;
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cached = { float(insets.left * sx), float(insets.top * sy), float(insets.right * sx),
                       float(insets.bottom * sy) };
        }
        pending = false;
    };
    if ([NSThread isMainThread]) {
        read();
        std::lock_guard<std::mutex> lock(cacheMutex);
        result = cached;
    } else {
        // Never synchronously wait for UIKit from the renderer/game thread.
        dispatch_async(dispatch_get_main_queue(), read);
    }
    return result;
}
}
#endif
