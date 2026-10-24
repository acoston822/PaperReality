#include "IosControllerNavigation.h"
#if defined(__IOS__)
#include "port/ui/NativeControllerNavigation.h"
#import <GameController/GameController.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#include <algorithm>
#include <limits>

namespace PN = Paperboat::NativeSettings;
static BOOL PNValidRow(UITableView* table, NSIndexPath* path);
@implementation PRControllerTarget
@end

PRControllerTarget*
PRControllerViewTarget(UIView* view, NSString* identifier, NSString* label, void (^activate)(void)) {
    PRControllerTarget* target = [[PRControllerTarget alloc] init];
    target.identifier = identifier;
    target.label = label;
    target.activate = activate;
    __weak UIView* weak = view;
    target.frame = ^CGRect(UIView* overlay) {
        return [weak convertRect:weak.bounds toView:overlay];
    };
    target.viewport = ^CGRect(UIView* overlay) {
        CGRect rect = overlay.bounds;
        for (UIView* ancestor = weak; ancestor && ancestor != overlay; ancestor = ancestor.superview)
            if (ancestor.clipsToBounds)
                rect = CGRectIntersection(rect, [ancestor convertRect:ancestor.bounds toView:overlay]);
        return rect;
    };
    return target;
}
PRControllerTarget* PRControllerRowTarget(
    UITableView* table, NSIndexPath* path, NSString* identifier, NSString* label, void (^activate)(void)
) {
    PRControllerTarget* target = PRControllerViewTarget(table, identifier, label, activate);
    __weak UITableView* weak = table;
    target.frame = ^CGRect(UIView* overlay) {
        return PNValidRow(weak, path) ? [weak convertRect:[weak rectForRowAtIndexPath:path] toView:overlay]
                                      : CGRectZero;
    };
    target.reveal = ^{
        if (!PNValidRow(weak, path))
            return;
        if (![weak.indexPathsForVisibleRows containsObject:path])
            [weak scrollToRowAtIndexPath:path atScrollPosition:UITableViewScrollPositionMiddle animated:NO];
        [weak layoutIfNeeded];
    };
    return target;
}
static BOOL PNValidRow(UITableView* table, NSIndexPath* path) {
    return table && path.section < table.numberOfSections && path.row < [table numberOfRowsInSection:path.section];
}
static UIViewController* PNTop() {
    UIWindow* window = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class])
            for (UIWindow* candidate in ((UIWindowScene*) scene).windows)
                if (candidate.screen == UIScreen.mainScreen && !candidate.hidden && candidate.rootViewController
                    && (!window || candidate.keyWindow))
                    window = candidate;
    if (!window) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        for (UIWindow* candidate in UIApplication.sharedApplication.windows)
            if (candidate.screen == UIScreen.mainScreen && !candidate.hidden && candidate.rootViewController
                && (!window || candidate.keyWindow))
                window = candidate;
#pragma clang diagnostic pop
    }
    UIViewController* top = window.rootViewController;
    while (top.presentedViewController && !top.presentedViewController.isBeingDismissed)
        top = top.presentedViewController;
    return top;
}
static BOOL PNUsingController = NO;
BOOL PRControllerInUse() {
    return PNUsingController;
}
BOOL PRControllerConnected() {
    return GCController.controllers.count > 0;
}

