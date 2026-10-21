#include "port/ui/NativeSettings.h"
#if defined(__IOS__)
#import <UIKit/UIKit.h>
#include <atomic>
#include <deque>
#include <memory>
#include <algorithm>
#include <cmath>

namespace N = Paperboat::NativeSettings;
// Shared materials keep the settings pages and editor submenus visually consistent.
// Use dynamic colors so a system appearance change also updates an already-open menu.
static UIColor* PRSurface() {
    return [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
            ? [UIColor colorWithRed:.09 green:.12 blue:.17 alpha:.90]
            : [UIColor colorWithWhite:1 alpha:.92];
    }];
}
static UIColor* PRCanvas() {
    return [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
            ? [UIColor colorWithRed:.025 green:.045 blue:.08 alpha:.82]
            : [UIColor colorWithRed:.93 green:.95 blue:.98 alpha:.86];
    }];
}
// Pages must cover the outgoing page during navigation. Transparent tables and
// bars let the previous controller's controls show through push/pop transitions.
static UIColor* PRPageCanvas() {
    return [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
            ? [UIColor colorWithRed:.045 green:.065 blue:.095 alpha:1]
            : [UIColor colorWithRed:.94 green:.96 blue:.98 alpha:1];
    }];
}
static void PRIsolatePage(UIViewController* page) {
    page.edgesForExtendedLayout = UIRectEdgeNone;
    page.extendedLayoutIncludesOpaqueBars = NO;
    page.view.backgroundColor = PRPageCanvas();
    page.view.clipsToBounds = YES;
}
static UIFont* PRFont(CGFloat size, UIFontWeight weight, UIFontTextStyle style) {
    return [[UIFontMetrics metricsForTextStyle:style] scaledFontForFont:[UIFont systemFontOfSize:size weight:weight]];
}
static void PRStyleTable(UITableView* table) {
    table.backgroundColor = UIColor.clearColor;
    table.separatorStyle = UITableViewCellSeparatorStyleNone;
    table.contentInset = UIEdgeInsetsMake(4, 0, 20, 0);
    table.sectionHeaderTopPadding = 0;
    table.estimatedSectionHeaderHeight = 38;
    table.estimatedSectionFooterHeight = 20;
}
static void PRStyleCard(UITableViewCell* cell) {
    UIBackgroundConfiguration* background = [UIBackgroundConfiguration clearConfiguration];
    background.backgroundColor = PRSurface();
    background.cornerRadius = 16;
    background.backgroundInsets = NSDirectionalEdgeInsetsMake(5, 12, 5, 12);
    background.strokeColor = [UIColor.separatorColor colorWithAlphaComponent:.16];
    background.strokeWidth = .5;
    cell.backgroundConfiguration = background;
    cell.contentView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(17, 30, 17, 30);
    cell.tintColor = UIColor.systemBlueColor;
}
static void PRStyleNavigation(UINavigationController* navigation) {
    UINavigationBarAppearance* appearance = [[UINavigationBarAppearance alloc] init];
    [appearance configureWithOpaqueBackground];
    appearance.backgroundColor = PRPageCanvas();
    appearance.titleTextAttributes = @ {
        NSFontAttributeName : PRFont(22, UIFontWeightBold, UIFontTextStyleTitle2),
        NSForegroundColorAttributeName : UIColor.labelColor
    };
    navigation.navigationBar.standardAppearance = appearance;
    navigation.navigationBar.scrollEdgeAppearance = appearance;
    navigation.navigationBar.compactAppearance = appearance;
    navigation.navigationBar.compactScrollEdgeAppearance = appearance;
    navigation.navigationBar.translucent = NO;
    navigation.navigationBar.prefersLargeTitles = NO;
    navigation.view.backgroundColor = PRPageCanvas();
    navigation.view.clipsToBounds = YES;
}
static NSString* PRText(const std::string& text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding] ?: @"";
}
static std::string PRString(NSString* text) {
    return text.UTF8String ?: "";
}
static std::atomic<bool> PRWanted { false };
static std::atomic<uint64_t> PRPresentation { 0 };
static UIViewController* PRTopController() {
    UIWindow* fallback = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class])
            continue;
        for (UIWindow* window in ((UIWindowScene*) scene).windows) {
            if (window.screen != UIScreen.mainScreen || window.hidden || !window.rootViewController)
                continue;
            if (window.isKeyWindow) {
                fallback = window;
                break;
            }
            if (!fallback)
                fallback = window;
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (!fallback)
        for (UIWindow* window in UIApplication.sharedApplication.windows) {
            if (window.screen == UIScreen.mainScreen && !window.hidden && window.rootViewController) {
                fallback = window;
                break;
            }
        }
#pragma clang diagnostic pop
    UIViewController* top = fallback.rootViewController;
    while (top.presentedViewController && !top.presentedViewController.isBeingDismissed)
        top = top.presentedViewController;
    return top;
}

static UIImage* PRImage(const N::Image& image, CGFloat displayScale) {
    if (image.width <= 0 || image.height <= 0 || image.rgba.size() < size_t(image.width) * image.height * 4)
        return nil;
    NSData* data = [NSData dataWithBytes:image.rgba.data() length:image.rgba.size()];
    displayScale = displayScale > 0 ? displayScale : UIScreen.mainScreen.scale;
    const CGFloat fit = std::min(44. / image.width, 44. / image.height);
    const CGSize pixels = CGSizeMake(
        std::max(1., std::round(image.width * fit * displayScale)),
        std::max(1., std::round(image.height * fit * displayScale))
    );
    const BOOL reducing = pixels.width < image.width || pixels.height < image.height;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef) data);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef cg = CGImageCreate(
        image.width, image.height, 8, 32, image.width * 4, space,
        CGBitmapInfo(kCGImageAlphaLast | kCGBitmapByteOrder32Big), provider, nullptr, reducing,
        kCGRenderingIntentDefault
    );
    UIImage* result = nil;
    if (cg) {
        UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat preferredFormat];
        format.scale = displayScale;
        format.opaque = NO;
        format.preferredRange = UIGraphicsImageRendererFormatRangeStandard;
        UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(44, 44)
                                                                                   format:format];
        UIImage* source = [UIImage imageWithCGImage:cg];
        result = [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
            CGContextSetInterpolationQuality(context.CGContext, reducing ? kCGInterpolationHigh : kCGInterpolationNone);
            // Align to physical pixels. UIKit receives a 44pt image already at
            // the display's density, so it doesn't enlarge a 1x thumbnail again.
            const CGRect target = CGRectMake(
                std::floor((44 * displayScale - pixels.width) / 2) / displayScale,
                std::floor((44 * displayScale - pixels.height) / 2) / displayScale, pixels.width / displayScale,
                pixels.height / displayScale
            );
            [source drawInRect:target];
        }];
    }
    if (cg)
        CGImageRelease(cg);
    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);
    return result;
}
static UIImage* PRBlankPortrait() {
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(42, 42)];
    return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [UIColor.separatorColor setStroke];
        UIBezierPath* path = [UIBezierPath bezierPathWithRoundedRect:CGRectMake(1, 1, 40, 40) cornerRadius:9];
        path.lineWidth = 1;
        [path stroke];
    }];
}

