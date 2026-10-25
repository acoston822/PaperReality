# Experimental UIKit settings (iOS only)

Branch: `experimental/uikit-settings`. This is a separate experiment from
`experimental/settings-ui`, based directly on the stable rendering/touch changes.
It does not change the desktop settings interface or the selected renderer.

The goal is a clean native iOS interface inspired by the concepts, rather than an
exact recreation. Opening Settings on iOS always uses UIKit. The switch back to
the original menu has been removed, and older saved interface preferences are
ignored. The shared settings definitions and callbacks remain available for
bringing future upstream settings into the iOS interface.

A fresh iOS launch starts with Settings closed, including after the app was
terminated with Settings open. Saved preferences and the last-selected page are
retained. Returning to an app that is still running keeps its current page open.

## Included

- Four expandable sidebar groups: Settings, Enhancements, Shaders, and Dev Tools.
  Destinations are generated from the existing menu definitions, including Input
  Viewer, Notifications and Mod Menu. The sidebar stays beside the page on landscape
  iPhones with enough usable width. Narrow layouts and accessibility text use a
  category browser button, with native navigation inside each page.
- Rounded material cards, a shared PaperReality header, blue selected sidebar rows,
  and consistent spacing across settings and editor submenus. Materials and colors
  follow the system Light/Dark appearance. The full-screen background is opaque;
  it does not blur/sample the live game surface. No game artwork is bundled for it.
  The header uses a transparent red origami boat cutout based on the app icon.
- Consecutive numeric settings share a two-column card layout when there is enough
  space. Narrow widths and accessibility text retain a single column.
- Native lists, switches, sliders with precise increment/decrement buttons,
  choice submenus, search, color pickers and confirmation alerts. System colors,
  Dynamic Type, safe areas and light/dark appearance follow iOS.
  Descriptions are read directly on the page; tapping a description or a slider
  card does not open a duplicate explanation alert.
- Existing CVar defaults, callbacks, race locks and disabled reasons are reused.
  Desktop-only window, renderer and ImGui appearance controls are omitted.
  The audio API selector remains available when multiple backends are supported.
- Native Input Viewer controls, shader settings supplied by loaded packs, mod
  enable/order controls and Files import.
- Upstream **Mute ISpy Sound** and **Always Chuck Quizmo** are available under
  Enhancements → Gameplay, with inline descriptions and controller support.
  Both default to off and use the existing saved gameplay settings and race lock.
- Graphics includes the original **Pixel Count Presets**: Custom, Native N64
  (240p), 2x (480p), 3x (720p), 4x (960p), 5x (1200p), 6x (1440p), Full HD
  (1080p), and 4K (2160p). Enable advanced settings and Fixed Vertical Resolution
  to use them. Selecting Custom preserves the current pixel count; editing
  Vertical Pixels switches the preset to Custom. The selection and resolution
  use the existing saved settings and leave the aspect ratio unchanged.
- Save Editor pages for player status, badges, items, party, letters, recipes,
  Star Pieces and tattles. A loaded save is required; changes persist when the
  player saves in game. Merely opening a page does not normalize save values.
- Blueprint category/character pages and Combat, Defense, Rewards and
  Susceptibility editors. Numeric preset JSON uses Files import/export.
- Portraits beside characters, companions, badges and items are decoded at runtime
  from installed resources. Missing assets produce blank outlined placeholders.
  Alternate assets follow the user's enabled pack selection, including when the
  original texture was already cached. Badges, items, companions and blueprint
  parts share the same loader, which checks palette-specific HD artwork before
  generic replacements and supports pack resource aliases. Missing or unusable
  replacements fall back to the original sprite; turning alternate assets off
  restores the originals. Blueprint parts are assembled in their original
  bounds, so a pack's higher-resolution components keep the sprite alignment.
