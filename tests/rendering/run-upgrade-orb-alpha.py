#!/usr/bin/env python3
"""Run the synthetic HD orb coverage regression without game or pack files."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true')
options = parser.parse_args()
extra = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if options.sanitize else []
with tempfile.TemporaryDirectory(prefix='paperreality-orb-alpha-') as directory:
    executable = Path(directory)/'orb-alpha'
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', *extra,
                    '-I'+str(root/'src'), str(root/'tests/rendering/upgrade-orb-alpha.cpp'),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