@class PRNativeRoot;
@interface PRNativePage : UITableViewController <UISearchResultsUpdating, UIColorPickerViewControllerDelegate> {
    std::string _key;
    N::Page _page;
    std::vector<N::Section> _sections;
    std::vector<std::vector<std::pair<size_t, size_t>>> _rows;
    uint64_t _generation;
    NSMutableDictionary<NSString*, UIImage*>* _images;
    NSMutableSet<NSString*>* _pendingImages;
    N::Row _colorRow;
    BOOL _twoColumns;
    NSTimer* _bindingTimer;
}
- (instancetype)initWithKey:(std::string)key;
- (void)refresh;
- (void)filter;
- (void)apply:(const N::Row&)row value:(double)value;
@end

@interface PRNativeChoices : UITableViewController <UISearchResultsUpdating> {
    N::Row _row;
    std::vector<N::Choice> _choices;
    __weak PRNativePage* _owner;
}
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner;
@end

@interface PRNativeSidebar : UITableViewController {
    std::vector<N::Group> _groups;
    NSInteger _expanded;
    NSString* _selected;
    __weak PRNativeRoot* _host;
    CGFloat _labelWidth;
}
- (instancetype)initWithGroups:(std::vector<N::Group>)groups initial:(std::string)initial host:(PRNativeRoot*)host;
- (void)selectPage:(const std::string&)key;
@end

@interface PRNativeRoot : UIViewController {
    UIView* _header;
    UIView* _primaryPanel;
    UIView* _detailPanel;
    UILabel* _category;
    UIButton* _menuButton;
    BOOL _showPrimary;
    BOOL _wide;
    BOOL _consoleTyping;
}
@property (nonatomic, strong) UINavigationController* primary;
@property (nonatomic, strong) UINavigationController* detail;
- (void)setViewController:(UIViewController*)controller forColumn:(UISplitViewControllerColumn)column;
- (void)showColumn:(UISplitViewControllerColumn)column;
- (void)openPage:(std::string)key;
- (void)setCategory:(NSString*)title;
- (void)closeSettings;
- (void)setConsoleTyping:(BOOL)typing;
@end
static PRNativeRoot* PRRoot = nil;

@interface PRNativeSliderControl : UIView {
    N::Row _row;
    __weak PRNativePage* _owner;
    UILabel* _value;
    UISlider* _slider;
}
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner;
@end

@implementation PRNativeSliderControl
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner {
    self = [super initWithFrame:CGRectZero];
    if (!self)
        return nil;
    _row = row;
    _owner = owner;
    self.backgroundColor = PRSurface();
    self.layer.cornerRadius = 16;
    self.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(14, 16, 14, 16);
    UILabel* title = [[UILabel alloc] init];
    title.text = PRText(row.title);
    title.font = PRFont(16, UIFontWeightSemibold, UIFontTextStyleBody);
    title.numberOfLines = 0;
    title.adjustsFontForContentSizeCategory = YES;
    _value = [[UILabel alloc] init];
    _value.font = [[UIFontMetrics metricsForTextStyle:UIFontTextStyleSubheadline]
        scaledFontForFont:[UIFont monospacedDigitSystemFontOfSize:15 weight:UIFontWeightSemibold]];
    _value.textColor = UIColor.systemBlueColor;
    _value.adjustsFontForContentSizeCategory = YES;
    [_value setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [_value setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    UIStackView* heading = [[UIStackView alloc] initWithArrangedSubviews:@[ title, _value ]];
    heading.spacing = 12;
    heading.alignment = UIStackViewAlignmentCenter;
    _slider = [[UISlider alloc] init];
    _slider.minimumValue = row.minimum;
    _slider.maximumValue = row.maximum;
    _slider.value = row.value;
    _slider.enabled = row.enabled;
    _slider.accessibilityLabel = PRText(row.title);
    [_slider addTarget:self action:@selector(changing:) forControlEvents:UIControlEventValueChanged];
    [_slider addTarget:self
                  action:@selector(commit:)
        forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];
    UIStackView* controls = [[UIStackView alloc] init];
    controls.axis = UILayoutConstraintAxisHorizontal;
    controls.spacing = 6;
    controls.alignment = UIStackViewAlignmentCenter;
    for (NSNumber* direction in @[ @(-1), @(1) ]) {
        if (direction.intValue == 1)
            [controls addArrangedSubview:_slider];
        UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
        UIButtonConfiguration* config = [UIButtonConfiguration tintedButtonConfiguration];
        config.image = [UIImage systemImageNamed:direction.intValue < 0 ? @"minus" : @"plus"];
        config.cornerStyle = UIButtonConfigurationCornerStyleMedium;
        button.configuration = config;
        button.enabled = row.enabled;
        button.accessibilityLabel =
            [NSString stringWithFormat:@"%@ %@", direction.intValue < 0 ? @"Decrease" : @"Increase", PRText(row.title)];
        [button.widthAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
        [button.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
        __weak PRNativeSliderControl* weak = self;
        [button addAction:[UIAction actionWithHandler:^(UIAction* action) {
                    PRNativeSliderControl* cell = weak;
                    if (!cell)
                        return;
                    cell->_slider.value += direction.intValue * cell->_row.step;
                    [cell changing:cell->_slider];
                    [cell commit:cell->_slider];
                }]
            forControlEvents:UIControlEventTouchUpInside];
        [controls addArrangedSubview:button];
    }
    UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[ heading, controls ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 8;
    if (!row.detail.empty()) {
        UILabel* help = [[UILabel alloc] init];
        help.text = PRText(row.detail);
        help.font = PRFont(13, UIFontWeightRegular, UIFontTextStyleFootnote);
        help.textColor = UIColor.secondaryLabelColor;
        help.numberOfLines = 0;
        help.adjustsFontForContentSizeCategory = YES;
        [stack addArrangedSubview:help];
    }
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stack];
    UILayoutGuide* margins = self.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:margins.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:margins.bottomAnchor]
    ]];
    self.alpha = row.enabled ? 1 : .5;
    [self changing:_slider];
    return self;
}
- (void)changing:(UISlider*)sender {
    double value = sender.value;
    if (_row.step > 0)
        value = _row.minimum + std::round((value - _row.minimum) / _row.step) * _row.step;
    value = std::clamp(value, _row.minimum, _row.maximum);
    NSNumberFormatter* format = [[NSNumberFormatter alloc] init];
    format.maximumFractionDigits = _row.kind == N::Kind::Integer ? 0 : 2;
    _value.text = [[format stringFromNumber:@(_row.percentage && _row.kind == N::Kind::Decimal ? value * 100 : value)]
        stringByAppendingString:_row.percentage ? @"%" : @""];
    sender.accessibilityValue = _value.text;
}
- (void)commit:(UISlider*)sender {
    double value = sender.value;
    if (_row.step > 0)
        value = _row.minimum + std::round((value - _row.minimum) / _row.step) * _row.step;
    [_owner apply:_row value:std::clamp(value, _row.minimum, _row.maximum)];
}
@end

@interface PRNativeSliderCell : UITableViewCell
- (instancetype)initWithRows:(const std::vector<N::Row>&)rows owner:(PRNativePage*)owner;
@end
@implementation PRNativeSliderCell
- (instancetype)initWithRows:(const std::vector<N::Row>&)rows owner:(PRNativePage*)owner {
    self = [super initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    if (!self)
        return nil;
    self.backgroundConfiguration = [UIBackgroundConfiguration clearConfiguration];
    self.selectionStyle = UITableViewCellSelectionStyleNone;
    UIStackView* stack = [[UIStackView alloc] init];
    stack.distribution = UIStackViewDistributionFillEqually;
    stack.spacing = 10;
    for (const auto& row : rows)
        [stack addArrangedSubview:[[PRNativeSliderControl alloc] initWithRow:row owner:owner]];
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:12],
        [stack.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-12],
        [stack.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:5],
        [stack.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-5]
    ]];
    return self;
}
@end

