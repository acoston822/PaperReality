#!/usr/bin/env python3
"""Link the model checks against an existing Linux Ninja debug build (no ROM).
Run under xvfb-run after building Paperboat with compile_commands.json enabled.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'build/ninja-debug'
commands = json.loads((build / 'compile_commands.json').read_text())
command = next(entry for entry in commands if entry['file'].endswith('/src/port/Game.cpp'))
with tempfile.TemporaryDirectory(prefix='paperreality-native-check-') as temp:
    temp = Path(temp)
    obj = temp / 'check.o'
    args = shlex.split(command['command'])
    game = temp / 'game.o'
    original = list(args)
    original[original.index('-o') + 1] = str(game)
    subprocess.run(original + ['-Dmain=paperreality_unused_main'], cwd=command['directory'], check=True)
    args[args.index('-o') + 1] = str(obj)
    args[args.index('-c') + 1] = str(root / 'tests/native-settings/check.cpp')
    subprocess.run(args, cwd=command['directory'], check=True)
    ninja = (build / 'build.ninja').read_text().splitlines()
    index = next(i for i, line in enumerate(ninja) if line.startswith('build Paperboat: '))
    objects = shlex.split(ninja[index].split(': ', 1)[1])[1:]
    objects = objects[:objects.index('|')]
    objects = [str(obj) if entry.endswith('/src/port/Game.cpp.o') else entry for entry in objects]
    objects.append(str(game))
    variables = {}
    for line in ninja[index + 1:]:
        if not line.startswith('  '): break
        name, value = line.strip().split(' = ', 1)
        variables[name] = value
    rsp = temp / 'link.rsp'
    rsp.write_text(' '.join(shlex.quote(entry) for entry in objects) + ' ' + variables.get('LINK_PATH', '') + ' ' + variables['LINK_LIBRARIES'])
    exe = temp / 'native-check'
    link = ['c++'] + shlex.split(variables['FLAGS'] + ' ' + variables['LINK_FLAGS'])
    link = [flag for flag in link if not flag.startswith('-Wl,--dependency-file=')]
    subprocess.run(link + ['@' + str(rsp), '-o', str(exe)], cwd=build, check=True)
    (temp / 'paperboat.o2r').symlink_to(build / 'paperboat.o2r')
    subprocess.run([str(exe)], cwd=temp, env=dict(os.environ, SDL_AUDIODRIVER="dummy", LIBGL_ALWAYS_SOFTWARE="1"), check=True, timeout=60)
