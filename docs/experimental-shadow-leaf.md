# Experimental shadow and leaf build

Based on v1.0.5 (39ba92a71b2083f4fd4dee63515e5c9c583bcdae).

This branch is for device testing. It is not a release and must not publish a release tag.

- Shadow draw matrices now interpolate at their final display-context addresses. Original float samples are retained through shadow copies, avoiding the fractional-height mismatch found in the first experiment. Native packed keyframe copies are preserved.
- The identified ground leaf (MODEL_o853) in kmr_07 is raised by 0.25 world units using the map's existing visual-transform path. The user recalls that offset helping; this is a targeted experimental workaround, not a general foliage fix.
- The old Render Diagnostics collector and controls are not imported. App name, bundle identifier, iPhone/iPad support, UIKit/controller/touch/AirPlay behavior and renderer dependency pins remain from 1.0.5.
- No game files, texture packs, recordings or image captures are included.

## Validation

Local CPU checks compile the release draw, matrix packing and interpolation implementations with synthetic fixtures and stubbed services. Baseline draw-address checks fail, the earlier experiment fails fractional support-plane checks, and the revised fix passes native/intermediate 60/120 FPS alignment, stationary/subpixel/larger motion, sorting, reflections, queued-frame ownership, non-shadow replacement behavior, fractional/sloped precision and cache-reset checks. Ordinary and AddressSanitizer runs pass (leak detection disabled due to traced execution environment). The leaf map script compiles with available headers. These local checks use available workspace headers, not a full iOS SDK build.

The workflow builds and checks the unsigned iOS IPA using the release-pinned dependencies, with an experimental artifact/version suffix. It has no release-publication step and does not pause or clean up unrelated workflows.

The portable CPU runner is tests/rendering/run-shadow-interpolation.py; it expects an existing Linux Ninja build with compile_commands.json and its logging libraries, as in the prior fixture. Run with --sanitize for AddressSanitizer.

## Device comparison

Compare 30, 60 and 120 FPS on the settled switch, during button movement and after jumping. Also check Parakarry/other partner shadows, slopes and reflections, and watch frame pacing. Compare the Goomba Road 3 leaf while standing, moving and wall-running, then reenter the map and restart. Confirm adjacent upright flowers and terrain collision are unchanged. Test existing texture configuration and original assets separately, keeping game files and texture packs local.