@interface PRNativeConsole : UIViewController <UITextFieldDelegate> {
    UITextView* _output;
    UITextField* _command;
    UILabel* _empty;
    UIButton* _run;
    UIBarButtonItem* _history;
    NSTimer* _timer;
    N::ConsoleSnapshot _snapshot;
    uint64_t _revision;
    BOOL _refreshing, _scrollNext;
}
- (void)refresh;
- (void)renderOutput;
@end

@implementation PRNativeConsole
- (void)viewDidLoad {
    [super viewDidLoad];
    PRIsolatePage(self);
    self.title = @"Console";
    _revision = ~uint64_t(0);
    _history = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"clock.arrow.circlepath"] menu:nil];
    _history.accessibilityLabel = @"Command history";
    UIBarButtonItem* clear = [[UIBarButtonItem alloc] initWithTitle:@"Clear"
                                                              style:UIBarButtonItemStylePlain
                                                             target:self
                                                             action:@selector(clearOutput)];
    self.navigationItem.rightBarButtonItems = @[ clear, _history ];

    _output = [[UITextView alloc] init];
    _output.accessibilityLabel = @"Console output";
    _output.accessibilityIdentifier = @"console-output";
    _output.editable = NO;
    _output.selectable = YES;
    _output.backgroundColor = PRSurface();
    _output.layer.cornerRadius = 16;
    _output.textContainerInset = UIEdgeInsetsMake(12, 12, 12, 12);
    _output.alwaysBounceVertical = YES;
    _output.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
    _output.adjustsFontForContentSizeCategory = YES;

    _empty = [[UILabel alloc] init];
    _empty.text = @"Console output appears here.";
    _empty.textColor = UIColor.secondaryLabelColor;
    _empty.font = PRFont(14, UIFontWeightRegular, UIFontTextStyleBody);
    _empty.textAlignment = NSTextAlignmentCenter;
    _empty.numberOfLines = 0;
    _empty.userInteractionEnabled = NO;

    UILabel* hint = [[UILabel alloc] init];
    hint.text = @"Enter help to list all available commands.";
    hint.textColor = UIColor.secondaryLabelColor;
    hint.font = PRFont(12, UIFontWeightRegular, UIFontTextStyleFootnote);
    hint.adjustsFontForContentSizeCategory = YES;
    hint.numberOfLines = 0;

    _command = [[UITextField alloc] init];
    _command.placeholder = @"Enter command…";
    _command.accessibilityIdentifier = @"console-command";
    _command.borderStyle = UITextBorderStyleRoundedRect;
    _command.backgroundColor = PRSurface();
    _command.font = [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody]
        scaledFontForFont:[UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular]];
    _command.adjustsFontForContentSizeCategory = YES;
    _command.autocorrectionType = UITextAutocorrectionTypeNo;
    _command.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _command.spellCheckingType = UITextSpellCheckingTypeNo;
    _command.smartQuotesType = UITextSmartQuotesTypeNo;
    _command.smartDashesType = UITextSmartDashesTypeNo;
    _command.returnKeyType = UIReturnKeyGo;
    _command.clearButtonMode = UITextFieldViewModeWhileEditing;
    _command.delegate = self;
    [_command addTarget:self action:@selector(editingChanged) forControlEvents:UIControlEventEditingChanged];
    UIToolbar* keyboardTools = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, 320, 44)];
    keyboardTools.items = @[
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil],
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:self
                                                      action:@selector(dismissKeyboard)]
    ];
    _command.inputAccessoryView = keyboardTools;

    _run = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration* configuration = [UIButtonConfiguration filledButtonConfiguration];
    configuration.title = @"Run";
    configuration.cornerStyle = UIButtonConfigurationCornerStyleMedium;
    _run.configuration = configuration;
    _run.accessibilityIdentifier = @"console-run";
    [_run addTarget:self action:@selector(submitCommand) forControlEvents:UIControlEventTouchUpInside];
    UIStackView* composer = [[UIStackView alloc] initWithArrangedSubviews:@[ _command, _run ]];
    composer.spacing = 8;
    [_command setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [_run setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];

    for (UIView* view in @[ _output, hint, composer ]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [self.view addSubview:view];
    }
    _empty.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_empty];
    UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
    NSLayoutConstraint* outputTop = [_output.topAnchor constraintEqualToAnchor:safe.topAnchor constant:8];
    outputTop.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        outputTop, [_output.heightAnchor constraintGreaterThanOrEqualToConstant:0],
        [_output.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [_output.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-12],
        [_output.bottomAnchor constraintEqualToAnchor:hint.topAnchor constant:-8],
        [hint.leadingAnchor constraintEqualToAnchor:_output.leadingAnchor constant:4],
        [hint.trailingAnchor constraintEqualToAnchor:_output.trailingAnchor constant:-4],
        [hint.bottomAnchor constraintEqualToAnchor:composer.topAnchor constant:-8],
        [composer.leadingAnchor constraintEqualToAnchor:_output.leadingAnchor],
        [composer.trailingAnchor constraintEqualToAnchor:_output.trailingAnchor],
        [composer.bottomAnchor constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor constant:-8],
        [composer.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [_run.widthAnchor constraintGreaterThanOrEqualToConstant:64],
        [_empty.centerYAnchor constraintEqualToAnchor:_output.centerYAnchor],
        [_empty.leadingAnchor constraintEqualToAnchor:_output.leadingAnchor constant:16],
        [_empty.trailingAnchor constraintEqualToAnchor:_output.trailingAnchor constant:-16]
    ]];
    [self editingChanged];
}
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self refresh];
    __weak PRNativeConsole* weak = self;
    _timer = [NSTimer scheduledTimerWithTimeInterval:.5
                                             repeats:YES
                                               block:^(NSTimer*) {
                                                   [weak refresh];
                                               }];
}
- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [_timer invalidate];
    _timer = nil;
    [self.view endEditing:YES];
}
- (void)editingChanged {
    _run.enabled =
        [_command.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet].length > 0;
}
- (BOOL)textFieldShouldReturn:(UITextField*)field {
    [self submitCommand];
    return NO;
}
- (void)textFieldDidBeginEditing:(UITextField*)field {
    // Landscape phones need the header space while the software keyboard is up.
    [PRRoot setConsoleTyping:YES];
    [self.navigationController setNavigationBarHidden:YES animated:NO];
}
- (void)textFieldDidEndEditing:(UITextField*)field {
    [PRRoot setConsoleTyping:NO];
    [self.navigationController setNavigationBarHidden:NO animated:NO];
}
- (void)dismissKeyboard {
    [_command resignFirstResponder];
}
- (void)submitCommand {
    NSString* text = [_command.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (!text.length)
        return;
    const auto command = PRString(text);
    _command.text = @"";
    [self editingChanged];
    _scrollNext = YES;
    __weak PRNativeConsole* weak = self;
    N::Enqueue([weak, command] {
        N::SubmitConsole(command);
        dispatch_async(dispatch_get_main_queue(), ^{
            [weak refresh];
        });
    });
}
- (void)clearOutput {
    __weak PRNativeConsole* weak = self;
    N::Enqueue([weak] {
        N::ClearConsole();
        dispatch_async(dispatch_get_main_queue(), ^{
            [weak refresh];
        });
    });
}
- (void)refresh {
    if (_refreshing)
        return;
    _refreshing = YES;
    __weak PRNativeConsole* weak = self;
    N::Enqueue([weak] {
        auto snapshot = std::make_shared<N::ConsoleSnapshot>(N::ReadConsole());
        dispatch_async(dispatch_get_main_queue(), ^{
            PRNativeConsole* controller = weak;
            if (!controller)
                return;
            controller->_refreshing = NO;
            if (controller->_revision == snapshot->revision)
                return;
            controller->_snapshot = std::move(*snapshot);
            controller->_revision = controller->_snapshot.revision;
            [controller renderOutput];
            NSMutableArray<UIMenuElement*>* entries = [NSMutableArray array];
            size_t count = 0;
            for (auto it = controller->_snapshot.history.rbegin();
                 it != controller->_snapshot.history.rend() && count < 20; ++it, ++count)
            {
                NSString* command = PRText(*it);
                [entries addObject:[UIAction actionWithTitle:command
                                                       image:nil
                                                  identifier:nil
                                                     handler:^(UIAction*) {
                                                         PRNativeConsole* owner = weak;
                                                         if (!owner)
                                                             return;
                                                         owner->_command.text = command;
                                                         [owner editingChanged];
                                                         [owner->_command becomeFirstResponder];
                                                     }]];
            }
            controller->_history.menu = [UIMenu menuWithTitle:@"Recent commands" children:entries];
            controller->_history.enabled = entries.count > 0;
        });
    });
}
- (void)renderOutput {
    const BOOL follow =
        _scrollNext || _output.contentOffset.y + _output.bounds.size.height >= _output.contentSize.height - 40;
    const CGPoint offset = _output.contentOffset;
    NSMutableAttributedString* text = [[NSMutableAttributedString alloc] initWithString:@""];
    UIFont* font = [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody]
        scaledFontForFont:[UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular]];
    NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
    paragraph.paragraphSpacing = 6;
    for (const auto& line : _snapshot.lines) {
        UIColor* color = line.error ? UIColor.systemRedColor
            : line.command          ? UIColor.systemBlueColor
                                    : UIColor.labelColor;
        [text appendAttributedString:[[NSAttributedString alloc]
                                         initWithString:[PRText(line.text) stringByAppendingString:@"\n"]
                                             attributes:@{
                                                 NSFontAttributeName : font,
                                                 NSForegroundColorAttributeName : color,
                                                 NSParagraphStyleAttributeName : paragraph
                                             }]];
    }
    _output.attributedText = text;
    _empty.hidden = !_snapshot.lines.empty();
    if (follow && text.length)
        [_output scrollRangeToVisible:NSMakeRange(text.length - 1, 1)];
    else
        _output.contentOffset = offset;
    _scrollNext = NO;
}
- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    if (self.isViewLoaded)
        [self renderOutput];
}
@end

