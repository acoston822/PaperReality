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
  follow the system Light/Dark appearance; no game artwork is bundled for the backdrop.
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

Controller bindings, Event Debugger and Value Viewer still open their existing
interfaces through explicit buttons. A **Back to UIKit Settings** control returns
to native settings. The full console also remains available this way; its native
page accepts commands. This experiment does not claim a native rewrite of those
advanced tools.

## Threading and asset handling

UIKit presentation and view updates run on the main queue. Immutable C++ page
snapshots cross to UIKit; edits, menu callbacks, portrait decoding and refresh
requests return through the render-frame queue. Game input is blocked while
UIKit settings are open. Presentation targets the main iPhone/iPad window,
excluding an AirPlay display. The full-screen overlay keeps SDL rendering alive.

No ROM, save, texture pack or extracted game artwork is included in this branch
or its tests. Runtime portraits are not checked-in screenshots or bundled art.
The implementation does not upload user assets. Blueprint exports contain the
existing numeric preset format, not sprite data.

For missing HD editor icons, open **Settings → Mod Menu → Export Sprite Texture
Report** on the device where the pack is installed. The small text file records
the reported badge/item icons and companion icons, candidate resource names,
archive filenames, dimensions, unreadable replacements, and the texture actually
selected by the thumbnail loader. It respects Alternate Assets and does not
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

The capture workflow reads artifacts directly because run status can remain
cached as queued after simulator results are available. An IPA is considered
available only after its artifact can be downloaded and its bundle build number
checked. A queued status alone is not evidence that compilation or packaging passed.

Verified for the opening fix: unsigned iOS build 40 succeeded, and the iPhone
simulator regression passed. General, Audio and navigation captures are made
from the actual view hierarchy in its landscape coordinates. The harness uses
sample preferences, and does not import a game. Captures are uploaded as test
artifacts; they are not encoded into check annotations.
