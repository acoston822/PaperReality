# Dialogue frame sampling candidate

Working baseline: build 131 runtime retained on experimental branch at
8bc367e941da6eeb1b8ce220cd7a047c0c21aaf8. The source checkout in
/workspace/PaperReality is older and is not the basis for the edited function.

The reported horizontal seam is visible below the second text line with both
original and replacement textures. The upgrade-message example is consistent
with the inspect frame, whose bottom border is drawn as separate 8x8 tiles over
the patterned interior. The old draw commands wrap both axes for every piece.
This permits filtered samples to read the opposite texture edge across a join.

The candidate clamps both axes on fixed pieces, keeps horizontal repeat on the
stretched top/bottom strips (indices 2 and 13), and keeps vertical repeat on the
stretched side strips (7 and 8). Geometry, fill, palette selection, alpha,
scissors, textures, and global filtering are unchanged.

The fixture compiles the actual extracted msg_draw_frame and inspects its real
GBI tile commands with synthetic image pointers. The baseline fails the join
addressing check; the candidate passes, emitting all sixteen pieces. A synthetic
contrasting texture edge demonstrates the wrapped filter contamination that
clamping prevents. AddressSanitizer passes with leak detection disabled for the
traced environment.

These are command and sampler-contract checks, not GPU pixel validation.
They demonstrate an edge-bleed mechanism that can produce a seam, but do not
prove that mechanism caused these particular screenshots. Test the experimental
candidate in inspect/upgrade, choice and postcard boxes, with original/HD assets,
at 1x/3x scale and with different filter settings. Confirm borders and animated
backgrounds still join correctly while opening/closing. Do not call it a
confirmed visual fix before device comparison. No artwork, game files or
screenshots are included in the source patch or fixture.