@implementation PRNativePage
- (instancetype)initWithKey:(std::string)key {
    self = [super initWithStyle:UITableViewStyleGrouped];
    if (self) {
        _key = std::move(key);
        _images = [NSMutableDictionary dictionary];
        _pendingImages = [NSMutableSet set];
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    PRStyleTable(self.tableView);
    PRIsolatePage(self);
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = 62;
    self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
    self.navigationItem.largeTitleDisplayMode = UINavigationItemLargeTitleDisplayModeNever;
    self.refreshControl = [[UIRefreshControl alloc] init];
    [self.refreshControl addTarget:self action:@selector(refresh) forControlEvents:UIControlEventValueChanged];
    [self refresh];
}
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    if (self.isViewLoaded)
        [self refresh];
    if (_key.starts_with("bindings/capture/")) {
        __weak PRNativePage* weak = self;
        _bindingTimer = [NSTimer scheduledTimerWithTimeInterval:.2
                                                        repeats:YES
                                                          block:^(NSTimer*) {
                                                              [weak refresh];
                                                          }];
    }
}
- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [_bindingTimer invalidate];
    _bindingTimer = nil;
    if (_key.starts_with("bindings/capture/")) {
        const auto key = _key;
        N::Enqueue([key] { N::CancelBindingCapture(key); });
    }
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    BOOL two = self.tableView.bounds.size.width >= 440
        && !UIContentSizeCategoryIsAccessibilityCategory(self.traitCollection.preferredContentSizeCategory);
    if (_twoColumns != two) {
        _twoColumns = two;
        [self filter];
    }
}
- (void)refresh {
    const uint64_t generation = ++_generation;
    const auto key = _key;
    __weak PRNativePage* weak = self;
    N::Enqueue([weak, key, generation] {
        auto page = std::make_shared<N::Page>(N::BuildPage(key));
        dispatch_async(dispatch_get_main_queue(), ^{
            PRNativePage* controller = weak;
            if (!controller || controller->_generation != generation)
                return;
            if (page->returnToParent) {
                if (controller.navigationController.topViewController == controller)
                    [controller.navigationController popViewControllerAnimated:YES];
                return;
            }
            controller->_page = std::move(*page);
            controller.title = PRText(controller->_page.title);
            [controller->_images removeAllObjects];
            [controller->_pendingImages removeAllObjects];
            if (controller->_page.searchable && !controller.navigationItem.searchController) {
                UISearchController* search = [[UISearchController alloc] initWithSearchResultsController:nil];
                search.obscuresBackgroundDuringPresentation = NO;
                search.searchResultsUpdater = controller;
                controller.navigationItem.searchController = search;
                controller.definesPresentationContext = YES;
            }
            [controller filter];
            [controller.refreshControl endRefreshing];
        });
    });
}
- (void)filter {
    _sections = _page.sections;
    NSString* query = self.navigationItem.searchController.searchBar.text ?: @"";
    if (query.length)
        for (auto& section : _sections) {
            std::erase_if(section.rows, [query](const N::Row& row) {
                return [PRText(row.title) rangeOfString:query
                                                options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch]
                           .location
                    == NSNotFound;
            });
        }
    std::erase_if(_sections, [](const auto& section) { return section.rows.empty(); });
    _rows.clear();
    for (const auto& section : _sections) {
        auto& ranges = _rows.emplace_back();
        auto numeric = [](const N::Row& row) { return row.kind == N::Kind::Integer || row.kind == N::Kind::Decimal; };
        for (size_t index = 0; index < section.rows.size();) {
            size_t count = _twoColumns && numeric(section.rows[index]) && index + 1 < section.rows.size()
                    && numeric(section.rows[index + 1])
                ? 2
                : 1;
            ranges.emplace_back(index, count);
            index += count;
        }
    }
    [self.tableView reloadData];
}
- (void)updateSearchResultsForSearchController:(UISearchController*)controller {
    [self filter];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
    return _sections.size();
}
- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
    return _rows[section].size();
}
- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section {
    return _sections[section].title.empty() ? nil : PRText(_sections[section].title);
}
- (NSString*)tableView:(UITableView*)tableView titleForFooterInSection:(NSInteger)section {
    return _sections[section].footer.empty() ? nil : PRText(_sections[section].footer);
}
- (CGFloat)tableView:(UITableView*)tableView heightForFooterInSection:(NSInteger)section {
    return _sections[section].footer.empty() ? .01 : UITableViewAutomaticDimension;
}
- (CGFloat)tableView:(UITableView*)tableView heightForHeaderInSection:(NSInteger)section {
    return _sections[section].title.empty() ? 6 : UITableViewAutomaticDimension;
}
- (UIView*)tableView:(UITableView*)tableView viewForHeaderInSection:(NSInteger)section {
    if (_sections[section].title.empty())
        return nil;
    UITableViewHeaderFooterView* header = [[UITableViewHeaderFooterView alloc] initWithReuseIdentifier:nil];
    UIListContentConfiguration* content = [UIListContentConfiguration groupedHeaderConfiguration];
    content.text = PRText(_sections[section].title);
    content.textProperties.font = PRFont(15, UIFontWeightSemibold, UIFontTextStyleSubheadline);
    content.textProperties.color = UIColor.secondaryLabelColor;
    content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(18, 28, 6, 28);
    header.contentConfiguration = content;
    header.backgroundView = [[UIView alloc] init];
    return header;
}
- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
    const auto& rows = _sections[indexPath.section].rows;
    const auto [index, columns] = _rows[indexPath.section][indexPath.row];
    const N::Row row = rows[index];
    if (row.kind == N::Kind::Integer || row.kind == N::Kind::Decimal) {
        std::vector<N::Row> controls { row };
        if (columns == 2 && index + 1 < rows.size())
            controls.push_back(rows[index + 1]);
        return [[PRNativeSliderCell alloc] initWithRows:controls owner:self];
    }
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    PRStyleCard(cell);
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    content.textProperties.font = PRFont(16, UIFontWeightSemibold, UIFontTextStyleBody);
    content.textProperties.numberOfLines = 0;
    content.secondaryTextProperties.font = PRFont(13, UIFontWeightRegular, UIFontTextStyleFootnote);
    content.secondaryTextProperties.color = UIColor.secondaryLabelColor;
    content.textToSecondaryTextVerticalPadding = 5;
    content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(18, 30, 18, 30);
    content.imageProperties.tintColor = UIColor.systemBlueColor;
    content.text = PRText(row.title);
    content.secondaryText = PRText(row.detail);
    content.secondaryTextProperties.numberOfLines = 0;
    if (!row.symbol.empty())
        content.image = [UIImage systemImageNamed:PRText(row.symbol)];
    if (row.portrait) {
        NSString* key = PRText(row.id);
        content.image = _images[key] ?: PRBlankPortrait();
        content.imageProperties.maximumSize = CGSizeMake(44, 44);
        content.imageProperties.reservedLayoutSize = CGSizeMake(44, 44);
        if (!_images[key] && ![_pendingImages containsObject:key]) {
            [_pendingImages addObject:key];
            __weak PRNativePage* weak = self;
            const uint64_t generation = _generation;
            N::Enqueue([weak, row, key, generation] {
                auto image = std::make_shared<N::Image>(row.portrait());
                dispatch_async(dispatch_get_main_queue(), ^{
                    PRNativePage* controller = weak;
                    if (!controller || controller->_generation != generation)
                        return;
                    controller->_images[key] =
                        PRImage(*image, controller.traitCollection.displayScale) ?: PRBlankPortrait();
                    [controller.tableView reloadData];
                });
            });
        }
    }
    if (row.destructive)
        content.textProperties.color = UIColor.systemRedColor;
    else if (row.kind == N::Kind::Action)
        content.textProperties.color = UIColor.systemBlueColor;
    if (row.kind == N::Kind::Choice) {
        for (const auto& choice : row.choices)
            if (choice.value == int(row.value))
                content.secondaryText = PRText(choice.title);
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    }
    if (row.kind == N::Kind::Link || row.kind == N::Kind::Text)
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    if (row.kind == N::Kind::Toggle) {
        UISwitch* toggle = [[UISwitch alloc] init];
        toggle.on = row.value != 0;
        toggle.enabled = row.enabled;
        toggle.accessibilityLabel = PRText(row.title);
        __weak PRNativePage* weak = self;
        [toggle addAction:[UIAction actionWithHandler:^(UIAction* action) {
                    UISwitch* sender = (UISwitch*) action.sender;
                    [weak apply:row value:sender.on ? 1 : 0];
                }]
            forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = toggle;
    }
    if (row.kind == N::Kind::Color) {
        const uint32_t rgba = uint32_t(row.value);
        UIView* swatch = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 28, 28)];
        swatch.layer.cornerRadius = 8;
        swatch.layer.borderWidth = 1;
        swatch.layer.borderColor = UIColor.separatorColor.CGColor;
        swatch.backgroundColor = [UIColor colorWithRed:((rgba >> 24) & 255) / 255.
                                                 green:((rgba >> 16) & 255) / 255.
                                                  blue:((rgba >> 8) & 255) / 255.
                                                 alpha:(rgba & 255) / 255.];
        cell.accessoryView = swatch;
    }
    cell.contentConfiguration = content;
    cell.userInteractionEnabled = row.enabled;
    cell.contentView.alpha = row.enabled ? 1 : .45;
    cell.accessibilityHint = PRText(row.detail);
    return cell;
}
- (void)apply:(const N::Row&)row value:(double)value {
    if (!row.enabled || !row.change)
        return;
    __weak PRNativePage* weak = self;
    N::Enqueue([weak, row, value] {
        row.change(value);
        dispatch_async(dispatch_get_main_queue(), ^{
            [weak refresh];
        });
    });
}
- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    const auto row = _sections[indexPath.section].rows[_rows[indexPath.section][indexPath.row].first];
    if (!row.enabled)
        return;
    if (row.kind == N::Kind::Link) {
        if (row.activate)
            N::Enqueue(row.activate);
        [self.navigationController pushViewController:[[PRNativePage alloc] initWithKey:row.target] animated:YES];
    } else if (row.kind == N::Kind::Choice)
        [self.navigationController pushViewController:[[PRNativeChoices alloc] initWithRow:row owner:self]
                                             animated:YES];
    else if (row.kind == N::Kind::Toggle)
        [self apply:row value:row.value == 0];
    else if (row.kind == N::Kind::Action && row.activate) {
        __weak PRNativePage* weak = self;
        N::Enqueue([weak, row] {
            row.activate();
            dispatch_async(dispatch_get_main_queue(), ^{
                [weak refresh];
            });
        });
    } else if (row.kind == N::Kind::Color) {
        _colorRow = row;
        const uint32_t c = uint32_t(row.value);
        UIColorPickerViewController* picker = [[UIColorPickerViewController alloc] init];
        picker.delegate = self;
        picker.supportsAlpha = row.alpha;
        picker.title = PRText(row.title);
        picker.selectedColor = [UIColor colorWithRed:((c >> 24) & 255) / 255.
                                               green:((c >> 16) & 255) / 255.
                                                blue:((c >> 8) & 255) / 255.
                                               alpha:(c & 255) / 255.];
        [self presentViewController:picker animated:YES completion:nil];
    } else if (row.kind == N::Kind::Text) {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:PRText(row.title)
                                                                       message:PRText(row.detail)
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [alert addTextFieldWithConfigurationHandler:^(UITextField* field) {
            field.autocapitalizationType = UITextAutocapitalizationTypeNone;
            field.autocorrectionType = UITextAutocorrectionTypeNo;
            field.text = PRText(row.text);
        }];
        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        __weak PRNativePage* weak = self;
        __weak UIAlertController* weakAlert = alert;
        [alert addAction:[UIAlertAction actionWithTitle:@"Run"
                                                  style:UIAlertActionStyleDefault
                                                handler:^(UIAlertAction* action) {
                                                    const auto text = PRString(weakAlert.textFields.firstObject.text);
                                                    N::Enqueue([weak, row, text] {
                                                        if (row.submit)
                                                            row.submit(text);
                                                        dispatch_async(dispatch_get_main_queue(), ^{
                                                            [weak refresh];
                                                        });
                                                    });
                                                }]];
        [self presentViewController:alert animated:YES completion:nil];
    } else if (!row.detail.empty()) {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:PRText(row.title)
                                                                       message:PRText(row.detail)
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
        [self presentViewController:alert animated:YES completion:nil];
    }
}
- (void)colorPickerViewControllerDidFinish:(UIColorPickerViewController*)controller {
    CGFloat r = 0, g = 0, b = 0, a = 1;
    [controller.selectedColor getRed:&r green:&g blue:&b alpha:&a];
    const uint32_t rgba = (uint32_t(std::lround(r * 255)) << 24) | (uint32_t(std::lround(g * 255)) << 16)
        | (uint32_t(std::lround(b * 255)) << 8) | uint32_t(std::lround(a * 255));
    [self apply:_colorRow value:rgba];
}
@end

