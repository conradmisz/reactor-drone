#!/usr/bin/env python3
"""
compileGame.py - Build the game without running (cross-platform)

Usage:
    python compileGame.py
"""

import subprocess
import sys
from pathlib import Path


def main():
    class_dir = Path(__file__).parent
    cpp_dir = class_dir / "CPP"

    print("=== Configuring build ===")
    subprocess.run(["cmake", "-B", "build", "-S", "."], cwd=cpp_dir, check=True)

    print("\n=== Building game ===")
    subprocess.run(
        ["cmake", "--build", "build", "--target", "game"],
        cwd=cpp_dir,
        check=True,
    )

    print("\n=== Build complete ===")
    print("Run the game with: python runGame.py")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[FAIL] Build failed: {e}")
        sys.exit(1)
