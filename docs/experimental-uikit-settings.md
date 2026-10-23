# Experimental UIKit settings (iOS only)

Branch: `experimental/uikit-settings`. This is a separate experiment from
`experimental/settings-ui`, based directly on the stable rendering/touch changes.
It does not change the desktop settings interface or the selected renderer.

The goal is a clean native iOS interface inspired by the concepts, rather than an
exact recreation. The iOS build opens UIKit settings by default. Turn off **Use
Native iOS Settings** in General to return to the classic menu; the same switch
in the classic General page restores UIKit.

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
- Consecutive numeric settings share a two-column card layout when there is enough
  space. Narrow widths and accessibility text retain a single column.
- Native lists, switches, sliders with precise increment/decrement buttons,
  choice submenus, search, color pickers and confirmation alerts. System colors,
  Dynamic Type, safe areas and light/dark appearance follow iOS.
- Existing CVar defaults, callbacks, race locks and disabled reasons are reused.
  Desktop-only window, renderer and ImGui appearance controls are omitted.
  The audio API selector remains available when multiple backends are supported.
- Native Input Viewer controls, shader settings supplied by loaded packs, mod
  enable/order controls and Files import.
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
- Fake Bowser, Toy Tank and Light Bulb use labeled system icons because their
  bodies are map models, not NPC sprites. Fake Bowser's hidden Koopa Bros idle
  placeholder is excluded. Rendering actual model thumbnails is not implemented.
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
the existing wait services default/tracking run-loop callbacks until its original
deadline. It does not render extra game frames or change the FPS CVars. Outside
the active native menu it falls back to the existing sleep. UI actions continue
to enqueue engine work for the next game frame; the wait does not reenter SDL or
the engine. Rendering work itself can still delay UIKit on the shared main thread.
The refresh request stops on dismissal/backgrounding; iOS retains control over
adaptive refresh, Low Power Mode and thermal limits. Only development IPAs display
callback timing; those readings do not measure GPU presentation.

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
