// Standalone platform reproduction: SDL and generated colors, no game code/assets.
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <fast/backends/IosWindowPlacement.h>

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    int x = 100, y = 100, width = 640, height = 480;
    Fast::ConfigureIosMainWindow(x, y, width, height);
    Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_METAL;
    if ([[NSProcessInfo.processInfo arguments] containsObject:@"--adaptive"])
        flags |= SDL_WINDOW_RESIZABLE;
    SDL_Window* window = SDL_CreateWindow("iPad window probe", x, y, width, height, flags);
    if (!window)
        return 2;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        return 3;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info))
        return 4;
    UIWindow* native = info.info.uikit.window;
    UIView* root = native.rootViewController.view;
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectMake(30, 70, 650, 180)];
    label.numberOfLines = 0;
    label.textColor = UIColor.whiteColor;
    label.font = [UIFont monospacedSystemFontOfSize:15 weight:UIFontWeightRegular];
    label.accessibilityIdentifier = @"WindowGeometry";
    [root addSubview:label];
    bool running = true;
    while (running) {
        @autoreleasepool {
            SDL_Event event;
            while (SDL_PollEvent(&event))
                if (event.type == SDL_QUIT)
                    running = false;
            SDL_GetWindowSize(window, &width, &height);
            CGRect screen = native.screen.bounds;
            CGRect bounds = native.bounds;
            NSDictionary* geometry = @{
                @"screenWidth" : @(screen.size.width),
                @"screenHeight" : @(screen.size.height),
                @"windowWidth" : @(bounds.size.width),
                @"windowHeight" : @(bounds.size.height),
                @"sdlWidth" : @(width),
                @"sdlHeight" : @(height),
                @"os" : UIDevice.currentDevice.systemVersion,
                @"scene" : @(native.windowScene != nil),
                @"requiresFullscreen" :
                    @([NSBundle.mainBundle objectForInfoDictionaryKey:@"UIRequiresFullScreen"] != nil &&
                      [[NSBundle.mainBundle objectForInfoDictionaryKey:@"UIRequiresFullScreen"] boolValue])
            };
            NSData* data = [NSJSONSerialization dataWithJSONObject:geometry options:NSJSONWritingSortedKeys error:nil];
            NSString* value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            if (![label.text isEqualToString:value]) {
                label.text = value;
                label.accessibilityValue = value;
            }
            label.frame = CGRectMake(30, 70, MAX(100, bounds.size.width - 60), 180);
            SDL_SetRenderDrawColor(renderer, 20, 60, 100, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
