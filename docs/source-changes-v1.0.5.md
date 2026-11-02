# PaperReality 1.0.5 source audit

This patch release collects production changes since `v1.0.4`
(`926b73159d9fb3d7c1bf565a1cd2916e4aa565ab`). The release is prepared on
`experimental/uikit-settings`; the leaf experiment remains isolated on
`experimental/leaf-render-diagnostics`.

## Application changes

- `src/draw_box.c` retains rotating-window vertices in their frame-owned display
  list, with an explicit branch over the embedded vertex storage. This replaces
  the shared 21-slot ring that could overwrite vertices while an older queued
  frame still used them. The corner path keeps a fixed vertex base while using
  absolute corner indices, avoiding reads outside the 16-vertex allocation.
  The fix originated in `e1220040a` and is retained unchanged.
- Upstream `6cecd01a7be758ccfdc296e5859726723c070f7c` (#278) changes the Huff N.
  Puff, Fake Bowser and Bandit script buffers from `s32` to `Bytecode` so their
  elements have the pointer width expected by script writes on 64-bit builds.
  `src/main_loop.c` also clears script and worker lists on soft reset. This
  four-file upstream merge introduces no new settings.
- `CMakeLists.txt` sets version 1.0.5. App name, bundle identifier, minimum OS,
  native iPhone/iPad device families and normal Release configuration are kept.
- The shadow interpolation experiment (`d7eafbf02`) remains reverted. Its source
  paths are identical to 1.0.4. Leaf diagnostics and lift controls (`6065198a5`
  and `49ea88354`) are not merged; the authored ground-leaf path is unchanged.
- UIKit, controller navigation, touch controls, AirPlay and Metal frame pacing
  are unchanged from 1.0.4. The Release settings timing HUD remains disabled.
- libultraship stays at `4f2fd5398871b92682dad87a5f0b416b9178b18f`;
  Torch stays at `72960ca16f4723f96aaf4037b76072c230af8c11`.

## Validation

- `tests/rendering/run-window-geometry.py` compiles production `draw_box.c` and
  `gu.c` against a CPU command-inspection fixture. It validates all corners,
  texture coordinates, initialized fields, two queued sets of 12 panels,
  display-context reuse and 64-panel pressure. The ordinary and AddressSanitizer
  runs pass. The fixture uses synthetic inputs and does not render game assets.
- The user reported no recurrence of the save-card symptom in 4:3 and Off modes
  at 60/120 FPS. Intermittence limits the conclusion; the test does not establish
  a fix for the unrelated ground-leaf flicker or every reported upstream issue.
- The unsigned IPA workflow runs UIKit SDK checks and the existing timing,
  pacing, model-portrait and controller regressions, compiles the release and
  verifies its archive and application metadata before publishing.
- Desktop builds remain paused. No simulator check is requested for this
  release. Existing simulator test infrastructure remains available for future
  explicitly requested debugging.

Only public application source and port resources are used in CI and the IPA.
User game files, HD texture packs and diagnostic captures are not included.
