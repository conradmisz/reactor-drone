#!/usr/bin/env python3
"""
run.py - Master command menu for this class

Usage:
    python run.py [-- ARG1 ARG2 ...]

Arguments after '--' are forwarded to the game executable when running.
"""

import getpass
import json
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent
CPP_DIR = SCRIPT_DIR / "CPP"
SCRIPTS_DISTRIBUTED = SCRIPT_DIR / "scripts" / "distributed"
SCRIPTS_LOCAL       = SCRIPT_DIR / "scripts" / "local"

# Game configuration for this class.
GAME_TARGET = "game"
GAME_CTEST_PREFIX = "^Game"
ENGINE_CTEST_PREFIX = "^(Engine|ResourceManager)"

# Valid key names recognized by the game's CLI parser.
VALID_KEYS = [
    "LEFT", "RIGHT", "UP", "DOWN", "SPACE",
    "F1", "F2", "F10", "H", "ESC",
    "PLUS", "MINUS", "W", "A", "S", "D",
]


def parse_game_args(argv: list[str]) -> list[str]:
    """Extract arguments after '--' separator from argv."""
    try:
        sep_index = argv.index("--")
        return argv[sep_index + 1:]
    except ValueError:
        return []


def find_game_executable() -> Path | None:
    """Locate the game executable using the standard search order.

    Returns the Path to the executable if found, or None if not found.
    Search order: direct -> Debug/ -> Release/
    """
    exe_suffix = ".exe" if sys.platform == "win32" else ""
    candidates = [
        CPP_DIR / "build" / "game" / f"game{exe_suffix}",
        CPP_DIR / "build" / "game" / "Debug" / f"game{exe_suffix}",
        CPP_DIR / "build" / "game" / "Release" / f"game{exe_suffix}",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


# Capture game arguments once at startup.
GAME_ARGS = parse_game_args(sys.argv)


def cmake_configure(debug=False):
    cmd = ["cmake", "-B", "build", "-S", "."]
    if debug:
        cmd += ["-DCMAKE_BUILD_TYPE=Debug"]
    else:
        cmd += ["-DCMAKE_BUILD_TYPE=Release"]
    subprocess.run(cmd, cwd=CPP_DIR, check=True)


def cmake_build(target=None):
    cmd = ["cmake", "--build", "build"]
    if target:
        cmd += ["--target", target]
    subprocess.run(cmd, cwd=CPP_DIR, check=True)


def ctest_run(filter_re=None, test_dir="build", extra_args=None, exclude_re=None):
    cmd = ["ctest", "--test-dir", test_dir, "--output-on-failure"]
    if filter_re:
        cmd += ["-R", filter_re]
    if exclude_re:
        cmd += ["-E", exclude_re]
    if extra_args:
        cmd += extra_args
    subprocess.run(cmd, cwd=CPP_DIR, check=True)


def pick(prompt, options):
    """Generic numbered picker. Returns index or None for exit."""
    print()
    print("=" * 50)
    print(f"  {prompt}")
    print("=" * 50)
    print()
    for i, label in enumerate(options, 1):
        print(f"  {i}. {label}")
    print(f"\n  0. Exit\n")
    while True:
        try:
            raw = input("Enter your choice: ").strip()
        except (KeyboardInterrupt, EOFError):
            print()
            return None
        if raw == "0":
            return None
        try:
            idx = int(raw) - 1
            if 0 <= idx < len(options):
                return idx
        except ValueError:
            pass
        print(f"Please enter 1-{len(options)} or 0.")


# ---------------------------------------------------------------------------
# Debug session builder
# ---------------------------------------------------------------------------

def _read_int(prompt, *, allow_empty=False):
    """Read a non-negative integer from the user."""
    while True:
        raw = input(prompt).strip()
        if allow_empty and raw == "":
            return None
        try:
            val = int(raw)
            if val >= 0:
                return val
        except ValueError:
            pass
        print("  Please enter a non-negative integer.")


def _read_positive_int(prompt, *, allow_empty=False):
    """Read a positive integer from the user."""
    while True:
        raw = input(prompt).strip()
        if allow_empty and raw == "":
            return None
        try:
            val = int(raw)
            if val > 0:
                return val
        except ValueError:
            pass
        print("  Please enter a positive integer.")


def _read_signed_int(prompt, *, allow_empty=False):
    """Read any integer (may be negative) from the user."""
    while True:
        raw = input(prompt).strip()
        if allow_empty and raw == "":
            return None
        try:
            return int(raw)
        except ValueError:
            print("  Please enter an integer.")


def _pick_key():
    """Let the user pick a key name from the valid list."""
    print("    Keys:")
    for i, k in enumerate(VALID_KEYS, 1):
        end = "\n" if i % 5 == 0 else "  "
        print(f"  {i:2}. {k:<6}", end=end)
    if len(VALID_KEYS) % 5 != 0:
        print()
    while True:
        raw = input("    Key number (0 to cancel): ").strip()
        if raw == "0":
            return None
        try:
            idx = int(raw) - 1
            if 0 <= idx < len(VALID_KEYS):
                return VALID_KEYS[idx]
        except ValueError:
            pass
        print(f"    Please enter 1-{len(VALID_KEYS)} or 0.")


def _print_timeline(session):
    """Print the current debug session timeline ordered by frame."""
    events = []
    for ka in session["keys"]:
        events.append((ka[0], f"KEY  {ka[1]}"))
    for hv in session["hovers"]:
        events.append((hv[0], f"HOVER ({hv[1]},{hv[2]})"))
    for cl in session["clicks"]:
        events.append((cl[0], f"CLICK ({cl[1]},{cl[2]})"))
    for f in session["dump_frames"]:
        events.append((f, "DUMP"))
    for f in session["trace_frames"]:
        events.append((f, "TRACE"))
    for f in session["screenshot_frames"]:
        events.append((f, "SCREENSHOT"))
    if session["stop_frame"] is not None:
        events.append((session["stop_frame"], "STOP"))

    # Sort by frame, then by type for stable ordering
    type_order = {"KEY": 0, "HOVER": 1, "CLICK": 2, "DUMP": 3, "TRACE": 4, "SCREENSHOT": 5, "STOP": 6}
    events.sort(key=lambda e: (e[0], type_order.get(e[1].split()[0], 99)))

    print()
    print("-" * 50)
    print("  Debug Session Timeline")
    print("-" * 50)

    if not events and not session["paused"] and session["fps"] == 0 and not session["verbose"]:
        print("  (empty - add some actions)")
    else:
        # Global settings
        settings = []
        if session["fps"] > 0:
            settings.append(f"FPS={session['fps']}")
        if session["paused"]:
            settings.append("start PAUSED")
        if session["verbose"]:
            settings.append("VERBOSE")
        if not session["clear_logs"]:
            settings.append("keep old logs")
        if settings:
            print(f"  Settings: {', '.join(settings)}")
            print()

        # Frame events
        if events:
            max_frame = max(e[0] for e in events)
            fw = max(len(str(max_frame)), 5)
            print(f"  {'Frame':<{fw}}  Action")
            print(f"  {'-' * fw}  {'-' * 20}")
            for frame, desc in events:
                print(f"  {frame:<{fw}}  {desc}")
        elif not settings:
            print("  (no frame events yet)")

    print("-" * 50)
    print()


def _build_command(session):
    """Build the game CLI argument list from the session dict."""
    args = []
    if session["keys"]:
        args.append("--keys")
        for frame, key in session["keys"]:
            args.append(f"{frame}:{key}")
    if session["hovers"]:
        args.append("--hover")
        for frame, x, y in session["hovers"]:
            args.append(f"{frame}:{x},{y}")
    if session["clicks"]:
        args.append("--clicks")
        for frame, x, y in session["clicks"]:
            args.append(f"{frame}:{x},{y}")
    if session["dump_frames"]:
        args.append("--dump")
        for f in session["dump_frames"]:
            args.append(str(f))
    if session["trace_frames"]:
        args.append("--trace")
        for f in session["trace_frames"]:
            args.append(str(f))
    if session["screenshot_frames"]:
        args.append("--screenshot")
        for f in session["screenshot_frames"]:
            args.append(str(f))
    if session["stop_frame"] is not None:
        args += ["--stopframe", str(session["stop_frame"])]
    if session["fps"] > 0:
        args += ["--fps", str(session["fps"])]
    if session["paused"]:
        args.append("--paused")
    if session["verbose"]:
        args.append("--verbose")
    if not session["clear_logs"]:
        args.append("--no-clear-logs")
    return args


def _remove_event(session):
    """Let the user remove an event from the session."""
    # Build a flat list of removable items
    items = []
    for i, (frame, key) in enumerate(session["keys"]):
        items.append(("key", i, f"Frame {frame}: KEY {key}"))
    for i, (frame, x, y) in enumerate(session["hovers"]):
        items.append(("hover", i, f"Frame {frame}: HOVER ({x},{y})"))
    for i, (frame, x, y) in enumerate(session["clicks"]):
        items.append(("click", i, f"Frame {frame}: CLICK ({x},{y})"))
    for i, f in enumerate(session["dump_frames"]):
        items.append(("dump", i, f"Frame {f}: DUMP"))
    for i, f in enumerate(session["trace_frames"]):
        items.append(("trace", i, f"Frame {f}: TRACE"))
    for i, f in enumerate(session["screenshot_frames"]):
        items.append(("screenshot", i, f"Frame {f}: SCREENSHOT"))
    if session["stop_frame"] is not None:
        items.append(("stop", 0, f"Frame {session['stop_frame']}: STOP"))

    if not items:
        print("  Nothing to remove.")
        return

    print("  Remove which event?")
    for i, (_, _, desc) in enumerate(items, 1):
        print(f"    {i}. {desc}")
    print(f"    0. Cancel")

    while True:
        raw = input("  Choice: ").strip()
        if raw == "0":
            return
        try:
            idx = int(raw) - 1
            if 0 <= idx < len(items):
                kind, sub_idx, desc = items[idx]
                if kind == "key":
                    session["keys"].pop(sub_idx)
                elif kind == "hover":
                    session["hovers"].pop(sub_idx)
                elif kind == "click":
                    session["clicks"].pop(sub_idx)
                elif kind == "dump":
                    session["dump_frames"].pop(sub_idx)
                elif kind == "trace":
                    session["trace_frames"].pop(sub_idx)
                elif kind == "screenshot":
                    session["screenshot_frames"].pop(sub_idx)
                elif kind == "stop":
                    session["stop_frame"] = None
                print(f"  Removed: {desc}")
                return
        except ValueError:
            pass
        print(f"  Please enter 1-{len(items)} or 0.")


def _save_session_json(session: dict, name: str, dest: Path = None) -> Path:
    """Save session to <dest>/<name>. Returns the path written."""
    if dest is None:
        dest = SCRIPTS_LOCAL
    dest.mkdir(parents=True, exist_ok=True)
    if not name.endswith(".json"):
        name += ".json"
    path = dest / name
    j = {}
    if session["fps"] > 0:               j["fps"]        = session["fps"]
    if session["stop_frame"] is not None: j["stop_frame"] = session["stop_frame"]
    if session["paused"]:                 j["paused"]     = session["paused"]
    if session["verbose"]:                j["verbose"]    = session["verbose"]
    if not session["clear_logs"]:         j["clear_logs"] = session["clear_logs"]
    if session["keys"]:
        j["keys"] = [{"frame": f, "key": k} for f, k in session["keys"]]
    if session["hovers"]:
        j["hover"] = [{"frame": f, "x": x, "y": y} for f, x, y in session["hovers"]]
    if session["clicks"]:
        j["clicks"] = [{"frame": f, "x": x, "y": y} for f, x, y in session["clicks"]]
    if session["dump_frames"]:        j["dump"]       = session["dump_frames"]
    if session["trace_frames"]:       j["trace"]      = session["trace_frames"]
    if session["screenshot_frames"]:  j["screenshot"] = session["screenshot_frames"]
    with open(path, "w") as f:
        json.dump(j, f, indent=2)
        f.write("\n")
    return path


def _load_session_json(path: Path) -> dict | None:
    """Load a JSON script file into a session dict. Returns None on error."""
    try:
        with open(path) as f:
            j = json.load(f)
    except Exception as e:
        print(f"  Error loading {path}: {e}")
        return None
    return {
        "keys":              [(k["frame"], k["key"]) for k in j.get("keys", [])],
        "hovers":            [(h["frame"], h["x"], h["y"]) for h in j.get("hover", [])],
        "clicks":            [(c["frame"], c["x"], c["y"]) for c in j.get("clicks", [])],
        "dump_frames":       j.get("dump", []),
        "trace_frames":      j.get("trace", []),
        "screenshot_frames": j.get("screenshot", []),
        "stop_frame":        j.get("stop_frame", None),
        "fps":               j.get("fps", 0),
        "paused":            j.get("paused", False),
        "verbose":           j.get("verbose", False),
        "clear_logs":        j.get("clear_logs", True),
    }


def _pick_script() -> Path | None:
    """List scripts from distributed/ then local/, let user pick. Returns path or None."""
    entries = []
    dist_files  = sorted(SCRIPTS_DISTRIBUTED.glob("*.json")) if SCRIPTS_DISTRIBUTED.exists() else []
    local_files = sorted(SCRIPTS_LOCAL.glob("*.json"))       if SCRIPTS_LOCAL.exists()       else []
    for p in dist_files:
        entries.append((f"[distributed] {p.name}", p))
    for p in local_files:
        entries.append((f"[local]       {p.name}", p))
    if not entries:
        print("  No scripts found in scripts/distributed/ or scripts/local/")
        return None
    print()
    for i, (label, _) in enumerate(entries, 1):
        print(f"  {i}. {label}")
    print(f"  0. Cancel")
    while True:
        raw = input("  Choice: ").strip()
        if raw == "0":
            return None
        try:
            idx = int(raw) - 1
            if 0 <= idx < len(entries):
                return entries[idx][1]
        except ValueError:
            pass
        print(f"  Please enter 1-{len(entries)} or 0.")


def debug_session_builder():
    """Interactive debug session builder with timeline preview."""
    session = {
        "keys": [],              # list of (frame, key_name)
        "hovers": [],            # list of (frame, x, y) -- --hover cursor position
        "clicks": [],            # list of (frame, x, y) -- --clicks mouse click
        "dump_frames": [],
        "trace_frames": [],
        "screenshot_frames": [],
        "stop_frame": None,
        "fps": 0,
        "paused": False,
        "verbose": False,
        "clear_logs": True,
    }

    menu = [
        "Add Key Press",
        "Add Repeated Key Press",
        "Add Frame Dump",
        "Add Frame Trace",
        "Add Screen Shot",
        "Add Hover",
        "Add Click",
        "Set Stop Frame",
        "Set FPS",
        "Toggle Paused Start",
        "Toggle Verbose",
        "Toggle Clear Logs",
        "Remove Event",
        "Save Session as Script",
        "Load Script",
        "Run Debug Session",
    ]

    while True:
        _print_timeline(session)
        choice = pick("Debug Session Builder", menu)
        if choice is None:
            return  # back to main menu

        try:
            if choice == 0:  # Add Key Press
                frame = _read_int("  Frame number: ")
                if frame is None:
                    continue
                key = _pick_key()
                if key is None:
                    continue
                session["keys"].append((frame, key))
                print(f"  Added: Frame {frame} -> KEY {key}")

            elif choice == 1:  # Add Repeated Key Press
                start = _read_int("  Start frame: ")
                if start is None:
                    continue
                key = _pick_key()
                if key is None:
                    continue
                count = _read_positive_int("  How many presses: ")
                if count is None:
                    continue
                interval = _read_positive_int("  Frames between presses (e.g. 1 = every frame): ")
                if interval is None:
                    continue
                for i in range(count):
                    session["keys"].append((start + i * interval, key))
                end_frame = start + (count - 1) * interval
                print(f"  Added: {count}x {key} from frame {start} to {end_frame} (every {interval} frames)")

            elif choice == 2:  # Add Frame Dump
                frame = _read_int("  Frame number to dump: ")
                if frame is None:
                    continue
                session["dump_frames"].append(frame)
                print(f"  Added: Frame {frame} -> DUMP")

            elif choice == 3:  # Add Frame Trace
                frame = _read_int("  Frame number to trace: ")
                if frame is None:
                    continue
                session["trace_frames"].append(frame)
                print(f"  Added: Frame {frame} -> TRACE")

            elif choice == 4:  # Add Screen Shot
                frame = _read_int("  Frame number to screenshot: ")
                if frame is None:
                    continue
                session["screenshot_frames"].append(frame)
                print(f"  Added: Frame {frame} -> SCREENSHOT")

            elif choice == 5:  # Add Hover
                frame = _read_int("  Frame number: ")
                if frame is None:
                    continue
                x = _read_signed_int("  Cursor world X: ")
                if x is None:
                    continue
                y = _read_signed_int("  Cursor world Y: ")
                if y is None:
                    continue
                session["hovers"].append((frame, x, y))
                print(f"  Added: Frame {frame} -> HOVER ({x},{y})")

            elif choice == 6:  # Add Click
                frame = _read_int("  Frame number: ")
                if frame is None:
                    continue
                x = _read_signed_int("  Click world X: ")
                if x is None:
                    continue
                y = _read_signed_int("  Click world Y: ")
                if y is None:
                    continue
                session["clicks"].append((frame, x, y))
                print(f"  Added: Frame {frame} -> CLICK ({x},{y})")

            elif choice == 7:  # Set Stop Frame
                frame = _read_int("  Stop at frame (empty to clear): ", allow_empty=True)
                session["stop_frame"] = frame
                if frame is not None:
                    print(f"  Stop frame set to {frame}")
                else:
                    print("  Stop frame cleared")

            elif choice == 8:  # Set FPS
                fps = _read_positive_int("  FPS (empty for default 60): ", allow_empty=True)
                session["fps"] = fps if fps else 0
                if fps:
                    print(f"  FPS set to {fps}")
                else:
                    print("  FPS reset to default (60)")

            elif choice == 9:  # Toggle Paused
                session["paused"] = not session["paused"]
                print(f"  Paused start: {'ON' if session['paused'] else 'OFF'}")

            elif choice == 10:  # Toggle Verbose
                session["verbose"] = not session["verbose"]
                print(f"  Verbose: {'ON' if session['verbose'] else 'OFF'}")

            elif choice == 11:  # Toggle Clear Logs
                session["clear_logs"] = not session["clear_logs"]
                print(f"  Clear logs: {'ON' if session['clear_logs'] else 'OFF'}")

            elif choice == 12:  # Remove Event
                _remove_event(session)

            elif choice == 13:  # Save Session as Script
                raw = input("  Save as (name only): ").strip()
                if raw:
                    dest = SCRIPTS_LOCAL
                    if getpass.getuser() == "juancho":
                        ans = input("  Save to [l]ocal or [d]istributed? ").strip().lower()
                        if ans.startswith("d"):
                            dest = SCRIPTS_DISTRIBUTED
                    saved_path = _save_session_json(session, raw, dest)
                    print(f"  Saved: {saved_path.relative_to(SCRIPT_DIR)}")

            elif choice == 14:  # Load Script
                path = _pick_script()
                if path is not None:
                    loaded = _load_session_json(path)
                    if loaded is not None:
                        session.update(loaded)
                        print(f"  Loaded: {path.relative_to(SCRIPT_DIR)}")

            elif choice == 15:  # Run
                args = _build_command(session)

                print("\n  Building game...")
                cmake_configure()
                cmake_build(GAME_TARGET)

                exe_path = find_game_executable()
                if exe_path is None:
                    print("\n  ERROR: Game executable not found.")
                    print(f"  Searched in: {CPP_DIR / 'build' / 'game'}")
                    print("  Build the game first or check your CMake configuration.\n")
                    continue

                full_cmd = [str(exe_path)] + args + GAME_ARGS
                print(f"\n  Command:\n    {' '.join(full_cmd)}\n")
                subprocess.run(full_cmd, cwd=SCRIPT_DIR)
                print()

        except (KeyboardInterrupt, EOFError):
            print("\n")
            continue


def main():
    debug_build = False

    def _build_label():
        return "Debug" if debug_build else "Release"

    actions = [
        "Compile Game",
        "Run Game",
        "Run Game (Debug Session)",
        "Run Tests (Engine + Game, no Timer)",
        "Run Tests (Engine + Game, with Timer)",
        "Run All Tests (no Timer)",
        "Run All Tests (with Timer)",
        "Run Engine Tests",
        "Run Game Tests",
    ]

    while True:
        # Dynamically show the toggle label
        toggle_label = f"Toggle Build Type (current: {_build_label()})"
        display = actions + [toggle_label]

        choice = pick("Actions", display)
        if choice is None:
            break

        print()
        try:
            if choice == len(actions):  # Toggle build type
                debug_build = not debug_build
                print(f"  Build type set to: {_build_label()}")
                continue

            if choice == 0:  # Compile
                cmake_configure(debug=debug_build)
                cmake_build(GAME_TARGET)
                print(f"\nBuild complete ({_build_label()}).")

            elif choice == 1:  # Run Game
                cmd = [sys.executable, str(SCRIPT_DIR / "runGame.py")]
                if GAME_ARGS:
                    cmd += ["--"] + GAME_ARGS
                subprocess.run(cmd, cwd=SCRIPT_DIR)

            elif choice == 2:  # Run Game (Debug Session)
                debug_session_builder()

            elif choice == 3:  # Tests (Engine + Game, no Timer)
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run(ENGINE_CTEST_PREFIX, exclude_re="Timer")
                ctest_run(GAME_CTEST_PREFIX)

            elif choice == 4:  # Tests (Engine + Game, with Timer)
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run(ENGINE_CTEST_PREFIX)
                ctest_run(GAME_CTEST_PREFIX)

            elif choice == 5:  # All Tests (no Timer)
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run(exclude_re="Timer")

            elif choice == 6:  # All Tests (with Timer)
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run()

            elif choice == 7:  # Engine Tests
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run(ENGINE_CTEST_PREFIX)

            elif choice == 8:  # Game-specific Tests
                cmake_configure(debug=debug_build)
                cmake_build()
                ctest_run(GAME_CTEST_PREFIX)

        except subprocess.CalledProcessError:
            pass  # error already printed by subprocess
        except KeyboardInterrupt:
            print("\n\nInterrupted.")


if __name__ == "__main__":
    main()
