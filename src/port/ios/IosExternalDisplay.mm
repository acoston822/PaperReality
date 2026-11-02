#include "IosExternalDisplay.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cmath>
#include <atomic>

// In libultraship's Metal backend. Passing nullptr stops the second screen.
extern "C" void LUS_SetExternalDisplayLayer(void* layer);

@interface PRMetalView : UIView
@end

@implementation PRMetalView
+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (void)didMoveToWindow {
    [super didMoveToWindow];
    [self setNeedsLayout];
}

- (void)layoutSubviews {
    [super layoutSubviews];
    if (self.window == nil || CGRectIsEmpty(self.bounds)) {
        return;
    }

    // UIKit can resize the root view after the AirPlay window becomes visible.
    // Match its final bounds, rather than keeping the screen size from Attach().
    const CGFloat scale = self.window.screen.scale;
    if (self.contentScaleFactor != scale) {
        self.contentScaleFactor = scale;
    }
    CAMetalLayer* layer = (CAMetalLayer*) self.layer;
    const CGSize pixels = CGSizeMake(std::ceil(self.bounds.size.width * scale),
                                     std::ceil(self.bounds.size.height * scale));
    if (!CGSizeEqualToSize(layer.drawableSize, pixels)) {
        layer.drawableSize = pixels;
    }
}
@end

namespace Paperboat {
namespace {

bool sObserving = false;
UIWindow* sWindow = nil;
std::atomic<uint32_t> sRefreshRate { 0 };

void Detach() {
    sRefreshRate.store(0, std::memory_order_relaxed);
    if (sWindow == nil) {
        return;
    }
    LUS_SetExternalDisplayLayer(nullptr);
    sWindow.hidden = YES;
    sWindow = nil;
}

void Attach(UIScreen* screen) {
    const uint32_t refreshRate =
        screen.maximumFramesPerSecond > 0 ? static_cast<uint32_t>(screen.maximumFramesPerSecond) : 60;
    if (sWindow != nil && sWindow.screen == screen) {
        if (!CGRectEqualToRect(sWindow.frame, screen.bounds)) {
            sWindow.frame = screen.bounds;
        }
        [sWindow.rootViewController.view setNeedsLayout];
        sRefreshRate.store(refreshRate, std::memory_order_relaxed);
        return;
    }
    Detach();

    UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
    window.screen = screen;
    window.backgroundColor = [UIColor blackColor];

    PRMetalView* view = [[PRMetalView alloc] initWithFrame:window.bounds];
    view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    CAMetalLayer* layer = (CAMetalLayer*) view.layer;

    UIViewController* controller = [[UIViewController alloc] init];
    controller.view = view;
    window.rootViewController = controller;
    window.hidden = NO;
    [window layoutIfNeeded];
    [view layoutIfNeeded];
    sWindow = window;

    LUS_SetExternalDisplayLayer((__bridge void*) layer);
    sRefreshRate.store(refreshRate, std::memory_order_relaxed);
}

// Main thread only.
void Refresh() {
    NSArray<UIScreen*>* screens = [UIScreen screens];
    UIScreen* external = screens.count > 1 ? screens[1] : [UIScreen mainScreen].mirroredScreen;
    if (external != nil && external != [UIScreen mainScreen]) {
        Attach(external);
    } else {
        Detach();
    }
}

} // namespace

uint32_t IosExternalDisplayRefreshRate() {
    return sRefreshRate.load(std::memory_order_relaxed);
}

void IosExternalDisplayInit() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!sObserving) {
            sObserving = true;
            NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
            [center addObserverForName:UIScreenDidConnectNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Refresh();
                            }];
            [center addObserverForName:UIScreenDidDisconnectNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Refresh();
                            }];
            [center addObserverForName:UIScreenModeDidChangeNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Refresh();
                            }];
            // Some iOS versions don't post the connect notification for AirPlay mirroring, so also check regularly.
            [NSTimer scheduledTimerWithTimeInterval:2.0
                                            repeats:YES
                                              block:^(NSTimer*) {
                                                  Refresh();
                                              }];
        }
        Refresh();
    });
}

} // namespace Paperboat

#endif
