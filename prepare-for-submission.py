#!/usr/bin/env python3
"""
Class-090 Submission Preparation Script

This script automates the preparation of your Class-090 workshop submission
(Pathfinding & AI - Tower Defense). It will:
1. Create the submission directory
2. Verify all required deliverable files are present
3. Report which files are still missing

Usage:
    python prepare-for-submission.py

Run from the repository root (the directory that contains CPP/ and assets/).
"""

import sys
from pathlib import Path


def print_header(message):
    """Print a formatted header message."""
    print("\n" + "=" * 70)
    print(f"  {message}")
    print("=" * 70)


def print_success(message):
    """Print a success message."""
    print(f"[OK] {message}")


def print_error(message):
    """Print an error message."""
    print(f"[FAIL] ERROR: {message}", file=sys.stderr)


def print_warning(message):
    """Print a warning message."""
    print(f"[!] WARNING: {message}")


def check_directory():
    """Verify we're in the correct directory (repo root with CPP/game)."""
    current_dir = Path.cwd()
    if not (current_dir / "CPP" / "game").exists():
        print_error("This script must be run from the repository root")
        print("Current directory:", current_dir)
        print("\nThe repository root contains the CPP/ and assets/ directories.")
        print("Please cd there, then run:")
        print("  python prepare-for-submission.py")
        return False
    return True


def create_submission_directory():
    """Create the submission directory if it doesn't exist."""
    print_header("Step 1: Creating Submission Directory")

    submission_dir = Path("submission")
    submission_dir.mkdir(parents=True, exist_ok=True)
    print_success(f"Created directory: {submission_dir}")

    return submission_dir


# Required deliverables for Class-090
REQUIRED_FILES = [
    {"name": "exercise1_GameData.json",   "description": "Exercise 1: Modify the Tilemap"},
    {"name": "exercise2_observations.md", "description": "Exercise 2: Explore A* (heuristics, Dijkstra, the frontier)"},
    {"name": "exercise3_observations.md", "description": "Exercise 3: Implement a Slow Tower"},
    {"name": "exercise4_GameData.json",   "description": "Exercise 4: Armored Enemies (data)"},
    {"name": "exercise4_observations.md", "description": "Exercise 4: Armored Enemies (write-up)"},
    {"name": "exercise5_GameData.json",   "description": "Exercise 5: The Mortar Tower (data)"},
    {"name": "exercise5_observations.md", "description": "Exercise 5: The Mortar Tower (write-up)"},
    {"name": "llm-assessment.md",         "description": "LLM Assessment"},
]


def verify_submission(submission_dir):
    """Verify all required files are present."""
    print_header("Step 2: Verifying Submission Files")

    found = 0
    missing = 0

    for req in REQUIRED_FILES:
        filepath = submission_dir / req["name"]
        if filepath.exists():
            size = filepath.stat().st_size
            print_success(f"{req['description']}: {req['name']} ({size} bytes)")
            found += 1
        else:
            print_error(f"Missing: {req['name']} - {req['description']}")
            missing += 1

    # List all files in submission directory
    print("\nAll files in submission directory:")
    if submission_dir.exists():
        for item in sorted(submission_dir.iterdir()):
            if item.is_file():
                print(f"  - {item.name}")

    return found, missing


def print_next_steps():
    """Print instructions for next steps."""
    print_header("Next Steps")

    print("""
Your submission files are ready! Now you need to commit and push to GitHub:

1. Add the submission directory:
   git add submission/

2. Commit with a clear message:
   git commit -m "Add Class-090 workshop submission"

3. Push to your GitHub Classroom repository:
   git push origin main

4. Verify on GitHub:
   - Go to your Class-090 repository on GitHub
   - Navigate to the submission/ directory
   - Verify all files are present

Your submission is complete once pushed to GitHub!
""")


def main():
    """Main execution function."""
    print_header("Class-090 Submission Preparation")
    print("This script will help you prepare your workshop submission.")

    if not check_directory():
        sys.exit(1)

    submission_dir = create_submission_directory()

    found, missing = verify_submission(submission_dir)

    print_header("Summary")
    print(f"  Found:   {found}/{len(REQUIRED_FILES)} files")
    print(f"  Missing: {missing}/{len(REQUIRED_FILES)} files")

    if missing == 0:
        print_success("All required files are present!")
        print_next_steps()
        sys.exit(0)
    else:
        print_warning(f"{missing} required file(s) missing.")
        print("\nPlease add the missing files before submitting.")
        sys.exit(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nAborted by user.")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        sys.exit(1)