@implementation PRNativeChoices
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner {
    self = [super initWithStyle:UITableViewStylePlain];
    if (self) {
        _row = row;
        _choices = row.choices;
        _owner = owner;
        self.title = PRText(row.title);
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    PRStyleTable(self.tableView);
    PRIsolatePage(self);
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = 52;
    if (_row.choices.size() > 12) {
        UISearchController* search = [[UISearchController alloc] initWithSearchResultsController:nil];
        search.obscuresBackgroundDuringPresentation = NO;
        search.searchResultsUpdater = self;
        self.navigationItem.searchController = search;
    }
}
- (void)updateSearchResultsForSearchController:(UISearchController*)controller {
    _choices = _row.choices;
    NSString* query = controller.searchBar.text ?: @"";
    if (query.length)
        std::erase_if(_choices, [query](const auto& choice) {
            return [PRText(choice.title) rangeOfString:query options:NSCaseInsensitiveSearch].location == NSNotFound;
        });
    [self.tableView reloadData];
}
- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
    return _choices.size();
}
- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
    const auto& choice = _choices[indexPath.row];
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    PRStyleCard(cell);
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    content.text = PRText(choice.title);
    cell.contentConfiguration = content;
    if (choice.value == int(_row.value))
        cell.accessoryType = UITableViewCellAccessoryCheckmark;
    return cell;
}
- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
    [_owner apply:_row value:_choices[indexPath.row].value];
    [self.navigationController popViewControllerAnimated:YES];
}
@end