- Fake Bowser, Toy Tank and Light Bulb use cached 256px thumbnails rendered from
  their imported map-model geometry and materials. Only the selected model
  subtree is included; the battle stage and Fake Bowser's hidden Koopa Bros
  sprite are excluded. Thumbnails use the enabled HD replacements and are
  invalidated when alternate assets or mounted archives change. Missing assets
  retain the blank placeholder. This static CPU preview preserves baked vertex
  colors and texture alpha, without advancing game animations or submitting work
  to the gameplay renderer. Animated effects and auxiliary texture combiners
  are outside the thumbnail preview. Material copies are bounded to 512px per
  side; source resources are unchanged. Three cached portraits use at most
  768 KiB of RGBA storage, separate from the existing UIKit image cache.
  The preview respects solid/cutout/translucent surface opacity and RGBA/ST
  vertex-edit commands. The exported Sprite Texture Report includes each model's
  geometry-loading result, parser failures, and actual material replacement
  selection/dimensions, so an unchanged thumbnail can be diagnosed without
  transferring game models or textures.
- Pages and navigation bars use opaque, appearance-aware backgrounds, keeping
  controls below their page title and hiding outgoing pages during navigation.
  Sidebar category and submenu labels share a smaller size fitted to the longest
  entry, keeping Enhancements on one line.

Event Debugger now opens a native, read-only inspector with expandable event
cards, Expand All/Collapse All, registered event IDs, caller source locations and
64-bit counts, actual listener IDs, source locations and Low/Normal/High priorities.
The registry is copied on the game-frame queue, with owned strings and no engine
pointers passed to UIKit. It refreshes once per second while visible, preserves
expansion/scroll state during count updates, and stops refreshing when left.
Live updates wait until scrolling finishes. Newly registered events and removed
listeners are reflected in fresh snapshots. Empty registries, missing metadata
and unavailable call-site instrumentation have explicit messages. Upstream only
records caller counts in debug builds; normal release IPAs still show registered
events and listeners, without inventing counts. The native page is also restored
when Settings reopens on Event Debugger. It follows system Light/Dark appearance
and Dynamic Type using the same opaque page, title and card styling as Settings.

Value Viewer settings are native too: Show Value Viewer, Floating Window,
Background Opacity, Overlay Size, Show Current Map and Show Mario's Position.
The opacity/size controls share the existing adaptive numeric-card layout.
They update the existing overlay CVars and cached drawing state on the game
queue, and preserve current preferences when the page opens. The overlay itself
continues using its existing in-game drawing path; its size is separate from
the settings interface. The existing 0.5 size fallback remains supported.

All styled tables suppress system scroll-edge material where that API exists.
Event Debugger additionally uses zero header/footer heights and estimates, and
no automatic inset adjustment inside its explicitly constrained content area.
This avoids blank sticky strips over the first/last visible event cards. The
simulator regression compares the outside card gutter against its unobstructed
canvas at normal, fractional and overscrolled offsets in Light and Dark mode.

Controller Bindings now opens native pages for all four ports, buttons, D-pad,
both analog sticks, sensitivity/dead zone/notch settings, global axis thresholds,
and controller-supported rumble, gyro and LED options. Gamepad capture runs on
the game queue, waits for held inputs to be released, and returns to the edited
binding after success. Cancel, leaving settings, and a 20-second timeout stop
capture; the old binding is retained until a replacement is detected. Existing
keyboard/mouse mappings remain visible and are preserved when adding gamepad
inputs or restoring gamepad defaults. Capture currently listens to SDL gamepad
buttons and axes. Input names use regular Unicode arrows in place of ImGui font
icons. Port choices survive a manual device refresh. Defaults and clearing a
port use native confirmation dialogs; rumble tests stop after half a second or
when settings closes.

The UIKit menus have their own controller navigation, independent of N64
bindings and ImGui navigation. D-pad/left stick moves focus, A selects/toggles,
B returns to the parent (then the sidebar, then closes Settings), Left/Right
adjusts a numeric setting, and L/R switches sidebar/detail panes. Vertical
traversal visits both controls in paired slider rows. X opens search or console
command entry; the right stick scrolls long descriptions, output and inspectors.
A theme-colored outline identifies the current control; touching the interface
hides the controller outline. Controller navigation does not add a bottom help
strip over the page. Selection is remembered per live page. Holding an
opening button cannot select the first action of a new screen or dialog.

Controller text entry includes case, digits, spaces, punctuation, deletion,
clear, Done and Cancel. Color editing uses exact RGB/RGBA channel values with
Done/Cancel. Registered app dialogs expose their existing actions without private
UIKit selectors; B cancels and cannot confirm a destructive action. All buttons,
including Start/Menu, remain available during binding capture; navigation resumes
when capture succeeds, is cancelled, or times out. The existing Select/Back menu
shortcut remains available. Physical controller navigation needs device testing.

