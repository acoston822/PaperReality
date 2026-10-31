#!/usr/bin/env python3
"""Compile the real opt-in leaf collector with synthetic model metadata; no assets/GPU."""
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
commands = json.loads((options.build.resolve() / 'compile_commands.json').read_text())
extra = ['-fsanitize=address', '-fno-omit-frame-pointer'] if options.sanitize else []
with tempfile.TemporaryDirectory(prefix='paperreality-leaf-trace-') as directory:
    temp = Path(directory)
    objects = []
    for relative, template in [
        ('src/model.c', 'src/model.c'),
        ('src/port/gu.c', 'src/port/gu.c'),
        ('src/port/diagnostics/LeafTrace.cpp', 'src/port/Engine.cpp'),
        ('tests/rendering/leaf-trace.cpp', 'src/port/Engine.cpp'),
    ]:
        entry = next(e for e in commands if Path(e['file']) == root / template)
        args = shlex.split(entry['command'])
        obj = temp / (Path(relative).stem + '.o')
        args[args.index('-o') + 1] = str(obj)
        args[args.index('-c') + 1] = str(root / relative)
        subprocess.run(args + ['-DPAPERREALITY_LEAF_DIAGNOSTICS=1', '-ffunction-sections', '-fdata-sections', *extra],
                       cwd=entry['directory'], check=True)
        objects.append(str(obj))
    exe = temp / 'leaf-trace'
    subprocess.run(['c++', *objects, *extra, '-Wl,--gc-sections', '-pthread', '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    disabled = temp / 'disabled.c'
    disabled.write_text('#include "port/diagnostics/LeafTrace.h"\n'
                        'int main(void) { int n=0; LeafTrace_Draw(++n,++n,++n,++n);'
                        'LeafTrace_Queued(++n,++n); LeafTrace_Task(++n,++n,++n);'
                        'LeafTrace_BeginFrame(); LeafTrace_EndFrame();'
                        'n+=LeafTrace_BeginLeafDraw(++n); LeafTrace_EndLeafDraw(++n); return n; }\n')
    subprocess.run(['cc', '-Isrc', str(disabled), '-o', str(temp / 'disabled')], cwd=root, check=True)
    subprocess.run([str(temp / 'disabled')], check=True)
    print('PASS: disabled hooks have no runtime argument evaluation')