@implementation PRNativeSidebar
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (std::abs(_labelWidth - self.tableView.bounds.size.width) > .5) {
        _labelWidth = self.tableView.bounds.size.width;
        [self.tableView reloadData];
    }
}
- (CGFloat)labelPointSize {
    // Reserve the icon, margins and disclosure area for every entry, then fit
    // the longest title once. Categories and destinations keep one shared size.
    const CGFloat available = MAX(60, self.tableView.bounds.size.width - 120);
    UIFont* base = PRFont(14, UIFontWeightSemibold, UIFontTextStyleSubheadline);
    CGFloat longest = 1;
    for (const auto& group : _groups) {
        longest = MAX(longest, [PRText(group.title) sizeWithAttributes:@{ NSFontAttributeName : base }].width);
        for (const auto& page : group.pages)
            longest = MAX(longest, [PRText(page.title) sizeWithAttributes:@{ NSFontAttributeName : base }].width);
    }
    return base.pointSize * MIN(1, available / longest);
}
- (instancetype)initWithGroups:(std::vector<N::Group>)groups initial:(std::string)initial host:(PRNativeRoot*)host {
    self = [super initWithStyle:UITableViewStylePlain];
    if (self) {
        _groups = std::move(groups);
        _host = host;
        _expanded = 0;
        _selected = PRText(initial);
        self.title = @"Browse";
        for (size_t index = 0; index < _groups.size(); ++index)
            for (const auto& page : _groups[index].pages)
                if (page.key == initial)
                    _expanded = index;
    }
    return self;
}
- (void)selectPage:(const std::string&)key {
    _selected = PRText(key);
    for (size_t index = 0; index < _groups.size(); ++index)
        for (const auto& page : _groups[index].pages)
            if (page.key == key) {
                _expanded = index;
                [_host setCategory:PRText(_groups[index].title)];
            }
    if (self.isViewLoaded)
        [self.tableView reloadData];
}
- (void)viewDidLoad {
    [super viewDidLoad];
    PRStyleTable(self.tableView);
    PRIsolatePage(self);
    self.tableView.contentInset = UIEdgeInsetsMake(8, 0, 12, 0);
    self.tableView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    self.tableView.showsVerticalScrollIndicator = NO;
    self.tableView.showsHorizontalScrollIndicator = NO;
    // Newer UIKit versions add a scroll-edge material even to a hidden-bar
    // sidebar. Use the public property dynamically when running with an older SDK.
    for (NSString* name in @[ @"topEdgeEffect", @"bottomEdgeEffect" ])
        if ([self.tableView respondsToSelector:NSSelectorFromString(name)]) {
            id effect = [self.tableView valueForKey:name];
            if ([effect respondsToSelector:NSSelectorFromString(@"setHidden:")])
                [effect setValue:@YES forKey:@"hidden"];
        }
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = 46;
}
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self.navigationController setNavigationBarHidden:YES animated:NO];
}
- (CGFloat)tableView:(UITableView*)tableView heightForHeaderInSection:(NSInteger)section {
    return .01;
}
- (CGFloat)tableView:(UITableView*)tableView heightForFooterInSection:(NSInteger)section {
    return .01;
}
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
    return _groups.size();
}
- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
    return 1 + (_expanded == section ? _groups[section].pages.size() : 0);
}
- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
    const auto& group = _groups[indexPath.section];
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    UIBackgroundConfiguration* background = [UIBackgroundConfiguration clearConfiguration];
    background.cornerRadius = 12;
    background.backgroundInsets = NSDirectionalEdgeInsetsMake(2, 8, 2, 8);
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(12, 18, 12, 18);
    content.imageToTextPadding = 12;
    content.imageProperties.preferredSymbolConfiguration = [UIImageSymbolConfiguration configurationWithPointSize:18];
    content.imageProperties.reservedLayoutSize = CGSizeMake(22, 22);
    content.imageProperties.tintColor = UIColor.secondaryLabelColor;
    content.textProperties.numberOfLines = 1;
    const CGFloat pointSize = [self labelPointSize];
    if (!indexPath.row) {
        content.text = PRText(group.title);
        content.image = [UIImage systemImageNamed:PRText(group.symbol)];
        content.textProperties.font = [UIFont systemFontOfSize:pointSize weight:UIFontWeightSemibold];
        cell.accessoryView = [[UIImageView alloc]
            initWithImage:[UIImage
                              systemImageNamed:_expanded == indexPath.section ? @"chevron.down" : @"chevron.right"]];
        cell.accessoryView.tintColor = UIColor.tertiaryLabelColor;
    } else {
        const auto& page = group.pages[indexPath.row - 1];
        content.textProperties.font = [UIFont systemFontOfSize:pointSize weight:UIFontWeightMedium];
        content.text = PRText(page.title);
        content.image = [UIImage systemImageNamed:PRText(page.symbol)];
        content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(13, 24, 13, 18);
        if ([_selected isEqualToString:PRText(page.key)]) {
            background.backgroundColor = UIColor.systemBlueColor;
            content.textProperties.color = UIColor.whiteColor;
            content.imageProperties.tintColor = UIColor.whiteColor;
            cell.accessibilityTraits |= UIAccessibilityTraitSelected;
        }
    }
    cell.backgroundConfiguration = background;
    cell.contentConfiguration = content;
    return cell;
}
- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if (!indexPath.row) {
        _expanded = _expanded == indexPath.section ? -1 : indexPath.section;
        [tableView reloadData];
    } else {
        const auto& page = _groups[indexPath.section].pages[indexPath.row - 1];
        _selected = PRText(page.key);
        [_host setCategory:PRText(_groups[indexPath.section].title)];
        [_host openPage:page.key];
        [tableView reloadData];
    }
}
@end

