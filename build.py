#!/usr/bin/env python3
import subprocess
import sys
import os
import importlib.util
import shutil
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "core" / "build"


def find_pybind11_dir():
    result = subprocess.run(
        [sys.executable, "-m", "pybind11", "--cmakedir"],
        capture_output=True, text=True,
    )
    if result.returncode:
        raise SystemExit(
            "Cannot find pybind11 in the current Python environment. "
            "Install it with python -m pip install pybind11."
        )
    directory = Path(result.stdout.strip()).resolve()
    if not (directory / "pybind11Config.cmake").is_file():
        raise SystemExit(f"pybind11 CMake configuration not found: {directory}")
    return directory


def build():
    pybind11_dir = find_pybind11_dir()
    print(f"pybind11 CMake directory: {pybind11_dir}", flush=True)
    BUILD_DIR.mkdir(exist_ok=True)
    cmake_command = ([sys.executable, "-m", "cmake"]
                     if importlib.util.find_spec("cmake") is not None
                     else [shutil.which("cmake") or "cmake"])
    configure_args = [
        "-S", str(ROOT / "core"),
        "-B", str(BUILD_DIR),
        "-DCMAKE_BUILD_TYPE=Release",
        "-Dpybind11_DIR=" + str(pybind11_dir),
        "-DPYBIND11_FINDPYTHON=ON",
        "-DPython_EXECUTABLE=" + sys.executable,
    ]
    subprocess.run(cmake_command + configure_args, check=True)
    subprocess.run(cmake_command + [
        "--build", str(BUILD_DIR),
        "--parallel",
        os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or str(os.cpu_count() or 1),
    ], check=True)

    so_files = list(BUILD_DIR.glob("**/rwrl_core*.so"))
    if not so_files:
        raise SystemExit("Python extension was not generated; check the CMake output.")

    dst = ROOT / "py"
    for f in so_files:
        target = dst / f.name
        # A replay process may still map the previous module. Replacing the
        # inode preserves that mapping; overwriting it can cause SIGBUS.
        with tempfile.NamedTemporaryFile(dir=dst, delete=False) as staged:
            staged_path = Path(staged.name)
            staged.write(f.read_bytes())
        try:
            os.replace(staged_path, target)
        finally:
            staged_path.unlink(missing_ok=True)
        print(f"Copied: {f.name} -> py/")


if __name__ == "__main__":
    build()
