# Experimental HD partner-upgrade mask correction

Build 135 supersedes build 134's default native-mask substitution. The earlier
shadow, leaf, startup and dialogue-frame changes are retained. Release remains
1.0.5; this is an experimental IPA.

## Evidence and correction

The supplied `tex_0` is a binary-v1 RGBA32 replacement, 128x128 pixels, RAW flag,
horizontal byte scale 32 and vertical pixel scale 8. It is grayscale throughout.
Every pixel has nonzero alpha (minimum 27/255), including 11,582 black RGB pixels.
The corners are black with alpha 27. This provides a concrete mechanism for a
dark rectangular overlay in an effect that uses the mask's alpha as coverage.
Device comparison is still required to establish that it explains the reported
patch. The resource inspected was supplied privately and is not in this repo.

The PM64 binary-v1 texture loader now delegates to the existing FAST loader and
then normalizes only `effects/effect_gfx_motion_blur_flame/tex_0` (root or alt/).
For valid grayscale RGBA32 RAW masks, alpha is capped at the grayscale intensity:
`alpha = min(alpha, red)`. RGB, width, height, byte/pixel scales and texture flags
are unchanged. Already lower alpha is retained. Black RGB therefore has zero
coverage; a white opaque core stays white and opaque. Coloured art, other assets,
other formats and invalid byte counts are untouched.

Normalization happens once on resource loading, before mip generation. It uses
a private pixel-buffer copy; archive files and shared file-buffer bytes are not
edited. No game/pack image data is embedded in source, tests, or the IPA.

## Device test

Keep Refolded HD/Alternate Assets enabled and replay the same partner upgrade.
The HD alpha correction is enabled by default and native substitution is off.
No console commands are needed unless a prior test explicitly saved the native
comparison CVar; if so, use `set gDeveloper.NativeUpgradeOrbMask 0` once.

For comparison, `set gDeveloper.UpgradeOrbHDAlpha 0` disables normalization for
the next resource load; fully close and relaunch the app before replaying.
Restore it with value 1 and relaunch. This loader-time switch does not modify
already-cached textures. `set gDeveloper.NativeUpgradeOrbMask 1` remains the
lower-resolution native fallback and can be returned to value 0.

A successful correction logs `[upgrade-orb] HD intensity coverage corrected
(128x128); RGB and scale retained`. Do not confuse this with build 134's native
mask log.

## Verification

`python3 tests/rendering/run-upgrade-orb-alpha.py` checks synthetic edge/core
pixels, all 65,536 intensity/alpha pairs, unchanged RGB and lower alpha,
idempotence, exact path matching, and rejection without partial edits for coloured
or invalid input. `--sanitize` enables AddressSanitizer/UndefinedBehaviorSanitizer.
Private local tests may inspect the user's supplied mask; it must not be
committed or uploaded. Metal and the real scene still require device validation.
