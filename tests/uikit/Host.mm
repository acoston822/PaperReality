// Asset-free runtime harness for the production UIKit controllers.
// The actual view/controller implementation is compiled unchanged below.
#include "../../src/port/ios/IosNativeSettings.mm"
#include <cstdio>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace Paperboat::NativeSettings {
static std::atomic<int> volumeEdited { -1 };
static dispatch_queue_t gameQueue = dispatch_queue_create("org.paperreality.uikit-smoke.game", DISPATCH_QUEUE_SERIAL);
void Enqueue(std::function<void()> action) {
    dispatch_async(gameQueue, ^{
        action();
    });
}
void Close() {
    SetPresented(false);
}
void RememberPage(const std::string&) {
}
void OpenClassic(const std::string&, const std::string&) {
    SetPresented(false);
}
std::string InitialPage() {
    return "menu/Settings/General";
}
std::vector<Group> BuildNavigation() {
    return { { "Settings",
               "gearshape",
               { { "menu/Settings/General", "General", "gearshape" },
                 { "audio", "Audio", "speaker.wave.2" },
                 { "controls", "Controls", "gamecontroller" },
                 { "graphics", "Graphics", "display" },
                 { "viewer", "Input Viewer", "waveform.path" },
                 { "notifications", "Notifications", "bell" },
                 { "mods", "Mod Menu", "puzzlepiece.extension" } } },
             { "Enhancements",
               "sparkles",
               { { "cheats", "Cheats", "star" },
                 { "gameplay", "Gameplay", "gamecontroller" },
                 { "visuals", "Visuals", "display" },
                 { "status", "Status Bar", "chart.bar" } } },
             { "Shaders", "slider.horizontal.3", { { "shaders", "Shader Packs", "slider.horizontal.3" } } },
             { "Dev Tools",
               "wrench.and.screwdriver",
               { { "dev", "General", "gearshape" },
                 { "stats", "Stats", "chart.bar" },
                 { "console", "Console", "terminal" },
                 { "events", "Event Debugger", "ladybug" },
                 { "values", "Value Viewer", "eye" },
                 { "save", "Save Editor", "externaldrive" },
                 { "blueprint", "Blueprint Editor", "square.stack.3d.up" } } } };
}
Page BuildPage(const std::string& key) {
    Page page { key, key == "controls-test" ? "Control Types" : "General", "", { } };
    if (key == "menu/Settings/General" || key == "audio") {
        auto row = [](const char* title, Kind kind) {
            Row r;
            r.id = title;
            r.title = title;
            r.kind = kind;
            r.change = [](double) { };
            r.activate = [] { };
            return r;
        };
        if (key == "audio") {
            page.title = "Audio";
            Section volumes { "", "", { } };
            int index = 0;
            for (const char* title :
                 { "Master Volume", "Main Music Volume", "Environment Volume", "Sound Effects Volume" })
            {
                auto r = row(title, Kind::Integer);
                r.change = [channel = index++](double) { volumeEdited = channel; };
                r.minimum = 0;
                r.maximum = 100;
                r.value = 100;
                r.step = 1;
                r.detail = std::string(title) == "Master Volume" ? "Scales the final audio output."
                    : std::string(title) == "Main Music Volume"  ? "Volume of background music."
                    : std::string(title) == "Environment Volume" ? "Volume of ambient environment tracks."
                                                                 : "Volume of sound effects.";
                volumes.rows.push_back(std::move(r));
            }
            page.sections.push_back(std::move(volumes));
        } else {
            auto native = row("Use Native iOS Settings", Kind::Toggle);
            native.value = 1;
            native.detail = "Use the experimental UIKit settings interface.";
            page.sections.push_back({ "", "", { native } });
            auto files = row("Open App Files Folder", Kind::Action);
            files.detail = "Opens the folder that contains the save and mods folders, etc.";
            page.sections.push_back({ "General Settings", "", { files } });
            auto from = row("Import From", Kind::Choice);
            from.choices = { { 0, "All slots" }, { 1, "Slot 1" }, { 2, "Slot 2" }, { 3, "Slot 3" }, { 4, "Slot 4" } };
            auto to = row("Import To", Kind::Choice);
            to.value = 1;
            to.choices = { { 1, "Slot 1" }, { 2, "Slot 2" }, { 3, "Slot 3" }, { 4, "Slot 4" } };
            page.sections.push_back({ "Save Conversion", "", { from, to, row("Import N64 Save", Kind::Action) } });
            auto about = row("PaperReality", Kind::Info);
            about.detail = "Experimental UIKit interface";
            about.symbol = "paperplane.fill";
            auto restart = row("Restart Game", Kind::Action);
            restart.destructive = true;
            page.sections.push_back({ "About", "", { about, restart } });
        }
        return page;
    }
    page.searchable = true;
    Section section { "Controls", "Synthetic settings only; no ROM, save or texture pack.", { } };
    for (const auto kind : { Kind::Info, Kind::Toggle, Kind::Integer, Kind::Decimal, Kind::Choice, Kind::Color,
                             Kind::Text, Kind::Link, Kind::Action })
    {
        Row row;
        row.id = "test/" + std::to_string(int(kind));
        row.title = "Control " + std::to_string(int(kind));
        row.kind = kind;
        row.detail = "A native control with explanatory text.";
        row.minimum = 0;
        row.maximum = 100;
        row.value = 50;
        row.step = 1;
        row.choices = { { 0, "First" }, { 50, "Second" }, { 100, "Third" } };
        row.target = "details";
        row.change = [](double) { };
        row.activate = [] { };
        row.submit = [](std::string) { };
        if (kind == Kind::Info)
            row.portrait = [] { return Image { }; };
        section.rows.push_back(std::move(row));
    }
    page.sections.push_back(std::move(section));
    return page;
}
}
static int signalLog = -1;
static void Crashed(int signal) {
    const char message[] = "UIKit smoke terminated by signal\n";
    write(signalLog, message, sizeof(message) - 1);
    void* frames[48];
    const int count = backtrace(frames, 48);
    backtrace_symbols_fd(frames, count, signalLog);
    _Exit(128 + signal);
}
static void Progress(const char* message) {
    dprintf(signalLog, "%s\n", message);
    fsync(signalLog);
}
static void Report(NSString* status, NSString* message) {
    NSDictionary* result = @ { @"status" : status, @"message" : message };
    NSData* data = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
    [data writeToFile:[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/UIKitSmoke.json"] atomically:YES];
}
static void Uncaught(NSException* exception) {
    Report(
        @"failed",
        [NSString stringWithFormat:@"%@: %@\n%@",
                                   exception.name,
                                   exception.reason,
                                   [exception.callStackSymbols componentsJoinedByString:@"\n"]]
    );
}
static void Require(bool condition, NSString* message) {
    if (!condition)
        [NSException raise:@"UIKitLayoutRegression" format:@"%@", message];
}
static PRNativePage* PRSmokePage(UIViewController* controller) {
    if ([controller isKindOfClass:PRNativePage.class])
        return (PRNativePage*) controller;
    for (UIViewController* child in [controller.childViewControllers reverseObjectEnumerator]) {
        if (auto page = PRSmokePage(child))
            return page;
    }
    return nil;
}
static UISlider* PRSmokeSlider(UIView* view, NSString* label) {
    if ([view isKindOfClass:UISlider.class] && [view.accessibilityLabel isEqualToString:label])
        return (UISlider*) view;
    for (UIView* child in view.subviews)
        if (UISlider* slider = PRSmokeSlider(child, label))
            return slider;
    return nil;
}
@interface SmokeDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* window;
@end
@implementation SmokeDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options {
    NSString* path = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/UIKitProgress.log"];
    signalLog = open(path.UTF8String, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    signal(SIGSEGV, Crashed);
    signal(SIGABRT, Crashed);
    signal(SIGBUS, Crashed);
    Progress("App launched");
    NSSetUncaughtExceptionHandler(Uncaught);
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[UIViewController alloc] init];
    self.window.rootViewController.view.backgroundColor = UIColor.systemBackgroundColor;
    [self.window makeKeyAndVisible];
    // Test-only file channel avoids Simulator's confirmation prompt for custom URLs.
    [NSTimer
        scheduledTimerWithTimeInterval:.2
                               repeats:YES
                                 block:^(NSTimer*) {
                                     NSString* folder = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
                                     NSString* commandPath = [folder stringByAppendingPathComponent:@"UIKitCommand"];
                                     NSString* command = [NSString stringWithContentsOfFile:commandPath
                                                                                   encoding:NSUTF8StringEncoding
                                                                                      error:nil];
                                     if (!command.length || !PRRoot.presentingViewController)
                                         return;
                                     [[NSFileManager defaultManager] removeItemAtPath:commandPath error:nil];
                                     PRRoot.overrideUserInterfaceStyle = [command hasSuffix:@"-dark"]
                                         ? UIUserInterfaceStyleDark
                                         : UIUserInterfaceStyleLight;
                                     if ([command isEqualToString:@"sidebar"])
                                         [PRRoot showColumn:UISplitViewControllerColumnPrimary];
                                     else
                                         [PRRoot
                                             openPage:[command hasPrefix:@"audio"] ? "audio" : "menu/Settings/General"];
                                     dispatch_after(
                                         dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC),
                                         dispatch_get_main_queue(),
                                         ^{
                                             // Capture in the app view's own landscape coordinates, without rotating an
                                             // existing image.
                                             UIView* view = PRRoot.view;
                                             UIGraphicsImageRenderer* capture =
                                                 [[UIGraphicsImageRenderer alloc] initWithSize:view.bounds.size];
                                             UIImage* image =
                                                 [capture imageWithActions:^(UIGraphicsImageRendererContext*) {
                                                     [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:YES];
                                                 }];
                                             [UIImagePNGRepresentation(image)
                                                 writeToFile:[folder stringByAppendingPathComponent:
                                                                         [command stringByAppendingString:@".png"]]
                                                  atomically:YES];
                                         }
                                     );
                                 }];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        Progress("Opening settings");
        N::Enqueue([] { N::SetPresented(true, N::BuildNavigation()); });
    });
    __block NSInteger phase = 0;
    const double started = NSProcessInfo.processInfo.systemUptime;
    [NSTimer
        scheduledTimerWithTimeInterval:.25
                               repeats:YES
                                 block:^(NSTimer* timer) {
                                     if (NSProcessInfo.processInfo.systemUptime - started > 60) {
                                         Report(
                                             @"failed",
                                             [NSString
                                                 stringWithFormat:
                                                     @"Timed out in phase %ld. Root: %@; detail: %@; children: %@",
                                                     (long) phase,
                                                     PRRoot,
                                                     PRRoot.detail,
                                                     PRRoot.childViewControllers]
                                         );
                                         [timer invalidate];
                                         return;
                                     }
                                     if (phase == 0 || phase == 5) {
                                         if (!PRRoot.presentingViewController || PRRoot.isBeingPresented)
                                             return;
                                         [PRRoot showColumn:UISplitViewControllerColumnSecondary];
                                         Progress(phase == 0 ? "Settings presented" : "Settings reopened");
                                         ++phase;
                                         return;
                                     }
                                     if (phase == 1 || phase == 2 || phase == 6) {
                                         PRNativePage* page = PRSmokePage(PRRoot);
                                         if (!page || ![page.tableView numberOfSections])
                                             return;
                                         if (phase == 2 && ![page.title isEqualToString:@"Control Types"])
                                             return;
                                         for (NSInteger section = 0; section < [page.tableView numberOfSections];
                                              ++section)
                                             for (NSInteger row = 0; row < [page.tableView
                                                                         numberOfRowsInSection:section];
                                                  ++row)
                                                 [page tableView:page.tableView
                                                     cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                                                              inSection:section]];
                                         Progress("Page loaded");
                                         if (phase == 1) {
                                             [PRRoot.view layoutIfNeeded];
                                             Require(
                                                 !PRRoot.primary.view.superview.hidden
                                                     && !PRRoot.detail.view.superview.hidden,
                                                 @"Landscape must show both the categories and the page."
                                             );
                                             [PRRoot openPage:"controls-test"];
                                             phase = 2;
                                             return;
                                         }
                                         if (phase == 2) {
                                             CGRect bounds = PRRoot.view.bounds;
                                             PRRoot.view.bounds = CGRectMake(0, 0, 375, 700);
                                             [PRRoot.view setNeedsLayout];
                                             [PRRoot.view layoutIfNeeded];
                                             [PRRoot browse];
                                             Require(
                                                 !PRRoot.primary.view.superview.hidden
                                                     && PRRoot.detail.view.superview.hidden,
                                                 @"Compact layout must allow browsing categories."
                                             );
                                             [PRRoot showColumn:UISplitViewControllerColumnSecondary];
                                             Require(
                                                 PRRoot.primary.view.superview.hidden
                                                     && !PRRoot.detail.view.superview.hidden,
                                                 @"Compact layout must return to its settings page."
                                             );
                                             PRRoot.view.bounds = bounds;
                                             [PRRoot.view setNeedsLayout];
                                             [PRRoot.view layoutIfNeeded];
                                             [page tableView:page.tableView
                                                 didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:7 inSection:0]];
                                             phase = 7;
                                             return;
                                         }
                                         if (phase == 6) {
                                             [PRRoot openPage:"audio"];
                                             phase = 11;
                                             return;
                                         }
                                     }
                                     if (phase >= 7 && PRRoot.detail.transitionCoordinator)
                                         return;
                                     if (phase == 7 && PRRoot.detail.viewControllers.count == 2) {
                                         [PRRoot.detail popViewControllerAnimated:NO];
                                         phase = 8;
                                         return;
                                     }
                                     if (phase == 8 && PRRoot.detail.viewControllers.count == 1) {
                                         PRNativePage* page = PRSmokePage(PRRoot);
                                         [page tableView:page.tableView
                                             didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:4 inSection:0]];
                                         phase = 9;
                                         return;
                                     }
                                     if (phase == 9 &&
                                         [PRRoot.detail.topViewController isKindOfClass:PRNativeChoices.class]) {
                                         PRNativeChoices* choices = (PRNativeChoices*) PRRoot.detail.topViewController;
                                         [choices tableView:choices.tableView
                                             didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
                                         phase = 10;
                                         return;
                                     }
                                     if (phase == 10 && PRRoot.detail.viewControllers.count == 1) {
                                         [PRRoot closeSettings];
                                         phase = 3;
                                         return;
                                     }
                                     if (phase == 11) {
                                         PRNativePage* page = PRSmokePage(PRRoot);
                                         if (![page.title isEqualToString:@"Audio"]
                                             || ![page.tableView numberOfSections])
                                             return;
                                         [page.tableView layoutIfNeeded];
                                         Require(
                                             [page.tableView numberOfRowsInSection:0] == 2,
                                             @"Wide audio layout should pair the four volume controls."
                                         );
                                         CGRect bounds = page.tableView.bounds;
                                         page.tableView.bounds = CGRectMake(0, 0, 320, 500);
                                         [page viewDidLayoutSubviews];
                                         Require(
                                             [page.tableView numberOfRowsInSection:0] == 4,
                                             @"Narrow audio layout must retain all four volume controls."
                                         );
                                         page.tableView.bounds = bounds;
                                         [page viewDidLayoutSubviews];
                                         UITableViewCell* cell = [page tableView:page.tableView
                                                           cellForRowAtIndexPath:[NSIndexPath indexPathForRow:1
                                                                                                    inSection:0]];
                                         UISlider* slider = PRSmokeSlider(cell, @"Sound Effects Volume");
                                         Require(slider != nil, @"Second card pair must contain the SFX channel.");
                                         slider.value = 83;
                                         [slider sendActionsForControlEvents:UIControlEventTouchUpInside];
                                         phase = 12;
                                         return;
                                     }
                                     if (phase == 12 && N::volumeEdited == 3) {
                                         Report(
                                             @"passed",
                                             @"Opened/reopened settings; rendered every control; verified "
                                             @"landscape/compact and nested navigation, adaptive volume cards, and the "
                                             @"correct channel edit through the game queue."
                                         );
                                         [timer invalidate];
                                         return;
                                     }
                                     if (phase == 3 && !PRRoot) {
                                         Progress("Settings closed");
                                         N::Enqueue([] { N::SetPresented(true, N::BuildNavigation()); });
                                         phase = 5;
                                     }
                                 }];
    return YES;
}
- (BOOL)application:(UIApplication*)application openURL:(NSURL*)url options:(NSDictionary*)options {
    if ([url.host isEqualToString:@"sidebar"])
        [PRRoot showColumn:UISplitViewControllerColumnPrimary];
    else
        [PRRoot openPage:[url.host isEqualToString:@"audio"] ? "audio" : "menu/Settings/General"];
    return YES;
}
@end
int main(int argc, char** argv) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(SmokeDelegate.class));
    }
}
