# iPad windowing reproduction

An explicitly requested simulator diagnostic, separate from the game and UIKit
settings smoke tests. It builds SDL 2.32.10 with generated colors, the production
window-placement helper, and the production iOS plist. No ROMs, game code, saves,
texture packs, screenshots, or downloadable artifacts are used.

`run.py` selects an available iPadOS 26.2 runtime, then uses public XCTest APIs to
look for Stage Manager / Windowed Apps and the native fullscreen control. It
reports native window and SDL sizes before and after activating that control.
A missing system control or unavailable windowing mode is a simulator limitation,
not proof that fullscreen works. The reported user's 27.2 version cannot be
verified on a 26.2 runtime.

The actual published v1.0.0 and v1.0.3 IPAs both declare UIDeviceFamily=[1]
(iPhone only), UIRequiresFullScreen=true, and landscape orientations only. The
probe compares the old and current window-placement paths with that iPhone-only
metadata, then changes only the device family to [1,2] for a native iPad
comparison. Both placement paths enable SDL fullscreen after creating the
renderer, matching the release's iOS gameMode initialization. Only the helper
and initialization sequence are reproduced; this is not the complete v1.0.0 app.

These changes affect only this temporary reproduction, never the application
plist or backend. All configurations use the same selected simulator SDK;
differences from the release's older linked SDK must also be considered.

The workflow removes its local build/results and uses the existing simulator-run
cleanup. It never uploads images, app bundles, test results, or logs as artifacts.
