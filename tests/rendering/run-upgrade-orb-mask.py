#!/usr/bin/env python3
"""Compile the production orb patch against synthetic masks (Linux Ninja build)."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', nargs='?', type=Path, default=root/'build/ninja-debug')
parser.add_argument('--sanitize', action='store_true')
options = parser.parse_args()
build = options.build.resolve()
commands = json.loads((build/'compile_commands.json').read_text())
entry = next(e for e in commands if Path(e['file']) == root/'src/port/patches/MotionBlurFlamePatches.cpp')
extra = ['-fsanitize=address', '-fno-omit-frame-pointer'] if options.sanitize else []
with tempfile.TemporaryDirectory(prefix='paperreality-upgrade-orb-') as directory:
    temp = Path(directory)
    args = shlex.split(entry['command'])
    obj = temp/'orb.o'
    args[args.index('-o')+1] = str(obj)
    args[args.index('-c')+1] = str(root/'tests/rendering/upgrade-orb-mask.cpp')
    subprocess.run(args + ['-ffunction-sections', '-fdata-sections', *extra],
                   cwd=entry['directory'], check=True)
    exe = temp/'orb-test'
    subprocess.run(['c++', str(obj), *extra, '-Wl,--gc-sections', '-o', str(exe)], check=True)
    for scenario in range(6):
        subprocess.run([str(exe), str(scenario)], check=True)
