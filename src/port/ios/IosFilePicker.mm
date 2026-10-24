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

// A controller can browse the app's own Files folder without depending on a
// document provider's remote UI. The normal Files picker remains available for
// iCloud and other providers; their sign-in/permission screens belong to iOS.
@interface PaperboatLocalFiles : UITableViewController <PRControllerNavigable> {
    NSURL* _root;
    NSURL* _folder;
    NSArray<NSURL*>* _entries;
    BOOL _multiple;
    BOOL _copying, _cancelled;
    NSString* _exportPath;
    NSMutableSet<NSURL*>* _selected;
    void (^_finish)(NSArray<NSURL*>*);
    void (^_systemFiles)(void);
}
- (instancetype)initWithRoot:(NSURL*)root
                      folder:(NSURL*)folder
                    multiple:(BOOL)multiple
                  exportPath:(NSString*)exportPath
                    selected:(NSMutableSet<NSURL*>*)selected
                      finish:(void (^)(NSArray<NSURL*>*))finish
                 systemFiles:(void (^)(void))systemFiles;
@end
@implementation PaperboatLocalFiles
- (instancetype)initWithRoot:(NSURL*)root
                      folder:(NSURL*)folder
                    multiple:(BOOL)multiple
                  exportPath:(NSString*)exportPath
                    selected:(NSMutableSet<NSURL*>*)selected
                      finish:(void (^)(NSArray<NSURL*>*))finish
                 systemFiles:(void (^)(void))systemFiles {
    self = [super initWithStyle:UITableViewStyleInsetGrouped];
    if (self) {
        _root = root;
        _folder = folder;
        _multiple = multiple;
        _exportPath = [exportPath copy];
        _selected = selected ?: [NSMutableSet set];
        _finish = [finish copy];
        _systemFiles = [systemFiles copy];
        self.title = [folder isEqual:root] ? @"App Files" : folder.lastPathComponent;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.tableView.backgroundColor = UIColor.systemGroupedBackgroundColor;
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Cancel"
                                                                              style:UIBarButtonItemStylePlain
                                                                             target:self
                                                                             action:@selector(cancel)];
    [self reloadEntries];
}
- (void)reloadEntries {
    NSArray* entries =
        [NSFileManager.defaultManager contentsOfDirectoryAtURL:_folder
                                    includingPropertiesForKeys:@[ NSURLIsDirectoryKey, NSURLIsSymbolicLinkKey ]
                                                       options:NSDirectoryEnumerationSkipsHiddenFiles
                                                         error:nil]
        ?: @[];
    NSMutableArray* safe = [NSMutableArray array];
    for (NSURL* url in entries) {
        NSNumber* link = nil;
        [url getResourceValue:&link forKey:NSURLIsSymbolicLinkKey error:nil];
        if (!link.boolValue)
            [safe addObject:url];
    }
    _entries = [safe sortedArrayUsingComparator:^NSComparisonResult(NSURL* a, NSURL* b) {
        NSNumber* ad = nil;
        NSNumber* bd = nil;
        [a getResourceValue:&ad forKey:NSURLIsDirectoryKey error:nil];
        [b getResourceValue:&bd forKey:NSURLIsDirectoryKey error:nil];
        if (ad.boolValue != bd.boolValue)
            return ad.boolValue ? NSOrderedAscending : NSOrderedDescending;
        return [a.lastPathComponent localizedStandardCompare:b.lastPathComponent];
    }];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView*)table {
    return 2;
}
- (NSInteger)tableView:(UITableView*)table numberOfRowsInSection:(NSInteger)section {
    return section == 0 ? 2 : _entries.count;
}
- (NSString*)tableView:(UITableView*)table titleForFooterInSection:(NSInteger)section {
    return section == 1 ? @"Files placed in PaperReality’s folder in the Files app appear here. Other Locations opens "
                          @"the iOS file browser."
                        : nil;
}
- (UITableViewCell*)tableView:(UITableView*)table cellForRowAtIndexPath:(NSIndexPath*)path {
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    if (path.section == 0) {
        content.text = path.row == 0 ? @"Other Locations…"
            : _exportPath.length     ? @"Save Here"
            : _multiple ? [NSString stringWithFormat:@"Open Selected (%lu)", (unsigned long) _selected.count]
                        : @"Refresh";
        content.textProperties.color = self.view.tintColor;
    } else {
        NSURL* url = _entries[path.row];
        NSNumber* directory = nil;
        [url getResourceValue:&directory forKey:NSURLIsDirectoryKey error:nil];
        content.text = url.lastPathComponent;
        content.image = [UIImage systemImageNamed:directory.boolValue ? @"folder" : @"doc"];
        if (directory.boolValue)
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        else if ([_selected containsObject:url])
            cell.accessoryType = UITableViewCellAccessoryCheckmark;
        if (_exportPath.length && !directory.boolValue)
            cell.userInteractionEnabled = NO;
    }
    cell.contentConfiguration = content;
    return cell;
}
- (void)finish:(NSArray<NSURL*>*)urls {
    void (^finish)(NSArray<NSURL*>*) = _finish;
    [self.navigationController dismissViewControllerAnimated:YES
                                                  completion:^{
                                                      if (finish)
                                                          finish(urls);
                                                  }];
}
- (void)complete:(NSArray<NSURL*>*)urls {
    if (!urls.count || _exportPath.length) {
        [self finish:urls];
        return;
    }
    if (_copying)
        return;
    _copying = YES;
    self.title = @"Copying files…";
    NSURL* directory = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]
        URLByAppendingPathComponent:[@"PaperReality-Picker-" stringByAppendingString:NSUUID.UUID.UUIDString]
                        isDirectory:YES];
    __weak PaperboatLocalFiles* weak = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSMutableArray<NSURL*>* copies = [NSMutableArray array];
        NSError* error = nil;
        for (NSURL* source in urls) {
            NSURL* folder = [directory URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
            if (![NSFileManager.defaultManager createDirectoryAtURL:folder
                                        withIntermediateDirectories:YES
                                                         attributes:nil
                                                              error:&error])
                break;
            NSURL* target = [folder URLByAppendingPathComponent:source.lastPathComponent];
            if (![NSFileManager.defaultManager copyItemAtURL:source toURL:target error:&error])
                break;
            [copies addObject:target];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            PaperboatLocalFiles* owner = weak;
            if (!owner || owner->_cancelled || error) {
                dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                    [NSFileManager.defaultManager removeItemAtURL:directory error:nil];
                });
                if (owner && !owner->_cancelled) {
                    owner->_copying = NO;
                    owner.title = owner->_folder.lastPathComponent;
                    UIAlertController* alert =
                        [UIAlertController alertControllerWithTitle:@"Import Failed"
                                                            message:@"The selected files could not be copied. Check "
                                                                    @"available storage and try again."
                                                     preferredStyle:UIAlertControllerStyleAlert];
                    PRControllerAddAlertAction(alert, @"OK", UIAlertActionStyleDefault, nil);
                    [owner presentViewController:alert animated:YES completion:nil];
                }
            } else
                [owner finish:copies];
        });
    });
}
- (void)cancel {
    _cancelled = YES;
    [self finish:@[]];
}
- (void)saveFile {
    NSURL* source = [NSURL fileURLWithPath:_exportPath];
    NSURL* target = [_folder URLByAppendingPathComponent:source.lastPathComponent];
    __weak PaperboatLocalFiles* weak = self;
    void (^write)(void) = ^{
        PaperboatLocalFiles* owner = weak;
        if (!owner)
            return;
        NSError* error = nil;
        BOOL same = [source.standardizedURL isEqual:target.standardizedURL];
        if (!same) {
            // Write to a temporary sibling, then replace atomically; a failed
            // export must not delete an existing save/report.
            NSURL* temporary = [owner->_folder
                URLByAppendingPathComponent:[@".export-" stringByAppendingString:NSUUID.UUID.UUIDString]];
            BOOL copied = [NSFileManager.defaultManager copyItemAtURL:source toURL:temporary error:&error];
            if (copied) {
                if ([NSFileManager.defaultManager fileExistsAtPath:target.path])
                    [NSFileManager.defaultManager replaceItemAtURL:target
                                                     withItemAtURL:temporary
                                                    backupItemName:nil
                                                           options:0
                                                  resultingItemURL:nil
                                                             error:&error];
                else
                    [NSFileManager.defaultManager moveItemAtURL:temporary toURL:target error:&error];
            }
            [NSFileManager.defaultManager removeItemAtURL:temporary error:nil];
        }
        if (error) {
            UIAlertController* alert =
                [UIAlertController alertControllerWithTitle:@"Export Failed"
                                                    message:@"The file could not be saved in this folder."
                                             preferredStyle:UIAlertControllerStyleAlert];
            PRControllerAddAlertAction(alert, @"OK", UIAlertActionStyleDefault, nil);
            [owner presentViewController:alert animated:YES completion:nil];
        } else
            [owner complete:@[ target ]];
    };
    if ([NSFileManager.defaultManager fileExistsAtPath:target.path]
        && ![source.standardizedURL isEqual:target.standardizedURL])
    {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Replace File?"
                                                                       message:target.lastPathComponent
                                                                preferredStyle:UIAlertControllerStyleAlert];
        PRControllerAddAlertAction(alert, @"Replace", UIAlertActionStyleDestructive, ^(UIAlertAction*) {
            write();
        });
        PRControllerAddAlertAction(alert, @"Cancel", UIAlertActionStyleCancel, nil);
        [self presentViewController:alert animated:YES completion:nil];
    } else
        write();
}
- (void)tableView:(UITableView*)table didSelectRowAtIndexPath:(NSIndexPath*)path {
    if (_copying)
        return;
    [table deselectRowAtIndexPath:path animated:YES];
    if (path.section == 0) {
        if (path.row == 0) {
            void (^open)(void) = _systemFiles;
            [self.navigationController dismissViewControllerAnimated:YES completion:open];
        } else if (_exportPath.length)
            [self saveFile];
        else if (_multiple && _selected.count)
            [self complete:_selected.allObjects];
        else {
            [self reloadEntries];
            [table reloadData];
        }
        return;
    }
    NSURL* url = _entries[path.row];
    NSNumber* directory = nil;
    [url getResourceValue:&directory forKey:NSURLIsDirectoryKey error:nil];
    if (directory.boolValue) {
        PaperboatLocalFiles* child = [[PaperboatLocalFiles alloc] initWithRoot:_root
                                                                        folder:url
                                                                      multiple:_multiple
                                                                    exportPath:_exportPath
                                                                      selected:_selected
                                                                        finish:_finish
                                                                   systemFiles:_systemFiles];
        [self.navigationController pushViewController:child animated:YES];
    } else if (!_exportPath.length) {
        if (!_multiple)
            [self complete:@[ url ]];
        else {
            if ([_selected containsObject:url])
                [_selected removeObject:url];
            else
                [_selected addObject:url];
            [table reloadData];
        }
    }
}
- (NSArray<PRControllerTarget*>*)controllerTargets {
    if (_copying) {
        __weak PaperboatLocalFiles* weak = self;
        return @[ PRControllerViewTarget(self.navigationController.navigationBar, @"copy/cancel", @"Cancel copying", ^{
            [weak cancel];
        }) ];
    }
    return PRControllerDefaultTargets(self);
}
- (id)controllerContext {
    return self.navigationController.topViewController;
}
- (BOOL)controllerBack {
    if (self.navigationController.viewControllers.count > 1)
        [self.navigationController popViewControllerAnimated:YES];
    else
        [self cancel];
    return YES;
}
@end

