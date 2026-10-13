#include "IosHaptics.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>

namespace Paperboat {

void HapticTap(int strength) {
    static UIImpactFeedbackGenerator* sGenerators[3] = { nil, nil, nil };
    const int index = strength < 0 ? 0 : (strength > 2 ? 2 : strength);
    // UIKit feedback generators are main-thread objects; the pad is polled on the game thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        if (sGenerators[index] == nil) {
            const UIImpactFeedbackStyle styles[3] = { UIImpactFeedbackStyleLight, UIImpactFeedbackStyleMedium,
                                                      UIImpactFeedbackStyleHeavy };
            sGenerators[index] = [[UIImpactFeedbackGenerator alloc] initWithStyle:styles[index]];
        }
        [sGenerators[index] impactOccurred];
    });
}

} // namespace Paperboat

#endif
