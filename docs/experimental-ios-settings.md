# Experimental iOS settings

Branch: `experimental/settings-ui`, based on `debug/jr-troopa-black-screen`
at `f2cc6350d`. The existing AirPlay, touch-layout and battle-start diagnostics
remain in this branch. This is an experimental UI build, not a release.

## Scope

The new menu is enabled by default **only on iOS**. Other platforms keep the
classic menu. Settings > General > Use iOS-style Settings switches between the
experimental and classic interfaces on iOS; it does not reset other settings.

The sidebar has four expandable groups with consistent icons, spacing and
selection styling. Opening another group collapses the previous group; selecting
a child opens its page. On narrow displays, the sidebar and content use separate
views, with a menu button to return to navigation.

- Settings: General, Audio, Controls, Graphics, Input Viewer, Notifications,
  Mod Menu. Controller bindings open within the menu, with a Back button.
- Enhancements: Cheats, Gameplay, Visuals (the existing Graphics page), Status Bar.
- Shaders: Shader Packs, retaining controls supplied by loaded packs.
- Dev Tools: General, Stats, Console, Event Debugger, Value Viewer, Save Editor,
  Blueprint Editor.

The shared appearance uses rounded cards, blue selections, green switches,
larger slider buttons and dropdown choices. This uses the existing ImGui renderer,
fonts and icons; it is not a UIKit implementation or a pixel-exact reproduction
of the generated concepts. No new blur pass is added.

Desktop fullscreen, window, cursor, renderer selection and manual menu scaling
controls are omitted in this interface. Audio API selection is retained. Game
resolution, touch-control size and HUD customization remain available. Existing
CVar names, callbacks and touch-layout positions are preserved.

## Editors and portraits

The Save Editor retains all eight tabs. The iOS layout adds touch-sized tab
buttons, named badge and item rows, inventory slots, companion unlock/rank cards,
recipe rows and letter-state dropdowns. Editing affects the same loaded game
state as the original editor; viewing a page must not change player statistics.

The Blueprint Editor retains its filters, presets, categories, combat, defense,
reward and susceptibility edits. Narrow layouts show the character list and
details separately. Companion categories retain their existing supported fields.

Portraits are loaded at runtime from the user's imported game data. Missing
portraits use an empty outlined placeholder. iOS companion portraits resolve
texture-pack replacements; blueprint portraits also support RGBA replacement
components, sampled into the original sprite component bounds. Unsupported
components fall back to original data. Alternate Assets selects a separate
portrait cache, so switching it updates the images; applying a changed pack list
continues to require the existing restart.

No ROMs, extracted game archives, game artwork, saves, uploaded attachments or
texture packs are added to the repository or the build workflow.

## Validation and preview

The existing Linux build can explicitly preview this interface without importing
a game:

```sh
./build/ninja-debug/Paperboat --settings-preview
```

The preview requires the normal generated `paperboat.o2r` containing the port's
fonts and UI resources. Use a separate `SHIP_HOME` directory for preview config
and logs; copy or link that port resource archive into it. It does not bypass
the normal import flow in a regular launch and does not start game logic.
The preview enable flag lives in memory and is not saved to the user's config.

Native compilation and automated preview checks cover sidebar navigation across
all 19 pages and persistence of selected pages. Preview tests also exercise
settings changes and responsive navigation. No private game data is required.

The Linux preview cannot establish Metal/iOS safe-area behavior, physical touch
or controller behavior, editor changes against a real save, or portrait fidelity
with actual game/texture-pack assets. Those require the experimental IPA on a
device. Test the first-run import flow, each group, scrolling and dropdowns,
touch-layout editing, controller bindings, AirPlay, editor tabs, and switching
Alternate Assets before considering this for a release.
