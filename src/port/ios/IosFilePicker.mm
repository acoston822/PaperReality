#include "IosFilePicker.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

@interface PaperboatPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, copy) void (^onDone)(NSArray<NSString*>*);
@end

@implementation PaperboatPickerDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    // asCopy: the file is already a private copy in the app's sandbox, so no security scope is needed.
    NSMutableArray<NSString*>* paths = [NSMutableArray array];
    for (NSURL* url in urls) {
        [paths addObject:url.path];
    }
    if (self.onDone) {
        self.onDone(paths);
    }
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    if (self.onDone) {
        self.onDone(@[]);
    }
}
@end

static UIWindow* FindWindow() {
    // Prefer the key window, but right after launch (or after the app returns from a system sheet) the game's
    // window may not be key yet, so fall back to any window that has a root view controller.
    UIWindow* fallback = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        for (UIWindow* candidate in ((UIWindowScene*) scene).windows) {
            if (candidate.isKeyWindow && candidate.rootViewController != nil) {
                return candidate;
            }
            if (fallback == nil && candidate.rootViewController != nil && !candidate.isHidden) {
                fallback = candidate;
            }
        }
    }
    if (fallback != nil) {
        return fallback;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Apps without the scene lifecycle keep their windows here.
    for (UIWindow* candidate in UIApplication.sharedApplication.windows) {
        if (candidate.rootViewController != nil && !candidate.isHidden) {
            return candidate;
        }
    }
#pragma clang diagnostic pop
    return nil;
}

static UIViewController* TopViewController() {
    UIViewController* top = FindWindow().rootViewController;
    while (top.presentedViewController != nil) {
        top = top.presentedViewController;
    }
    return top;
}

namespace Paperboat {

void IosPickFiles(bool allowMultiple, std::function<void(std::vector<std::string>)> onPicked) {
    // Held until the picker answers; the delegate is only weakly referenced by the controller.
    static PaperboatPickerDelegate* sDelegate = nil;

    static bool sPresenting = false;
    if (sPresenting) {
        return; // a picker is already open (or about to open): ignore extra taps instead of stacking another
    }
    sPresenting = true;

    // Try again for a couple of seconds if the window isn't ready, instead of treating that as a cancel.
    __block int attempts = 0;
    __block void (^present)(void) = nil;
    present = ^{
        UIViewController* top = TopViewController();
        if (top == nil || top.isBeingPresented || top.isBeingDismissed) {
            if (++attempts < 20) {
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (0.1 * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), present);
                return;
            }
            present = nil;
            sPresenting = false;
            SPDLOG_WARN("[picker] no window to present from, giving up");
            onPicked({});
            return;
        }
        present = nil;
        SPDLOG_WARN("[picker] presenting after {} retries", attempts);
        // Any file: N64 ROM extensions have no registered type, so filtering by type would hide them.
        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeData ] asCopy:YES];
        picker.allowsMultipleSelection = allowMultiple;
        sDelegate = [[PaperboatPickerDelegate alloc] init];
        sDelegate.onDone = ^(NSArray<NSString*>* paths) {
            sDelegate = nil;
            sPresenting = false;
            SPDLOG_WARN("[picker] done, {} file(s)", (int) paths.count);
            std::vector<std::string> result;
            for (NSString* path in paths) {
                if (path.UTF8String != nullptr) {
                    result.emplace_back(path.UTF8String);
                }
            }
            onPicked(std::move(result));
        };
        picker.delegate = sDelegate;
        [top presentViewController:picker animated:YES completion:nil];
    };
    dispatch_async(dispatch_get_main_queue(), present);
}

void IosPickFile(std::function<void(std::string)> onPicked) {
    IosPickFiles(false, [onPicked](std::vector<std::string> paths) {
        onPicked(paths.empty() ? std::string() : paths.front());
    });
}

} // namespace Paperboat

#endif
