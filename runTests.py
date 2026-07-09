#!/usr/bin/env python3
"""
runTests.py - Build and run all tests (cross-platform)

Runs engine tests and game tests with output on failure.
Timer tests are excluded by default (they are timing-sensitive and flaky on loaded systems).

Usage:
    python runTests.py              # Run all tests except timer tests
    python runTests.py --plusTimer   # Run all tests including timer tests
"""

import argparse
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Build and run tests")
    parser.add_argument(
        "--plusTimer",
        action="store_true",
        help="Also run timer tests (excluded by default due to timing sensitivity)",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    cpp_dir = script_dir / "CPP"

    print("=== Configuring build ===")
    subprocess.run(["cmake", "-B", "build", "-S", "."], cwd=cpp_dir, check=True)

    print("\n=== Building project ===")
    subprocess.run(["cmake", "--build", "build"], cwd=cpp_dir, check=True)

    exclude_args = []
    if not args.plusTimer:
        exclude_args = ["-E", "Timer"]

    print("\n=== Running Engine Tests ===")
    subprocess.run(
        ["ctest", "--test-dir", "build", "-R", "^(Engine|ResourceManager)", "--output-on-failure"]
        + exclude_args,
        cwd=cpp_dir,
        check=True,
    )

    print("\n=== Running Game Tests ===")
    subprocess.run(
        ["ctest", "--test-dir", "build", "-R", "^Game", "--output-on-failure"],
        cwd=cpp_dir,
        check=True,
    )

    if args.plusTimer:
        print("\n=== Running Timer Tests ===")
        subprocess.run(
            ["ctest", "--test-dir", "build", "-R", "Timer", "--output-on-failure"],
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
