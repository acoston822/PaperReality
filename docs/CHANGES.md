# PaperReality changes from HarbourMasters/PaperBoat

Base: HarbourMasters/PaperBoat `develop` at 7f038b85. 29 files changed, 863 insertions, 62 deletions. These changes have not all been tested on a device.

## Build and packaging (iOS)
- GitHub Actions workflow `ios-unsigned.yml` builds an unsigned IPA on every push except `develop`. The run number is stamped in as the build number (CFBundleVersion, and `-bN` on the version string) and is logged at startup.
- iOS build fixes in the first commit: Game Mode keys, display-list terminator, alt-only `_img` lookup, darkness-stencil cast (`screen_overlays.c`, `screen_render_util.c`, `MapTexturePatches.cpp`).
- CMake: Objective-C++ (`src/port/ios/*.mm`, ARC) and the UIKit and UniformTypeIdentifiers frameworks on iOS.
- App icon: red paper boat (`ios/Icon.png`). `ios/PaperReality-Icon.png` is a copy the workflow always copies over the bundle icon, so an upstream merge can't bring the old one back. `ios/plist.in` updated.
- libultraship submodule now points at `acoston822/libultraship` (JeodC `lus-converge` plus Metal present pacing, branch `present-pacing-clean`).

## Frame pacing
- Metal present pacing comes from the libultraship fork. This repo only carries the submodule pin. The diagnostics and experiments used to find it (frame-stats logging, pipeline logging, preferred frame rate hint) were removed again.

## Touch controls (`TouchControls.cpp`)
- Round L/R/Z buttons were tried, then L/R became rounded rectangles (about 5u by 2u) with gray rings; Z is a purple circle. Ring thickness matches the other buttons and the letters were removed.
- Slightly larger C buttons; translucent colored fills; pressed buttons fill to the ring color. Gray L/R stay clear when idle.
- Joystick: round themed ring, small translucent thumb dot in the menu theme color, optional "Hide Joystick When Idle" (fully invisible, still shown in the layout editor).
- "Show Touch D-Pad" setting.
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
- `acoston822/libultraship` branch `present-pacing-clean` (29f14dd6) holds the Metal present pacing.
- Branches: `upstream-clean` (stable), `exp/present-pacing`, `feat/first-run-greeting`, `feat/touch-joystick-autohide-swipe-scroll`, `feat/mods-addfiles-stick-look` (latest), plus `hud-test` and `ios-standalone`.
