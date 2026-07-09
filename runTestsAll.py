#!/usr/bin/env python3
"""
runTestsAll.py - Build and run all tests including unit and property (cross-platform)

Usage:
    python runTestsAll.py
"""

import subprocess
import sys
from pathlib import Path


def main():
    script_dir = Path(__file__).parent
    cpp_dir = script_dir / "CPP"

    print("=== Configuring build ===")
    subprocess.run(["cmake", "-S", ".", "-B", "build"], cwd=cpp_dir, check=True)

    print("\n=== Building project ===")
    subprocess.run(["cmake", "--build", "build"], cwd=cpp_dir, check=True)

    print("\n=== Running All Tests ===")
    subprocess.run(
        ["ctest", "--test-dir", "build", "--output-on-failure"],
        cwd=cpp_dir,
        check=True,
    )

    print("\n=== All tests complete! ===")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\nBuild/test failed: {e}")
        sys.exit(1)
