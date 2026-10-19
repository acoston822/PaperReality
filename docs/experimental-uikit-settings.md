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
  Viewer, Notifications and Mod Menu. Compact screens use native navigation.
- Native grouped lists, switches, sliders with precise increment/decrement buttons,
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
  Alternate assets follow the user's enabled pack selection; compatible RGBA
  replacements are supported. Blueprint parts are assembled in their original
  bounds, so a pack's higher-resolution components keep the sprite alignment.

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
