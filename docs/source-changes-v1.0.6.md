# PaperReality 1.0.6 source audit

This patch release promotes the application source tested in build 136
(`341a7a6074dc38bdc66d2212ed1e62772f46640a`). Its baseline is `v1.0.5`
(`39ba92a71b2083f4fd4dee63515e5c9c583bcdae`). Release preparation changes
only the project version, release workflow and release documentation relative
to build 136; application behavior and dependency pins are retained.

## Rendering changes since 1.0.5

- `src/port/gu.c` records float matrix samples before encoding. The interpolation
  recorder retains these samples through shadow copies. `src/entity_model.c`
  records the actual frame-owned matrix addresses submitted for shadow draws,
  including reflected shadows, without changing their packed native keyframes.
- `src/world/area_kmr/kmr_07/main.c` applies a 0.25-unit visual translation to
  `MODEL_o853` for the affected ground leaf. Collision and other foliage are not
  changed. The separate broad leaf experiment and rendertrace controls are not
  merged.
- `GameEngine_GetAspectRatio` handles an uninitialized render aspect using the
  configured render dimensions or native 4:3. A positive initialized aspect
  continues through the existing path. This prevents inverted startup-fade edges.
- `src/msg_draw.c` clamps fixed dialogue-frame texture pieces while preserving
  repetition along stretched edge strips. Geometry, fill and palettes are kept.
- The PM64 binary-v1 importer delegates to the existing FAST loader, then handles
  only grayscale RGBA32 RAW replacements for
  `effects/effect_gfx_motion_blur_flame/tex_0` (root or alt/). It caps alpha at
  intensity using a private buffer, retaining RGB, dimensions, scale factors and
  already lower alpha. Other images and archive/shared-buffer bytes are untouched.
- The native-mask diagnostic fallback remains available through its existing
  development CVar but defaults off. HD alpha correction defaults on. The release
  does not substitute original low-resolution art by default.

## Save Editor

`IsUnusedEditorItem` now classifies ten additional distinct unused badges and
eleven unobtainable duplicate IDs. Together with the eight badges already
classified in earlier releases, the native badge editor has 80 regular rows and
29 unused rows. The duplicate groups retain the normal obtainable counts:
Happy Heart 2, Flower Saver 2, Damage Dodge 2, Power Plus 2, Happy Flower 2,
Defend Plus 1, HP Plus 3 and FP Plus 3. Ownership, equipment and save layout are
unchanged; existing row actions remain available.

## Build and distribution

- CMake project version is 1.0.6. The release workflow retains macOS 15,
  Xcode 16.4 and iPhoneOS SDK 18.5. Release builds use the standard Intel host
  after GitHub failed to acquire an ARM-hosted runner for build 137. The iOS
  output still targets ARM64 devices. Experimental builds keep their prior host.
- The release builds from `release/v1.0.6`, with UI diagnostics disabled. The
  version and notes must match before configuration. A local tag gives CMake the
  release stamp; the remote tag/release is created only after the build and
  package checks pass. Publication refuses a stale source branch or an existing
  release/tag pointing at different source.
- The normal experimental branch remains artifact-only. Release publication is
  restricted to the exact release branch and uses a separate job with
  `contents: write`; the build job keeps read-only repository access.
- App name, bundle ID, iOS/iPadOS 16.3 minimum, device families and Settings HUD
  defaults are retained. libultraship remains
  `4f2fd5398871b92682dad87a5f0b416b9178b18f`; Torch remains
  `72960ca16f4723f96aaf4037b76072c230af8c11`.
- No performance improvement is claimed from the earlier battery/temperature
  investigation. No global filtering, unrelated render experiment or dependency
  update is added for this release. Desktop builds remain paused.

## Validation and limits

Build 136 passed full iOS compilation, UIKit SDK checks, startup and HD alpha
regressions, IPA CRC and iPhone/iPad metadata verification. The source-only badge
audit confirms every duplicate's normal/unused counts. Earlier synthetic shadow,
frame-command and supplied-mask checks passed, including sanitizers where run.
These fixtures contain synthetic inputs; user-supplied image bytes are private.

The user reported the shadow/leaf corrections working, the startup bar gone and
the HD glow improved. Dialogue-frame seams and the badge layout have more limited
device confirmation. The release build reruns its supported checks and verifies
version 1.0.6, identity, minimum OS and both device families before publication.
No simulator investigation is requested by this release task.

Only public application source and port resources are used in CI and the IPA.
Game files, texture packs, uploaded masks and diagnostic captures are excluded.
