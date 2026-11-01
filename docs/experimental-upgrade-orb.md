# Experimental partner-upgrade orb mask comparison

Based on build 133. All shadow, leaf, startup and dialogue-frame candidate changes are retained.

## What this tests

The black patch over Goombario coincides with the type-0 energy orb used by
SuperBlock_AnimateEnergyOrbs. Its port rendering path samples a 16x16 intensity
texture for both colour and coverage. Native I8 decoding supplies the intensity
as alpha; an imported replacement can take a different format/alpha path. This
is a hypothesis consistent with an opaque rectangle, not proof that the author's
pack contains invalid alpha. The user's pack is MasterKillua's Paper Mario
Refolded HD; no pack images or bytes have been inspected or uploaded.

This build substitutes a stable copy of the exact native tex_0 I8 mask for the
type-0 orb. It loads the bytes from the user's existing game archive on-device,
validates 16x16 dimensions and 256-byte size, and uses a normal raw-I8 texture
load. Other effect types are untouched. HD characters/scenery remain enabled,
as do the existing orb colour, size, geometry, trail and combiner calculations.
No mask data is embedded in source or the IPA. Missing/invalid native data falls
back to the previous path with a one-time log; successful loading logs once.

This is a targeted compatibility workaround and comparison. A positive result
narrows the problem to the replacement-mask/format/sampling path; it does not
by itself distinguish pack alpha, importer behavior and tile metadata.

## Compare in the same build

Keep Alternate Assets ON. The native mask is enabled by default.
Replay the upgrade from the same pre-upgrade save, then compare these Console commands:

- `set gDeveloper.NativeUpgradeOrbMask 1`: native I8 mask (candidate workaround).
- `set gDeveloper.NativeUpgradeOrbMask 0`: original build-133 orb drawing path.

Close Console before observing/replaying. If the rectangle disappears in mode 1
and returns in mode 0, report that along with whether the glow/trail still looks
correct. Return to mode 1 afterward. This development CVar can be saved by the
Console, so set it explicitly when comparing. Do not disable the entire pack.

In Paperboat.log, a successful mask initialization prints
`[upgrade-orb] Native 16x16 I8 mask ready`. If the log instead says the mask was
unavailable and the previous path was kept, a visual result does not test the
native-mask hypothesis. No private data or texture bytes are logged.

## Validation

The synthetic fixture compiles the production effect patch, inspects the actual
texture load, checks I8 render-tile semantics, all 256 copied bytes and stability
against source mutation. It covers enabled/disabled mode, other effect types,
missing data, wrong size/width and cached reuse. Ordinary and AddressSanitizer
runs pass locally (leak detection disabled for the traced environment).
It does not execute Metal or the user's pack; device comparison is required.

The Linux Ninja helper is `tests/rendering/run-upgrade-orb-mask.py [build] [--sanitize]`.
