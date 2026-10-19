#include "port/ui/NativeSettings.h"
#if defined(__IOS__)
#import <UIKit/UIKit.h>
#include <atomic>
#include <deque>
#include <memory>
#include <algorithm>
#include <cmath>

namespace N = Paperboat::NativeSettings;
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

static UIImage* PRImage(const N::Image& image) {
    if (image.width <= 0 || image.height <= 0 || image.rgba.size() < size_t(image.width) * image.height * 4)
        return nil;
    NSData* data = [NSData dataWithBytes:image.rgba.data() length:image.rgba.size()];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef) data);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef cg = CGImageCreate(
        image.width, image.height, 8, 32, image.width * 4, space,
        CGBitmapInfo(kCGImageAlphaLast | kCGBitmapByteOrder32Big), provider, nullptr, NO, kCGRenderingIntentDefault
    );
    UIImage* result = cg ? [UIImage imageWithCGImage:cg] : nil;
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
    uint64_t _generation;
    NSMutableDictionary<NSString*, UIImage*>* _images;
    NSMutableSet<NSString*>* _pendingImages;
    N::Row _colorRow;
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
}
- (instancetype)initWithGroups:(std::vector<N::Group>)groups initial:(std::string)initial host:(PRNativeRoot*)host;
@end

@interface PRNativeRoot : UISplitViewController <UISplitViewControllerDelegate>
@property (nonatomic, strong) UINavigationController* detail;
- (void)openPage:(std::string)key;
- (void)closeSettings;
@end
static PRNativeRoot* PRRoot = nil;

@interface PRNativeSliderCell : UITableViewCell {
    N::Row _row;
    __weak PRNativePage* _owner;
    UILabel* _value;
    UISlider* _slider;
}
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner;
@end

@implementation PRNativeSliderCell
- (instancetype)initWithRow:(const N::Row&)row owner:(PRNativePage*)owner {
    self = [super initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    if (!self)
        return nil;
    _row = row;
    _owner = owner;
    self.selectionStyle = UITableViewCellSelectionStyleNone;
    UILabel* title = [[UILabel alloc] init];
    title.text = PRText(row.title);
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    title.numberOfLines = 0;
    title.adjustsFontForContentSizeCategory = YES;
    _value = [[UILabel alloc] init];
    _value.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _value.textColor = UIColor.secondaryLabelColor;
    _value.adjustsFontForContentSizeCategory = YES;
    [_value setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
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
    controls.spacing = 12;
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
        __weak PRNativeSliderCell* weak = self;
        [button addAction:[UIAction actionWithHandler:^(UIAction* action) {
                    PRNativeSliderCell* cell = weak;
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
        help.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
        help.textColor = UIColor.secondaryLabelColor;
        help.numberOfLines = 0;
        help.adjustsFontForContentSizeCategory = YES;
        [stack addArrangedSubview:help];
    }
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:stack];
    UILayoutGuide* margins = self.contentView.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:margins.topAnchor constant:4],
        [stack.bottomAnchor constraintEqualToAnchor:margins.bottomAnchor constant:-4]
    ]];
    self.contentView.alpha = row.enabled ? 1 : .5;
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

@implementation PRNativePage
- (instancetype)initWithKey:(std::string)key {
    self = [super initWithStyle:UITableViewStyleInsetGrouped];
    if (self) {
        _key = std::move(key);
        _images = [NSMutableDictionary dictionary];
        _pendingImages = [NSMutableSet set];
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = 62;
    self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:PRRoot
                                                      action:@selector(closeSettings)];
    self.navigationItem.largeTitleDisplayMode = UINavigationItemLargeTitleDisplayModeAlways;
    self.refreshControl = [[UIRefreshControl alloc] init];
    [self.refreshControl addTarget:self action:@selector(refresh) forControlEvents:UIControlEventValueChanged];
    [self refresh];
}
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    if (self.isViewLoaded)
        [self refresh];
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
    [self.tableView reloadData];
}
- (void)updateSearchResultsForSearchController:(UISearchController*)controller {
    [self filter];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
    return _sections.size();
}
- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
    return _sections[section].rows.size();
}
- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section {
    return PRText(_sections[section].title);
}
- (NSString*)tableView:(UITableView*)tableView titleForFooterInSection:(NSInteger)section {
    return PRText(_sections[section].footer);
}
- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
    const N::Row row = _sections[indexPath.section].rows[indexPath.row];
    if (row.kind == N::Kind::Integer || row.kind == N::Kind::Decimal)
        return [[PRNativeSliderCell alloc] initWithRow:row owner:self];
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    content.text = PRText(row.title);
    content.secondaryText = PRText(row.detail);
    content.secondaryTextProperties.numberOfLines = 3;
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
                    controller->_images[key] = PRImage(*image) ?: PRBlankPortrait();
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
    const auto row = _sections[indexPath.section].rows[indexPath.row];
    if (!row.enabled)
        return;
    if (row.kind == N::Kind::Link)
        [self.navigationController pushViewController:[[PRNativePage alloc] initWithKey:row.target] animated:YES];
    else if (row.kind == N::Kind::Choice)
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
    self = [super initWithStyle:UITableViewStyleInsetGrouped];
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
- (instancetype)initWithGroups:(std::vector<N::Group>)groups initial:(std::string)initial host:(PRNativeRoot*)host {
    self = [super initWithStyle:UITableViewStyleInsetGrouped];
    if (self) {
        _groups = std::move(groups);
        _host = host;
        _expanded = 0;
        _selected = PRText(initial);
        self.title = @"PaperReality";
        for (size_t index = 0; index < _groups.size(); ++index)
            for (const auto& page : _groups[index].pages)
                if (page.key == initial)
                    _expanded = index;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = 52;
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:_host
                                                      action:@selector(closeSettings)];
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
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    if (!indexPath.row) {
        content.text = PRText(group.title);
        content.image = [UIImage systemImageNamed:PRText(group.symbol)];
        content.textProperties.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        cell.accessoryView = [[UIImageView alloc]
            initWithImage:[UIImage
                              systemImageNamed:_expanded == indexPath.section ? @"chevron.down" : @"chevron.right"]];
        cell.accessoryView.tintColor = UIColor.tertiaryLabelColor;
    } else {
        const auto& page = group.pages[indexPath.row - 1];
        content.text = PRText(page.title);
        content.image = [UIImage systemImageNamed:PRText(page.symbol)];
        cell.indentationLevel = 1;
        cell.indentationWidth = 12;
        if ([_selected isEqualToString:PRText(page.key)]) {
            UIBackgroundConfiguration* background = [UIBackgroundConfiguration listGroupedCellConfiguration];
            background.backgroundColor = [UIColor.systemBlueColor colorWithAlphaComponent:.13];
            cell.backgroundConfiguration = background;
            content.textProperties.color = UIColor.systemBlueColor;
        }
    }
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
        [_host openPage:page.key];
        [tableView reloadData];
    }
}
@end