// Save callbacks separately: UIAlertAction intentionally has no public API for
// invoking its handler. Never call private selectors or synthesize touch events.
@interface PNAlertAction : NSObject
@property (nonatomic, strong) UIAlertAction* action;
@property (nonatomic, copy) void (^invoke)(void);
@end
@implementation PNAlertAction
@end
static char PNAlertActionsKey;
void PRControllerAddAlertAction(
    UIAlertController* alert, NSString* title, UIAlertActionStyle style, void (^handler)(UIAlertAction*)
) {
    NSMutableArray* entries = objc_getAssociatedObject(alert, &PNAlertActionsKey);
    if (!entries) {
        entries = [NSMutableArray array];
        objc_setAssociatedObject(alert, &PNAlertActionsKey, entries, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    UIAlertAction* action = [UIAlertAction actionWithTitle:title style:style handler:handler];
    PNAlertAction* entry = [[PNAlertAction alloc] init];
    entry.action = action;
    __weak UIAlertAction* weak = action;
    entry.invoke = ^{
        if (handler)
            handler(weak);
    };
    [entries addObject:entry];
    [alert addAction:action];
}
static UIView* PNFindLabel(UIView* view, NSString* text) {
    if ([view isKindOfClass:UILabel.class] && [((UILabel*) view).text isEqualToString:text])
        return view;
    for (UIView* child in view.subviews) {
        UIView* found = PNFindLabel(child, text);
        if (found)
            return found;
    }
    return nil;
}
static UIScrollView* PNFindScroll(UIView* view) {
    if (view.hidden || !view.userInteractionEnabled)
        return nil;
    if ([view isKindOfClass:UIScrollView.class])
        return (UIScrollView*) view;
    for (UIView* child in view.subviews) {
        UIScrollView* scroll = PNFindScroll(child);
        if (scroll)
            return scroll;
    }
    return nil;
}

@interface PNTextEditor : UIViewController <PRControllerNavigable> {
    NSMutableString* _text;
    UITextView* _preview;
    UIStackView* _keys;
    BOOL _upper, _symbols;
    void (^_save)(NSString*);
}
- (instancetype)initWithTitle:(NSString*)title value:(NSString*)value save:(void (^)(NSString*))save;
@end
@implementation PNTextEditor
- (instancetype)initWithTitle:(NSString*)title value:(NSString*)value save:(void (^)(NSString*))save {
    self = [super init];
    if (self) {
        self.title = title;
        _text = [value mutableCopy] ?: [NSMutableString string];
        _save = [save copy];
        self.modalPresentationStyle = UIModalPresentationFullScreen;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    UILabel* title = [[UILabel alloc] init];
    title.text = self.title;
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _preview = [[UITextView alloc] init];
    _preview.editable = NO;
    _preview.text = _text;
    _preview.backgroundColor = UIColor.secondarySystemBackgroundColor;
    _preview.font = [UIFont monospacedSystemFontOfSize:17 weight:UIFontWeightRegular];
    _preview.layer.cornerRadius = 10;
    [_preview.heightAnchor constraintEqualToConstant:54].active = YES;
    _keys = [[UIStackView alloc] init];
    _keys.axis = UILayoutConstraintAxisVertical;
    _keys.spacing = 4;
    UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[ title, _preview, _keys ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 8;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:stack];
    UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
        [stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
        [stack.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
        [stack.topAnchor constraintGreaterThanOrEqualToAnchor:safe.topAnchor constant:4],
        [stack.bottomAnchor constraintLessThanOrEqualToAnchor:safe.bottomAnchor constant:-4]
    ]];
    [self buildKeys];
}
- (UIButton*)button:(NSString*)title identifier:(NSString*)identifier action:(void (^)(void))action {
    UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration* config = [UIButtonConfiguration tintedButtonConfiguration];
    config.title = title;
    config.contentInsets = NSDirectionalEdgeInsetsMake(5, 4, 5, 4);
    button.configuration = config;
    button.accessibilityIdentifier = identifier;
    [button.heightAnchor constraintGreaterThanOrEqualToConstant:32].active = YES;
    [button addAction:[UIAction actionWithHandler:^(UIAction*) {
                action();
            }]
        forControlEvents:UIControlEventTouchUpInside];
    return button;
}
- (void)showText {
    _preview.text = _text;
    if (_text.length)
        [_preview scrollRangeToVisible:NSMakeRange(_text.length - 1, 1)];
}
- (void)erase {
    if (_text.length)
        [_text deleteCharactersInRange:[_text rangeOfComposedCharacterSequenceAtIndex:_text.length - 1]];
    [self showText];
}
- (void)buildKeys {
    for (UIView* child in _keys.arrangedSubviews) {
        [_keys removeArrangedSubview:child];
        [child removeFromSuperview];
    }
    NSArray<NSString*>* rows = _symbols ? @[ @"!@#$%^&*()", @"[]{}<>_+=-", @"\\|;:'\",.?/", @"`~" ]
                                        : @[ @"1234567890", @"qwertyuiop", @"asdfghjkl", @"zxcvbnm" ];
    __weak PNTextEditor* weak = self;
    NSInteger rowIndex = 0;
    for (NSString* raw in rows) {
        NSString* letters = _upper ? raw.uppercaseString : raw;
        UIStackView* row = [[UIStackView alloc] init];
        row.distribution = UIStackViewDistributionFillEqually;
        row.spacing = 4;
        for (NSUInteger i = 0; i < letters.length; ++i) {
            NSString* letter = [letters substringWithRange:NSMakeRange(i, 1)];
            [row addArrangedSubview:[self button:letter
                                        identifier:[NSString stringWithFormat:@"key/%ld/%lu", (long) rowIndex,
                                                                              (unsigned long) i]
                                            action:^{
                                                PNTextEditor* owner = weak;
                                                if (owner) {
                                                    [owner->_text appendString:letter];
                                                    [owner showText];
                                                }
                                            }]];
        }
        [_keys addArrangedSubview:row];
        ++rowIndex;
    }
    UIStackView* tools = [[UIStackView alloc] init];
    tools.distribution = UIStackViewDistributionFillEqually;
    tools.spacing = 4;
    [tools addArrangedSubview:[self button:@"Case"
                                  identifier:@"case"
                                      action:^{
                                          PNTextEditor* owner = weak;
                                          if (owner) {
                                              owner->_upper = !owner->_upper;
                                              [owner buildKeys];
                                          }
                                      }]];
    [tools addArrangedSubview:[self button:_symbols ? @"ABC" : @"Symbols"
                                  identifier:@"symbols"
                                      action:^{
                                          PNTextEditor* owner = weak;
                                          if (owner) {
                                              owner->_symbols = !owner->_symbols;
                                              [owner buildKeys];
                                          }
                                      }]];
    [tools addArrangedSubview:[self button:@"Space"
                                  identifier:@"space"
                                      action:^{
                                          PNTextEditor* owner = weak;
                                          if (owner) {
                                              [owner->_text appendString:@" "];
                                              [owner showText];
                                          }
                                      }]];
    [tools addArrangedSubview:[self button:@"Delete (Y)"
                                  identifier:@"delete"
                                      action:^{
                                          [weak erase];
                                      }]];
    [tools addArrangedSubview:[self button:@"Clear"
                                  identifier:@"clear"
                                      action:^{
                                          PNTextEditor* owner = weak;
                                          if (owner) {
                                              [owner->_text setString:@""];
                                              [owner showText];
                                          }
                                      }]];
    [tools addArrangedSubview:[self button:@"Cancel (B)"
                                  identifier:@"cancel"
                                      action:^{
                                          [weak dismissViewControllerAnimated:YES completion:nil];
                                      }]];
    [tools addArrangedSubview:[self button:@"Done"
                                  identifier:@"save"
                                      action:^{
                                          PNTextEditor* owner = weak;
                                          if (owner) {
                                              NSString* text = [owner->_text copy];
                                              void (^save)(NSString*) = owner->_save;
                                              [owner dismissViewControllerAnimated:YES
                                                                        completion:^{
                                                                            if (save)
                                                                                save(text);
                                                                        }];
                                          }
                                      }]];
    [_keys addArrangedSubview:tools];
}
- (NSArray<PRControllerTarget*>*)controllerTargets {
    return PRControllerDefaultTargets(self);
}
- (BOOL)controllerBack {
    [self dismissViewControllerAnimated:YES completion:nil];
    return YES;
}
@end
void PRControllerEditText(UIViewController* presenter, NSString* title, NSString* value, void (^save)(NSString*)) {
    [presenter.view endEditing:YES];
    [presenter presentViewController:[[PNTextEditor alloc] initWithTitle:title value:value save:save]
                            animated:YES
                          completion:nil];
}

@interface PNActionList : UITableViewController <PRControllerNavigable> {
    NSArray<NSString*>* _choices;
    void (^_choose)(NSInteger);
}
- (instancetype)initWithTitle:(NSString*)title choices:(NSArray<NSString*>*)choices choose:(void (^)(NSInteger))choose;
@end
@implementation PNActionList
- (instancetype)initWithTitle:(NSString*)title choices:(NSArray<NSString*>*)choices choose:(void (^)(NSInteger))choose {
    self = [super initWithStyle:UITableViewStyleInsetGrouped];
    if (self) {
        self.title = title;
        _choices = [choices copy];
        _choose = [choose copy];
        self.modalPresentationStyle = UIModalPresentationFullScreen;
    }
    return self;
}
- (NSInteger)tableView:(UITableView*)table numberOfRowsInSection:(NSInteger)section {
    return _choices.count + 1;
}
- (NSString*)tableView:(UITableView*)table titleForHeaderInSection:(NSInteger)section {
    return self.title;
}
- (UITableViewCell*)tableView:(UITableView*)table cellForRowAtIndexPath:(NSIndexPath*)path {
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    UIListContentConfiguration* content = cell.defaultContentConfiguration;
    content.text = path.row == 0 ? @"Cancel" : _choices[path.row - 1];
    cell.contentConfiguration = content;
    return cell;
}
- (void)tableView:(UITableView*)table didSelectRowAtIndexPath:(NSIndexPath*)path {
    void (^choose)(NSInteger) = _choose;
    [self dismissViewControllerAnimated:YES
                             completion:^{
                                 if (path.row && choose)
                                     choose(path.row - 1);
                             }];
}
- (NSArray<PRControllerTarget*>*)controllerTargets {
    return PRControllerDefaultTargets(self);
}
- (BOOL)controllerBack {
    [self dismissViewControllerAnimated:YES completion:nil];
    return YES;
}
@end
void PRControllerChoose(
    UIViewController* presenter, NSString* title, NSArray<NSString*>* choices, void (^choose)(NSInteger)
) {
    [presenter presentViewController:[[PNActionList alloc] initWithTitle:title choices:choices choose:choose]
                            animated:YES
                          completion:nil];
}

static void PNCollect(UIView* view, UIViewController* page, NSMutableArray<PRControllerTarget*>* targets) {
    if (view.hidden || view.alpha < .01 || !view.userInteractionEnabled)
        return;
    if ([view isKindOfClass:UITableView.class]) {
        UITableView* table = (UITableView*) view;
        __weak UITableView* weak = table;
        for (NSInteger section = 0; section < table.numberOfSections; ++section)
            for (NSInteger row = 0; row < [table numberOfRowsInSection:section]; ++row) {
                NSIndexPath* path = [NSIndexPath indexPathForRow:row inSection:section];
                UITableViewCell* cell = [table cellForRowAtIndexPath:path];
                if (cell && !cell.userInteractionEnabled)
                    continue;
                [targets
                    addObject:PRControllerRowTarget(
                                  table,
                                  path,
                                  [NSString stringWithFormat:@"%p/row/%ld/%ld", table, (long) section, (long) row],
                                  cell.textLabel.text ?: @"Row",
                                  ^{
                                      UITableView* owner = weak;
                                      id<UITableViewDelegate> delegate = owner.delegate;
                                      NSIndexPath* selected = path;
                                      if ([delegate respondsToSelector:@selector(tableView:willSelectRowAtIndexPath:)])
                                          selected = [delegate tableView:owner willSelectRowAtIndexPath:path];
                                      if (selected &&
                                          [delegate respondsToSelector:@selector(tableView:didSelectRowAtIndexPath:)])
                                          [delegate tableView:owner didSelectRowAtIndexPath:selected];
                                  }
                              )];
            }
        return;
    }
    if ([view isKindOfClass:UICollectionView.class]) {
        UICollectionView* collection = (UICollectionView*) view;
        __weak UICollectionView* weak = collection;
        for (NSInteger section = 0; section < collection.numberOfSections; ++section)
            for (NSInteger item = 0; item < [collection numberOfItemsInSection:section]; ++item) {
                NSIndexPath* path = [NSIndexPath indexPathForItem:item inSection:section];
                PRControllerTarget* target = PRControllerViewTarget(
                    collection,
                    [NSString stringWithFormat:@"%p/item/%ld/%ld", collection, (long) section, (long) item],
                    @"Item",
                    ^{
                        UICollectionView* owner = weak;
                        id<UICollectionViewDelegate> delegate = owner.delegate;
                        if ([delegate respondsToSelector:@selector(collectionView:shouldSelectItemAtIndexPath:)]
                            && ![delegate collectionView:owner shouldSelectItemAtIndexPath:path])
                            return;
                        [owner selectItemAtIndexPath:path
                                            animated:NO
                                      scrollPosition:UICollectionViewScrollPositionNone];
                        if ([delegate respondsToSelector:@selector(collectionView:didSelectItemAtIndexPath:)])
                            [delegate collectionView:owner didSelectItemAtIndexPath:path];
                    }
                );
                target.frame = ^CGRect(UIView* overlay) {
                    return [weak convertRect:[weak layoutAttributesForItemAtIndexPath:path].frame toView:overlay];
                };
                target.reveal = ^{
                    if (![weak.indexPathsForVisibleItems containsObject:path])
                        [weak scrollToItemAtIndexPath:path
                                     atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                                             animated:NO];
                    [weak layoutIfNeeded];
                };
                [targets addObject:target];
            }
        return;
    }
    if ([view isKindOfClass:UIControl.class]) {
        UIControl* control = (UIControl*) view;
        if (!control.enabled)
            return;
        __weak UIControl* weak = control;
        __weak UIViewController* owner = page;
        NSString* identifier = view.accessibilityIdentifier ?: [NSString stringWithFormat:@"%p", view];
        PRControllerTarget* target = PRControllerViewTarget(view, identifier, view.accessibilityLabel ?: @"Control", ^{
            UIControl* sender = weak;
            if ([sender isKindOfClass:UITextField.class]) {
                UITextField* field = (UITextField*) sender;
                PRControllerEditText(owner, field.placeholder ?: @"Text", field.text, ^(NSString* text) {
                    field.text = text;
                    [field sendActionsForControlEvents:UIControlEventEditingChanged];
                });
            } else if ([sender isKindOfClass:UISwitch.class]) {
                UISwitch* toggle = (UISwitch*) sender;
                toggle.on = !toggle.on;
                [toggle sendActionsForControlEvents:UIControlEventValueChanged];
            } else {
                UIControlEvents events = sender.allControlEvents;
                [sender sendActionsForControlEvents:(events & UIControlEventTouchUpInside)
                            ? UIControlEventTouchUpInside
                            : UIControlEventPrimaryActionTriggered];
            }
        });
        if ([control isKindOfClass:UISlider.class]) {
            target.adjust = ^(NSInteger direction) {
                UISlider* slider = (UISlider*) weak;
                slider.value = std::clamp(
                    slider.value + direction * (slider.maximumValue - slider.minimumValue) / 100.f,
                    slider.minimumValue,
                    slider.maximumValue
                );
                [slider sendActionsForControlEvents:UIControlEventValueChanged];
                [slider sendActionsForControlEvents:UIControlEventTouchUpInside];
            };
        }
        [targets addObject:target];
        return;
    }
    for (UIView* child in view.subviews)
        PNCollect(child, page, targets);
}
NSArray<PRControllerTarget*>* PRControllerDefaultTargets(UIViewController* page) {
    NSMutableArray<PRControllerTarget*>* targets = [NSMutableArray array];
    PNCollect(page.view, page, targets);
    for (UIBarButtonItem* item in
         [page.navigationItem.leftBarButtonItems ?: @[]
             arrayByAddingObjectsFromArray:page.navigationItem.rightBarButtonItems ?: @[]])
    {
        if (!item.enabled || !item.action)
            continue;
        __weak UIBarButtonItem* weak = item;
        PRControllerTarget* target = PRControllerViewTarget(page.navigationController.navigationBar ?: page.view, [NSString stringWithFormat:@"bar/%p",item], item.title ?: item.accessibilityLabel ?: @"Action", ^{ UIBarButtonItem* button = weak; [UIApplication.sharedApplication sendAction:button.action to:button.target from:button forEvent:nil]; });
        [targets addObject:target];
    }
    return targets;
}

@interface PNControllerNavigator : NSObject {
    NSTimer* _timer;
    UIView* _outline;
    UILabel* _hint;
    PN::ControllerRepeat _repeat;
    __weak id _context;
    NSMapTable<id, NSString*>* _selection;
    BOOL _engaged;
    UITapGestureRecognizer* _touch;
    __weak UIWindow* _window;
    PRControllerTarget* _focused;
}
- (void)poll;
- (void)handle:(uint32_t)keys controller:(UIViewController*)top;
@end
static PNControllerNavigator* PNNavigator;
@implementation PNControllerNavigator
- (instancetype)init {
    self = [super init];
    if (!self)
        return nil;
    _selection = [NSMapTable weakToStrongObjectsMapTable];
    for (NSNotificationName name in @[
             GCControllerDidConnectNotification, GCControllerDidDisconnectNotification,
             UIApplicationDidBecomeActiveNotification, UIApplicationWillResignActiveNotification
         ])
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(lifecycle:) name:name object:nil];
    [self sync];
    return self;
}
- (void)lifecycle:(NSNotification*)notification {
    // Controller notifications are not guaranteed to arrive on UIKit's queue.
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([notification.name isEqualToString:UIApplicationWillResignActiveNotification]) {
            [_timer invalidate];
            _timer = nil;
            [self hide];
        } else
            [self sync];
    });
}
- (void)sync {
    if (!PRControllerConnected() || UIApplication.sharedApplication.applicationState != UIApplicationStateActive) {
        [_timer invalidate];
        _timer = nil;
        [self hide];
        return;
    }
    if (_timer)
        return;
    __weak PNControllerNavigator* weak = self;
    _timer = [NSTimer timerWithTimeInterval:1. / 60
                                    repeats:YES
                                      block:^(NSTimer*) {
                                          [weak poll];
                                      }];
    [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
}
- (void)touched {
    _engaged = NO;
    PNUsingController = NO;
    _outline.hidden = YES;
    _hint.hidden = YES;
}
- (void)hide {
    _outline.hidden = YES;
    _hint.hidden = YES;
    _repeat.Reset();
    _context = nil;
    _focused = nil;
    PNUsingController = NO;
}
- (void)poll {
    UIViewController* top = PNTop();
    BOOL usable = [top conformsToProtocol:@protocol(PRControllerNavigable)] ||
        [top isKindOfClass:UIDocumentPickerViewController.class]
        || ([top isKindOfClass:UIAlertController.class] && objc_getAssociatedObject(top, &PNAlertActionsKey));
    if (!usable || UIApplication.sharedApplication.applicationState != UIApplicationStateActive
        || GCController.controllers.count == 0)
    {
        [self hide];
        return;
    }
    if (top.isBeingPresented || top.isBeingDismissed || top.transitionCoordinator) {
        _repeat.Reset();
        return;
    }
    uint32_t held = 0;
    float scrollY = 0;
    for (GCController* controller in GCController.controllers) {
        GCExtendedGamepad* pad = controller.extendedGamepad;
        if (!pad)
            continue;
        float x = pad.dpad.xAxis.value, y = pad.dpad.yAxis.value;
        if (std::abs(pad.leftThumbstick.xAxis.value) > .55f)
            x = pad.leftThumbstick.xAxis.value;
        if (std::abs(pad.leftThumbstick.yAxis.value) > .55f)
            y = pad.leftThumbstick.yAxis.value;
        if (std::abs(x) > std::abs(y)) {
            if (x < -.5f)
                held |= PN::PadLeft;
            if (x > .5f)
                held |= PN::PadRight;
        } else {
            if (y > .5f)
                held |= PN::PadUp;
            if (y < -.5f)
                held |= PN::PadDown;
        }
        if (pad.buttonA.pressed)
            held |= PN::PadAccept;
        if (pad.buttonB.pressed)
            held |= PN::PadBack;
        if (pad.buttonX.pressed)
            held |= PN::PadSearch;
        if (pad.buttonY.pressed)
            held |= PN::PadDelete;
        if (pad.leftShoulder.pressed)
            held |= PN::PadPrevious;
        if (pad.rightShoulder.pressed)
            held |= PN::PadNext;
        if (pad.buttonMenu.pressed)
            held |= PN::PadMenu;
        if (std::abs(pad.rightThumbstick.yAxis.value) > std::abs(scrollY))
            scrollY = pad.rightThumbstick.yAxis.value;
    }
    id context = [top respondsToSelector:@selector(controllerContext)]
        ? [(id<PRControllerNavigable>) top controllerContext]
        : top;
    if (_context != context) {
        _context = context;
        _repeat.Reset();
        _focused = nil;
    }
    const auto pressed = _repeat.Update(held, CACurrentMediaTime());
    BOOL capturing = [top respondsToSelector:@selector(controllerCapturing)] &&
        [(id<PRControllerNavigable>) top controllerCapturing];
    if (!capturing && std::abs(scrollY) > .3f) {
        UIViewController* active = [context isKindOfClass:UIViewController.class] ? context : top;
        UIScrollView* scroll = PNFindScroll(active.view);
        if (scroll) {
            CGFloat minimum = -scroll.adjustedContentInset.top;
            CGFloat maximum =
                MAX(minimum,
                    scroll.contentSize.height - scroll.bounds.size.height + scroll.adjustedContentInset.bottom);
            [scroll setContentOffset:CGPointMake(
                                         scroll.contentOffset.x,
                                         std::clamp(scroll.contentOffset.y - scrollY * 10, minimum, maximum)
                                     )
                            animated:NO];
        }
    }
    if (pressed) {
        _engaged = YES;
        PNUsingController = YES;
        [self handle:pressed controller:top];
        if (pressed & (PN::PadAccept | PN::PadBack | PN::PadMenu | PN::PadPrevious | PN::PadNext | PN::PadSearch)) {
            _focused = nil;
            _outline.hidden = YES;
            _hint.hidden = YES;
            return;
        }
    }
    // Refresh the outline after asynchronous page/thumbnail reloads and scrolling.
    if (_engaged)
        [self handle:0 controller:top];
}
- (NSArray<PRControllerTarget*>*)targets:(UIViewController*)top {
    if ([top conformsToProtocol:@protocol(PRControllerNavigable)])
        return [(id<PRControllerNavigable>) top controllerTargets];
    if ([top isKindOfClass:UIAlertController.class]) {
        UIAlertController* alert = (UIAlertController*) top;
        NSMutableArray* targets = [NSMutableArray array];
        for (UITextField* field in alert.textFields) {
            __weak UITextField* weak = field;
            [targets
                addObject:PRControllerViewTarget(
                              field, [NSString stringWithFormat:@"field/%p", field], field.placeholder ?: @"Text", ^{
                                  PRControllerEditText(alert, alert.title ?: @"Text", weak.text, ^(NSString* text) {
                                      weak.text = text;
                                  });
                              }
                          )];
        }
        for (PNAlertAction* entry in objc_getAssociatedObject(alert, &PNAlertActionsKey)) {
            if (!entry.action.enabled)
                continue;
            UIView* label = PNFindLabel(alert.view, entry.action.title) ?: alert.view;
            __weak UIAlertController* weak = alert;
            [targets addObject:PRControllerViewTarget(label, entry.action.title, entry.action.title, ^{
                         [weak dismissViewControllerAnimated:YES completion:entry.invoke];
                     })];
        }
        return targets;
    }
    // Files uses public table/collection selection and control actions. Provider
    // authentication remains owned by iOS, outside the app's navigation layer.
    return PRControllerDefaultTargets(top);
}
- (void)back:(UIViewController*)top {
    if ([top respondsToSelector:@selector(controllerBack)] && [(id<PRControllerNavigable>) top controllerBack])
        return;
    if ([top isKindOfClass:UIAlertController.class]) {
        for (PNAlertAction* entry in objc_getAssociatedObject(top, &PNAlertActionsKey))
            if (entry.action.style == UIAlertActionStyleCancel) {
                [top dismissViewControllerAnimated:YES completion:entry.invoke];
                return;
            }
        return; // Never confirm a destructive or mandatory action with B.
    }
    if ([top isKindOfClass:UIDocumentPickerViewController.class]) {
        UIDocumentPickerViewController* picker = (UIDocumentPickerViewController*) top;
        if ([picker.delegate respondsToSelector:@selector(documentPickerWasCancelled:)])
            [picker.delegate documentPickerWasCancelled:picker];
        [picker dismissViewControllerAnimated:YES completion:nil];
    }
}
- (void)handle:(uint32_t)keys controller:(UIViewController*)top {
    id<PRControllerNavigable> page = (id) top;
    if ([page respondsToSelector:@selector(controllerCapturing)] && [page controllerCapturing]) {
        _outline.hidden = YES;
        _hint.hidden = YES;
        _focused = nil;
        return; // Every input belongs to binding capture, including Start/Menu.
    }
    if (keys & (PN::PadMenu | PN::PadBack)) {
        [self back:top];
        return;
    }
    if ((keys & (PN::PadPrevious | PN::PadNext)) && [page respondsToSelector:@selector(controllerPanel:)]) {
        [page controllerPanel:(keys & PN::PadPrevious) ? -1 : 1];
        _repeat.Reset();
        return;
    }
    if ((keys & PN::PadSearch) && [page respondsToSelector:@selector(controllerSearch)]) {
        [page controllerSearch];
        return;
    }
    if ((keys & PN::PadDelete) && [top isKindOfClass:PNTextEditor.class])
        [(PNTextEditor*) top erase];
    if (!keys && _focused) {
        [self draw:_focused controller:top];
        return;
    }
    NSArray<PRControllerTarget*>* targets = [self targets:top];
    if (!targets.count) {
        _outline.hidden = YES;
        _hint.hidden = YES;
        return;
    }
    id context = _context ?: top;
    NSString* identifier = [_selection objectForKey:context];
    NSUInteger index = [targets indexOfObjectPassingTest:^BOOL(PRControllerTarget* target, NSUInteger, BOOL*) {
        return [target.identifier isEqualToString:identifier];
    }];
    if (index == NSNotFound)
        index = 0;
    PRControllerTarget* target = targets[index];
    if (keys & (PN::PadLeft | PN::PadRight)) {
        NSInteger direction = (keys & PN::PadLeft) ? -1 : 1;
        if (target.adjust) {
            target.adjust(direction);
            return;
        }
        if ([page respondsToSelector:@selector(controllerPanel:)]) {
            [page controllerPanel:direction];
            _repeat.Reset();
            return;
        }
    }
    uint32_t movement = keys & (PN::PadUp | PN::PadDown | PN::PadLeft | PN::PadRight);
    if (movement && (movement & (PN::PadUp | PN::PadDown)) && [page respondsToSelector:@selector(controllerPanel:)]) {
        // Vertical traversal includes both sliders in a paired row. Left/Right
        // remains available for adjustment; L/R always changes pane.
        NSInteger next = std::clamp(
            NSInteger(index) + ((movement & PN::PadUp) ? -1 : 1), NSInteger(0), NSInteger(targets.count) - 1
        );
        index = NSUInteger(next);
        target = targets[index];
    } else if (movement) {
        // Geometry handles paired controls and the on-screen keyboard. Table
        // targets include offscreen rows, so long lists remain fully reachable.
        UIView* overlay = top.view.window;
        CGRect current = target.frame(overlay);
        CGPoint center = CGPointMake(CGRectGetMidX(current), CGRectGetMidY(current));
        double best = std::numeric_limits<double>::infinity();
        NSUInteger next = index;
        BOOL horizontal = movement & (PN::PadLeft | PN::PadRight);
        NSInteger sign = movement & (PN::PadUp | PN::PadLeft) ? -1 : 1;
        for (NSUInteger i = 0; i < targets.count; ++i) {
            if (i == index)
                continue;
            CGRect candidate = targets[i].frame(overlay);
            double dx = CGRectGetMidX(candidate) - center.x, dy = CGRectGetMidY(candidate) - center.y;
            double along = (horizontal ? dx : dy) * sign, across = std::abs(horizontal ? dy : dx);
            if (along <= 1)
                continue;
            double score = along + across * 3;
            if (score < best) {
                best = score;
                next = i;
            }
        }
        index = next;
        target = targets[index];
    }
    [_selection setObject:target.identifier forKey:context];
    _focused = target;
    if (keys & PN::PadAccept) {
        if (target.activate)
            target.activate();
        _repeat.Reset();
        return;
    }
    if (movement && target.reveal)
        target.reveal();
    [self draw:target controller:top];
}
- (void)draw:(PRControllerTarget*)target controller:(UIViewController*)top {
    UIWindow* window = top.view.window;
    if (!window)
        return;
    if (_window != window) {
        if (_touch)
            [_window removeGestureRecognizer:_touch];
        _touch = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(touched)];
        _touch.cancelsTouchesInView = NO;
        _touch.delaysTouchesBegan = NO;
        _touch.delaysTouchesEnded = NO;
        [window addGestureRecognizer:_touch];
        _window = window;
    }
    if (!_outline) {
        _outline = [[UIView alloc] init];
        _outline.userInteractionEnabled = NO;
        _outline.layer.cornerRadius = 10;
        _outline.layer.borderWidth = 3;
        _outline.accessibilityIdentifier = @"controller-focus";
        _hint = [[UILabel alloc] init];
        _hint.userInteractionEnabled = NO;
        _hint.font = [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold];
        _hint.textAlignment = NSTextAlignmentCenter;
        _hint.backgroundColor = UIColor.secondarySystemBackgroundColor;
        _hint.layer.cornerRadius = 6;
        _hint.clipsToBounds = YES;
        _hint.accessibilityIdentifier = @"controller-hint";
    }
    CGRect frame = CGRectInset(target.frame(window), 3, 3);
    _outline.layer.borderColor = top.view.tintColor.CGColor;
    CGRect clipped = CGRectIntersection(frame, target.viewport ? target.viewport(window) : window.bounds);
    clipped = CGRectIntersection(clipped, [top.view convertRect:top.view.bounds toView:window]);
    _outline.hidden = CGRectIsEmpty(clipped) || CGRectIsNull(clipped);
    _outline.frame = _outline.hidden ? CGRectZero : clipped;
    NSString* controls = [top respondsToSelector:@selector(controllerPanel:)]
        ? @"A Select · B Back · L/R Panels · X Search"
        : [top isKindOfClass:PNTextEditor.class] ? @"A Type · B Cancel · Y Delete"
                                                 : @"A Select · B Back";
    _hint.text = [NSString stringWithFormat:@"%@  ·  %@", target.label ?: @"", controls];
    _hint.frame = CGRectMake(
        CGRectGetMinX(window.safeAreaLayoutGuide.layoutFrame) + 8,
        CGRectGetMaxY(window.safeAreaLayoutGuide.layoutFrame) - 19,
        CGRectGetWidth(window.safeAreaLayoutGuide.layoutFrame) - 16,
        18
    );
    _hint.hidden = NO;
    [window addSubview:_outline];
    [window addSubview:_hint];
}
@end
namespace Paperboat {
void IosStartControllerNavigation() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!PNNavigator)
            PNNavigator = [[PNControllerNavigator alloc] init];
    });
}
}
#endif