@interface PaperboatFilesNavigation : UINavigationController <PRControllerNavigable>
@end
@implementation PaperboatFilesNavigation
- (NSArray<PRControllerTarget*>*)controllerTargets {
    return [(id<PRControllerNavigable>) self.topViewController controllerTargets];
}
- (id)controllerContext {
    return self.topViewController;
}
- (BOOL)controllerBack {
    return [(id<PRControllerNavigable>) self.topViewController controllerBack];
}
@end

namespace {
struct PickerRequest {
    bool multiple = false;
    bool systemFiles = false;
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
    if (PRControllerConnected() && !request->systemFiles) {
        NSURL* root =
            [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
        PaperboatLocalFiles* browser = [[PaperboatLocalFiles alloc] initWithRoot:root
            folder:root
            multiple:request->multiple
            exportPath:[NSString stringWithUTF8String:request->exportPath.c_str()]
            selected:nil
            finish:^(NSArray<NSURL*>* urls) {
                std::vector<std::string> paths;
                for (NSURL* url in urls)
                    if (url.path.UTF8String)
                        paths.emplace_back(url.path.UTF8String);
                FinishPicker(request, std::move(paths));
            }
            systemFiles:^{
                request->systemFiles = true;
                PresentPicker(request);
            }];
        PaperboatFilesNavigation* navigation = [[PaperboatFilesNavigation alloc] initWithRootViewController:browser];
        navigation.view.tintColor = top.view.tintColor;
        navigation.modalPresentationStyle = UIModalPresentationFullScreen;
        activeController = navigation;
        [top presentViewController:navigation animated:YES completion:nil];
        WatchPicker(request);
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
        if (PRControllerConnected()) {
            IosPickFiles(false, [](std::vector<std::string>) { });
            return;
        }
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