@implementation PRNativeRoot
- (instancetype)init {
    self = [super initWithStyle:UISplitViewControllerStyleDoubleColumn];
    if (self) {
        self.preferredDisplayMode = UISplitViewControllerDisplayModeOneBesideSecondary;
        self.preferredSplitBehavior = UISplitViewControllerSplitBehaviorTile;
        self.minimumPrimaryColumnWidth = 230;
        self.maximumPrimaryColumnWidth = 320;
        self.preferredPrimaryColumnWidthFraction = .28;
        self.delegate = self;
        self.modalPresentationStyle = UIModalPresentationOverFullScreen;
        self.modalInPresentation = YES;
        self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;
    }
    return self;
}
- (void)openPage:(std::string)key {
    N::Enqueue([key] { N::RememberPage(key); });
    PRNativePage* page = [[PRNativePage alloc] initWithKey:std::move(key)];
    [self.detail setViewControllers:@[ page ] animated:NO];
    [self showColumn:UISplitViewControllerColumnSecondary];
}
- (void)closeSettings {
    N::Enqueue([] { N::Close(); });
}
- (UISplitViewControllerColumn)splitViewController:(UISplitViewController*)controller
         topColumnForCollapsingToProposedTopColumn:(UISplitViewControllerColumn)column {
    return UISplitViewControllerColumnPrimary;
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
    if (!top || top.presentedViewController || top.isBeingPresented || top.isBeingDismissed) {
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
            N::Enqueue(std::move(callback));
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
        __block int attempts = 0;
        __block void (^show)(void) = nil;
        show = ^{
            if (generation != PRPresentation.load()) {
                show = nil;
                return;
            }
            UIViewController* top = PRTopController();
            if (!top || top.presentedViewController || top.isBeingPresented || top.isBeingDismissed) {
                if (++attempts < 30) {
                    dispatch_after(
                        dispatch_time(DISPATCH_TIME_NOW, int64_t(.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), show
                    );
                    return;
                }
                show = nil;
                Enqueue([] { OpenClassic("Settings", "General"); });
                return;
            }
            show = nil;
            PRRoot = [[PRNativeRoot alloc] init];
            PRNativeSidebar* sidebar = [[PRNativeSidebar alloc] initWithGroups:*groups initial:initial host:PRRoot];
            UINavigationController* primary = [[UINavigationController alloc] initWithRootViewController:sidebar];
            primary.navigationBar.prefersLargeTitles = YES;
            PRRoot.detail =
                [[UINavigationController alloc] initWithRootViewController:[[PRNativePage alloc] initWithKey:initial]];
            PRRoot.detail.navigationBar.prefersLargeTitles = YES;
            [PRRoot setViewController:primary forColumn:UISplitViewControllerColumnPrimary];
            [PRRoot setViewController:PRRoot.detail forColumn:UISplitViewControllerColumnSecondary];
            [top presentViewController:PRRoot animated:YES completion:nil];
        };
        show();
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
