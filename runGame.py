#!/usr/bin/env python3
"""
runGame.py - Build and run the game (cross-platform)

Usage:
    python runGame.py [-- ARG1 ARG2 ...]

Arguments after the '--' separator are forwarded to the game executable.
Example:
    python runGame.py -- --seed 42 --verbose
"""

import subprocess
import sys
from pathlib import Path


def parse_game_args(argv: list) -> list:
    """Extract arguments after '--' separator from argv.

    Returns all elements after the first '--' token. If no '--' is present,
    returns an empty list.
    """
    try:
        sep_index = argv.index("--")
        return argv[sep_index + 1:]
    except ValueError:
        return []


def find_game_executable(cpp_path: Path) -> Path:
    """Locate the game executable using the standard search order.

    Search order (first existing path wins):
      1. CPP/build/game/game(.exe)          - single-config generators (Make, Ninja)
      2. CPP/build/game/Debug/game(.exe)    - multi-config Debug (Visual Studio)
      3. CPP/build/game/Release/game(.exe)  - multi-config Release

    Raises SystemExit if the executable is not found at any candidate path.
    """
    exe_suffix = ".exe" if sys.platform == "win32" else ""
    candidates = [
        cpp_path / "build" / "game" / f"game{exe_suffix}",
        cpp_path / "build" / "game" / "Debug" / f"game{exe_suffix}",
        cpp_path / "build" / "game" / "Release" / f"game{exe_suffix}",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    print("\nERROR: Could not find game executable. Searched in:")
    print(f"   {cpp_path / 'build' / 'game'}")
    sys.exit(1)


def main():
    class_path = Path(__file__).parent
    cpp_path = class_path / "CPP"
    game_args = parse_game_args(sys.argv)

    print("=== Configuring build ===")
    subprocess.run(["cmake", "-B", "build", "-S", "."], cwd=cpp_path, check=True)

    print("\n=== Building game ===")
    subprocess.run(
        ["cmake", "--build", "build", "--target", "game"],
        cwd=cpp_path,
        check=True,
    )

    build_path = find_game_executable(cpp_path)

    print("\n=== Running game ===\n")
    subprocess.run([str(build_path)] + game_args, check=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\nFailed: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nGame interrupted by user")
