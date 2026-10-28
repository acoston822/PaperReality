# PaperReality changes from HarbourMasters/PaperBoat

Base: HarbourMasters/PaperBoat `develop` at 781d164cf, merged into
`experimental/uikit-settings` on 2026-10-24. Previously synced through d98b4b63
(#262, #263, #264, #266; originally 7f038b85). These changes have not all been
tested on a device.

PaperReality 1.0.3 collects the changes since the first PaperReality release.
See the [release notes](release-notes/v1.0.3.md) for the user-facing changes and
the [source audit](source-changes-v1.0.3.md) for implementation details, source
inventories, upstream commits, and pinned backend changes.

## After 1.0.4 — test build

- **Animated window geometry**: keep rotating panel vertices in their own
  display list instead of a shared 21-slot ring that a newer queued frame can
  overwrite. Also keep the corner vertex base pointer fixed so all four corners
  stay within their 16-vertex allocation; the old non-`SHIFT` path advanced it
  while still indexing from the original base. A local regression verifies corner
  positions, texture coordinates, initialization and queued-frame stability.
  This targets the intermittent save-card
  transition flicker; confirmation of that exact symptom still needs the test
  IPA on a device, and absence of an intermittent flicker in one test is not proof
  of a fix. Upstream [#219](https://github.com/HarbourMasters/PaperBoat/issues/219)
  reports similar file-select flicker on Windows/DirectX 11 at 75 Hz; a shared
  cause has not been established. The published 1.0.4 release is unchanged.

## PaperReality 1.0.4

See the [release notes](release-notes/v1.0.4.md) for changes since 1.0.3 and
the [source audit](source-changes-v1.0.4.md) for implementation and validation.

- **Native iPad support**: target both iPhone and iPad in the iOS app bundle.
  Earlier IPAs declared iPhone-only support, so iPadOS used iPhone compatibility
  mode. The IPA workflow verifies the packaged device families and existing app
  name and bundle identifier. Build 117 passed the native iPad window check in
  the iPadOS 26.2 simulator; the reporting user confirmed it resolved their
  fullscreen issue on an iPad.
- **Controller navigation**: remove the bottom controller help strip while
  retaining navigation and the focus outline. Native dialog action outlines
  follow the whole button instead of its text, including dialogs whose heading
  repeats an action title.

## Upstream sync — 2026-10-24

- `befe89c37` (#269): fixes an animator buffer use-after-free reported during
  Munchlesia by retiring display-list and vertex copies once per drawn frame,
  rather than once per animator update; increases the temporary vertex buffer
  pool to accommodate the retained copies.
- `abac004a3` (#270): adds **Mute ISpy Sound**, keeping I Spy's visual notification
  while optionally silencing its sound.
- `781d164cf` (#272): adds **Always Chuck Quizmo** in eligible locations, while
  retaining story-progress and unanswered-question requirements.
- Both settings appear under **Enhancements → Gameplay** in UIKit, reuse the
  upstream CVars and callbacks, default to off, and respect the existing race
  lock. Their native controls are checked against the actual gameplay event
  listeners. The libultraship pin and the iOS interface, AirPlay, touch and pacing
  changes are retained.

## Since 1.0.0 (branch `feat/airplay-second-screen`)
- **AirPlay refresh-rate matching**: limit the game's effective interpolation target to the external display's reported refresh rate while connected. A 120 Hz phone no longer requests 120 game frames through a synchronous 60 Hz TV output, which could hold up game progress. Lower configured targets are preserved, disconnect restores the normal phone target automatically, and Settings continues to request the phone's refresh rate independently. Saved preferences, Metal present pacing and the game-only TV output are retained.
- **AirPlay connected before launch**: explicitly create the interactive iOS window on the phone using SDL display 0's logical bounds. Previously, saved physical-pixel dimensions and desktop window positions could make SDL select an already-connected external display; the game-only AirPlay window could then cover the touch controls. The display-selection regression is reproduced with synthetic SDL2 displays and checked for cold launch, later connection, disconnect, portrait/tablet bounds and stale TV-sized settings. Real-device AirPlay verification is still required. No diagnostic HUD or continuous AirPlay logging is added. Run the regression with `python3 tests/native-settings/ios-window-placement.py` (SDL2 static library and pkg-config required).
- **AirPlay layout fix**: resize the external Metal drawable when UIKit lays out its view or the display mode changes. Build 27 was reported working across multiple devices and repeated boots. Temporary AirPlay diagnostics were removed after validation; the layout and position fixes remain.
- **AirPlay second screen** (always on, no setting). When an external screen or AirPlay display is connected, the game image is shown there and the phone shows only the touch controls and menus, with black where the game image would be. Touch controls are not drawn on the external screen. Code: `src/port/ios/IosExternalDisplay.h/.mm` (watches UIScreen connect/disconnect, and polls for a mirrored screen), plus the libultraship fork (`external-display` branch): a second `CAMetalLayer` output of the finished game image without the UI, and skipping the game image on the device while an external screen is active.
- **Second screen position fix**: on Apple the game picture could be shifted inside the render texture (shown on the TV as a corner image with garbage around it) when MSAA was on and the render size equalled the on-screen game size. The offset now only applies when the framebuffer really matches the window (libultraship `interpreter.cpp`).
- **Touch layout**: screenshot-based default positions for A, B, C, Z, L, R and Start, with L/R mirrored at the same height, a C-button diamond of equal height and width and the menu button at bottom-left. Saved custom positions are preserved; Reset in the layout editor restores these defaults.
- **Smooth buttons**: A, B, C and Start now use the same smooth round outline as L, R and Z.
- **Hide With Controller** only reacts to real game controllers, not other joystick-class devices.
- **Popups and dropdowns**: swipe scrolling now runs for every open popup list (such as the colour dropdowns), not only the pane lists.
- **First-run reliability**: the Files picker finds the game window even when it is not yet the key window and retries instead of treating that as a cancel, and ignores taps while open. First-run screens present frames without Metal present pacing (restored before the game starts), and a "Loading game..." message is drawn before game start-up blocks the thread, so the last dialog no longer sits frozen until the app is backgrounded.
- **Logging**: `Paperboat.log` is flushed to disk every second so it survives the app being swiped away.
- **Crash fix kept over upstream**: the missing `gsSPEndDisplayList()` at the end of `D_8014EA48` in `src/screen_overlays.c` stays. Upstream still crashes on first boot on iOS without it.

## Build and packaging (iOS)
- GitHub Actions workflow `ios-unsigned.yml` builds an unsigned IPA on every push except `develop`. The run number is stamped in as the build number (CFBundleVersion, and `-bN` on the version string) and is logged at startup.
- iOS build fixes in the first commit: Game Mode keys, display-list terminator, alt-only `_img` lookup, darkness-stencil cast (`screen_overlays.c`, `screen_render_util.c`, `MapTexturePatches.cpp`).
- CMake: Objective-C++ (`src/port/ios/*.mm`, ARC) and the UIKit and UniformTypeIdentifiers frameworks on iOS.
- App icon: red paper boat (`ios/Icon.png`). `ios/PaperReality-Icon.png` is a copy the workflow always copies over the bundle icon, so an upstream merge can't bring the old one back. `ios/plist.in` updated.
- libultraship submodule now points at `acoston822/libultraship` (JeodC `lus-converge`, Metal present pacing, and the second-screen output; branch `external-display`).

## Frame pacing
- Metal present pacing comes from the libultraship fork. This repo only carries the submodule pin. The diagnostics and experiments used to find it (frame-stats logging, pipeline logging, preferred frame rate hint) were removed again.

## Touch controls (`TouchControls.cpp`)
- Round L/R/Z buttons were tried, then L/R became rounded rectangles (about 5u by 2u) with gray rings; Z is a purple circle. Ring thickness matches the other buttons and the letters were removed.
- Slightly larger C buttons; translucent colored fills; pressed buttons fill to the ring color. Gray L/R stay clear when idle.
- Joystick: round themed ring, small translucent thumb dot in the menu theme color, optional "Hide Joystick When Idle" (fully invisible, still shown in the layout editor). Default resting position moved farther right and slightly higher to match the reference screenshot; floating behavior and saved positions are preserved.
- "Show Touch D-Pad" defaults to off. Users can enable it in settings; saved choices are preserved.
- Haptic feedback setting with a strength dropdown (`IosHaptics.h/.mm`).
- "Hide With Controller": touch controls hide and ignore touches while a game controller is connected, and return on disconnect. On by default; always visible in the layout editor.

## Controller
- Select/Back opens the port menu; the engine still closes it. Controller menu navigation defaults on once a controller is detected. Controller detection and button presses are logged (`[controller]`).

## Menus and UI
- Swipe-to-scroll with flick momentum for settings panes and every popup/dropdown list (`TouchDragScroll.h/.cpp`, `Menu.cpp`, `Engine.cpp`).
- ImGui Menu Scaling options: 1.5, 1.75, 2.1, 2.3.
- Popups can have their own scale (`PaperboatModals`, `PaperboatGui`).
- Save Editor: Max HP/FP sliders also set hardMax so values persist across area changes.

## First launch and files (iOS)
- First-launch greeting asks the user to import their ROM through the Files picker and copies it to the right folder before the o2r prompt (`IosFilePicker.h/.mm`, `Engine.cpp`).
- First-run boxes (welcome, ROMs found, unsupported ROM, Run PaperBoat, extraction progress) are drawn at 1.75x on iOS.
- Mods page "Add Files" button: picks .o2r files and copies them into `Documents/mods`, then shows a "Restart Required" prompt (Restart Now / Restart Later). Alternate Assets still has to be switched on for texture packs.

## Not in this repo but relevant
- `acoston822/libultraship` branch `external-display` (c4e02eae) holds the Metal present pacing and the second-screen output.
- Branches: `upstream-clean` (stable), `exp/present-pacing`, `feat/first-run-greeting`, `feat/touch-joystick-autohide-swipe-scroll`, `feat/mods-addfiles-stick-look` (latest), plus `hud-test` and `ios-standalone`.