@implementation PRNativeRoot
- (instancetype)init {
    self = [super init];
    if (self) {
        self.modalPresentationStyle = UIModalPresentationOverFullScreen;
        self.modalInPresentation = YES;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.clearColor;
    UIVisualEffectView* material = [[UIVisualEffectView alloc]
        initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterial]];
    material.frame = self.view.bounds;
    material.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    material.userInteractionEnabled = NO;
    [self.view addSubview:material];
    UIView* canvas = [[UIView alloc] initWithFrame:self.view.bounds];
    canvas.backgroundColor = PRCanvas();
    canvas.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    canvas.userInteractionEnabled = NO;
    [self.view addSubview:canvas];
    _header = [[UIView alloc] init];
    [self.view addSubview:_header];
    UIImageView* logo = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"paperplane.fill"]];
    logo.tintColor = UIColor.systemRedColor;
    logo.contentMode = UIViewContentModeScaleAspectFit;
    [logo.widthAnchor constraintEqualToConstant:28].active = YES;
    UILabel* name = [[UILabel alloc] init];
    name.text = @"PaperReality";
    name.font = PRFont(20, UIFontWeightBold, UIFontTextStyleHeadline);
    name.adjustsFontForContentSizeCategory = YES;
    name.adjustsFontSizeToFitWidth = YES;
    name.minimumScaleFactor = .7;
    _category = [[UILabel alloc] init];
    _category.text = @"Settings";
    _category.font = PRFont(15, UIFontWeightRegular, UIFontTextStyleSubheadline);
    _category.textColor = UIColor.secondaryLabelColor;
    _category.adjustsFontForContentSizeCategory = YES;
    UIStackView* brand = [[UIStackView alloc] initWithArrangedSubviews:@[ logo, name, _category ]];
    brand.spacing = 12;
    brand.alignment = UIStackViewAlignmentCenter;
    brand.translatesAutoresizingMaskIntoConstraints = NO;
    [_header addSubview:brand];
    UIButton* done = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration* config = [UIButtonConfiguration filledButtonConfiguration];
    config.title = @"Done";
    config.cornerStyle = UIButtonConfigurationCornerStyleMedium;
    config.contentInsets = NSDirectionalEdgeInsetsMake(10, 20, 10, 20);
    done.configuration = config;
    [done addTarget:self action:@selector(closeSettings) forControlEvents:UIControlEventTouchUpInside];
    done.translatesAutoresizingMaskIntoConstraints = NO;
    [_header addSubview:done];
    [NSLayoutConstraint activateConstraints:@[
        [brand.leadingAnchor constraintEqualToAnchor:_header.leadingAnchor constant:8],
        [brand.centerYAnchor constraintEqualToAnchor:_header.centerYAnchor],
        [brand.trailingAnchor constraintLessThanOrEqualToAnchor:done.leadingAnchor constant:-16],
        [done.trailingAnchor constraintEqualToAnchor:_header.trailingAnchor constant:-8],
        [done.centerYAnchor constraintEqualToAnchor:_header.centerYAnchor],
        [done.heightAnchor constraintGreaterThanOrEqualToConstant:44]
    ]];
    _primaryPanel = [[UIView alloc] init];
    _primaryPanel.backgroundColor = PRSurface();
    _primaryPanel.layer.cornerRadius = 18;
    _primaryPanel.clipsToBounds = YES;
    [self.view addSubview:_primaryPanel];
    _detailPanel = [[UIView alloc] init];
    _detailPanel.clipsToBounds = YES;
    [self.view addSubview:_detailPanel];
    _menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_menuButton setImage:[UIImage systemImageNamed:@"sidebar.left"] forState:UIControlStateNormal];
    _menuButton.accessibilityLabel = @"Browse settings categories";
    [_menuButton addTarget:self action:@selector(browse) forControlEvents:UIControlEventTouchUpInside];
    _menuButton.frame = CGRectMake(0, 0, 44, 44);
}
- (void)setCategory:(NSString*)title {
    _category.text = title;
}
- (void)setViewController:(UIViewController*)controller forColumn:(UISplitViewControllerColumn)column {
    [self loadViewIfNeeded];
    [self addChildViewController:controller];
    UIView* panel = column == UISplitViewControllerColumnPrimary ? _primaryPanel : _detailPanel;
    controller.view.frame = panel.bounds;
    controller.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [panel addSubview:controller.view];
    [controller didMoveToParentViewController:self];
    if (column == UISplitViewControllerColumnPrimary) {
        self.primary = (UINavigationController*) controller;
        [self.primary setNavigationBarHidden:YES];
    } else {
        self.detail = (UINavigationController*) controller;
    }
    [self.view setNeedsLayout];
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    UIEdgeInsets safe = self.view.safeAreaInsets;
    CGFloat width = self.view.bounds.size.width - safe.left - safe.right - 24;
    CGFloat height = self.view.bounds.size.height - safe.top - safe.bottom;
    // Use the available width, not iPhone's compact size class, for the landscape sidebar.
    // Large accessibility text falls back to one pane so controls retain room to wrap.
    _wide = width >= 700
        && !UIContentSizeCategoryIsAccessibilityCategory(self.traitCollection.preferredContentSizeCategory);
    CGFloat headerHeight = _consoleTyping                                                                 ? 0
        : UIContentSizeCategoryIsAccessibilityCategory(self.traitCollection.preferredContentSizeCategory) ? 76
                                                                                                          : 56;
    _header.hidden = _consoleTyping;
    _header.frame = CGRectMake(safe.left + 12, safe.top, width, headerHeight);
    CGFloat y = safe.top + headerHeight + 6;
    CGFloat bodyHeight = MAX(0, height - headerHeight - 14);
    CGFloat sidebarWidth = _wide ? std::clamp(width * .28, 220., 290.) : width;
    _primaryPanel.frame = CGRectMake(safe.left + 12, y, sidebarWidth, bodyHeight);
    CGFloat x = safe.left + 12 + (_wide ? sidebarWidth + 12 : 0);
    _detailPanel.frame = CGRectMake(x, y, width - (_wide ? sidebarWidth + 12 : 0), bodyHeight);
    _primaryPanel.hidden = !_wide && !_showPrimary;
    _detailPanel.hidden = !_wide && _showPrimary;
    _category.hidden =
        width < 500 || UIContentSizeCategoryIsAccessibilityCategory(self.traitCollection.preferredContentSizeCategory);
    UIViewController* page = self.detail.viewControllers.firstObject;
    page.navigationItem.leftBarButtonItem = _wide ? nil : [[UIBarButtonItem alloc] initWithCustomView:_menuButton];
}
- (void)traitCollectionDidChange:(UITraitCollection*)previous {
    [super traitCollectionDidChange:previous];
    [self.view setNeedsLayout];
}
- (void)browse {
    [self showColumn:UISplitViewControllerColumnPrimary];
}
- (void)setConsoleTyping:(BOOL)typing {
    _consoleTyping = typing;
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
}
- (void)showColumn:(UISplitViewControllerColumn)column {
    _showPrimary = column == UISplitViewControllerColumnPrimary;
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
}
- (void)openPage:(std::string)key {
    N::Enqueue([key] { N::RememberPage(key); });
    [(PRNativeSidebar*) self.primary.viewControllers.firstObject selectPage:key];
    UIViewController* page = (key == "menu/Dev Tools/Console" || key == "console")
        ? [[PRNativeConsole alloc] init]
        : [[PRNativePage alloc] initWithKey:std::move(key)];
    [self.detail setViewControllers:@[ page ] animated:NO];
    [self showColumn:UISplitViewControllerColumnSecondary];
}
- (void)closeSettings {
    N::Enqueue([] { N::Close(); });
}
- (NSArray<UIKeyCommand*>*)keyCommands {
    return @[ [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(closeSettings)] ];
}
@end

