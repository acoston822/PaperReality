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

## Startup fade test — next experimental build

The user reports the shadow and ground-leaf symptoms appear resolved in build 129.
This build retains those changes and adds a startup aspect guard. Before the
first render pass, the interpreter's cached aspect can be zero; the fullscreen
fade can then calculate inverted edges at the screen centre and draw a narrow
black strip. Use configured target dimensions while the cached aspect is invalid,
or native 4:3 if dimensions are not ready. Initialized finite positive aspects
retain their existing behavior.

The CPU regression compiles the actual getter extracted from Engine.cpp with
synthetic initial state. It checks missing/zero-initialized interpreter, usable
target dimensions, zero/negative/nonfinite cache, unavailable dimensions and
initialized aspects. The pre-fix source fails; the guard passes. The workflow
runs this regression before the iOS build. This is a candidate for the brief
central strip in the supplied clip; it still requires device confirmation.
The separate Dynamic Island at the screen edge is not the reported defect.

Test a cold launch from the Home Screen at both 60 and 120 FPS. Watch the centre
line through the menu and Start buttons during the first splash frames; also
confirm the usual fade and aspect/layout remain correct. No launch storyboard,
status-bar behavior, game artwork, texture packs or recording captures are changed.

## Performance comparison after build 131

The user reports build 129 is smooth in the same areas where 131 drops frames.
An artifact-only audit of both existing IPAs confirmed Xcode 15.4 / SDK 17.5
for 129 and Xcode 16.4 / SDK 18.5 for 131. Both have the high-refresh permission
and Game Mode keys enabled. This does not yet identify a causal code change.

This comparison restores Engine.cpp exactly from build 129 (the only runtime
source file changed in 131) while explicitly retaining Xcode 16.4 / SDK 18.5.
It therefore retains the shadow and leaf fixes but intentionally omits the
startup aspect guard; the brief startup bar may return. The startup regression
is not run because this comparison deliberately restores its failing baseline.
No allocator optimization is included: that cache was shared by smooth build 129.

Use the same FPS/aspect/texture settings and area when comparing 129, 131 and
this build. If this rebuild is smooth, the startup source change becomes the
leading distinction. If it also drops frames, the changed toolchain or another
uncontrolled factor remains relevant. Neither outcome alone is a profile.
