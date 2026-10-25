# PaperReality 1.0.3 source audit

Audited on 2026-10-25. Application source: `33199eb0f`, the merge used for
Actions build 99. Release preparation adds documentation, explicitly sets the
iOS app's display name to PaperReality, and prevents superseded builds from
publishing an IPA after a release tag is corrected. Gameplay and the renderer
are unchanged from the audited application source.

## Baselines

- Previous PaperReality release: `v1.0.0`, commit `2ac2975668fc7d5325f8aee5ff376ea4dc1b27b7`.
- Original PaperBoat base for that release: `7f038b85`.
- Upstream included in 1.0.3: HarbourMasters/PaperBoat `develop` at `781d164cf`.
- Renderer/controller library: acoston822/libultraship at
  `4f2fd5398871b92682dad87a5f0b416b9178b18f`, branch
  `experimental/uikit-frame-pacing`.
- Torch stays pinned at `72960ca16f4723f96aaf4037b76072c230af8c11`.

At the audited application commit, the difference from the previous PaperReality
release is 84 files, 12,359 insertions, and 172 deletions. This includes imported
upstream changes, tests, documentation, workflows, and the new branding image;
it is not a count of gameplay-code changes alone. Against the included upstream
head, the fork differs in 80 files, with 12,888 insertions and 171 deletions.
Submodule internals are accounted for separately below. Release documentation
itself is additional to those snapshot counts.

## iOS interface and settings

`IosNativeSettings.mm` supplies the UIKit root, expandable categories, native
setting pages and choices, slider cards, console transcript, event inspector,
color editor, sidebar, and presentation lifecycle. Light/dark appearance,
Dynamic Type, narrow-screen navigation, and the existing menu theme are
supported. Opaque canvases, clipped panels, pixel-aligned edges, sidebar
rasterization, zero empty section chrome, and isolated navigation pages address
the reported seams and overlapping content. Descriptions stay inline.

`NativeSettings.*` owns snapshots and the render-frame action queue.
`NativeMenu.cpp` maps the shared upstream widget definitions, values, disabled
reasons, callbacks, and settings lock to native rows. `NativePanels.cpp` maps
Input Viewer and shader controls. The shared definitions remain available for
future upstream settings, while iOS always opens UIKit and ignores the former
interface-selection preference. First-run prompts and existing in-game overlays
retain their existing rendering paths.

`ResolutionEditor.cpp` exposes custom aspect ratio, fixed vertical resolution,
Pixel Count Presets, manual vertical pixels, and integer/pixel-perfect controls.
Preset and manual edits share the existing saved CVars. Desktop-only window,
renderer, and ImGui appearance controls are omitted from the UIKit mapping.
Opening a setting's page does not reset saved values. Initial iOS setup closes
the persisted menu window, preserving the selected page and other preferences.

UIKit runs on the main queue; engine edits and asset decoding return through
the game-frame queue. Snapshots own their strings and callbacks. Game input is
blocked while the native settings are open. Presentation uses the phone's window,
with bounded retries and input cleanup if presentation cannot complete.

## Controller and touch input

`IosControllerNavigation.*` supplies focus, touch/controller handover, repeat and
held-button handling, pane changes, right-stick scrolling, app-dialog targets,
text entry, and controller support for native colors and actions. It reads
GameController state without replacing SDL handlers. Polling stops when the app
is inactive or no controller is connected. App-owned import/export buttons are
targets; the system Files picker is the touch-only exception.

`NativeControllerBindings.cpp` edits ports, assigned devices, buttons, D-pad,
both sticks, response/dead-zone/notch parameters, rumble, gyro, and LEDs. Capture
waits for neutral input, retains the old mapping until success, and supports
cancel/timeout. Native navigation yields while bindings are being captured.
Gyro configuration reads supported controller sensors but adds no motion-controlled
gameplay or phone-gyro mapping.

`TouchControls.cpp` retains floating joystick behavior and saved custom positions.
The shipped default uses mirrored shoulders, equal horizontal/vertical C-button
spacing, a revised stick resting position, smooth vector discs, and a hidden
D-pad by default. Controller detection ignores non-gamepad joystick devices.
Layout editing accepts controller movement/reset/finish and withholds editing
and held completion inputs from gameplay. The haptic feedback, idle-stick hiding,
and hide-with-controller options from the first release remain available.