struct PRAlertInfo {
    std::string title, message, first, second;
    std::function<void()> firstAction, secondAction;
};
static std::deque<PRAlertInfo> PRAlerts;
static bool PRAlertVisible = false;
static void PRShowNextAlert() {
    if (PRAlertVisible || PRAlerts.empty() || !PRWanted.load())
        return;
    UIViewController* top = PRTopController();
    if (!top || top.presentedViewController || top.isBeingPresented || top.isBeingDismissed || top.transitionCoordinator
        || [top isKindOfClass:UIDocumentPickerViewController.class] || [top isKindOfClass:UIAlertController.class])
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.15 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            PRShowNextAlert();
        });
        return;
    }
    const auto item = PRAlerts.front();
    PRAlerts.pop_front();
    PRAlertVisible = true;
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:PRText(item.title)
                                                                   message:PRText(item.message)
                                                            preferredStyle:UIAlertControllerStyleAlert];
    auto finish = [](std::function<void()> callback) {
        if (callback)
            N::Enqueue([callback = std::move(callback)] {
                callback();
                dispatch_async(dispatch_get_main_queue(), ^{
                    UIViewController* page = PRRoot.detail.topViewController;
                    if ([page isKindOfClass:PRNativePage.class])
                        [(PRNativePage*) page refresh];
                });
            });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.35 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            PRAlertVisible = false;
            PRShowNextAlert();
        });
    };
    [alert addAction:[UIAlertAction actionWithTitle:PRText(item.first.empty() ? "OK" : item.first)
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction*) {
                                                finish(item.firstAction);
                                            }]];
    if (!item.second.empty())
        [alert addAction:[UIAlertAction actionWithTitle:PRText(item.second)
                                                  style:UIAlertActionStyleCancel
                                                handler:^(UIAlertAction*) {
                                                    finish(item.secondAction);
                                                }]];
    [top presentViewController:alert animated:YES completion:nil];
}

// Main queue only. Each delayed attempt owns its captures until it returns.
// Clearing a self-capturing block from inside its first synchronous invocation
// could destroy its std::string/shared_ptr captures while they were still in use.
static void PRPresentSettings(
    std::shared_ptr<std::vector<N::Group>> groups, std::string initial, uint64_t generation, int attempt = 0
) {
    if (generation != PRPresentation.load())
        return;
    UIViewController* top = PRTopController();
    if (!top || top.presentedViewController || top.isBeingPresented || top.isBeingDismissed) {
        if (attempt < 29) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                PRPresentSettings(groups, initial, generation, attempt + 1);
            });
        } else {
            N::Enqueue([] { N::OpenClassic("Settings", "General"); });
        }
        return;
    }
    PRRoot = [[PRNativeRoot alloc] init];
    PRNativeSidebar* sidebar = [[PRNativeSidebar alloc] initWithGroups:*groups initial:initial host:PRRoot];
    UINavigationController* primary = [[UINavigationController alloc] initWithRootViewController:sidebar];
    PRStyleNavigation(primary);
    PRRoot.detail =
        [[UINavigationController alloc] initWithRootViewController:[[PRNativePage alloc] initWithKey:initial]];
    PRStyleNavigation(PRRoot.detail);
    [PRRoot setViewController:primary forColumn:UISplitViewControllerColumnPrimary];
    [PRRoot setViewController:PRRoot.detail forColumn:UISplitViewControllerColumnSecondary];
    for (const auto& group : *groups)
        for (const auto& page : group.pages)
            if (page.key == initial)
                [PRRoot setCategory:PRText(group.title)];
    [top presentViewController:PRRoot animated:YES completion:nil];
}

namespace Paperboat::NativeSettings {
bool IsPresented() {
    return PRWanted.load();
}
void SetPresented(bool presented, std::vector<Group> navigation) {
    const uint64_t generation = ++PRPresentation;
    PRWanted = presented;
    auto groups = std::make_shared<std::vector<Group>>(std::move(navigation));
    const std::string initial = presented ? InitialPage() : "";
    dispatch_async(dispatch_get_main_queue(), ^{
        if (generation != PRPresentation.load())
            return;
        if (!presented) {
            if (PRRoot.presentingViewController)
                [PRRoot dismissViewControllerAnimated:YES completion:nil];
            PRRoot = nil;
            PRAlerts.clear();
            PRAlertVisible = false;
            return;
        }
        PRPresentSettings(groups, initial, generation);
    });
}
bool Alert(
    std::string title,
    std::string message,
    std::string first,
    std::string second,
    std::function<void()> firstAction,
    std::function<void()> secondAction
) {
    if (!PRWanted.load())
        return false;
    auto alert = std::make_shared<PRAlertInfo>(PRAlertInfo { std::move(title), std::move(message), std::move(first),
                                                             std::move(second), std::move(firstAction),
                                                             std::move(secondAction) });
    dispatch_async(dispatch_get_main_queue(), ^{
        PRAlerts.push_back(*alert);
        PRShowNextAlert();
    });
    return true;
}
} // namespace Paperboat::NativeSettings
#endif
