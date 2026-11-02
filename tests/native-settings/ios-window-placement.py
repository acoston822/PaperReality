#!/usr/bin/env python3
"""Check iOS window placement with synthetic SDL2 displays; no simulator or assets."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def pkg_config(*args):
    return subprocess.check_output(["pkg-config", *args, "sdl2"], text=True).strip()


archive = Path(pkg_config("--variable=libdir")) / "libSDL2.a"
if not archive.is_file():
    raise SystemExit("The SDL2 static library is needed for synthetic display fixtures.")
flags = [flag for flag in shlex.split(pkg_config("--static", "--libs")) if flag != "-lSDL2"]
with tempfile.TemporaryDirectory(prefix="paperreality-ios-window-") as temp:
    executable = Path(temp) / "window-check"
    subprocess.run([
        "c++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
        *shlex.split(pkg_config("--cflags")),
        "-I" + str(root / "external/libultraship/include"),
        str(root / "tests/native-settings/ios-window-placement.cpp"),
        str(archive), *flags, "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True, timeout=20)
