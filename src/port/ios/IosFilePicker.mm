#include "IosFilePicker.h"
#include "IosControllerNavigation.h"

#if defined(__IOS__)
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

@interface PaperboatPickerDelegate : NSObject <UIDocumentPickerDelegate, UIAdaptivePresentationControllerDelegate>
@property(nonatomic, copy) void (^onDone)(NSArray<NSString*>*);
@end

@implementation PaperboatPickerDelegate
- (void)finish:(NSArray<NSURL*>*)urls {
    NSMutableArray<NSString*>* paths = [NSMutableArray array];
    for (NSURL* url in urls)
        if (url.path)
            [paths addObject:url.path];
    // Own the callback until it returns, even if it releases the global delegate.
    void (^done)(NSArray<NSString*>*) = [self.onDone copy];
    self.onDone = nil;
    if (done)
        done(paths);
}
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    [self finish:urls];
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    [self finish:@[]];
}
- (void)presentationControllerDidDismiss:(UIPresentationController*)controller {
    [self finish:@[]];
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
            if (candidate.screen != UIScreen.mainScreen)
                continue;
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
        if (candidate.screen == UIScreen.mainScreen && candidate.rootViewController != nil && !candidate.isHidden) {
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

namespace {
struct PickerRequest {
    bool multiple = false;
    std::string exportPath;
    std::function<void(std::vector<std::string>)> completion;
};
// Only the main queue touches presentation state and the retained delegate.
std::deque<std::shared_ptr<PickerRequest>> pending;
std::shared_ptr<PickerRequest> active;
PaperboatPickerDelegate* pickerDelegate = nil;
__weak UIViewController* activeController = nil;

void StartNextPicker();
void FinishPicker(const std::shared_ptr<PickerRequest>& request, std::vector<std::string> paths) {
    if (active != request)
        return;
    auto completion = request->completion;
    active.reset();
    activeController = nil;
    pickerDelegate = nil;
    if (completion)
        completion(std::move(paths));
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        StartNextPicker();
    });
}
// A menu shortcut can dismiss the presenting Settings controller while Files
// is open. Programmatic dismissal does not call the picker's cancel delegate.
// Release the pending request so a later import/export can still open.
void WatchPicker(const std::shared_ptr<PickerRequest>& request) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (active != request)
            return;
        UIViewController* controller = activeController;
        if (!controller
            || (!controller.presentingViewController && !controller.isBeingPresented && !controller.isBeingDismissed
                && !controller.viewIfLoaded.window))
            FinishPicker(request, {});
        else
            WatchPicker(request);
    });
}
void PickerMessage(NSString* message) {
    UIViewController* top = TopViewController();
    if (!top || top.isBeingPresented || top.isBeingDismissed || [top isKindOfClass:UIAlertController.class]) {
        NSLog(@"PaperReality Files: %@", message);
        return;
    }
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Files"
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    PRControllerAddAlertAction(alert, @"OK", UIAlertActionStyleDefault, nil);
    [top presentViewController:alert animated:YES completion:nil];
}
void PresentPicker(std::shared_ptr<PickerRequest> request, int attempts = 0) {
    if (active != request)
        return;
    UIViewController* top = TopViewController();
    if (!top || !top.view.window || top.isBeingPresented || top.isBeingDismissed || top.transitionCoordinator ||
        [top isKindOfClass:UIAlertController.class] || [top isKindOfClass:UIDocumentPickerViewController.class])
    {
        if (attempts < 30) {
            // A plain function owns the request across retries. Never clear a
            // recursive block while its C++ callback captures are in use.
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                PresentPicker(request, attempts + 1);
            });
        } else {
            FinishPicker(request, { });
            PickerMessage(@"Could not open Files. Close any open dialog and try again.");
        }
        return;
    }
    UIDocumentPickerViewController* picker;
    if (request->exportPath.empty()) {
        // ROM and pack extensions may have no registered UTType.
        picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeData ] asCopy:YES];
        picker.allowsMultipleSelection = request->multiple;
    } else {
        NSString* path = [NSString stringWithUTF8String:request->exportPath.c_str()];
        if (!path || ![NSFileManager.defaultManager fileExistsAtPath:path]) {
            FinishPicker(request, { });
            PickerMessage(@"The export file could not be found. Please generate it again.");
            return;
        }
        picker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[ [NSURL fileURLWithPath:path] ]
                                                                       asCopy:YES];
    }
    pickerDelegate = [[PaperboatPickerDelegate alloc] init];
    pickerDelegate.onDone = ^(NSArray<NSString*>* paths) {
        std::vector<std::string> result;
        for (NSString* path in paths)
            if (path.UTF8String)
                result.emplace_back(path.UTF8String);
        FinishPicker(request, std::move(result));
    };
    picker.delegate = pickerDelegate;
    activeController = picker;
    [top presentViewController:picker animated:YES completion:nil];
    picker.presentationController.delegate = pickerDelegate;
    WatchPicker(request);
}
void StartNextPicker() {
    if (active || pending.empty())
        return;
    active = pending.front();
    pending.pop_front();
    PresentPicker(active);
}
void QueuePicker(std::shared_ptr<PickerRequest> request) {
    dispatch_async(dispatch_get_main_queue(), ^{
        pending.push_back(request);
        StartNextPicker();
    });
}
}

namespace Paperboat {
void IosExportFile(std::string path, std::function<void(bool)> onDone) {
    auto request = std::make_shared<PickerRequest>();
    request->exportPath = std::move(path);
    request->completion = [onDone = std::move(onDone)](std::vector<std::string> paths) {
        if (onDone)
            onDone(!paths.empty());
    };
    QueuePicker(std::move(request));
}
void IosPickFiles(bool allowMultiple, std::function<void(std::vector<std::string>)> onPicked) {
    auto request = std::make_shared<PickerRequest>();
    request->multiple = allowMultiple;
    request->completion = std::move(onPicked);
    QueuePicker(std::move(request));
}
void IosPickFile(std::function<void(std::string)> onPicked) {
    IosPickFiles(false, [onPicked = std::move(onPicked)](std::vector<std::string> paths) {
        if (onPicked)
            onPicked(paths.empty() ? std::string() : paths.front());
    });
}
void IosOpenAppFiles(std::string path) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* directory = [NSString stringWithUTF8String:path.c_str()];
        NSURLComponents* components = [NSURLComponents componentsWithURL:[NSURL fileURLWithPath:directory
                                                                                    isDirectory:YES]
                                                 resolvingAgainstBaseURL:NO];
        components.scheme = @"shareddocuments";
        [UIApplication.sharedApplication openURL:components.URL
            options:@{ }
            completionHandler:^(BOOL opened) {
                if (!opened)
                    PickerMessage(
                        @"Open the Files app, then Browse → On My iPhone (or iPad) → PaperReality to find saves and "
                        @"mods."
                    );
            }];
    });
}
} // namespace Paperboat
#endif