Imports and exports always use the standard iOS Files picker, including when a
controller is connected. File selection is a touch-only exception to controller
navigation; controller polling does not inspect, highlight, activate or dismiss
its controls. The app's import/export buttons remain controller-accessible, and
Open App Files Folder continues to open the Files app. Existing private-copy,
multiple-selection, export and cancellation behavior is retained. Picker request
cleanup also handles programmatic dismissal of its presenting Settings screen.

First-run ImGui prompts enable controller navigation by default without overriding
an existing saved preference. Touch-layout editing also supports L/R selection, D-pad nudging, A/B Done and X Reset; editing inputs
are withheld from gameplay, including the held confirmation button on exit.

The controller poller reads GameController state without replacing SDL handlers
or changing mappings. It runs only with a connected controller while the app is
active, and acts only on native menus/sheets. It does not change game frame
pacing or reintroduce the development timing HUD. Pure repeat/held-input checks
run with the IPA SDK checks. The UIKit harness additionally covers target
identities, paired controls, pane switching, color channels and text entry when
an explicit simulator debugging check is requested.

The sidebar uses its own opaque, clipped canvas with automatic content insets,
sticky section-header space, and scroll-edge decoration disabled. Its navigation
bar stays hidden when scrolling, avoiding a strip above the category selections.
Both fixed panel containers and each table's stationary background use the opaque
page canvas. Panel edges align to physical pixels, including fractional safe-area
insets, to keep elastic scrolling from exposing the game's backdrop through a seam.
The local UIKit harness compares top/bottom edge pixels over black and white
backdrops while pulling both lists past their limits, in Light and Dark appearance.
That overscroll regression requires a Mac simulator and cannot run directly
in the Linux workspace.

The entire settings root now has an opaque Light/Dark canvas, with the live
`UIVisualEffectView` backdrop removed. This targets a reported on-device line
artifact that disappears during screenshots/recording and returns afterward.
The compositing cause has not been established, and capture-based simulator
checks cannot verify this physical-display behavior. The harness additionally
compares the full window over two synthetic presenting-view colors during
sidebar overscroll, including panel corners and gutters, and checks that it
detects a deliberately translucent overlay. Confirmation with recording stopped
on the affected iPhone is still necessary.

The opaque-background candidate did not resolve the affected device's lines.
The next candidate composites just the fixed sidebar (including its scrolling
rows and rounded clip) into one Core Animation surface at the display's pixel
density. Other settings panels retain normal composition. Empty sidebar section
headers/footers now have zero height rather than accumulating fractional gaps.
The harness checks those boundaries, the rasterization scale after resizing,
and whole-window coverage at normal and fractional scroll offsets as well as
overscroll. These checks still cannot establish whether the physical-display
artifact is resolved; that requires testing with screen recording stopped.

The Console is fully native: a selectable, scrollable monospaced transcript,
command field, Run and Clear controls, and a recent-command menu. A note explains
that `help` lists all available commands. It has no severity tabs, search bar,
suggestion buttons, or separate ImGui console button. Commands use the existing
registry on the game queue; invalid arguments become visible errors. Output is
bounded to 500 entries/256 KiB, and the last 100 commands remain available during
the app session. Clear removes output and retains command history.

All iOS calls through the shared file-picker bridge use the system Files picker,
including N64 save imports and classic blueprint presets. Imported copies return
to the game queue before use and are removed after reading. Export callbacks
write a temporary file that Files copies to the chosen destination; cancellation
also removes that staging file. Import and export presentations are serialized
on the main queue, retain their callbacks through presentation retries, and
report a failure instead of silently abandoning an export. Open App Files Folder
uses the Files app's document-folder URL and gives manual navigation instructions
if Files cannot open it. Desktop file dialogs are unchanged.

## Threading and asset handling

UIKit presentation and view updates run on the main queue. Immutable C++ page
snapshots cross to UIKit; edits, menu callbacks, portrait decoding and refresh
requests return through the render-frame queue. Game input is blocked while
UIKit settings are open. Presentation targets the main iPhone/iPad window,
excluding an AirPlay display. The full-screen overlay keeps SDL rendering alive.

