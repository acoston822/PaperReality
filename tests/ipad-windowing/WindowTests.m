#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>

@interface WindowTests : XCTestCase
@end
@implementation WindowTests
- (XCUIElement*)elementNamed:(NSString*)name in:(XCUIApplication*)app {
    return [[app descendantsMatchingType:XCUIElementTypeAny]
               matchingPredicate:[NSPredicate predicateWithFormat:@"label == %@ OR identifier == %@", name, name]]
        .firstMatch;
}
- (void)testWindowExpansion {
    self.continueAfterFailure = NO;
    XCUIDevice.sharedDevice.orientation = UIDeviceOrientationLandscapeLeft;
    XCUIApplication* settings = [[XCUIApplication alloc] initWithBundleIdentifier:@"com.apple.Preferences"];
    [settings launch];
    XCUIElement* multitasking = [self elementNamed:@"Multitasking & Gestures" in:settings];
    for (int attempt = 0; attempt < 4 && !multitasking.exists; ++attempt)
        [settings swipeUp];
    NSMutableDictionary* observation = [NSMutableDictionary dictionary];
    observation[@"multitaskingSettingsAvailable"] = @(multitasking.exists);
    BOOL modeSelected = NO;
    if (multitasking.exists) {
        [multitasking tap];
        for (NSString* name in @[ @"Stage Manager", @"Windowed Apps" ]) {
            XCUIElement* mode = [self elementNamed:name in:settings];
            if ([mode waitForExistenceWithTimeout:3] && mode.hittable) {
                [mode tap];
                observation[@"selectedMode"] = name;
                modeSelected = YES;
                break;
            }
        }
    }
    observation[@"windowingModeSelected"] = @(modeSelected);
    NSLog(@"IPAD_SETTINGS_TREE %@", settings.debugDescription);
    XCUIApplication* app = [[XCUIApplication alloc] initWithBundleIdentifier:@"org.paperreality.window-probe"];
    app.launchArguments = NSProcessInfo.processInfo.environment[@"PROBE_LEGACY"] ? @[ @"--legacy-window" ] : @[];
    [app launch];
    XCUIElement* geometry = app.staticTexts[@"WindowGeometry"];
    XCTAssertTrue([geometry waitForExistenceWithTimeout:20], @"SDL probe did not create its native window");
    observation[@"before"] = geometry.value ?: geometry.label;
    XCUIApplication* springboard = [[XCUIApplication alloc] initWithBundleIdentifier:@"com.apple.springboard"];
    NSLog(@"IPAD_APP_TREE %@", app.debugDescription);
    NSLog(@"IPAD_SPRINGBOARD_TREE %@", springboard.debugDescription);
    XCUIElement* expand = nil;
    for (XCUIApplication* owner in @[ springboard, app ]) {
        XCUIElementQuery* buttons = [[owner descendantsMatchingType:XCUIElementTypeButton]
            matchingPredicate:[NSPredicate predicateWithFormat:@"label MATCHES[c] %@ OR identifier MATCHES[c] %@",
                                                               @"(Enter )?Full[ -]?Screen|Maximi[sz]e|Zoom",
                                                               @"(Enter )?Full[ -]?Screen|Maximi[sz]e|Zoom"]];
        for (XCUIElement* button in buttons.allElementsBoundByIndex)
            if (button.hittable) {
                expand = button;
                break;
            }
        if (expand)
            break;
    }
    observation[@"expandControlAvailable"] = @(expand != nil);
    if (expand) {
        observation[@"expandControlLabel"] = expand.label;
        [expand tap];
        // Wait for the public native-window bounds to settle after system animation.
        NSString* previous = geometry.value ?: geometry.label;
        for (int attempt = 0; attempt < 20; ++attempt) {
            [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.2]];
            NSString* current = geometry.value ?: geometry.label;
            if (![current isEqualToString:previous])
                break;
        }
    }
    observation[@"after"] = geometry.value ?: geometry.label;
    NSData* data = [NSJSONSerialization dataWithJSONObject:observation options:NSJSONWritingSortedKeys error:nil];
    NSLog(@"IPAD_WINDOWING_OBSERVATION %@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    // Lack of simulator windowing support is reported as a limitation, never as
    // a passing fullscreen regression. Geometry is an observation, not a fix.
    [app terminate];
}
@end
