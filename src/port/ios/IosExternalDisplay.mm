#include "IosExternalDisplay.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

// In libultraship's Metal backend. Passing nullptr stops the second screen.
extern "C" void LUS_SetExternalDisplayLayer(void* layer);
// Writes a line to Paperboat.log (defined in TouchControls.cpp).
extern "C" void IosExternalDisplay_Log(const char* message);

@interface PRMetalView : UIView
@end

@implementation PRMetalView
+ (Class)layerClass {
    return [CAMetalLayer class];
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
        return;
    }
    Detach();

    UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
    window.screen = screen;
    window.backgroundColor = [UIColor blackColor];

    PRMetalView* view = [[PRMetalView alloc] initWithFrame:screen.bounds];
    CAMetalLayer* layer = (CAMetalLayer*) view.layer;
    layer.contentsScale = screen.nativeScale;
    layer.drawableSize =
        CGSizeMake(screen.bounds.size.width * screen.nativeScale, screen.bounds.size.height * screen.nativeScale);

    UIViewController* controller = [[UIViewController alloc] init];
    controller.view = view;
    window.rootViewController = controller;
    window.hidden = NO;
    sWindow = window;
    Log([NSString stringWithFormat:@"window created on external screen %.0fx%.0f", screen.bounds.size.width,
                                   screen.bounds.size.height]);

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
