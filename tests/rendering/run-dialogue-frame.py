#!/usr/bin/env python3
"""Inspect production message-frame tile addressing using synthetic assets (Linux Ninja build)."""
import argparse
import json
from pathlib import Path
import re
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
entry = next(e for e in commands if Path(e['file']) == root/'src/msg_draw.c')
source = (root/'src/msg_draw.c').read_text()
match = re.search(r'void msg_draw_frame\([^;]*?\) \{', source)
if not match:
    raise RuntimeError('Production message frame function not found')
depth = 0
for end in range(source.index('{', match.start()), len(source)):
    depth += (source[end] == '{') - (source[end] == '}')
    if depth == 0: break
extra = ['-fsanitize=address', '-fno-omit-frame-pointer'] if options.sanitize else []
with tempfile.TemporaryDirectory(prefix='paperreality-dialogue-frame-') as directory:
    temp = Path(directory)
    (temp/'dialogue-frame-production.inc').write_text(source[match.start():end + 1] + '\n')
    args = shlex.split(entry['command'])
    obj = temp/'frame.o'
    args[args.index('-o')+1] = str(obj)
    args[args.index('-c')+1] = str(root/'tests/rendering/dialogue-frame.c')
    subprocess.run(args + ['-I'+str(temp), '-ffunction-sections', '-fdata-sections', *extra],
                   cwd=entry['directory'], check=True)
    exe = temp/'frame-test'
    subprocess.run(['c++', str(obj), *extra, '-Wl,--gc-sections', '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
