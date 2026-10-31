# Experimental leaf rendering diagnostics

Branch: `experimental/leaf-render-diagnostics`.

Based on PaperReality `8a117cfd8`, with all existing UIKit, controller, iPad,
AirPlay and pacing changes and the Build 121 animated-window geometry fix.
The unsuccessful Build 122 shadow-matrix interpolation experiment remains
reverted. Published releases and `experimental/uikit-settings` are unchanged.

Upstream is merged through `6cecd01a7` (#278, October 28, 2026): pointer-width
script buffers for Huff N. Puff, Fake Bowser/Koopa Bros. and Bandits, plus script
and worker cleanup on soft reset. This adds no settings and changes neither the
libultraship nor torch pins.

The unsigned IPA on this branch has the `-leaf-diagnostics` filename/artifact
suffix, the existing PaperReality name and `com.damien.paperreality` bundle ID,
and native iPhone/iPad support. It installs as the same app. The UIKit FPS overlay
is off. No simulator or capture workflow is needed for this investigation.

## Capture on the device

1. Enter Goomba Road 3 (`kmr_07`), beside the blue/yellow spring and raised save
   point shown in the reference screenshots. Keep the affected leaf in view.
2. Open **Dev Tools → Console**. Restore the depth experiment with
   `set gSettings.ZFightingMode 0`, then run `leaftrace start`.
3. Close Settings. Stand still for about 4 seconds, move for about 5 seconds,
   run against the wall for about 5 seconds, and stand still again.
4. Reopen Console and run `leaftrace stop`, then `leaftrace export`.
5. Save **PaperReality-Leaf-Diagnostics.txt** through Files and attach the report
   to the conversation. `leaftrace status` reports the captured frame count.

Capture is opt-in. It ignores Settings time, other maps, battle contexts and
non-default cameras. It stops after 600 game-render frames (about 20 seconds at
30 game updates/second), at a 6 MiB text limit, or when leaving/reloading the map.
`leaftrace start` replaces the previous in-memory capture. The report survives
stopping/changing maps while the app remains open; it is not retained after exit
unless exported. No texture pack, ROM, save or new video is needed.

## What the report can establish

The JSON-lines text report records map-node IDs, texture names, bounds, flags,
queue distances, bounds-culling decisions, actual CPU callback order, and
model draw/material settings. It also records player position/input/animation,
camera position/viewport, desired FPS/refresh matching and relevant CVars.

`owner_tree_id` is the owning model's tree ID; group children can share this ID.
Each distinct model-node pointer is instead represented by a capture-local
numeric `node` token. Function/argument tokens identify callback order without
exporting addresses. Model matrices are hashed, not exported. The header lists
the field order for compact per-frame arrays.

Material flags describe the selected port material-state list, not a complete
GPU state capture. A model's `geometry_emitted` value means its CPU draw path
appended a model display-list call. It does not prove that its pixels survived
GPU depth/alpha/culling tests or that the imported display list drew triangles.
This is an investigation build, not a proposed flicker fix.

No images, texture pixels, raw display lists, vertices, saves, folder paths or
memory addresses are exported or uploaded. Export opens Files locally; there
is no automatic network submission. The capture reads engine state and leaves
rendering decisions, geometry, timing and settings unchanged.

## Local verification

```sh
python3 tests/rendering/run-leaf-trace.py build/ninja-debug
python3 tests/rendering/run-leaf-trace.py build/ninja-debug --sanitize
```

The real collector and production model-property lookup are tested against
generated model metadata and static material flags. Checks cover opt-in/map
filters, stationary versus wall input, inventory IDs, queue/cull/draw metadata,
unchanged model/material memory, stop/restart, frame/byte bounds and concurrent
snapshot reads. Disabled hook arguments have no runtime evaluation. Tests use no
ROM, textures, application context, GPU or simulator. They do not reproduce the
on-device disappearing leaf.
