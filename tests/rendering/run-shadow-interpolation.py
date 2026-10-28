#!/usr/bin/env python3
"""Inspect production shadow draw commands and interpolation, without game assets or a GPU."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', nargs='?', type=Path, default=root / 'build/ninja-debug')
parser.add_argument('--sanitize', action='store_true')
options = parser.parse_args()
build = options.build.resolve()
commands = json.loads((build / 'compile_commands.json').read_text())
extra = ['-fsanitize=address', '-fno-omit-frame-pointer'] if options.sanitize else []
with tempfile.TemporaryDirectory(prefix='paperreality-shadow-interpolation-') as directory:
    temp = Path(directory)
    objects = []
    for relative, template in [
        ('src/entity_model.c', 'src/entity_model.c'),
        ('src/port/gu.c', 'src/port/gu.c'),
        ('src/port/interpolation/FrameInterpolation.cpp', 'src/port/interpolation/FrameInterpolation.cpp'),
        ('tests/rendering/shadow-matrices.c', 'src/entity_model.c'),
        ('tests/rendering/shadow-interpolation.cpp', 'src/port/interpolation/FrameInterpolation.cpp'),
    ]:
        entry = next(e for e in commands if Path(e['file']) == root / template)
        args = shlex.split(entry['command'])
        obj = temp / (Path(relative).stem + '.o')
        args[args.index('-o') + 1] = str(obj)
        args[args.index('-c') + 1] = str(root / relative)
        subprocess.run(args + ['-ffunction-sections', '-fdata-sections', *extra], cwd=entry['directory'], check=True)
        objects.append(str(obj))
    # Reuse only the existing build's logging libraries, not the application.
    ninja = (build / 'build.ninja').read_text().splitlines()
    start = next(i for i, line in enumerate(ninja) if line.startswith('build Paperboat: '))
    libraries = next(line for line in ninja[start + 1:] if line.strip().startswith('LINK_LIBRARIES ='))
    libraries = list(dict.fromkeys(p for p in shlex.split(libraries.split('=', 1)[1])
                                   if Path(p).name.startswith(('libspdlog.so', 'libfmt.so'))))
    exe = temp / 'shadow-interpolation'
    rpaths = ['-Wl,-rpath,' + str(Path(p).parent) for p in libraries]
    subprocess.run(['c++', *objects, *extra, '-Wl,--gc-sections', *libraries, *rpaths, '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
