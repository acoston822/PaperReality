#include "IosExternalDisplay.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cmath>

// In libultraship's Metal backend. Passing nullptr stops the second screen.
extern "C" void LUS_SetExternalDisplayLayer(void* layer);
// Writes a line to Paperboat.log (defined in TouchControls.cpp).
extern "C" void IosExternalDisplay_Log(const char* message);

@interface PRMetalView : UIView {
    BOOL _loggedLayout;
    CGRect _lastLoggedFrame;
    CGRect _lastLoggedBounds;
    CGFloat _lastLoggedScale;
}
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

    if (!_loggedLayout || !CGRectEqualToRect(_lastLoggedFrame, self.frame) ||
        !CGRectEqualToRect(_lastLoggedBounds, self.bounds) || _lastLoggedScale != scale) {
        _loggedLayout = YES;
        _lastLoggedFrame = self.frame;
        _lastLoggedBounds = self.bounds;
        _lastLoggedScale = scale;
        IosExternalDisplay_Log([[NSString stringWithFormat:
            @"layout window=%@ view=%@ bounds=%@ scale=%.2f drawable=%@",
            NSStringFromCGRect(self.window.bounds), NSStringFromCGRect(self.frame),
            NSStringFromCGRect(self.bounds), scale, NSStringFromCGSize(layer.drawableSize)] UTF8String]);
    }
}
@end

namespace Paperboat {
namespace {

bool sObserving = false;
UIWindow* sWindow = nil;
NSUInteger sLastScreenCount = 0;

void Log(NSString* message) {
    IosExternalDisplay_Log([message UTF8String]);
}

void Detach() {
    if (sWindow == nil) {
        return;
    }
    LUS_SetExternalDisplayLayer(nullptr);
    sWindow.hidden = YES;
    sWindow = nil;
}

void Attach(UIScreen* screen) {
    if (sWindow != nil && sWindow.screen == screen) {
        if (!CGRectEqualToRect(sWindow.frame, screen.bounds)) {
            sWindow.frame = screen.bounds;
        }
        [sWindow.rootViewController.view setNeedsLayout];
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
    Log([NSString stringWithFormat:@"window created: screen=%@ mode=%@ scale=%.2f nativeScale=%.2f iOS=%@ build=%@",
                                   NSStringFromCGRect(screen.bounds), NSStringFromCGSize(screen.currentMode.size),
                                   screen.scale, screen.nativeScale, [UIDevice currentDevice].systemVersion,
                                   [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"]]);

    LUS_SetExternalDisplayLayer((__bridge void*) layer);
}

// Main thread only.
void Refresh() {
    NSArray<UIScreen*>* screens = [UIScreen screens];
    UIScreen* external = screens.count > 1 ? screens[1] : [UIScreen mainScreen].mirroredScreen;
    if (screens.count != sLastScreenCount) {
        sLastScreenCount = screens.count;
        Log([NSString stringWithFormat:@"screens=%lu mirrored=%d external=%d", (unsigned long) screens.count,
                                       [UIScreen mainScreen].mirroredScreen != nil, external != nil]);
    }
    if (external != nil && external != [UIScreen mainScreen]) {
        Attach(external);
    } else {
        Detach();
    }
}

} // namespace

void IosExternalDisplayInit() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!sObserving) {
            sObserving = true;
            NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
            [center addObserverForName:UIScreenDidConnectNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Log(@"screen connected");
                                Refresh();
                            }];
            [center addObserverForName:UIScreenDidDisconnectNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Log(@"screen disconnected");
                                Refresh();
                            }];
            [center addObserverForName:UIScreenModeDidChangeNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Log(@"screen mode changed");
                                Refresh();
                            }];
            // Some iOS versions don't post the connect notification for AirPlay mirroring, so also check regularly.
            [NSTimer scheduledTimerWithTimeInterval:2.0
                                            repeats:YES
                                              block:^(NSTimer*) {
                                                  Refresh();
                                              }];
            Log(@"watching for external screens");
        }
        Refresh();
    });
}

} // namespace Paperboat

#endif
