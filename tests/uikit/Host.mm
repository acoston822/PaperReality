// Asset-free runtime harness for the production UIKit controllers.
// The actual view/controller implementation is compiled unchanged below.
#include "../../src/port/ios/IosNativeSettings.mm"
#include <cstdio>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace Paperboat::NativeSettings {
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
               { { "menu/Settings/General", "General", "gearshape" }, { "controls", "Controls", "gamecontroller" } } },
             { "Enhancements", "sparkles", { { "enhancements", "Gameplay", "gamecontroller" } } },
             { "Shaders", "slider.horizontal.3", { { "shaders", "Shader Packs", "slider.horizontal.3" } } },
             { "Dev Tools",
               "wrench.and.screwdriver",
               { { "save", "Save Editor", "externaldrive" },
                 { "blueprint", "Blueprint Editor", "square.stack.3d.up" } } } };
}
Page BuildPage(const std::string& key) {
    Page page { key, "General", "", { } };
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        Progress("Opening settings");
        N::Enqueue([] { N::SetPresented(true, N::BuildNavigation()); });
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 4 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        if (!PRRoot || !PRRoot.presentingViewController) {
            Report(@"failed", @"Settings were not presented");
            return;
        }
        [PRRoot showColumn:UISplitViewControllerColumnSecondary];
        PRNativePage* page = (PRNativePage*) PRRoot.detail.topViewController;
        [page.view layoutIfNeeded];
        if ([page.tableView numberOfSections] != 1) {
            Report(@"failed", @"Settings model was not applied");
            return;
        }
        for (NSInteger row = 0; row < [page.tableView numberOfRowsInSection:0]; ++row) {
            [page tableView:page.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row inSection:0]];
        }
        [PRRoot openPage:"details"];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 6 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        if (!PRRoot.presentingViewController) {
            Report(@"failed", @"Settings disappeared");
            return;
        }
        [PRRoot closeSettings];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        N::Enqueue([] { N::SetPresented(true, N::BuildNavigation()); });
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 11 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        if (!PRRoot.presentingViewController) {
            Report(@"failed", @"Settings failed to reopen");
            return;
        }
        Report(@"passed", @"Opened settings, rendered every control type, navigated, closed and reopened.");
    });
    return YES;
}
@end
int main(int argc, char** argv) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(SmokeDelegate.class));
    }
}
