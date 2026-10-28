# Window geometry regression

Run against an existing Linux CMake build with `compile_commands.json`:

```sh
python3 tests/rendering/run-window-geometry.py build/ninja-debug
python3 tests/rendering/run-window-geometry.py build/ninja-debug --sanitize
```

The fixture compiles production `draw_box.c` and `gu.c` with the build's compiler
flags. It only emits and inspects commands; no ROM, textures, application context,
GPU, or simulator is used. Texture middleware is replaced with command emitters
and matrix recording is disabled.

It queues two sets of 12 rotating panels before retiring either, then tests
64 panels in one frame and reuse of the retired display context. Checks follow
display-list branches around embedded data, validate all four corners' positions
and texture coordinates, require initialized vertex fields, and verify that later
frames cannot modify the earlier frame's vertices. The original code fails corner
validation; enabling its `SHIFT` corner path exposes the shared-ring overwrite.

This proves the geometry defects and their regression coverage. Device testing
is still required to connect the fix to a particular recorded graphical symptom.

## Shadow interpolation regression

```sh
python3 tests/rendering/run-shadow-interpolation.py build/ninja-debug
python3 tests/rendering/run-shadow-interpolation.py build/ninja-debug --sanitize
```

This compiles production `entity_model.c`, `gu.c` and `FrameInterpolation.cpp`.
Synthetic entity models exercise the actual A/B queued draw callbacks and the
direct E path. The fixture inspects the matrix addresses in emitted commands
and their interpolation replacements. Model display lists and render-mode lists
are empty; no artwork, game data, application context, GPU or simulator is used.

Checks cover reversed object order between frames, normal and reflected shadows,
reflections appearing/disappearing, retained queued-frame contents, and unchanged
non-shadow draw behavior. The interpolation target is stubbed to 60/120 FPS and
replay checks the endpoints and quarter/half/three-quarter fractions. This tests
matrix ownership and interpolation, not actual display pacing or depth testing.
The previous code fails because the emitted shadow matrix has no replacement.
Device testing must still determine whether this resolves the recorded flicker.