Settings owns a display-link request for the screen's maximum refresh rate while
visible, independently of Current FPS and Match Refresh Rate. The iOS SDL frame
limiter has an optional application wait callback: while native settings is open,
the wait services default/tracking run-loop callbacks until its planned deadline.
It does not render extra game frames or change the FPS CVars. Outside
the active native menu it sleeps until the same deadline. UI actions continue
to enqueue engine work for the next game frame; the wait does not reenter SDL or
the engine. Rendering work itself can still delay UIKit on the shared main thread.
The refresh request stops on dismissal/backgrounding; iOS retains control over
adaptive refresh, Low Power Mode and thermal limits. Only development IPAs display
callback timing; those readings do not measure GPU presentation.

The iOS frame limiter plans deadlines from a fixed epoch and integer frame index,
including fractional intervals at 30/60/120 FPS and arbitrary selected rates.
It waits for the full deadline rather than applying the macOS 1 ms early-wake
allowance, and rechecks after interrupted sleeps. Small wake-up errors do not
accumulate into subsequent deadlines. Missed deadlines, clock discontinuities,
FPS changes and Metal pacing-mode changes restart the cadence, avoiding bursts
of catch-up frames. Metal's existing minimum presentation spacing remains in
place. Desktop limiter code is unchanged. Simulator checks use the pinned
backend clock header to service actual UIKit callbacks during a 30 FPS wait;
they do not establish physical-device presentation smoothness.

Development timing also shows the game target (not measured game FPS), the Metal
pacing switch, and peak render-work/drawable-acquisition wall times for the recent
sample window. Render work covers graphics preparation and individual draws,
excludes the cooperative UIKit wait, but includes
drawable acquisition, so these two peaks are not additive. They do not measure
CPU or GPU execution time. The optional Metal observer is installed only by the
development app; normal builds collect no drawable timestamps. These readings
help distinguish rendering delays from drawable waits without disabling Metal's
presentation pacing or changing the game FPS. They reset with the menu clock.

No ROM, save, texture pack or extracted game artwork is included in this branch
or its tests. Runtime portraits are not checked-in screenshots or bundled art.
The implementation does not upload user assets. Blueprint exports contain the
existing numeric preset format, not sprite data.

For missing HD editor icons, open **Settings → Mod Menu → Export Sprite Texture
Report** on the device where the pack is installed. The small text file records
the reported badge/item icons (including Kaiden) and companion icons, candidate
resource names, archive filenames, dimensions, unreadable replacements, and the
texture actually selected by the thumbnail loader. A complete presence audit also
covers every image registered in the Save Editor (even if the base file is
absent), every installed `icons/` and `ui/` raster, and every character in the
Blueprint Editor, using the same first-frame components as its portrait. That
section reports matching replacement entries without decoding/caching all HD
pixels; presence alone does not establish that an entry is usable. Counts identify
the report's coverage. It respects Alternate Assets and does not
change the pack selection. It includes no image bytes, saves, ROM contents or
device folder paths. The system Files picker saves a copy wherever the user
chooses; nothing is sent automatically. Packs stay on the device.

Native blueprint portraits retain replacement texture pixels while assembling
parts in the original animation coordinate system. Mixed-resolution parts share
a common canvas scale and use source-over alpha blending; unusually large output
is capped at 2048 pixels per side to bound temporary memory. UIKit creates a
44-point thumbnail at the display's actual pixel density, enlarging small sprites
without interpolation and using high-quality reduction for larger artwork. No
texture-pack files are rewritten. Synthetic portrait checks live in
`tests/native-settings/portraits.cpp`; the UIKit harness also verifies Retina
dimensions and that enlarging a checkerboard introduces no blurred colors.

The native Save Editor places Auto Jump, Super Jump, Auto Smash, Berserker,
Kaiden, Right On, Healthy Healthy and Earthquake Jump in an **Unused** section
below the regular badges. Battle Items puts Insecticide Herb and Please Come
Back in the same section below its regular items. Their portraits and editing
controls still work; the section identifies entries unavailable through normal
gameplay.

## Validation

