# Render Diagnostics: ground-leaf experiment

This work stays on `experimental/leaf-render-diagnostics`, with upstream merged
through #278 and the unsuccessful shadow interpolation change still reverted.
New test IPAs use the `-render-diagnostics` artifact/version label. They retain
the PaperReality app name, bundle identifier and native iPhone/iPad support.

Build 125's report and the user's confirmation show the leaf disappearing while
its CPU draw submission continues: all 600 samples submitted the low flower/leaf
mesh, without changes in logged flags, material, texture selection or matrix
hash. The selected material is alpha-tested ordinary-Z geometry, so the decal
Z-fighting control was not a direct test of its material. Its bounds reach the
same ground level as the terrain. This suggests a controlled ground-contact test;
it does not establish the exact cause of the missing pixels.

## Compare three modes

At the same leaf in Goomba Road 3 (`kmr_07`), enter each command in Console, close
Settings, and reproduce the flicker with the same movement or wall-running:

| Console command | What it does |
| --- | --- |
| `rendertrace lift off` | Original draw path; default. |
| `rendertrace lift 0` | Pushes/pops an identity transform, with no upward offset. |
| `rendertrace lift 0.25` | Takes the same transform path, raising only the identified leaf by 0.25 world units. |

If needed, `rendertrace lift 1` tests a larger one-unit offset. Values are limited
to 0–1, and nonfinite/invalid inputs are rejected. Return to
`rendertrace lift off` afterward. The mode resets to off on any map load or app
restart and is not saved in preferences.

The identity control separates the effect of refreshing matrix/render state from
actually moving the mesh away from the ground. If zero alone helps, do not
attribute the improvement to lifting the leaf. If only positive lift helps, ground
contact/depth overlap becomes a stronger lead. Neither result alone proves a
universal fix for other scenery.

The experiment matches the map, the public `MODEL_o853` identifier, model type,
alpha-test render mode, and flower texture identifier. It adds a temporary
model-view push/multiply/pop around that mesh only. Authored vertices, material,
alpha testing, depth flags, collision data, other models and frame pacing remain
unchanged. The extra matrix uses the existing interpolation recorder normally;
the reverted shadow-matrix experiment is not restored. Bounds culling continues
to use the original model bounds, which is appropriate for this small diagnostic
offset while the leaf is fully in view.

## Export a comparison capture

Use `rendertrace start`, close Settings and reproduce the issue, then reopen
Console and run `rendertrace stop` and `rendertrace export`. Save
`PaperReality-Render-Diagnostics.txt` through Files. The original `leaftrace`
command remains a compatibility alias.

Capture retains the Build 125 limits: opt-in, target map only, up to 600 game
frames and a 6 MiB text buffer; Settings time is excluded. Format 2 also records
`leaf_lift_mode` (-1=off, 0=identity, positive=offset) and `leaf_lift_applied`
(node/value pairs for actual draw overrides). Matrix values are not exported.
The report stays local until the user exports and shares it. No ROM, textures,
images, raw display lists, vertices, saves, folder paths or memory addresses are
included. The settings FPS HUD remains off.

The [Build 125 capture guide](leaf-render-diagnostics.md) describes the original
metadata fields and their limits. These remain CPU submission observations, not
a GPU pixel capture.

## Verification

`python3 tests/rendering/run-leaf-trace.py [build] [--sanitize]` exercises the real
collector, the production property lookup and matrix routines with synthetic
models. It checks default-off behavior, identity/positive translations, matching
only the intended model/material/map/camera, balanced commands, unchanged source
model/material data, recorded override metadata, reset behavior, invalid values,
and matrix-buffer bounds. Original capture lifecycle/concurrency checks remain.
No game assets, application context, GPU or simulator are used. Device comparison
is required to establish any relationship to the recorded flicker.
