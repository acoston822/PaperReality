// Asset-free runtime harness for the production UIKit controllers.
// The actual view/controller implementation is compiled unchanged below.
#include "../../src/port/ios/IosNativeSettings.mm"
#import <QuartzCore/QuartzCore.h>
#include <cstdio>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace Paperboat::NativeSettings {
static std::atomic<int> volumeEdited { -1 };
static dispatch_queue_t gameQueue = dispatch_queue_create("org.paperreality.uikit-smoke.game", DISPATCH_QUEUE_SERIAL);
static ConsoleSnapshot consoleFixture;
static EventSnapshot eventFixture { true,
                                    false,
                                    { { 0,
                                        "GameFrameUpdate",
                                        { { "src/map.c", 418, 9007199254740993ULL } },
                                        { { 4294967296LL, "src/high.cpp", 10, EventPriority::High },
                                          { 2, "src/normal.cpp", 20, EventPriority::Normal },
                                          { 3, "src/low.cpp", 30, EventPriority::Low } } },
                                      { 24, "OnMapLoad", { }, { } } } };
EventSnapshot ReadEvents() {
    return eventFixture;
}
ConsoleSnapshot ReadConsole() {
    return consoleFixture;
}
void SubmitConsole(std::string command) {
    consoleFixture.history.push_back(command);
    consoleFixture.lines.push_back({ "> " + command, true, false });
    consoleFixture.lines.push_back({ "Commands: help, clear, reset, get, set", false, false });
    ++consoleFixture.revision;
}
void ClearConsole() {
    consoleFixture.lines.clear();
    ++consoleFixture.revision;
}
void CancelBindingCapture(const std::string&) {
}
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
    if (key == "menu/Dev Tools/Value Viewer") {
        page.title = "Value Viewer";
        auto toggle = [](const char* title) {
            Row row;
            row.title = title;
            row.kind = Kind::Toggle;
            row.change = [](double) { };
            return row;
        };
        Row opacity;
        opacity.title = "Background Opacity";
        opacity.kind = Kind::Decimal;
        opacity.maximum = 1;
        opacity.step = .01;
        opacity.value = .5;
        opacity.percentage = true;
        opacity.change = [](double) { };
        Row size = opacity;
        size.title = "Overlay Size";
        size.minimum = .5;
        size.maximum = 2.5;
        size.step = .1;
        size.percentage = false;
        page.sections = { { "", "", { toggle("Show Value Viewer") } },
                          { "Window Options", "", { toggle("Floating Window"), opacity, size } },
                          { "Value Options", "", { toggle("Show Current Map"), toggle("Show Mario's Position") } } };
        return page;
    }
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
            auto api = row("Audio API", Kind::Choice);
            api.choices = { { 0, "SDL" } };
            api.enabled = false;
            api.detail = "Changing the audio API requires a relaunch.";
            volumes.rows.push_back(std::move(api));
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
        if (kind == Kind::Choice) {
            row.title = "Menu Theme";
            row.value = 10;
            row.choices.clear();
            for (const char* title : { "Red", "Dark Red", "Orange", "Green", "Dark Green", "Light Blue", "Blue",
                                       "Dark Blue", "Indigo", "Violet", "Purple", "Brown", "Gray", "Dark Gray" })
                row.choices.push_back({ int(row.choices.size()), title });
        }
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
        [NSString stringWithFormat:@"%@: %@\n%@", exception.name, exception.reason,
                                   [exception.callStackSymbols componentsJoinedByString:@"\n"]]
    );
}
static void Require(bool condition, NSString* message) {
    if (!condition)
        [NSException raise:@"UIKitLayoutRegression" format:@"%@", message];
}
static void CheckPageSeparation(UINavigationController* navigation) {
    UIViewController* page = navigation.topViewController;
    [navigation.view layoutIfNeeded];
    const CGRect content = [page.view convertRect:page.view.bounds toView:navigation.view];
    const CGRect bar = [navigation.navigationBar convertRect:navigation.navigationBar.bounds toView:navigation.view];
    Require(CGRectGetMinY(content) >= CGRectGetMaxY(bar) - .5, @"Settings content overlaps its title/back banner.");
    Require(
        navigation.view.clipsToBounds && navigation.view.superview.clipsToBounds,
        @"A navigating page can escape the detail panel."
    );
    // The outgoing page carries a magenta test marker. Capture the real view
    // hierarchy to detect it showing through a destination or its navigation bar.
    UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = 1;
    format.opaque = YES;
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:navigation.view.bounds.size
                                                                               format:format];
    UIImage* capture = [renderer imageWithActions:^(UIGraphicsImageRendererContext*) {
        [navigation.view drawViewHierarchyInRect:navigation.view.bounds afterScreenUpdates:YES];
    }];
    const size_t width = CGImageGetWidth(capture.CGImage), height = CGImageGetHeight(capture.CGImage);
    std::vector<uint8_t> rgba(width * height * 4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        rgba.data(), width, height, 8, width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), capture.CGImage);
    CGContextRelease(context);
    CGColorSpaceRelease(space);
    for (size_t i = 0; i < rgba.size(); i += 4)
        Require(
            !(rgba[i] > 240 && rgba[i + 1] < 15 && rgba[i + 2] > 240),
            @"Outgoing settings are bleeding through the destination screen."
        );
}
static UILabel* FindLabel(UIView* view, NSString* text) {
    if ([view isKindOfClass:UILabel.class] && [((UILabel*) view).text isEqualToString:text])
        return (UILabel*) view;
    for (UIView* child in view.subviews)
        if (UILabel* label = FindLabel(child, text))
            return label;
    return nil;
}
static UIButton* FindButton(UIView* view, NSString* identifier) {
    if ([view isKindOfClass:UIButton.class] && [view.accessibilityIdentifier isEqualToString:identifier])
        return (UIButton*) view;
    for (UIView* child in view.subviews)
        if (UIButton* button = FindButton(child, identifier))
            return button;
    return nil;
}
static std::vector<uint8_t> ListGutterPixels(CGRect region) {
    UIView* root = PRRoot.view;
    Require(region.size.height > 0, @"Flat list has no visible gutter.");
    UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = 1;
    format.opaque = YES;
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:region.size format:format];
    UIImage* capture = [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        CGContextTranslateCTM(context.CGContext, -region.origin.x, -region.origin.y);
        [root drawViewHierarchyInRect:root.bounds afterScreenUpdates:YES];
    }];
    const size_t width = CGImageGetWidth(capture.CGImage), height = CGImageGetHeight(capture.CGImage);
    std::vector<uint8_t> pixels(width * height * 4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), capture.CGImage);
    CGContextRelease(context);
    CGColorSpaceRelease(space);
    return pixels;
}
static void CheckListBars(UITableView* table) {
    [PRRoot.view layoutIfNeeded];
    const CGPoint previous = table.contentOffset;
    const UIUserInterfaceStyle previousStyle = PRRoot.overrideUserInterfaceStyle;
    for (NSNumber* style in @[ @(UIUserInterfaceStyleLight), @(UIUserInterfaceStyleDark) ]) {
        PRRoot.overrideUserInterfaceStyle = UIUserInterfaceStyle(style.integerValue);
        [PRRoot.view layoutIfNeeded];
        const CGFloat maximum = MAX(0, table.contentSize.height - table.bounds.size.height + table.contentInset.bottom);
        for (NSNumber* offset in @[ @0, @37.25, @(maximum), @(maximum + 48.25) ]) {
            [table setContentOffset:CGPointMake(0, offset.doubleValue) animated:NO];
            [table layoutIfNeeded];
            [PRRoot.view layoutIfNeeded];
            Require(
                [table rectForHeaderInSection:0].size.height == 0 && [table rectForFooterInSection:0].size.height == 0,
                @"Flat list reserves a blank header/footer strip."
            );
            // Keep one physical region for both captures. Hiding the list can
            // change UIKit's primary-scroll-view layout and resize a reference.
            const CGRect frame = [table convertRect:table.bounds toView:PRRoot.view];
            const CGRect region =
                CGRectMake(ceil(frame.origin.x) + 2, ceil(frame.origin.y) + 2, 2, floor(frame.size.height) - 4);
            UIView* canvasProbe = [[UIView alloc] initWithFrame:region];
            canvasProbe.backgroundColor = PRPageCanvas();
            canvasProbe.userInteractionEnabled = NO;
            [PRRoot.view addSubview:canvasProbe];
            const auto canvas = ListGutterPixels(region);
            [canvasProbe removeFromSuperview];
            const auto visible = ListGutterPixels(region);
            Require(canvas.size() == visible.size(), @"List gutter resized during scrolling.");
            for (size_t i = 0; i < canvas.size(); ++i)
                Require(
                    abs(int(canvas[i]) - int(visible[i])) <= 2,
                    [NSString stringWithFormat:
                                  @"List gutter obscured: style %@, offset %@, channel %zu, canvas %u, visible %u.",
                                  style, offset, i, canvas[i], visible[i]]
                );
        }
    }
    PRRoot.overrideUserInterfaceStyle = previousStyle;
    [table setContentOffset:previous animated:NO];
}
static void CheckEventCards(PRNativeEvents* events) {
    UITableView* table = [events valueForKey:@"table"];
    const auto first = [NSIndexPath indexPathForRow:0 inSection:0];
    const auto second = [NSIndexPath indexPathForRow:1 inSection:0];
    UITableViewCell* card = [events tableView:table cellForRowAtIndexPath:first];
    Require(!FindLabel(card, @"High"), @"Events should start collapsed.");
    UIButton* toggle = FindButton(card, @"event-0");
    Require(toggle != nil, @"Event card has no accessible expansion control.");
    [toggle sendActionsForControlEvents:UIControlEventTouchUpInside];
    card = [events tableView:table cellForRowAtIndexPath:first];
    Require(
        FindLabel(card, @"9007199254740993 calls") && FindLabel(card, @"High") && FindLabel(card, @"Normal")
            && FindLabel(card, @"Low"),
        @"Expanded events lost counts or listener priorities."
    );
    Require(FindLabel(card, @"Listener ID 4294967296 · src"), @"Actual 64-bit listener ID lost.");
    [events collapseAll];
    card = [events tableView:table cellForRowAtIndexPath:first];
    Require(!FindLabel(card, @"High"), @"Collapse All left event details visible.");
    [events expandAll];
    card = [events tableView:table cellForRowAtIndexPath:second];
    Require(
        FindLabel(card, @"This build does not record call sites.") && FindLabel(card, @"No listeners registered."),
        @"Empty event details do not explain unavailable diagnostics."
    );
    CheckPageSeparation(PRRoot.detail);
    CheckListBars(table);
}
static std::vector<uint8_t> PanelEdgePixels(UIView* panel) {
    UIView* root = PRRoot.view;
    const CGFloat scale = root.window.screen.scale;
    const CGRect frame = [panel convertRect:panel.bounds toView:root];
    // Include the first/last physical pixels along the straight panel edges.
    // Continuous corners extend farther than the nominal circular radius: do
    // not mistake their intentionally partial coverage for a horizontal seam.
    const CGFloat cornerInset = MAX(48, panel.layer.cornerRadius * 2);
    const CGFloat left = std::ceil((CGRectGetMinX(frame) + cornerInset) * scale) / scale;
    const CGFloat right = std::floor((CGRectGetMaxX(frame) - cornerInset) * scale) / scale;
    const CGFloat top = std::floor(CGRectGetMinY(frame) * scale + 1e-6) / scale;
    const CGFloat bottom = std::ceil(CGRectGetMaxY(frame) * scale - 1e-6) / scale;
    const CGRect region = CGRectMake(left, top, right - left, bottom - top);
    UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = scale;
    format.opaque = YES;
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:region.size format:format];
    UIImage* image = [renderer imageWithActions:^(UIGraphicsImageRendererContext* capture) {
        CGContextTranslateCTM(capture.CGContext, -left, -top);
        [root drawViewHierarchyInRect:root.bounds afterScreenUpdates:YES];
    }];
    const size_t width = CGImageGetWidth(image.CGImage), height = CGImageGetHeight(image.CGImage);
    Require(width > 0 && height > 2, @"Scroll edge capture has no visible panel.");
    Require(
        width == std::llround(region.size.width * scale) && height == std::llround(region.size.height * scale),
        [NSString stringWithFormat:@"Edge capture added pixels: actual %zux%zu, region %@, scale %g.", width, height,
                                   NSStringFromCGRect(region), scale]
    );
    std::vector<uint8_t> rgba(width * height * 4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        rgba.data(), width, height, 8, width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.CGImage);
    CGContextRelease(context);
    CGColorSpaceRelease(space);
    std::vector<uint8_t> edges(rgba.begin(), rgba.begin() + width * 4);
    edges.insert(edges.end(), rgba.end() - width * 4, rgba.end());
    return edges;
}
static std::vector<uint8_t> WindowPixels(UIWindow* window) {
    @autoreleasepool {
        [window layoutIfNeeded];
        UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat defaultFormat];
        format.scale = window.screen.scale;
        format.opaque = YES;
        UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:window.bounds.size
                                                                                   format:format];
        UIImage* image = [renderer imageWithActions:^(UIGraphicsImageRendererContext* capture) {
            [window.layer renderInContext:capture.CGContext];
        }];
        const size_t width = CGImageGetWidth(image.CGImage), height = CGImageGetHeight(image.CGImage);
        Require(width > 0 && height > 0, @"Window coverage capture is empty.");
        std::vector<uint8_t> rgba(width * height * 4);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            rgba.data(), width, height, 8, width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
        );
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.CGImage);
        CGContextRelease(context);
        CGColorSpaceRelease(space);
        return rgba;
    }
}
static int WindowBackdropDifference(UIWindow* window, UIView* underlying) {
    underlying.backgroundColor = UIColor.blackColor;
    const auto black = WindowPixels(window);
    underlying.backgroundColor = UIColor.whiteColor;
    const auto white = WindowPixels(window);
    Require(black.size() == white.size(), @"Window size changed during coverage check.");
    int largest = 0;
    for (size_t i = 0; i < black.size(); ++i)
        largest = MAX(largest, std::abs(int(black[i]) - int(white[i])));
    return largest;
}
static void CheckOverlayCoverage() {
    // Check the entire settings overlay, including the gutters/corner cutouts,
    // over the presenting view. The per-panel probe cannot test this backing.
    // Offscreen checks establish coverage, not the physical display behavior
    // that changes when screen recording is enabled on an actual iPhone.
    UIView* root = PRRoot.view;
    UIView* underlying = PRRoot.presentingViewController.view;
    UIWindow* window = root.window;
    Require(window && underlying.window == window, @"Coverage check needs the visible presenting view.");
    UIColor* previousBackground = underlying.backgroundColor;
    CGFloat previousAlpha = root.alpha;
    UITableView* sidebar = ((UITableViewController*) PRRoot.primary.topViewController).tableView;
    const CGPoint previousOffset = sidebar.contentOffset;
    const CGFloat minimum = -sidebar.adjustedContentInset.top;
    const CGFloat maximum =
        MAX(minimum, sidebar.contentSize.height - sidebar.bounds.size.height + sidebar.adjustedContentInset.bottom);
    root.alpha = .9;
    Require(
        WindowBackdropDifference(window, underlying) > 1, @"Window coverage check missed intentional transparency."
    );
    root.alpha = previousAlpha;
    const CGFloat halfPixel = .5 / window.screen.scale;
    for (NSNumber* offset in @[
             @(minimum - 56.25), @(minimum), @(minimum + halfPixel), @((minimum + maximum) / 2 + halfPixel), @(maximum),
             @(maximum + 56.25)
         ])
    {
        [sidebar setContentOffset:CGPointMake(0, offset.doubleValue) animated:NO];
        const int difference = WindowBackdropDifference(window, underlying);
        Require(
            difference <= 1,
            [NSString stringWithFormat:@"Underlying view is visible through settings: delta %d, root %@, window %@.",
                                       difference, NSStringFromCGRect(root.frame), NSStringFromCGRect(window.bounds)]
        );
    }
    underlying.backgroundColor = previousBackground;
    [sidebar setContentOffset:previousOffset animated:NO];
}
static void CheckScrollPanelCoverage() {
    const UIUserInterfaceStyle previousStyle = PRRoot.overrideUserInterfaceStyle;
    const UIEdgeInsets previousInsets = PRRoot.additionalSafeAreaInsets;
    const CGFloat scale = PRRoot.view.window.screen.scale;
    // Exercise subpixel safe areas too, so an integer-point simulator viewport
    // cannot hide a partially covered edge pixel.
    PRRoot.additionalSafeAreaInsets = UIEdgeInsetsMake(
        previousInsets.top + .5 / scale, previousInsets.left, previousInsets.bottom + .25 / scale, previousInsets.right
    );
    [PRRoot.view setNeedsLayout];
    [PRRoot.view layoutIfNeeded];
    for (NSNumber* style in @[ @(UIUserInterfaceStyleLight), @(UIUserInterfaceStyleDark) ]) {
        PRRoot.overrideUserInterfaceStyle = (UIUserInterfaceStyle) style.integerValue;
        for (UINavigationController* navigation in @[ PRRoot.primary, PRRoot.detail ]) {
            UITableView* table = ((UITableViewController*) navigation.topViewController).tableView;
            UIView* panel = navigation.view.superview;
            const CGPoint previousOffset = table.contentOffset;
            [navigation.view layoutIfNeeded];
            const CGFloat minimum = -table.adjustedContentInset.top;
            const CGFloat maximum =
                MAX(minimum, table.contentSize.height - table.bounds.size.height + table.adjustedContentInset.bottom);
            UIView* backdrop = [[UIView alloc] initWithFrame:PRRoot.view.bounds];
            [PRRoot.view insertSubview:backdrop belowSubview:panel];
            // Keep native elastic scrolling. Sample while pulled beyond each
            // limit and with fractional offsets, not just at the resting edge.
            for (NSNumber* offset in @[ @(minimum - 56.25), @(maximum + 56.25) ]) {
                [table setContentOffset:CGPointMake(0, offset.doubleValue) animated:NO];
                [table layoutIfNeeded];
                // A deliberately translucent panel must fail the same visual
                // comparison. This guards against empty/incorrect captures
                // concealing a real backdrop leak in the normal opaque case.
                const CGFloat previousAlpha = panel.alpha;
                panel.alpha = .9;
                backdrop.backgroundColor = UIColor.blackColor;
                const auto translucentBlack = PanelEdgePixels(panel);
                backdrop.backgroundColor = UIColor.whiteColor;
                const auto translucentWhite = PanelEdgePixels(panel);
                panel.alpha = previousAlpha;
                bool detected = false;
                for (size_t i = 0; i < translucentBlack.size(); ++i)
                    detected = detected || std::abs(int(translucentBlack[i]) - int(translucentWhite.at(i))) > 1;
                Require(detected, @"The edge test failed to detect a deliberately translucent panel.");
                backdrop.backgroundColor = UIColor.blackColor;
                const auto black = PanelEdgePixels(panel);
                backdrop.backgroundColor = UIColor.whiteColor;
                const auto white = PanelEdgePixels(panel);
                Require(black.size() == white.size(), @"Panel size changed while checking scroll coverage.");
                size_t first = black.size(), changed = 0;
                int largest = 0;
                for (size_t i = 0; i < black.size(); ++i) {
                    const int difference = std::abs(int(black[i]) - int(white[i]));
                    if (difference > 1) {
                        first = MIN(first, i);
                        ++changed;
                        largest = MAX(largest, difference);
                    }
                }
                if (changed) {
                    const size_t pixel = (first / 4) * 4;
                    Require(
                        false,
                        [NSString stringWithFormat:
                                      @"Backdrop visible: %@, style %ld, offset %g, frame %@, root %@, hidden %d; "
                                      @"%zu changed channels, max delta %d, sample %zu/%zu, black [%u,%u,%u,%u], "
                                      @"white [%u,%u,%u,%u].",
                                      navigation == PRRoot.primary ? @"sidebar" : @"detail", (long) style.integerValue,
                                      offset.doubleValue,
                                      NSStringFromCGRect([panel convertRect:panel.bounds toView:PRRoot.view]),
                                      NSStringFromCGRect(PRRoot.view.bounds), panel.hidden, changed, largest, first / 4,
                                      black.size() / 4, black[pixel], black[pixel + 1], black[pixel + 2],
                                      black[pixel + 3], white[pixel], white[pixel + 1], white[pixel + 2],
                                      white[pixel + 3]]
                    );
                }
            }
            [backdrop removeFromSuperview];
            [table setContentOffset:previousOffset animated:NO];
        }
        CheckOverlayCoverage();
    }
    PRRoot.overrideUserInterfaceStyle = previousStyle;
    PRRoot.additionalSafeAreaInsets = previousInsets;
    [PRRoot.view setNeedsLayout];
    [PRRoot.view layoutIfNeeded];
    Progress("Scroll/overlay coverage checked: light/dark, top/bottom overscroll, fractional safe areas");
}
static void CheckSidebarLabels() {
    PRNativeSidebar* sidebar = (PRNativeSidebar*) PRRoot.primary.topViewController;
    // Scrolling must not reveal a hidden navigation bar or reserve a title strip.
    [sidebar.tableView layoutIfNeeded];
    UIView* panel = PRRoot.primary.view.superview;
    Require(
        panel.layer.shouldRasterize && fabs(panel.layer.rasterizationScale - panel.window.screen.scale) < .01,
        @"The sidebar must composite at the display's pixel density."
    );
    for (NSInteger section = 0; section < sidebar.tableView.numberOfSections; ++section) {
        Require(
            [sidebar.tableView rectForHeaderInSection:section].size.height == 0 &&
                [sidebar.tableView rectForFooterInSection:section].size.height == 0,
            @"Empty sidebar sections must not introduce fractional-height bands between rows."
        );
    }
    const CGPoint previous = sidebar.tableView.contentOffset;
    [sidebar.tableView
        setContentOffset:CGPointMake(
                             0, MAX(0, sidebar.tableView.contentSize.height - sidebar.tableView.bounds.size.height)
                         )
                animated:NO];
    [PRRoot.primary.view layoutIfNeeded];
    Require(PRRoot.primary.navigationBarHidden, @"Scrolling the sidebar exposed a navigation bar.");
    const CGRect sidebarFrame = [sidebar.view convertRect:sidebar.view.bounds toView:PRRoot.primary.view];
    Require(fabs(CGRectGetMinY(sidebarFrame)) < .5, @"The sidebar reserved a hidden title-bar strip.");
    [sidebar.tableView setContentOffset:previous animated:NO];
    CGFloat sharedSize = 0;
    for (NSInteger section = 0; section < [sidebar.tableView numberOfSections]; ++section)
        for (NSInteger row = 0; row < [sidebar.tableView numberOfRowsInSection:section]; ++row) {
            UITableViewCell* cell = [sidebar tableView:sidebar.tableView
                                 cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row inSection:section]];
            cell.frame = CGRectMake(0, 0, sidebar.tableView.bounds.size.width, 60);
            [cell layoutIfNeeded];
            UIListContentConfiguration* content = (UIListContentConfiguration*) cell.contentConfiguration;
            if (!sharedSize)
                sharedSize = content.textProperties.font.pointSize;
            Require(
                std::abs(content.textProperties.font.pointSize - sharedSize) < .01,
                @"Sidebar categories and submenu labels have different font sizes."
            );
            Require(content.textProperties.numberOfLines == 1, @"Sidebar labels can wrap onto a second line.");
            if ([content.text isEqualToString:@"Enhancements"]) {
                UILabel* label = FindLabel(cell, @"Enhancements");
                Require(
                    label &&
                        [label.text sizeWithAttributes:@ { NSFontAttributeName : label.font }].width
                            <= label.bounds.size.width + .5,
                    @"Enhancements does not fit on one line."
                );
            }
        }
}
static void CheckPortraitRendering() {
    N::Image pixels { 4, 4, { } };
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const uint8_t color = (x + y) % 2 ? 255 : 0;
            pixels.rgba.insert(pixels.rgba.end(), { color, color, color, 255 });
        }
    UIImage* thumbnail = PRImage(pixels, 3);
    Require(
        thumbnail != nil && thumbnail.scale == 3 && thumbnail.size.width == 44
            && CGImageGetWidth(thumbnail.CGImage) == 132 && CGImageGetHeight(thumbnail.CGImage) == 132,
        @"Portrait thumbnails must have real Retina pixels at their display size."
    );
    Require(
        CGImageGetBitsPerPixel(thumbnail.CGImage) == 32 && CGImageGetBitsPerComponent(thumbnail.CGImage) == 8,
        @"Portrait renderer must produce a standard RGBA thumbnail."
    );
    CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(thumbnail.CGImage));
    const auto* bytes = CFDataGetBytePtr(data);
    const size_t stride = CGImageGetBytesPerRow(thumbnail.CGImage);
    bool crisp = true;
    for (size_t y = 0; y < 132; ++y)
        for (size_t x = 0; x < 132 * 4; ++x)
            crisp = crisp && (bytes[y * stride + x] == 0 || bytes[y * stride + x] == 255);
    CFRelease(data);
    Require(crisp, @"Upscaling original pixel art introduced blurred intermediate colors.");
    N::Image hd { 256, 128, std::vector<uint8_t>(256 * 128 * 4, 255) };
    UIImage* reduced = PRImage(hd, 2);
    Require(
        reduced != nil && reduced.scale == 2 && CGImageGetWidth(reduced.CGImage) == 88,
        @"HD artwork must produce a thumbnail at the current display density."
    );
    Require(PRImage({ }, 3) == nil, @"Unavailable portraits must retain the blank placeholder.");
    Progress("Portrait rendering passed: Retina density, crisp pixel art, HD reduction, missing assets");
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
static NSIndexPath* PRSmokeIndex(PRNativePage* page, N::Kind kind) {
    NSString* title = kind == N::Kind::Choice ? @"Menu Theme" : [NSString stringWithFormat:@"Control %d", int(kind)];
    for (NSInteger section = 0; section < [page.tableView numberOfSections]; ++section)
        for (NSInteger row = 0; row < [page.tableView numberOfRowsInSection:section]; ++row) {
            NSIndexPath* index = [NSIndexPath indexPathForRow:row inSection:section];
            UITableViewCell* cell = [page tableView:page.tableView cellForRowAtIndexPath:index];
            if ([cell.contentConfiguration isKindOfClass:UIListContentConfiguration.class] &&
                [((UIListContentConfiguration*) cell.contentConfiguration).text isEqualToString:title])
                return index;
        }
    [NSException raise:@"UIKitMissingControl" format:@"%@ is missing", title];
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
    CheckPortraitRendering();
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
    __block UIView* bleedProbe = nil;
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
                                                     (long) phase, PRRoot, PRRoot.detail, PRRoot.childViewControllers]
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
                                             CheckPageSeparation(PRRoot.detail);
                                             CheckSidebarLabels();
                                             Require(
                                                 !PRRoot.primary.view.superview.hidden
                                                     && !PRRoot.detail.view.superview.hidden,
                                                 @"Landscape must show both the categories and the page."
                                             );
                                             CheckScrollPanelCoverage();
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
                                             CheckSidebarLabels();
                                             bleedProbe = [[UIView alloc] initWithFrame:page.view.bounds];
                                             bleedProbe.backgroundColor = UIColor.magentaColor;
                                             bleedProbe.autoresizingMask =
                                                 UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
                                             [page.view addSubview:bleedProbe];
                                             [page tableView:page.tableView
                                                 didSelectRowAtIndexPath:PRSmokeIndex(page, N::Kind::Link)];
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
                                         PRNativePage* nested = (PRNativePage*) PRRoot.detail.topViewController;
                                         if (![nested.tableView numberOfSections])
                                             return;
                                         [nested.tableView setContentOffset:CGPointMake(0, 140) animated:NO];
                                         CheckPageSeparation(PRRoot.detail);
                                         [PRRoot.detail popViewControllerAnimated:NO];
                                         phase = 8;
                                         return;
                                     }
                                     if (phase == 8 && PRRoot.detail.viewControllers.count == 1) {
                                         PRNativePage* page = PRSmokePage(PRRoot);
                                         [page tableView:page.tableView
                                             didSelectRowAtIndexPath:PRSmokeIndex(page, N::Kind::Choice)];
                                         phase = 9;
                                         return;
                                     }
                                     if (phase == 9 &&
                                         [PRRoot.detail.topViewController isKindOfClass:PRNativeChoices.class]) {
                                         PRNativeChoices* choices = (PRNativeChoices*) PRRoot.detail.topViewController;
                                         CheckPageSeparation(PRRoot.detail);
                                         Require(
                                             [choices.tableView numberOfRowsInSection:0] == 14,
                                             @"Menu Theme fixture must exercise the full scrolling color list."
                                         );
                                         CheckListBars(choices.tableView);
                                         UISearchController* search = choices.navigationItem.searchController;
                                         search.searchBar.text = @"Dark Red";
                                         [choices updateSearchResultsForSearchController:search];
                                         Require(
                                             [choices.tableView numberOfRowsInSection:0] == 1,
                                             @"Choice filtering did not retain the matching color."
                                         );
                                         CheckListBars(choices.tableView);
                                         search.searchBar.text = @"";
                                         [choices updateSearchResultsForSearchController:search];
                                         [choices tableView:choices.tableView
                                             didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
                                         phase = 10;
                                         return;
                                     }
                                     if (phase == 10 && PRRoot.detail.viewControllers.count == 1) {
                                         [bleedProbe removeFromSuperview];
                                         bleedProbe = nil;
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
                                             [page.tableView numberOfRowsInSection:0] == 3,
                                             @"Wide audio layout should pair the four volume controls."
                                         );
                                         CGRect bounds = page.tableView.bounds;
                                         page.tableView.bounds = CGRectMake(0, 0, 320, 500);
                                         [page viewDidLayoutSubviews];
                                         Require(
                                             [page.tableView numberOfRowsInSection:0] == 5,
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
                                         [PRRoot openPage:"console"];
                                         phase = 13;
                                         return;
                                     }
                                     if (phase == 13) {
                                         Require(
                                             [PRRoot.detail.topViewController isKindOfClass:PRNativeConsole.class],
                                             @"Console must be fully native, without opening an ImGui window."
                                         );
                                         PRNativeConsole* console = (PRNativeConsole*) PRRoot.detail.topViewController;
                                         UITextField* command = [console valueForKey:@"command"];
                                         command.text = @"help";
                                         [console submitCommand];
                                         phase = 14;
                                         return;
                                     }
                                     if (phase == 14) {
                                         PRNativeConsole* console = (PRNativeConsole*) PRRoot.detail.topViewController;
                                         UITextView* output = [console valueForKey:@"output"];
                                         if (![output.text containsString:@"Commands:"])
                                             return;
                                         CheckPageSeparation(PRRoot.detail);
                                         [console clearOutput];
                                         phase = 15;
                                         return;
                                     }
                                     if (phase == 15) {
                                         PRNativeConsole* console = (PRNativeConsole*) PRRoot.detail.topViewController;
                                         UITextView* output = [console valueForKey:@"output"];
                                         if (output.text.length)
                                             return;
                                         [PRRoot openPage:"menu/Dev Tools/Event Debugger"];
                                         phase = 16;
                                         return;
                                     }
                                     if (phase == 16) {
                                         Require(
                                             [PRRoot.detail.topViewController isKindOfClass:PRNativeEvents.class],
                                             @"Event Debugger must open its native inspector."
                                         );
                                         PRNativeEvents* events = (PRNativeEvents*) PRRoot.detail.topViewController;
                                         UITableView* table = [events valueForKey:@"table"];
                                         if ([table numberOfRowsInSection:0] != 2)
                                             return;
                                         CheckEventCards(events);
                                         N::Enqueue([] {
                                             N::eventFixture.events[0].callers[0].calls = 9007199254740994ULL;
                                         });
                                         [events refresh];
                                         phase = 17;
                                         return;
                                     }
                                     if (phase == 17) {
                                         PRNativeEvents* events = (PRNativeEvents*) PRRoot.detail.topViewController;
                                         UITableView* table = [events valueForKey:@"table"];
                                         UITableViewCell* card =
                                             [table cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
                                         if (!FindLabel(card, @"9007199254740994 calls"))
                                             return;
                                         N::Enqueue([] { N::eventFixture.events.clear(); });
                                         [events refresh];
                                         phase = 18;
                                         return;
                                     }
                                     if (phase == 18) {
                                         PRNativeEvents* events = (PRNativeEvents*) PRRoot.detail.topViewController;
                                         UITableView* table = [events valueForKey:@"table"];
                                         if ([table numberOfRowsInSection:0])
                                             return;
                                         Require(
                                             FindLabel(events.view, @"No events registered."),
                                             @"Empty registry has no native message."
                                         );
                                         Require(
                                             !FindButton(events.view, @"events-expand-all").enabled,
                                             @"Expand All should be disabled for an empty registry."
                                         );
                                         Require(
                                             [PRPageController("menu/Dev Tools/Event Debugger")
                                                 isKindOfClass:PRNativeEvents.class],
                                             @"Restoring Event Debugger would open a generic page."
                                         );
                                         [PRRoot openPage:"menu/Dev Tools/Value Viewer"];
                                         phase = 19;
                                         return;
                                     }
                                     if (phase == 19) {
                                         PRNativePage* page = PRSmokePage(PRRoot);
                                         if (!page || ![page.title isEqualToString:@"Value Viewer"])
                                             return;
                                         Require(
                                             [page.tableView numberOfSections] == 3 &&
                                                 [page.tableView numberOfRowsInSection:1] == 2,
                                             @"Value Viewer must pair opacity/size beside its floating toggle."
                                         );
                                         UITableViewCell* pair = [page tableView:page.tableView
                                                           cellForRowAtIndexPath:[NSIndexPath indexPathForRow:1
                                                                                                    inSection:1]];
                                         Require(
                                             PRSmokeSlider(pair, @"Background Opacity")
                                                 && PRSmokeSlider(pair, @"Overlay Size"),
                                             @"Value Viewer numeric controls are missing."
                                         );
                                         CheckPageSeparation(PRRoot.detail);
                                         Report(
                                             @"passed",
                                             @"Opened/reopened settings; rendered every control; verified "
                                             @"landscape/compact and nested navigation, adaptive volume cards, and the "
                                             @"correct channel edit through the game queue; Retina portraits and crisp "
                                             @"pixel scaling; opaque overlay/nested pages, scrolled title separation "
                                             @"and "
                                             @"uniform one-line sidebar labels; native console submission, output and "
                                             @"clearing; native event expansion/collapse, live 64-bit counts, "
                                             @"priorities "
                                             @"and empty registry handling; event scroll-bar coverage in light/dark "
                                             @"and native Value Viewer cards."
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
