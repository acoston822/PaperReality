#include "IosExternalDisplay.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

// In libultraship's Metal backend. Passing nullptr stops the second screen.
extern "C" void LUS_SetExternalDisplayLayer(void* layer);

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

    LUS_SetExternalDisplayLayer((__bridge void*) layer);
}

// Main thread only.
void Refresh() {
    NSArray<UIScreen*>* screens = [UIScreen screens];
    if (screens.count > 1) {
        Attach(screens[1]);
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
                                Refresh();
                            }];
            [center addObserverForName:UIScreenDidDisconnectNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                Refresh();
                            }];
        }
        Refresh();
    });
}

} // namespace Paperboat

#endif