Build the normal Linux Ninja debug target first with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`, then run:

```sh
xvfb-run -a python3 tests/native-settings/run.py build/ninja-debug
```

The integration harness links the actual native models and menu callbacks against
that build, using port resources and a temporary synthetic save. It checks the
frame queue, navigation restoration, disabled controls, save guards, resolution
persistence, submenu traversal and empty portrait behavior without game assets.
Generated pixel fixtures also check HD selection after warming the original cache,
on/off toggles, palette variants, aliases, partial packs, invalid replacements,
original palette fallback and preservation of replacement dimensions and pixels.
It never runs a game or writes into a user's save directory.

Validated locally: the Linux build and integration harness pass (804 pages,
5,583 rows), including actual Input Viewer visibility and clean shutdown.
The Apple SDK Objective-C++ check also passed on the macOS Actions runner.

The event snapshot regression runs without a renderer or game resources. Compile
`tests/native-settings/events.cpp` with the actual
`external/libultraship/src/ship/events/EventSystem.cpp`, using C++20 and include
paths `src` and `external/libultraship/include`. Run it with and without `_DEBUG`
to cover instrumentation availability, real dispatch counts, 64-bit metadata,
stable ordering, listener removal, null metadata and immutable snapshot ownership.
The optional UIKit harness also covers event card expansion/collapse, live count
updates, priority labels and empty-registry messages. Routine IPA builds syntax
check the UIKit source and harness with the Apple SDK; they do not launch a
simulator. Runtime simulator checks remain manual-only.

The iOS Actions workflow checks Objective-C++ sources with the Apple SDK before
building the full unsigned IPA. Linux checks cannot validate UIKit presentation.
Device checks remain necessary for visual polish, opening/closing settings during
play, touch layout editing, Files pickers, controller input and AirPlay. Test
light/dark appearance, large text and compact layouts as part of that review.

## Settings-opening regression

The presentation retry originally cleared a recursive block during its first
synchronous call, releasing C++ captures before the screen finished using them.
Presentation now uses a function with owned arguments, including delayed retries.
The asset-free iPhone simulator harness opens settings, waits for page models,
renders the control types, navigates, closes, and reopens. Run on a Mac with an
iOS simulator runtime installed:

```sh
python3 tests/uikit/run.py
```

The screenshots from that harness use sample preferences and blank portraits.
They contain no game files, saves or extracted artwork. Private game assets must
never be uploaded to these GitHub runners for testing or screenshots.

The card-layout regression also checks landscape sidebar visibility, compact
category/page switching, nested editor-style pages and choice navigation. Captures
include both Light and Dark appearance. These checks do not establish controller
navigation or in-game behavior on a physical device.

The refined layout passed the iPhone simulator harness on source `9168fe05b`:
opening/reopening, all control types, nested navigation, paired volume cards,
single-column fallback, and editing the correct channel through the queue. The
fixture retains an Audio API selector beside the numeric controls to check mixed
sections. General and Audio captures are actual UIKit views with synthetic settings.

The simulator harness can run locally on a Mac. The UIKit runtime workflow is
run only for requested debugging or renders; routine pushes do not run it.
It checks only the native UI, its headers and synthetic fixtures; it does
not check out game sources, resources, submodules or private uploads. It runs
`tests/uikit/run.py --no-captures`, which skips writing screenshots and gallery
images. No simulator apps, screenshots, reports or logs are uploaded as artifacts,
published to PRs/Pages, or committed to capture branches. Its text-only workflow
run and logs remain visible in Actions during testing. A cleanup job inside the
existing IPA workflow waits for simulator runs to finish, then removes completed
runs belonging only to the UIKit smoke/capture workflow paths. It uses GitHub's
API from the hosted runner, without requiring API access from the development
workspace. The newest unsuccessful simulator result is retained for diagnosis;
after a successful rerun, older results are removed too. IPA and other build runs
are excluded from deletion. No separate cleanup workflow is created.
For a requested debugging investigation, updating `.github/uikit-debug-request.txt`
starts the separate UIKit check. A commit containing `[uikit-debug]` asks the IPA
cleanup job to wait for that run to register before cleanup. Manual dispatch is
also defined. This is opt-in; editing normal sources does not start a simulator.
The unsigned IPA build workflow remains available. An IPA is considered available
only after its artifact can be downloaded and its bundle build number checked.
A queued status alone is not evidence that compilation or packaging passed.

Verified for the opening fix: unsigned iOS build 40 succeeded, and the iPhone
simulator regression passed. General, Audio and navigation captures are made
from the actual view hierarchy in its landscape coordinates. The harness uses
sample preferences, and does not import a game. Captures remain in the local
test output directory.
