#!/usr/bin/env python3
"""
runGameTests.py - Build and run game tests (cross-platform)

Usage:
    python runGameTests.py
"""

import subprocess
import sys
from pathlib import Path


def main():
    class_dir = Path(__file__).parent
    cpp_dir = class_dir / "CPP"

    print("=== Configuring build ===")
    subprocess.run(["cmake", "-B", "build", "-S", "."], cwd=cpp_dir, check=True)

    print("\n=== Building project ===")
    subprocess.run(["cmake", "--build", "build"], cwd=cpp_dir, check=True)

    print("\n=== Running Game Tests ===")
    subprocess.run(
        ["ctest", "--test-dir", "build", "-R", "^Game", "--output-on-failure"],
        cwd=cpp_dir,
        check=True,
    )

    print("\n=== Game tests complete! ===")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\nBuild/test failed: {e}")
        sys.exit(1)