## Editor artwork and model portraits

Save and Blueprint editors build native pages while retaining the shared game
data and numeric Blueprint import/export format. Save editing guards against an
unloaded save. The Max HP/FP controls also update their hard maximums so changes
persist across area transitions. Eight unobtainable badges and two battle items are grouped as Unused;
the classification does not assert the creator's intent or remove their code.

`NativePortrait.h`, the Blueprint composition code, and the shared image loader
retain mixed-resolution component positions, alpha, and high-resolution pixels.
Replacement lookup checks palette-specific names, generic replacements, and
resource aliases; enabled pack priority is respected and original art remains
the fallback. Cache invalidation follows alternate-asset/archive changes.

`NativeModelPortrait.*` renders static orthographic thumbnails of imported Fake
Bowser, Light Bulb, and Toy Tank geometry. It selects the intended model subtree,
handles vertex colors and texture alpha, supports relevant vertex modifications,
and resolves original or HD materials using the same selection path. Previews
are 256 pixels, with at most three cached CPU images (768 KiB); temporary material
copies are bounded to 512 pixels per side. This does not modify game animations
or source texture-pack data.

The Sprite Texture Report audits installed UI/icon raster names, Blueprint
components, and model materials. It exports selection metadata and dimensions,
not image bytes or save contents. Missing resources stay blank. No user ROM,
game archive, save, or HD pack is committed or bundled for editor portraits.

## AirPlay, presentation, and timing

`IosExternalDisplay.*` observes and polls external-screen changes, manages the
game-only Metal view, and refreshes drawable size during layout/mode changes.
The backend renders the final game image separately from the phone controls and
menus. Saved physical-pixel window dimensions are replaced by primary-display
logical bounds at iOS window creation, avoiding startup on an already-connected
TV. The viewport offset is applied only when the framebuffer matches the window.

`IosDisplayTiming.h` and `GameEngine::GetInterpolationFPS()` cap the effective
game target to a connected display's reported refresh rate, with a 30 Hz floor,
while preserving a lower selected target. Disconnect restores the ordinary
phone target; the saved preference is not overwritten.

The backend's `FramePacer.h` uses steady epoch/index deadlines, waits through
the complete iOS deadline, and resets after stalls, clock discontinuities,
target changes, and pacing-mode changes. Metal presentation spacing follows
the selected game target. The optional iOS wait callback services UIKit's
default/tracking run loops during existing waits, without extra game frames or
engine reentry. UIKit's visible display-link request is independent of the game
FPS preference, subject to iOS adaptive refresh and thermal/power policy.

`NativeFrameTiming.h` and `NativeRenderTiming.h` support opt-in development
measurements. The normal Release configuration compiles the Settings timing HUD
out. Development variants are visibly labeled and blocked from release
publication. Temporary AirPlay and first-run tracing was removed; the existing
bounded battle-start log entries remain. The Jr. Troopa report was not reproduced
and has no confirmed root cause. MetalFX was discussed but is not implemented.

## Files, startup, and platform fixes

The first-run picker locates the phone's main window even before it is key and
retries presentation. Extraction/import prompts temporarily bypass Metal present
pacing and restore it afterward. A loading message precedes blocking game setup.
Controller navigation is enabled for first-run prompts without overwriting an
existing preference. The logger flushes once per second.

`FilePicker.cpp` routes iOS imports/exports to `IosFilePicker.*`; requests are
serialized, imports return private temporary copies to game-thread callbacks,
exports stage files for the Files picker, and cancellation/dismissal cleans up
state and temporary data. Open App Files Folder uses Files rather than a desktop
file URL. The Mod Menu adds file import and metadata report export with native
restart/confirmation dialogs.

