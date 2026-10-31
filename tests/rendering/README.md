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

## Leaf capture regression

See [the experimental diagnostic guide](../../docs/leaf-render-diagnostics.md)
for scope and device instructions. Run `run-leaf-trace.py` (optionally with
`--sanitize`) against the existing Linux build. The test uses the real collector
and model-property lookup with synthetic metadata; no GPU or game data is used.
