#!/usr/bin/env python3
"""Compile the production startup aspect getter with synthetic renderer state."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/port/Engine.cpp').read_text()
signature = 'extern "C" float GameEngine_GetAspectRatio(void) {'
start = source.index(signature)
depth = 0
for end in range(source.index('{', start), len(source)):
    depth += (source[end] == '{') - (source[end] == '}')
    if depth == 0:
        break
else:
    raise RuntimeError('Unterminated production aspect getter')
with tempfile.TemporaryDirectory(prefix='paperreality-boot-aspect-') as directory:
    temp = Path(directory)
    (temp / 'boot-aspect-production.inc').write_text(source[start:end + 1] + '\n')
    executable = temp / 'boot-aspect'
    subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), '-std=c++20',
                    '-Wall', '-Wextra', '-Werror', '-I' + str(temp),
                    str(root / 'tests/rendering/boot-aspect.cpp'), '-o', str(executable)],
                   check=True)
    subprocess.run([str(executable)], check=True)
