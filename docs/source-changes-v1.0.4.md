# PaperReality 1.0.4 source audit

This patch release collects changes since `v1.0.3`
(`48b25a7b7e9c20b263f57fb30c3884a031eb01be`). The application changes were
included in Build 117 at `4ad04dd8c`; subsequent work only improves the isolated
simulator test infrastructure, updates the release version, and adds release
documentation and packaging validation.

## Application changes

- `CMakeLists.txt` targets device families `1,2`, so the same app supports native
  iPhone and iPad windows. It also sets the project version to 1.0.4. The app
  name, bundle ID, minimum OS version, orientation policy, and existing
  `UIRequiresFullScreen` setting are retained.
- `src/port/ios/IosControllerNavigation.mm` removes the bottom help strip and
  resolves native alert action bounds through public accessibility and
  hit-testing APIs. Hidden labels are ignored; matching headings cannot take
  focus from action captions. The controller still activates the registered
  action callback and skips disabled actions.
- Gameplay and renderer sources are unchanged from 1.0.3.
- libultraship stays at `4f2fd5398871b92682dad87a5f0b416b9178b18f`;
  Torch stays at `72960ca16f4723f96aaf4037b76072c230af8c11`.

## Validation and build infrastructure

- `tests/uikit/Host.mm` checks full native dialog button focus for horizontal,
  stacked, disabled, and repeated heading/action text cases. The UIKit runtime
  check passed for both normal and development variants before Build 107.
- `tests/ipad-windowing/` reproduces the production SDL window setup with
  generated colors, without loading the game or texture packs. The focused
  check passed on an iPad Pro 13-inch (M4) simulator running iPadOS 26.2. It
  reported native iPad interface mode, matching SDL/window dimensions, and a
  full 1376 × 1032 canvas with Stage Manager selected. No system fullscreen
  control was exposed to automation, so that interaction was not checked.
- The reporting user confirmed Build 117 resolved the iPad fullscreen issue.
  Their exact hardware and the reported iPadOS version were not independently
  verified; the simulator result applies to iPadOS 26.2.
- The IPA workflow verifies the packaged archive and metadata. Simulator-only
  requests skip IPA compilation, preserve active IPA builds, and clean up
  completed simulator runs after a successful check. Desktop builds remain
  paused. Simulator test bundles and captures are not uploaded.
- This release uses the existing runtime test results and runs the regular
  release compilation, UIKit SDK checks, and timing/controller regression
  checks. It does not request another simulator run.

Only public application source and port resources are used in CI and the IPA.
User game files and HD texture packs are not included.
