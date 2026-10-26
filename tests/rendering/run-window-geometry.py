#!/usr/bin/env python3
"""Compile the actual window renderer with a small CPU-only lifetime fixture.

Usage: python3 tests/rendering/run-window-geometry.py [build/ninja-debug]
Requires an existing Linux CMake build with compile_commands.json. No ROM,
textures, application context, GPU or simulator is used.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', nargs='?', type=Path, default=root / 'build/ninja-debug')
parser.add_argument('--sanitize', action='store_true', help='Run with AddressSanitizer')
options = parser.parse_args()
build = options.build.resolve()
extra_flags = ['-fsanitize=address', '-fno-omit-frame-pointer'] if options.sanitize else []
commands = json.loads((build / 'compile_commands.json').read_text())
with tempfile.TemporaryDirectory(prefix='paperreality-window-geometry-') as directory:
    temp = Path(directory)
    objects = []
    for relative in ['src/draw_box.c', 'src/port/gu.c', 'tests/rendering/window-geometry.c']:
        source = root / relative
        template = source if relative.startswith('src/') else root / 'src/draw_box.c'
        entry = next(e for e in commands if Path(e['file']) == template)
        args = shlex.split(entry['command'])
        obj = temp / (source.stem + '.o')
        args[args.index('-o') + 1] = str(obj)
        args[args.index('-c') + 1] = str(source)
        subprocess.run(args + ['-ffunction-sections', '-fdata-sections', *extra_flags], cwd=entry['directory'], check=True)
        objects.append(str(obj))
    exe = temp / 'window-geometry'
    subprocess.run(['cc', *objects, *extra_flags, '-Wl,--gc-sections', '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
