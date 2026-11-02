#include <fast/backends/IosWindowPlacement.h>
#include <cassert>
#include <iostream>

// Test-only SDL display fixtures. Production uses public SDL APIs exclusively.
// Link SDL2 statically: its shared library hides these internal fixture helpers.
extern "C" int SDL_AddBasicVideoDisplay(const SDL_DisplayMode* mode);
extern "C" void SDL_DelVideoDisplay(int displayIndex);
extern "C" void SDL_ResetDisplayModes(int displayIndex);

static void AddDisplay(int width, int height) {
    SDL_DisplayMode mode {};
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = width;
    mode.h = height;
    mode.refresh_rate = 60;
    assert(SDL_AddBasicVideoDisplay(&mode) >= 0);
}

static void ResetDisplays(int width, int height, bool airplay) {
    while (SDL_GetNumVideoDisplays() > 0) {
        const int index = SDL_GetNumVideoDisplays() - 1;
        SDL_ResetDisplayModes(index);
        SDL_DelVideoDisplay(index);
    }
    AddDisplay(width, height);
    if (airplay)
        AddDisplay(1920, 1080);
}

static void CheckWindow(int phoneWidth, int phoneHeight, SDL_Rect saved, bool airplay) {
    ResetDisplays(phoneWidth, phoneHeight, airplay);
    Fast::ConfigureIosMainWindow(saved.x, saved.y, saved.w, saved.h);
    assert(saved.w == phoneWidth && saved.h == phoneHeight);
    SDL_Window* window = SDL_CreateWindow(
        "iOS window placement check", saved.x, saved.y, saved.w, saved.h, SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS
    );
    assert(window != nullptr);
    assert(SDL_GetWindowDisplayIndex(window) == 0);
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    assert(width == phoneWidth && height == phoneHeight);
    assert(SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) == 0);
    assert(SDL_GetWindowDisplayIndex(window) == 0);
    if (!airplay) {
        // Connecting after creation must also keep the main window on the phone.
        AddDisplay(1920, 1080);
        assert(SDL_GetWindowDisplayIndex(window) == 0);
    }
    SDL_DelVideoDisplay(1);
    assert(SDL_GetWindowDisplayIndex(window) == 0);
    SDL_DestroyWindow(window);
}

int main() {
    assert(SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER, "dummy", SDL_HINT_OVERRIDE) == SDL_TRUE);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    ResetDisplays(852, 393, true);
    const SDL_Rect savedPixels { 100, 100, 2556, 1179 };
    // Reproduce the original failure using SDL's real display selection logic.
    assert(SDL_GetRectDisplayIndex(&savedPixels) == 1);
    SDL_Window* legacy = SDL_CreateWindow(
        "Saved pixel dimensions", savedPixels.x, savedPixels.y, savedPixels.w, savedPixels.h,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS
    );
    assert(legacy != nullptr && SDL_GetWindowDisplayIndex(legacy) == 1);
    SDL_DestroyWindow(legacy);
    CheckWindow(852, 393, savedPixels, true);
    CheckWindow(852, 393, savedPixels, false);
    CheckWindow(393, 852, { 100, 100, 1179, 2556 }, true);
    CheckWindow(1024, 768, { 100, 100, 2048, 1536 }, true);
    CheckWindow(852, 393, { 2000, 800, 3840, 2160 }, true);
    CheckWindow(852, 393, { 100, 100, 852, 393 }, true);
    CheckWindow(852, 393, { 0, 0, 0, 0 }, true);
    SDL_Quit();
    std::cout
        << "PASS: Retina startup regression, phone/tablet orientation, AirPlay before/after launch and disconnect\n";
}