First-release fixes still present include the missing display-list terminator in
`D_8014EA48`, integer casts for darkness-stencil texture rectangles, alternate-only
named-image detection, iOS Game Mode and Files keys, and ARM64/ARC/framework
configuration. The app keeps its PaperReality bundle identifier and red boat
icon; the header uses a separate transparent cutout. The release explicitly sets
CFBundleDisplayName and CFBundleName to PaperReality while retaining the internal
Paperboat executable name and the existing bundle identifier.

## Upstream and validation

The seven upstream commits listed below are included in git history rather than
reimplemented as unrelated local copies. The newest two switches are native
Enhancements → Gameplay rows with accurate descriptions. Integration checks fire
their actual event listeners from UI callbacks, verify defaults and saved values,
and ensure the settings lock rejects stale callbacks.

The audited source passed the full Linux build and 1,021-page/6,149-row native
integration check. Build 99 also completed the actual iOS Release build and IPA
packaging, with both UIKit diagnostic configurations syntax-checked first.
Synthetic fixtures cover portraits, model materials, controller capture and
repeat, frame deadlines, external display selection/rates, file routing, and
resolution persistence. Simulator checks are reserved for requested debugging or
renders, with cleanup of their runs/artifacts. Desktop build/release workflows
stay paused. A fresh tagged IPA is built for this release.

These checks do not establish physical-controller compatibility on every device,
actual GPU-presented settings FPS, or final AirPlay refresh-switch performance.
Device feedback confirmed improved scrolling, touch-control operation, and the
reported sidebar-seam fix.

## Reproducing and reviewing the source

```sh
git clone --recursive --branch v1.0.3 https://github.com/acoston822/PaperReality.git
cd PaperReality
git submodule update --init --recursive
```

The tag identifies the release source; the gitlinks identify its library and
Torch revisions. GitHub's automatic source archives do not embed submodule
contents. See `docs/android-ios-web.md` for local Apple-toolchain instructions.
The release IPA uses `IOS_SIGNING=OFF`, `PAPERREALITY_UI_DIAGNOSTICS=OFF`, and
`cmake --build ... --config Release`; its bundle version is the Actions run number
and its displayed release version is 1.0.3.

Complete release comparison:
https://github.com/acoston822/PaperReality/compare/v1.0.0...v1.0.3

The following inventories are generated from the audited git revisions. They
include workflows, documentation, tests, and app branding as well as source, and
do not imply that every historic diagnostic experiment remains enabled.

## Imported upstream commits since the original PaperReality base

