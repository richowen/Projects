# Prepend common Windows MinGW-w64 locations to PATH so `pio test -e native`
# works without requiring a fresh shell after installing a compiler.
# Safe no-op on Linux/macOS (globs simply resolve to nothing).
import os, glob

Import("env")

_candidates = [
    os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*\mingw64\bin"),
    os.path.expandvars(r"%LOCALAPPDATA%\Programs\mingw64\bin"),
    r"C:\msys64\ucrt64\bin",
    r"C:\msys64\mingw64\bin",
    r"C:\mingw64\bin",
]

_extra = []
for pat in _candidates:
    for p in glob.glob(pat):
        if os.path.isdir(p) and p not in _extra:
            _extra.append(p)

if _extra:
    sep = os.pathsep
    env["ENV"]["PATH"] = sep.join(_extra) + sep + env["ENV"].get("PATH", "")
    print("[native_path] prepended: " + ", ".join(_extra))
