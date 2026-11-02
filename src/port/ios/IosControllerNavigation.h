#pragma once
#if defined(__IOS__)
namespace Paperboat {
void IosStartControllerNavigation();
}
#ifdef __OBJC__
#import <UIKit/UIKit.h>

// Explicit, public targets keep gamepad input independent of UIKit's private
// view classes and of the game's remappable N64 bindings.
@interface PRControllerTarget : NSObject
@property (nonatomic, copy) NSString* identifier;
@property (nonatomic, copy) NSString* label;
@property (nonatomic, copy) CGRect (^frame)(UIView* overlay);
@property (nonatomic, copy) CGRect (^viewport)(UIView* overlay);
@property (nonatomic, copy) void (^reveal)(void);
@property (nonatomic, copy) void (^activate)(void);
@property (nonatomic, copy) void (^adjust)(NSInteger direction);
@end

@protocol PRControllerNavigable <NSObject>
- (NSArray<PRControllerTarget*>*)controllerTargets;
@optional
- (BOOL)controllerBack;
- (void)controllerPanel:(NSInteger)direction;
- (void)controllerSearch;
- (BOOL)controllerCapturing;
- (id)controllerContext;
@end

PRControllerTarget* PRControllerViewTarget(UIView* view, NSString* identifier, NSString* label, void (^activate)(void));
PRControllerTarget* PRControllerRowTarget(
    UITableView* table, NSIndexPath* path, NSString* identifier, NSString* label, void (^activate)(void)
);
NSArray<PRControllerTarget*>* PRControllerDefaultTargets(UIViewController* page);
BOOL PRControllerInUse();
BOOL PRControllerConnected();
void PRControllerEditText(UIViewController* presenter, NSString* title, NSString* value, void (^save)(NSString*));
void PRControllerChoose(
    UIViewController* presenter, NSString* title, NSArray<NSString*>* choices, void (^choose)(NSInteger)
);
void PRControllerAddAlertAction(
    UIAlertController* alert, NSString* title, UIAlertActionStyle style, void (^handler)(UIAlertAction*)
);
#endif
#endif