| Commit | Upstream change |
| --- | --- |
| [`6efc9f151`](https://github.com/HarbourMasters/PaperBoat/commit/6efc9f151) | Fixed builds for other platforms (#262) |
| [`445663842`](https://github.com/HarbourMasters/PaperBoat/commit/445663842) | fix popup text offset reading uninit msgID (clang-cl) (#263) |
| [`f63142ad8`](https://github.com/HarbourMasters/PaperBoat/commit/f63142ad8) | Added enhancement to show hidden blocks based on multiple factors (#264) |
| [`d98b4b630`](https://github.com/HarbourMasters/PaperBoat/commit/d98b4b630) | Restore web build and GitHub Pages deploy; fix wasm link/runtime traps, size pthread pool per device, add title and disclaimer to web shell (#266) |
| [`befe89c37`](https://github.com/HarbourMasters/PaperBoat/commit/befe89c37) | fix UAF crash in GfxSpVertex of Munchlesia by freeing animator DL/vertex copies once per drawn frame instead of per animator update (#269) |
| [`abac004a3`](https://github.com/HarbourMasters/PaperBoat/commit/abac004a3) | Add Enhancement to Mute ISpy notification sound (#270) |
| [`781d164cf`](https://github.com/HarbourMasters/PaperBoat/commit/781d164cf) | Add enhancement for Chuck quizmo to always appear while you have the proper story progress. (#272) |

## Changed files since PaperReality v1.0.0

84 tracked paths. A = added; M = modified.

| Status | Path |
| --- | --- |
| A | `.github/uikit-debug-request.txt` |
| M | `.github/workflows/build.yml` |
| M | `.github/workflows/ios-unsigned.yml` |
| A | `.github/workflows/pages.yml` |
| A | `.github/workflows/uikit-smoke.yml` |
| M | `CMakeLists.txt` |
| M | `README.md` |
| M | `android/app/build.gradle.kts` |
| M | `docs/CHANGES.md` |
| M | `docs/UPSTREAM.md` |
| A | `docs/debug-jr-troopa.md` |
| A | `docs/experimental-uikit-settings.md` |
| M | `external/libultraship` |
| M | `include/functions.h` |
| M | `include/model.h` |
| M | `include/ultra64.h` |
| A | `ios/PaperReality-Boat.png` |
| M | `src/animator.c` |
| M | `src/battle/menu_moves.c` |
| M | `src/battle/states/start_battle.c` |
| M | `src/collision.c` |
| M | `src/entity/ItemBlock.c` |
| M | `src/i_spy.c` |
| M | `src/main_loop.c` |
| M | `src/port/Engine.cpp` |
| M | `src/port/FilePicker.cpp` |
| M | `src/port/enhancements/PortEnhancements.cpp` |
| A | `src/port/enhancements/gameplay/AlwaysChuckQuizmo.cpp` |
| A | `src/port/enhancements/gameplay/MuteISpySound.cpp` |
| A | `src/port/enhancements/gameplay/ShowHiddenBlocks.cpp` |
| M | `src/port/hooks/list/GameEvent.h` |
| A | `src/port/ios/IosControllerNavigation.h` |
| A | `src/port/ios/IosControllerNavigation.mm` |
| A | `src/port/ios/IosDisplayTiming.h` |
| A | `src/port/ios/IosExternalDisplay.h` |
| A | `src/port/ios/IosExternalDisplay.mm` |
| M | `src/port/ios/IosFilePicker.h` |
| M | `src/port/ios/IosFilePicker.mm` |
| A | `src/port/ios/IosNativeSettings.mm` |
| M | `src/port/nusys_overrides.c` |
| M | `src/port/ui/ActorBlueprintEditor.cpp` |
| A | `src/port/ui/NativeControllerBindings.cpp` |
| A | `src/port/ui/NativeControllerNavigation.h` |
| A | `src/port/ui/NativeEventSnapshot.h` |
| A | `src/port/ui/NativeFrameTiming.h` |
| A | `src/port/ui/NativeMenu.cpp` |
| A | `src/port/ui/NativeModelPortrait.cpp` |
| A | `src/port/ui/NativeModelPortrait.h` |
| A | `src/port/ui/NativePanels.cpp` |
| A | `src/port/ui/NativePortrait.h` |
| A | `src/port/ui/NativeRenderTiming.h` |
| A | `src/port/ui/NativeSettings.cpp` |
| A | `src/port/ui/NativeSettings.h` |
| A | `src/port/ui/NativeTheme.h` |
| M | `src/port/ui/PaperboatGui.cpp` |
| M | `src/port/ui/PaperboatMenu.cpp` |
| M | `src/port/ui/PaperboatMenu.h` |
| M | `src/port/ui/PaperboatMenuEnhancements.cpp` |
| M | `src/port/ui/PaperboatMenuSettings.cpp` |
| M | `src/port/ui/PaperboatModMenuWindow.cpp` |
| M | `src/port/ui/PaperboatModals.cpp` |
| M | `src/port/ui/ResolutionEditor.cpp` |
| M | `src/port/ui/TouchControls.cpp` |
| M | `src/port/ui/devtools/saveeditor/SaveEditor.cpp` |
| M | `src/port/ui/devtools/valueviewer/ValueViewer.cpp` |
| M | `src/port/web/shell.html` |
| M | `src/state_battle.c` |
| M | `src/world/area_mac/mac_04/rooms.c` |
| M | `src/world/common/complete/Quizmo.inc.c` |
| M | `src/world/partner/bombette.c` |
| A | `tests/native-settings/check.cpp` |
| A | `tests/native-settings/controller-navigation.cpp` |
| A | `tests/native-settings/display-timing.cpp` |
| A | `tests/native-settings/events.cpp` |
| A | `tests/native-settings/frame-pacer.cpp` |
| A | `tests/native-settings/frame-timing.cpp` |
| A | `tests/native-settings/ios-window-placement.cpp` |
| A | `tests/native-settings/ios-window-placement.py` |
| A | `tests/native-settings/model-portraits.cpp` |
| A | `tests/native-settings/portraits.cpp` |
| A | `tests/native-settings/run.py` |
| A | `tests/native-settings/textures.py` |
| A | `tests/uikit/Host.mm` |
| A | `tests/uikit/run.py` |

## Current fork differences from included upstream

80 tracked paths. A = added; M = modified.

| Status | Path |
| --- | --- |
| A | `.github/uikit-debug-request.txt` |
| A | `.github/workflows/ios-unsigned.yml` |
| A | `.github/workflows/uikit-smoke.yml` |
| M | `.gitmodules` |
| M | `CMakeLists.txt` |
| M | `README.md` |
| A | `docs/CHANGES.md` |
| A | `docs/UPSTREAM.md` |
| A | `docs/debug-jr-troopa.md` |
| A | `docs/experimental-uikit-settings.md` |
| A | `docs/release-notes/v1.0.0.md` |
| M | `external/libultraship` |
| M | `ios/Icon.png` |
| A | `ios/PaperReality-Boat.png` |
| A | `ios/PaperReality-Icon.png` |
| M | `ios/plist.in` |
| M | `src/battle/states/start_battle.c` |
| M | `src/port/Engine.cpp` |
| M | `src/port/Engine.h` |
| M | `src/port/FilePicker.cpp` |
| A | `src/port/ios/IosControllerNavigation.h` |
| A | `src/port/ios/IosControllerNavigation.mm` |
| A | `src/port/ios/IosDisplayTiming.h` |
| A | `src/port/ios/IosExternalDisplay.h` |
| A | `src/port/ios/IosExternalDisplay.mm` |
| A | `src/port/ios/IosFilePicker.h` |
| A | `src/port/ios/IosFilePicker.mm` |
| A | `src/port/ios/IosHaptics.h` |
| A | `src/port/ios/IosHaptics.mm` |
| A | `src/port/ios/IosNativeSettings.mm` |
| M | `src/port/os/Cont.cpp` |
| M | `src/port/patches/MapTexturePatches.cpp` |
| M | `src/port/ui/ActorBlueprintEditor.cpp` |
| M | `src/port/ui/Menu.cpp` |
| A | `src/port/ui/NativeControllerBindings.cpp` |
| A | `src/port/ui/NativeControllerNavigation.h` |
| A | `src/port/ui/NativeEventSnapshot.h` |
| A | `src/port/ui/NativeFrameTiming.h` |
| A | `src/port/ui/NativeMenu.cpp` |
| A | `src/port/ui/NativeModelPortrait.cpp` |
| A | `src/port/ui/NativeModelPortrait.h` |
| A | `src/port/ui/NativePanels.cpp` |
| A | `src/port/ui/NativePortrait.h` |
| A | `src/port/ui/NativeRenderTiming.h` |
| A | `src/port/ui/NativeSettings.cpp` |
| A | `src/port/ui/NativeSettings.h` |
| A | `src/port/ui/NativeTheme.h` |
| M | `src/port/ui/PaperboatGui.cpp` |
| M | `src/port/ui/PaperboatGui.hpp` |
| M | `src/port/ui/PaperboatInputEditorWindow.cpp` |
| M | `src/port/ui/PaperboatMenu.cpp` |
| M | `src/port/ui/PaperboatMenu.h` |
| M | `src/port/ui/PaperboatMenuSettings.cpp` |
| M | `src/port/ui/PaperboatModMenuWindow.cpp` |
| M | `src/port/ui/PaperboatModals.cpp` |
| M | `src/port/ui/PaperboatModals.h` |
| M | `src/port/ui/ResolutionEditor.cpp` |
| M | `src/port/ui/TouchControls.cpp` |
| A | `src/port/ui/TouchDragScroll.cpp` |
| A | `src/port/ui/TouchDragScroll.h` |
| M | `src/port/ui/UIWidgets.hpp` |
| M | `src/port/ui/devtools/saveeditor/SaveEditor.cpp` |
| M | `src/port/ui/devtools/valueviewer/ValueViewer.cpp` |
| M | `src/screen_overlays.c` |
| M | `src/screen_render_util.c` |
| M | `src/state_battle.c` |
| A | `tests/native-settings/check.cpp` |
| A | `tests/native-settings/controller-navigation.cpp` |
| A | `tests/native-settings/display-timing.cpp` |
| A | `tests/native-settings/events.cpp` |
| A | `tests/native-settings/frame-pacer.cpp` |
| A | `tests/native-settings/frame-timing.cpp` |
| A | `tests/native-settings/ios-window-placement.cpp` |
| A | `tests/native-settings/ios-window-placement.py` |
| A | `tests/native-settings/model-portraits.cpp` |
| A | `tests/native-settings/portraits.cpp` |
| A | `tests/native-settings/run.py` |
| A | `tests/native-settings/textures.py` |
| A | `tests/uikit/Host.mm` |
| A | `tests/uikit/run.py` |

## libultraship backend changes relative to the original upstream pin

Baseline `7f1bd60e9442d83acd3bee00c9d8a07270d0777e`; included fork head `4f2fd5398871b92682dad87a5f0b416b9178b18f`.

```text
include/fast/backends/FramePacer.h         |  38 +++++++
 include/fast/backends/IosWindowPlacement.h |  20 ++++
 include/fast/backends/gfx_metal.h          |   8 ++
 include/fast/backends/gfx_rendering_api.h  |   3 +
 include/fast/backends/gfx_sdl.h            |   7 ++
 include/fast/backends/ios_frame_wait.h     |  13 +++
 include/fast/interpreter.h                 |   3 +
 src/fast/Fast3dGui.cpp                     |   3 +-
 src/fast/backends/gfx_metal.cpp            | 172 ++++++++++++++++++++++++++++-
 src/fast/backends/gfx_sdl2.cpp             |  46 ++++++++
 src/fast/interpreter.cpp                   |  24 ++--
 11 files changed, 325 insertions(+), 12 deletions(-)
```

### Backend commit history

This includes temporary diagnostics subsequently removed; the final source and the behavior described above determine what ships.

| Commit | Change |
| --- | --- |
| [`bd8a8ab2`](https://github.com/acoston822/libultraship/commit/bd8a8ab2) | Metal: space presents with a minimum duration (experimental, gMetalPresentPacing) |
| [`29f14dd6`](https://github.com/acoston822/libultraship/commit/29f14dd6) | Metal pacing: derive the minimum present gap from the target FPS instead of fixed 60 |
| [`bfc7d47e`](https://github.com/acoston822/libultraship/commit/bfc7d47e) | Metal: second screen (AirPlay) output of the game image without the UI |
| [`4f23b084`](https://github.com/acoston822/libultraship/commit/4f23b084) | Second screen: don't draw the game on the device screen while one is attached |
| [`9712772f`](https://github.com/acoston822/libultraship/commit/9712772f) | Metal: log long nextDrawable waits (diagnostic) |
| [`9f7878b6`](https://github.com/acoston822/libultraship/commit/9f7878b6) | Apple: only offset by the window viewport when the game framebuffer really matches the window size |
| [`c4e02eae`](https://github.com/acoston822/libultraship/commit/c4e02eae) | Metal: drop the temporary nextDrawable timing log |
| [`d6fedb04`](https://github.com/acoston822/libultraship/commit/d6fedb04) | Log AirPlay render and output geometry for device diagnosis |
| [`736287c3`](https://github.com/acoston822/libultraship/commit/736287c3) | Remove temporary AirPlay diagnostic logging |
| [`52897a3b`](https://github.com/acoston822/libultraship/commit/52897a3b) | Allow UIKit event servicing during iOS frame waits |
| [`346b2453`](https://github.com/acoston822/libultraship/commit/346b2453) | Expose optional iOS drawable wait timing |
| [`7d8a87c7`](https://github.com/acoston822/libultraship/commit/7d8a87c7) | Use steady frame deadlines on iOS |
| [`4f2fd539`](https://github.com/acoston822/libultraship/commit/4f2fd539) | Keep the primary iOS window on the phone at launch |
