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

The second configuration removes UIRequiresFullScreen, allows portrait on iPad,
and sets SDL_WINDOW_RESIZABLE, to compare modern windowing eligibility. These
changes affect only this temporary reproduction, never the application plist or
backend. Both configurations use the same selected simulator SDK; differences
from the release's older linked SDK must also be considered.

The workflow removes its local build/results and uses the existing simulator-run
cleanup. It never uploads images, app bundles, test results, or logs as artifacts.
