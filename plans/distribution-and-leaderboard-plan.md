# Distribution & Global Leaderboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Reactor Drone as a Windows installer exe (GitHub Releases + itch.io) with one-click in-game updates, plus a global leaderboard (unique usernames, Highest/Cumulative tabs) backed by a Cloudflare Worker + D1.

**Architecture:** MinGW cross-compile from Linux produces the Windows build; Inno Setup produces the installer; a two-job GitHub Actions workflow releases on tag. The game gains a libcurl-based async HTTP wrapper, identity fields on the existing `MetaSave`, two new UI phases (name entry `PHASE_NAME_ENTRY = 6`, leaderboard `PHASE_LEADERBOARD = 7`), and score submission at the existing run-end bank site. The Worker exposes register/score/top/version endpoints over D1.

**Tech Stack:** CMake + MinGW-w64, SDL3/SDL3_image/SDL3_ttf (prebuilt mingw devel tarballs), libcurl (system on Linux, curl.se mingw zip on Windows), Inno Setup 6, GitHub Actions, Cloudflare Workers + D1 (wrangler), Catch2.

## Global Constraints

- Spec: `agentProjectDocs/specs/distribution-and-leaderboard.md`. Repo: `conradmisz/reactor-drone`, branch `feature/distribution` off `master`.
- House gates after EVERY task (from `CLAUDE.md`): warning-free build (only Lua's vendored `tmpnam` allowed), all ctests green (`python runTestsAll.py`), and the replay canary — `SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000` twice, byte-identical summaries.
- House docs (from `CLAUDE.md` / `ai-workflow-rules.md`): read `agentProjectDocs/code-standards.md` before writing code; `agentProjectDocs/ui-context.md` before any UI/menu work; `ENGINE.md` before touching `CPP/engine/` or `main.cpp` frame order. Tasks that add game surface append ONE numbered entry to `agentProjectDocs/decisions.md` (continue after the current head, D193 at branch time) and the final task updates `agentProjectDocs/progress-tracker.md`.
- DETERMINISM (house invariant): nothing network-derived may reach the simulation. Networking is hard-OFF whenever `--stopframe` is set — `net::set_enabled(!opts.stop_frame.has_value())` — so headless/test/canary runs make zero network calls.
- Auth simplification (spec §7): static `X-Game-Key` header, not HMAC — equivalent security since the secret ships in the binary; ~150 fewer lines.
- Game version lives in ONE place: `CPP/game/version.hpp` (`GAME_VERSION "2.0.0"`). Release tag `vX.Y.Z` must equal `GAME_VERSION` (CI enforces).
- Secrets: Worker-side `GAME_KEY` set via `wrangler secret put GAME_KEY`; the same value is committed game-side in `net_config.hpp` (accepted — it ships in the binary anyway).
- All commits end with the standard Co-Authored-By / Claude-Session trailer used on this branch.

---

### Task 1: MinGW toolchain + Windows cross-build

**Files:**
- Create: `CPP/cmake/mingw-w64-x86_64.cmake`
- Create: `CPP/fetch-win-deps.sh`
- Modify: `CPP/CMakeLists.txt` (tests off for cross-build), `CPP/game/CMakeLists.txt` (WIN32 output name), portability fixes as the compiler reveals them
- Test: Wine smoke run (no new ctest; existing Linux ctests + canary must stay green)

**Interfaces:**
- Produces: `CPP/build-win/` containing `ReactorDrone.exe`; `CPP/win-deps/` with SDL3/SDL3_image/SDL3_ttf mingw prebuilts. Configure command used by Task 4's CI:
  `cmake -S CPP -B CPP/build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_PREFIX_PATH=$PWD/CPP/win-deps -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release`

- [ ] **Step 1: Verify host toolchain** (installed by the user pre-plan)

`x86_64-w64-mingw32-g++ --version` (prefer the `-posix` suffixed binary if present; `-v 2>&1 | grep -i thread` must show posix threads — `std::thread`/`std::future` in Task 6 need it) and `wine --version`.

- [ ] **Step 2: Write the toolchain file**

`CPP/cmake/mingw-w64-x86_64.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Keep the runtime self-contained: only SDL/curl DLLs ship, not mingw's.
add_link_options(-static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
```
(If the `-posix` compiler names don't exist on this distro, drop the suffix.)

- [ ] **Step 3: Write the dep-fetch script**

`CPP/fetch-win-deps.sh` (idempotent; check https://github.com/libsdl-org/SDL/releases for the current 3.x patch versions and pin them here — the Linux build's SDL3 version is the floor):
```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p win-deps && cd win-deps
SDL_VER=3.2.20; IMG_VER=3.2.4; TTF_VER=3.2.2   # pin: newest release at implementation time
fetch() { # $1 repo, $2 name, $3 ver
  [ -d "$2-$3" ] && return 0
  curl -fLO "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-devel-$3-mingw.tar.gz"
  tar xf "$2-devel-$3-mingw.tar.gz"
}
fetch SDL SDL3 "$SDL_VER"
fetch SDL_image SDL3_image "$IMG_VER"
fetch SDL_ttf SDL3_ttf "$TTF_VER"
# Flatten the x86_64 sysroots so one CMAKE_PREFIX_PATH covers all three.
for d in SDL3-$SDL_VER SDL3_image-$IMG_VER SDL3_ttf-$TTF_VER; do
  cp -rn "$d/x86_64-w64-mingw32/"* . 2>/dev/null || true
done
echo "win-deps ready"
```
Run: `chmod +x CPP/fetch-win-deps.sh && CPP/fetch-win-deps.sh`. Add `CPP/win-deps/` and `CPP/build-win/` to `.gitignore`.

- [ ] **Step 4: Make CMake cross-aware**

In `CPP/CMakeLists.txt`: wrap the Catch2 `FetchContent` block and every tests `add_subdirectory` in `if(BUILD_TESTING)` (default ON — Linux behavior unchanged; cross-build passes `-DBUILD_TESTING=OFF`). In `CPP/game/CMakeLists.txt` add:
```cmake
if(WIN32)
  set_target_properties(game PROPERTIES OUTPUT_NAME ReactorDrone WIN32_EXECUTABLE TRUE)
endif()
```
(`WIN32_EXECUTABLE` hides the console window; if the executable target is not literally named `game`, apply to the actual target.)

- [ ] **Step 5: Configure + build, fix portability breaks**

Run (from repo root):
```bash
cmake -S CPP -B CPP/build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
  -DCMAKE_PREFIX_PATH=$PWD/CPP/win-deps -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build CPP/build-win -j
```
Expected first-pass breaks and their fixes (apply only what fires):
- `M_PI` undefined → `#define _USE_MATH_DEFINES` before `<cmath>`, or a local `constexpr`.
- POSIX-only headers (`<unistd.h>`, `<sys/...>`) → guard `#ifndef _WIN32` or replace with `std::filesystem`.
- `meta_save_path()` / any path building assuming `/` — `std::filesystem` handles both; verify `saves/` creation works on Windows (`std::filesystem::create_directories` already portable).
- Lua builds fine under mingw; if its `tmpnam` warning escalates, extend the existing `target_compile_options(lua_static PRIVATE -w)`.
Iterate until `ReactorDrone.exe` links.

- [ ] **Step 6: Wine smoke test (headless, deterministic)**

```bash
EXED=$(dirname $(find CPP/build-win -name ReactorDrone.exe)); cp CPP/win-deps/bin/*.dll "$EXED/"
cd "$EXED" && SDL_VIDEODRIVER=dummy wine ReactorDrone.exe --seed 42 --keys 5:SPACE --stopframe 300
```
Expected: clean exit with the same summary line format as the Linux binary. Then a windowed `wine ReactorDrone.exe` (no flags) for a visual once-over: title menu renders, fonts visible (proves SDL3_ttf), start a run.

- [ ] **Step 7: Verify Linux untouched**

`python runTestsAll.py` → all green. Native rebuild warning-free. Replay canary twice → byte-identical.

- [ ] **Step 8: Commit**

```bash
git add CPP/cmake CPP/fetch-win-deps.sh CPP/CMakeLists.txt CPP/game/CMakeLists.txt .gitignore <touched sources>
git commit -m "feat: MinGW cross-compile toolchain, Windows build of ReactorDrone.exe"
```

---

### Task 2: Package script

**Files:**
- Create: `installer/package-win.sh`
- Test: re-run idempotent; staged game runs under Wine from the stage dir

**Interfaces:**
- Consumes: Task 1's `CPP/build-win` + `CPP/win-deps`.
- Produces: `installer/stage/` = `ReactorDrone.exe`, all DLLs from `CPP/win-deps/*/bin`, `assets/` (Task 6's curl DLL is picked up automatically by the same glob).

- [ ] **Step 1: Write the script**

`installer/package-win.sh`:
```bash
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/installer/stage"
EXE="$(find "$ROOT/CPP/build-win" -name ReactorDrone.exe | head -1)"
[ -n "$EXE" ] || { echo "build first (Task 1)"; exit 1; }
rm -rf "$STAGE" && mkdir -p "$STAGE"
cp "$EXE" "$STAGE/"
find "$ROOT/CPP/win-deps" -name '*.dll' -path '*/bin/*' -exec cp {} "$STAGE/" \;
cp -r "$ROOT/assets" "$STAGE/assets"
echo "staged: $(du -sh "$STAGE" | cut -f1)"
```
Add `installer/stage/` to `.gitignore`.

- [ ] **Step 2: Run + verify under Wine**

```bash
chmod +x installer/package-win.sh && installer/package-win.sh
cd installer/stage && SDL_VIDEODRIVER=dummy wine ReactorDrone.exe --seed 42 --stopframe 60
```
Expected: clean run from the stage dir. If assets resolve relative to CWD only, that is acceptable — the installer's shortcut sets `WorkingDir` to `{app}` (Task 3); note which one it was in the report. Also verify a `saves/` dir appears under the stage (or the exe's chosen location) after a run banks — the installer must not require admin to write it (`{app}` under Program Files is NOT writable; if `meta_save_path()` writes next to the exe, flag it in the report as a finding for Task 3's brief — expected resolution: keep game files in `{app}`, move saves to a user-writable dir on Windows, e.g. `SDL_GetPrefPath("conradm", "ReactorDrone")`, smallest change in `meta_save_path()` guarded `#ifdef _WIN32`).

- [ ] **Step 3: Commit**

```bash
git add installer/package-win.sh .gitignore
git commit -m "feat: Windows package staging script"
```

---

### Task 2b: Machine-independent paths on Windows

Inserted after Task 2 discovered the blocker: `CPP/CMakeLists.txt:26` bakes
`CLASS_ROOT_DIR="${CMAKE_SOURCE_DIR}/.."` — the BUILD machine's absolute
source path — into the binary. On any other PC that path does not exist, so
the staged game cannot find `GameData.json`, sidecars, scripts, images or
fonts, and crashes before the title screen. Saves are equally affected, and
`{app}` under Program Files is not user-writable anyway.

**Files:**
- Modify: `CPP/engine/project_paths.hpp` (the whole fix surface — every
  affected call site routes through its two functions)
- Modify: `CPP/game/meta_save.cpp:12`, `CPP/game/run_save.cpp:55,59`,
  `CPP/game/settings_save.hpp:33` (switch from `class_root()` to the new
  writable-data accessor)
- Modify: `ENGINE.md` (house rule: engine changes update it in the SAME commit)
- Test: `CPP/engine/tests/unit/test_project_paths.cpp` (new; pure, no SDL init)

**Interfaces:**
- Produces, in `namespace project_paths`:
  - `std::string assets_dir();` — READ-ONLY game data. Linux/dev: unchanged,
    `CLASS_ROOT_DIR "/assets"`. Windows: `<exe dir>/assets` via
    `SDL_GetBasePath()`, matching Task 2's flat stage layout
    (exe + DLLs + `assets/` as siblings).
  - `std::string user_data_dir();` — NEW. WRITABLE per-user data (saves,
    settings). Linux/dev: `class_root()` (unchanged on-disk behavior —
    `<root>/saves/...`). Windows: `SDL_GetPrefPath("conradm", "ReactorDrone")`.
    Returns a path WITHOUT a trailing separator on both platforms so existing
    `+ "/saves/meta.json"` style concatenation at call sites keeps working —
    `SDL_GetPrefPath` returns a trailing slash, so strip it.
  - `class_root()` stays as-is for anything genuinely about the source tree.

- [ ] **Step 1: Read the house docs first**

`ENGINE.md` (this is an engine change — it must be updated in the same
commit), `agentProjectDocs/code-standards.md`, and
`agentProjectDocs/architecture.md` Invariants.

- [ ] **Step 2: Write the failing test**

`CPP/engine/tests/unit/test_project_paths.cpp` — register it in the engine
tests CMake exactly like its siblings:
```cpp
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "../../project_paths.hpp"

TEST_CASE("assets_dir points at an existing assets directory", "[paths]") {
    REQUIRE(std::filesystem::exists(project_paths::assets_dir()));
}
TEST_CASE("user_data_dir is absolute and has no trailing separator", "[paths]") {
    std::string d = project_paths::user_data_dir();
    REQUIRE_FALSE(d.empty());
    REQUIRE(std::filesystem::path(d).is_absolute());
    REQUIRE(d.back() != '/');
    REQUIRE(d.back() != '\\');
}
TEST_CASE("save paths still land under user_data_dir", "[paths]") {
    // meta_save_path() must be composed from user_data_dir(), not class_root()
    REQUIRE(meta_save_path().rfind(project_paths::user_data_dir(), 0) == 0);
}
```
(The third case needs `#include "../../../game/meta_save.hpp"` — if the
engine test target cannot see game headers, put that one case in the GAME
unit tests instead, next to the existing `test_meta_save.cpp`.)

- [ ] **Step 3: Run to verify it fails** — `python runTestsAll.py`: compile
error, `user_data_dir` undeclared.

- [ ] **Step 4: Implement**

In `project_paths.hpp`, keep the Linux/dev branch byte-identical in behavior
and add the `_WIN32` branch:
```cpp
inline std::string assets_dir() {
#ifdef _WIN32
    // Installed layout is flat: assets/ sits beside the exe (installer/package-win.sh).
    char* base = SDL_GetBasePath();           // may be null if SDL is not initialized
    std::string dir = base ? std::string(base) : std::string("./");
    if (base) SDL_free(base);
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
    return dir + "/assets";
#else
    return std::string(CLASS_ROOT_DIR) + "/assets";
#endif
}

/// Writable per-user data (saves, settings). Never inside {app} on Windows —
/// Program Files is not user-writable.
inline std::string user_data_dir() {
#ifdef _WIN32
    char* pref = SDL_GetPrefPath("conradm", "ReactorDrone");
    std::string dir = pref ? std::string(pref) : std::string("./");
    if (pref) SDL_free(pref);
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
    return dir;
#else
    return class_root();
#endif
}
```
Include `<SDL3/SDL_filesystem.h>` under the `_WIN32` guard only, so the
header stays dependency-free for non-Windows consumers. Then switch the four
save call sites (`meta_save.cpp:12`, `run_save.cpp:55,59`,
`settings_save.hpp:33`) from `class_root()` to `user_data_dir()`.

- [ ] **Step 5: Do NOT touch the test harness's macro use**

`CPP/engine/tests/unit/test_resource_manager.cpp`, `test_sidecar_loader.cpp`,
`test_script_system.cpp`, `test_lua_manager.cpp` and two property-test files
use the raw `CLASS_ROOT_DIR` macro directly to find
`CPP/engine/tests/test_assets`. Leave them alone — they only ever build
natively.

- [ ] **Step 6: House gates (determinism matters here)**

`python runTestsAll.py` green; warning-free build; replay canary twice,
byte-identical. Traps: the resolved path must never be printed to stdout
(it would enter the canary summary), and must not derive from anything that
varies per run. Also verify the Linux "run from any CWD" convenience still
holds: `cd /tmp && ~/…/CPP/build/game/game --seed 42 --stopframe 60`.

- [ ] **Step 7: Prove portability on Windows — the point of the task**

```bash
cmake --build CPP/build-win -j && installer/package-win.sh
# Hide the baked source path so a false green is impossible:
mv assets /tmp/assets-hidden && mv saves /tmp/saves-hidden 2>/dev/null || true
cd installer/stage && SDL_VIDEODRIVER=dummy wine ReactorDrone.exe --seed 42 --keys 5:SPACE --stopframe 600
```
Expected: runs to completion from the stage dir alone. Then a windowed
`wine ReactorDrone.exe --seed 42 --stopframe 400 --screenshot 200` and view
the BMP — the title menu and fonts must render (fonts come through the same
`assets_dir()` path). Verify a save file appears under Wine's prefpath
(`~/.wine/drive_c/users/$USER/AppData/Roaming/conradm/ReactorDrone/`) after a
run banks. Restore: `mv /tmp/assets-hidden assets && mv /tmp/saves-hidden saves`.

- [ ] **Step 8: Commit** (engine change → `ENGINE.md` in the same commit; add
a `decisions.md` entry at the next free id for the read-only-vs-writable path
split)

```bash
git add CPP/engine/project_paths.hpp CPP/game/meta_save.cpp CPP/game/run_save.cpp \
        CPP/game/settings_save.hpp CPP/engine/tests ENGINE.md agentProjectDocs/decisions.md
git commit -m "fix: resolve assets beside the exe and saves in prefpath on Windows"
```

---

### Task 3: Inno Setup installer script

**Files:**
- Create: `installer/reactor-drone.iss`, `CPP/game/version.hpp`
- Modify (if Task 2 flagged it): `CPP/game/meta_save.cpp` (`meta_save_path()` → `SDL_GetPrefPath` on Windows)
- Test: compiled + exercised in CI (Task 4); manual install/update/uninstall walkthrough on real Windows before first release

**Interfaces:**
- Consumes: `installer/stage/` (Task 2).
- Produces: `ReactorDrone-Setup-<version>.exe`. Silent-update contract for Task 10: `setup.exe /SILENT /NORESTART` upgrades in place and relaunches the game.
- Produces: `CPP/game/version.hpp` → `#define GAME_VERSION "2.0.0"` (consumed by Tasks 4 and 10).

- [ ] **Step 1: Create the version header**

```cpp
#pragma once
#define GAME_VERSION "2.0.0"
```

- [ ] **Step 2: Write the Inno script**

`installer/reactor-drone.iss` (`MyAppVersion` overridden by CI via `/DMyAppVersion=...`):
```ini
#ifndef MyAppVersion
  #define MyAppVersion "2.0.0"
#endif
[Setup]
AppId={{B7E6A4D2-9C41-4B7A-8F2E-REACTORDRONE1}
AppName=Reactor Drone
AppVersion={#MyAppVersion}
AppPublisher=Conrad Miszczak
DefaultDirName={autopf}\Reactor Drone
DefaultGroupName=Reactor Drone
OutputBaseFilename=ReactorDrone-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\ReactorDrone.exe

[Files]
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\Reactor Drone"; Filename: "{app}\ReactorDrone.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall Reactor Drone"; Filename: "{uninstallexe}"

[Run]
; Interactive install: offer launch checkbox.
Filename: "{app}\ReactorDrone.exe"; Description: "Launch Reactor Drone"; \
  WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
; Silent update (Task 10): relaunch the game automatically.
Filename: "{app}\ReactorDrone.exe"; WorkingDir: "{app}"; Flags: nowait; Check: WizardSilent
```

- [ ] **Step 3: (superseded)** The saves/assets location fix moved to Task 2b, which runs before this task. Nothing to do here.

- [ ] **Step 4: Commit**

```bash
git add installer/reactor-drone.iss CPP/game/version.hpp <meta_save.cpp if touched>
git commit -m "feat: Inno Setup installer script + single-source version header"
```

---

### Task 4: CI release workflow (GitHub Releases + itch.io)

**Files:**
- Create: `.github/workflows/release.yml`
- Test: push tag `v2.0.0-rc1`; workflow green end-to-end, draft release holds the artifacts

**Interfaces:**
- Consumes: Tasks 1–3 (configure command, package script, .iss).
- Produces: on tag `v*`: GitHub Release with `ReactorDrone-Setup-<ver>.exe` + `ReactorDrone-win64.zip`; itch.io push. Installer URL pattern consumed by Task 5's `/version`: `https://github.com/conradmisz/reactor-drone/releases/download/v<ver>/ReactorDrone-Setup-<ver>.exe`.

- [ ] **Step 1: itch.io page + secrets** (manual, user does this)

Create the itch.io game page (slug recorded in the workflow), generate an API key, add repo secret `BUTLER_API_KEY`. Push `feature/distribution` to origin first.

- [ ] **Step 2: Write the workflow**

`.github/workflows/release.yml`:
```yaml
name: release
on:
  push:
    tags: ['v*']
jobs:
  build-win:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y g++-mingw-w64-x86-64-posix
      - run: CPP/fetch-win-deps.sh
      - run: |
          cmake -S CPP -B CPP/build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
            -DCMAKE_PREFIX_PATH=$PWD/CPP/win-deps -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
          cmake --build CPP/build-win -j
      - run: installer/package-win.sh
      - uses: actions/upload-artifact@v4
        with: { name: stage, path: installer/stage }
  release:
    needs: build-win
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with: { name: stage, path: installer/stage }
      - name: Version from tag
        shell: bash
        run: echo "VER=${GITHUB_REF_NAME#v}" >> "$GITHUB_ENV"
      - name: Tag matches version.hpp (rc suffix allowed)
        shell: bash
        run: grep -q "\"${VER%%-*}\"" CPP/game/version.hpp
      - run: choco install innosetup -y
      - name: Build installer
        shell: bash
        run: '"/c/Program Files (x86)/Inno Setup 6/ISCC.exe" -DMyAppVersion=$VER installer/reactor-drone.iss'
      - name: Zip portable build
        shell: bash
        run: cd installer/stage && 7z a ../../ReactorDrone-win64.zip .
      - name: GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          draft: true
          files: |
            installer/Output/ReactorDrone-Setup-*.exe
            ReactorDrone-win64.zip
      - name: Push to itch.io
        shell: bash
        env: { BUTLER_API_KEY: '${{ secrets.BUTLER_API_KEY }}' }
        run: |
          curl -fL https://broth.itch.zone/butler/windows-amd64/LATEST/archive/default -o butler.zip
          7z x butler.zip
          ./butler.exe push installer/stage <ITCH_USER>/reactor-drone:windows --userversion "$VER"
```
Replace `<ITCH_USER>/reactor-drone` with the real slug from Step 1.

- [ ] **Step 3: Exercise it**

```bash
git add .github/workflows/release.yml && git commit -m "feat: tag-triggered Windows release workflow (GH Releases + itch.io)"
git push -u origin feature/distribution
git tag v2.0.0-rc1 && git push origin v2.0.0-rc1
```
`gh run watch` until green: draft release holds installer + zip, itch page shows the build. Fix-forward until green; then delete the rc tag and draft release.

---

### Task 5: Leaderboard backend (Cloudflare Worker + D1)

**Files:**
- Create: `backend/wrangler.toml`, `backend/schema.sql`, `backend/src/worker.js`, `backend/test.sh`
- Test: `backend/test.sh` against the deployed Worker

**Interfaces:**
- Produces (consumed by Tasks 6–10; BASE = deployed `https://<name>.<account>.workers.dev`):
  - `POST /register` body `{"player_id":"<uuid>","name":"str"}` → `200 {"ok":true}` | `409 {"error":"name_taken"}` | `400`
  - `POST /score` header `X-Game-Key` body `{"player_id":"<uuid>","score":int}` → `200` | `401` | `400`
  - `GET /top?mode=high|total` → `200 {"rows":[{"name":"str","score":int}, ...]}` (≤20, desc)
  - `GET /version` → `200 {"version":"2.0.0","installer_url":"https://github.com/conradmisz/reactor-drone/releases/download/v2.0.0/ReactorDrone-Setup-2.0.0.exe"}`

- [ ] **Step 1: Scaffold + schema** (user does the `wrangler login` browser step when prompted)

```bash
mkdir -p backend/src && cd backend
npx wrangler d1 create reactor-drone-db
```
`backend/schema.sql`:
```sql
CREATE TABLE IF NOT EXISTS players (
  id   TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE COLLATE NOCASE
);
CREATE TABLE IF NOT EXISTS scores (
  player_id TEXT NOT NULL REFERENCES players(id),
  score     INTEGER NOT NULL,
  ts        INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_scores_player ON scores(player_id);
```
`backend/wrangler.toml` (paste the `database_id` printed by `d1 create`):
```toml
name = "reactor-drone-api"
main = "src/worker.js"
compatibility_date = "2026-08-11"
[[d1_databases]]
binding = "DB"
database_name = "reactor-drone-db"
database_id = "<from d1 create>"
[vars]
RELEASE_VERSION = "2.0.0"
INSTALLER_URL = "https://github.com/conradmisz/reactor-drone/releases/download/v2.0.0/ReactorDrone-Setup-2.0.0.exe"
```
Apply schema: `npx wrangler d1 execute reactor-drone-db --remote --file schema.sql`

- [ ] **Step 2: Write the Worker**

`backend/src/worker.js`:
```js
const json = (obj, status = 200) =>
  new Response(JSON.stringify(obj), { status, headers: { 'content-type': 'application/json' } });

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    try {
      if (req.method === 'GET' && url.pathname === '/version')
        return json({ version: env.RELEASE_VERSION, installer_url: env.INSTALLER_URL });

      if (req.method === 'GET' && url.pathname === '/top') {
        const mode = url.searchParams.get('mode') === 'total' ? 'total' : 'high';
        const agg = mode === 'total' ? 'SUM(s.score)' : 'MAX(s.score)';
        const { results } = await env.DB.prepare(
          `SELECT p.name AS name, ${agg} AS score FROM scores s JOIN players p ON p.id = s.player_id
           GROUP BY s.player_id ORDER BY score DESC LIMIT 20`).all();
        return json({ rows: results });
      }

      if (req.method === 'POST' && url.pathname === '/register') {
        const { player_id, name } = await req.json();
        if (!player_id || !name || name.length > 24 || !/^[\x20-\x7e]+$/.test(name)) return json({ error: 'bad_request' }, 400);
        try {
          await env.DB.prepare(
            'INSERT INTO players (id, name) VALUES (?1, ?2) ON CONFLICT(id) DO UPDATE SET name = ?2')
            .bind(player_id, name.trim()).run();
        } catch (e) {
          if (String(e).includes('UNIQUE')) return json({ error: 'name_taken' }, 409);
          throw e;
        }
        return json({ ok: true });
      }

      if (req.method === 'POST' && url.pathname === '/score') {
        if (req.headers.get('X-Game-Key') !== env.GAME_KEY) return json({ error: 'unauthorized' }, 401);
        const { player_id, score } = await req.json();
        if (!player_id || !Number.isInteger(score) || score < 0 || score > 10_000_000) return json({ error: 'bad_request' }, 400);
        const player = await env.DB.prepare('SELECT id FROM players WHERE id = ?1').bind(player_id).first();
        if (!player) return json({ error: 'unknown_player' }, 400);
        await env.DB.prepare('INSERT INTO scores (player_id, score) VALUES (?1, ?2)').bind(player_id, score).run();
        return json({ ok: true });
      }

      return json({ error: 'not_found' }, 404);
    } catch {
      return json({ error: 'bad_request' }, 400);
    }
  }
};
```

- [ ] **Step 3: Deploy + set secret**

```bash
cd backend
openssl rand -hex 16   # GAME_KEY; also goes into net_config.hpp (Task 6)
npx wrangler secret put GAME_KEY
npx wrangler deploy    # record the BASE url for Task 6
```

- [ ] **Step 4: Write + run endpoint tests**

`backend/test.sh`:
```bash
#!/usr/bin/env bash
set -euo pipefail
BASE="${BASE:?}" KEY="${KEY:?}"
c() { curl -s -o /dev/null -w '%{http_code}' "$@"; }
J='content-type: application/json'
U1=11111111-aaaa-bbbb-cccc-000000000001 U2=11111111-aaaa-bbbb-cccc-000000000002
[ "$(c -XPOST "$BASE/register" -H "$J" -d "{\"player_id\":\"$U1\",\"name\":\"testconrad\"}")" = 200 ]
[ "$(c -XPOST "$BASE/register" -H "$J" -d "{\"player_id\":\"$U2\",\"name\":\"TESTCONRAD\"}")" = 409 ]  # case-insensitive unique
[ "$(c -XPOST "$BASE/score" -H "$J" -d "{\"player_id\":\"$U1\",\"score\":100}")" = 401 ]               # no key
[ "$(c -XPOST "$BASE/score" -H "$J" -H "X-Game-Key: $KEY" -d "{\"player_id\":\"$U1\",\"score\":100}")" = 200 ]
[ "$(c -XPOST "$BASE/score" -H "$J" -H "X-Game-Key: $KEY" -d "{\"player_id\":\"$U1\",\"score\":250}")" = 200 ]
curl -s "$BASE/top?mode=high"  | grep -q '"score":250'
curl -s "$BASE/top?mode=total" | grep -q '"score":350'
curl -s "$BASE/version" | grep -q '"version"'
echo ALL PASS
```
Run: `BASE=https://... KEY=... backend/test.sh` → `ALL PASS`. Clean up:
`npx wrangler d1 execute reactor-drone-db --remote --command "DELETE FROM scores; DELETE FROM players"`

- [ ] **Step 5: Commit**

```bash
git add backend
git commit -m "feat: leaderboard backend (CF Worker + D1): register/score/top/version"
```

---

### Task 6: Async HTTP client in the game

**Files:**
- Create: `CPP/game/net/http_client.hpp`, `CPP/game/net/http_client.cpp`, `CPP/game/net/net_config.hpp`
- Modify: `CPP/game/CMakeLists.txt` (link curl), `CPP/fetch-win-deps.sh` (curl mingw zip), `CPP/game/main.cpp` (one line: enable/disable)
- Test: `CPP/game/tests/unit/test_http_client.cpp` (Catch2, `file://` URL — no network)

**Interfaces:**
- Produces (consumed by Tasks 7–10):
```cpp
namespace net {
struct Response { long status = 0; std::string body; bool ok() const { return status >= 200 && status < 300; } };
// Fire-and-poll: returns a future immediately; never blocks the game loop.
std::future<Response> get(const std::string& url);
std::future<Response> post_json(const std::string& url, const std::string& json_body,
                                const std::string& game_key = "");  // sets X-Game-Key when non-empty
bool enabled();          // false in headless mode -> get/post return status 0 instantly
void set_enabled(bool);  // main.cpp: net::set_enabled(!opts.stop_frame.has_value());
}
```
- `net_config.hpp`: `constexpr const char* NET_BASE = "<worker url>"; constexpr const char* NET_GAME_KEY = "<GAME_KEY from Task 5>";`

- [ ] **Step 1: Add curl to both builds**

Linux (already present or: `sudo apt install libcurl4-openssl-dev` — if missing, report NEEDS_CONTEXT rather than sudo-ing). `CPP/game/CMakeLists.txt`:
```cmake
find_package(CURL REQUIRED)
target_link_libraries(game PRIVATE CURL::libcurl)
```
(also link the tests target if it compiles `http_client.cpp`). Windows — append to `CPP/fetch-win-deps.sh` (check https://curl.se/windows/ for current version):
```bash
CURL_VER=8.16.0_1
if [ ! -d "curl-mingw" ]; then
  curl -fLO "https://curl.se/windows/dl-$CURL_VER/curl-$CURL_VER-win64-mingw.zip"
  unzip -q "curl-$CURL_VER-win64-mingw.zip" -d curl-tmp
  mv curl-tmp/curl-* curl-mingw && rm -r curl-tmp
  cp -rn curl-mingw/include/* include/ && cp -rn curl-mingw/lib/* lib/ && cp -n curl-mingw/bin/*.dll bin/
fi
```
Re-run the script; re-run the Task 1 cross-configure to confirm `find_package(CURL)` resolves from `win-deps`.

- [ ] **Step 2: Write the failing test**

`CPP/game/tests/unit/test_http_client.cpp` (register in the tests CMake exactly like the sibling unit tests):
```cpp
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "../../net/http_client.hpp"

TEST_CASE("disabled client short-circuits with status 0", "[net]") {
    net::set_enabled(false);
    auto f = net::get("https://example.invalid/x");
    REQUIRE(f.get().status == 0);
}

TEST_CASE("get fetches a local file url", "[net]") {
    net::set_enabled(true);
    std::ofstream("/tmp/rd_http_test.txt") << "hello-net";
    auto f = net::get("file:///tmp/rd_http_test.txt");
    auto r = f.get();
    REQUIRE(r.body == "hello-net");
    net::set_enabled(false);
}
```

- [ ] **Step 3: Run to verify failure** — `python runGameTests.py`: compile error (no http_client.hpp).

- [ ] **Step 4: Implement**

`CPP/game/net/http_client.hpp`: the interface block above (`#pragma once`, `<future>`, `<string>`).
`CPP/game/net/http_client.cpp`:
```cpp
#include "http_client.hpp"
#include <atomic>
#include <curl/curl.h>

namespace net {
namespace {
std::atomic<bool> g_enabled{false};
size_t write_cb(char* p, size_t sz, size_t nm, void* out) {
    static_cast<std::string*>(out)->append(p, sz * nm);
    return sz * nm;
}
Response run(const std::string& url, const std::string* body, const std::string& key) {
    Response r;
    CURL* c = curl_easy_init();
    if (!c) return r;
    curl_slist* hdrs = nullptr;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    if (body) {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
        if (!key.empty()) hdrs = curl_slist_append(hdrs, ("X-Game-Key: " + key).c_str());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body->c_str());
    }
    if (curl_easy_perform(c) == CURLE_OK) {
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
        if (r.status == 0 && !r.body.empty()) r.status = 200;  // file:// has no HTTP status
    }
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return r;
}
}  // namespace

bool enabled() { return g_enabled.load(); }
void set_enabled(bool on) {
    static bool inited = false;
    if (on && !inited) { curl_global_init(CURL_GLOBAL_DEFAULT); inited = true; }
    g_enabled.store(on);
}
std::future<Response> get(const std::string& url) {
    if (!enabled()) { std::promise<Response> p; p.set_value({}); return p.get_future(); }
    return std::async(std::launch::async, [url] { return run(url, nullptr, ""); });
}
std::future<Response> post_json(const std::string& url, const std::string& body, const std::string& key) {
    if (!enabled()) { std::promise<Response> p; p.set_value({}); return p.get_future(); }
    return std::async(std::launch::async, [url, body, key] { return run(url, &body, key); });
}
}  // namespace net
```
`net_config.hpp`: the two constants (from Task 5). Add `net/http_client.cpp` to the game target sources. In `main.cpp`, right after CLI parsing: `net::set_enabled(!opts.stop_frame.has_value());`

- [ ] **Step 5: House gates** — `python runTestsAll.py` all green (existing + new), warning-free build, canary twice byte-identical.

- [ ] **Step 6: Cross-build + commit**

Rebuild `CPP/build-win` (curl must link). Then:
```bash
git add CPP/game/net CPP/game/tests/unit/test_http_client.cpp CPP/game/CMakeLists.txt CPP/game/tests/CMakeLists.txt CPP/fetch-win-deps.sh CPP/game/main.cpp
git commit -m "feat: async libcurl HTTP client, networking disabled in headless mode"
```

---

### Task 7: Player identity on MetaSave + username registration UI

**Files:**
- Modify: `CPP/game/meta_save.hpp` / `meta_save.cpp` (identity fields + `generate_uuid`), `CPP/game/main.cpp` (`PHASE_NAME_ENTRY = 6`, text input, flow), UI rendering site per `agentProjectDocs/ui-context.md` (likely `game_hud_system.cpp` or the menu system — follow the house pattern for the existing title menu)
- Test: extend `CPP/game/tests/unit/test_meta_save.cpp`

**Interfaces:**
- Consumes: `net::post_json`, `NET_BASE`, `POST /register` contract; existing `MetaSave` load/write (already garbage-tolerant).
- Produces (consumed by Tasks 8–9): on `MetaSave`: `std::string player_id;` (UUID, generated at first startup and persisted immediately), `std::string player_name;`, `bool registered = false;`. Free function in `meta_save.hpp`: `std::string generate_uuid();` (16 `std::random_device` bytes, hex 8-4-4-4-12). Phase flow: unregistered + online → `PHASE_NAME_ENTRY = 6` before title; registered or offline/headless → straight to title (headless NEVER enters the phase — determinism).

- [ ] **Step 1: Failing tests** (append to `test_meta_save.cpp`)

```cpp
TEST_CASE("identity fields round-trip through meta save", "[meta]") {
    MetaSave m;
    m.player_id = "abc-123"; m.player_name = "Conrad"; m.registered = true;
    std::string path = "/tmp/rd_meta_test.json";
    REQUIRE(meta_write(path, m));
    MetaSave q = meta_load(path);
    REQUIRE(q.player_id == "abc-123");
    REQUIRE(q.player_name == "Conrad");
    REQUIRE(q.registered);
}
TEST_CASE("old save without identity fields loads with defaults", "[meta]") {
    std::string path = "/tmp/rd_meta_old.json";
    std::ofstream(path) << "{\"lifetime_score\": 500}";
    MetaSave q = meta_load(path);
    REQUIRE(q.lifetime_score == 500);
    REQUIRE(q.player_id.empty());
    REQUIRE_FALSE(q.registered);
}
TEST_CASE("uuid shape", "[meta]") {
    std::string u = generate_uuid();
    REQUIRE(u.size() == 36);
    REQUIRE(u[8] == '-'); REQUIRE(u[13] == '-'); REQUIRE(u[18] == '-'); REQUIRE(u[23] == '-');
    REQUIRE(u != generate_uuid());
}
```

- [ ] **Step 2: Run to verify failure** — `python runGameTests.py`: compile errors (no such fields).

- [ ] **Step 3: Implement** — add the three fields to `MetaSave`, serialize/deserialize them in `meta_save.cpp` alongside the existing fields (missing keys → defaults, matching the existing tolerance), implement `generate_uuid()` there. Follow the DETERMINISM doc-comment: identity never reaches the simulation.

- [ ] **Step 4: Tests green, then commit** — `git commit -m "feat: player identity (uuid + name) on MetaSave"`.

- [ ] **Step 5: Name-entry phase in main.cpp** (read `ui-context.md` + the existing title-menu code first; render in the same style/site as existing menus)

- `PHASE_NAME_ENTRY = 6` in the Phase enum.
- Startup: after `meta_load`, if `meta.player_id.empty()` → `meta.player_id = generate_uuid(); meta_write(...)`. Initial phase: `(!meta.registered && net::enabled()) ? PHASE_NAME_ENTRY : PHASE_TITLE`.
- While in the phase: `SDL_StartTextInput(window)` on entry / `SDL_StopTextInput` on exit; `SDL_EVENT_TEXT_INPUT` appends `event.text.text` (cap 24, printable ASCII), BACKSPACE pops, RETURN submits, ESC skips to title unregistered (retry next launch).
- Submit: `pending_register = net::post_json(std::string(NET_BASE) + "/register", nlohmann::json{{"player_id", meta.player_id}, {"name", name_buf}}.dump());` poll `wait_for(0ms)` per frame; 200 → `meta.player_name = name_buf; meta.registered = true; meta_write(...)`, to title; 409 → error line "Name taken — try another"; other → "Offline — ESC to skip".
- Rename (spec: renameable): key `N` on the title screen re-enters the phase pre-filled with the current name; same submit path (server upserts on player_id); ESC cancels keeping the old name. Add `N` to the title hints.
- Render: title "CHOOSE YOUR PILOT NAME", live buffer with trailing `_`, error/hint lines — in the house menu style.

- [ ] **Step 6: House gates + manual verify**

`python runTestsAll.py`, warning check, canary twice (headless skips the phase — must be byte-identical to pre-task). Manual: first launch (move `saves/meta.json` aside) → name entry → register → relaunch skips it; duplicate name → 409 message; `N` renames.

- [ ] **Step 7: Commit + decisions entry**

Append the D-entry (next number after current head) to `agentProjectDocs/decisions.md`: identity-on-MetaSave, unique-name registration, ESC-skip semantics. `git commit -m "feat: first-launch pilot-name registration with unique-name retry"`.

---

### Task 8: Score submission at run end

**Files:**
- Modify: `CPP/game/main.cpp` (submit where scores bank), HUD/gameover rendering site (status line)
- Test: manual (network path); house gates for regression

**Interfaces:**
- Consumes: `net::post_json` + `NET_GAME_KEY`, `MetaSave` identity (Task 7), `POST /score`, the existing run-end bank site (`meta.lifetime_score += bb.get_or<int>("score", 0)` near main.cpp:839).

- [ ] **Step 1: Submit once per run**

At the run-end bank site (the same place `lifetime_score` accrues — NOT the mid-run bank at ~line 1377), if `net::enabled() && meta.registered`:
```cpp
pending_score = net::post_json(std::string(NET_BASE) + "/score",
    nlohmann::json{{"player_id", meta.player_id}, {"score", bb.get_or<int>("score", 0)}}.dump(),
    NET_GAME_KEY);
```
Set a status string ("Submitting score...") on the blackboard; poll per frame → "Score submitted!" (2xx) / "Score not submitted (offline)". Starting a new run clears it. Submission happens on the transition edge exactly once per run.

- [ ] **Step 2: Render** the status line on the gameover/victory screen in the house style.

- [ ] **Step 3: Verify** — manual run to death → submitted, `curl "$BASE/top?mode=high"` shows it; bad NET_BASE → offline message, game unaffected. House gates (canary: headless banks but never submits — byte-identical).

- [ ] **Step 4: Commit** — `git commit -m "feat: submit run score to global leaderboard at run end"`.

---

### Task 9: Leaderboard screen (Highest / Cumulative tabs)

**Files:**
- Modify: `CPP/game/main.cpp` (`PHASE_LEADERBOARD = 7`, fetch + tab input), UI rendering site per house pattern
- Test: manual against live Worker; house gates

**Interfaces:**
- Consumes: `GET /top?mode=high|total`, `net::get`.
- Produces: blackboard keys `"lb_rows"` (pre-formatted `"1. name  1234\n"` lines), `"lb_mode"` (`high|total`), `"lb_status"` (`loading|ok|error`).

- [ ] **Step 1: Phase + input**

- `PHASE_LEADERBOARD = 7`. Entry: key `L` from the title menu (add to hints; hidden/no-op when `!net::enabled()`); inside: TAB toggles mode, ESC back to title.
- On enter or toggle: `pending_top = net::get(std::string(NET_BASE) + "/top?mode=" + mode);` `lb_status = "loading"`. On ready: `nlohmann::json` parse in try/catch (fail → `error`), build rows:
```cpp
std::string rows; int rank = 1;
for (auto& r : j["rows"]) rows += std::to_string(rank++) + ". " + r["name"].get<std::string>() + "  " + std::to_string(r["score"].get<long long>()) + "\n";
```

- [ ] **Step 2: Render** — header "LEADERBOARD — HIGHEST" / "— CUMULATIVE", one line per row (split `\n`), footer "TAB switch · ESC back"; loading/error = single status line. House menu style (`ui-context.md`).

- [ ] **Step 3: Verify** — manual: rows live, TAB switches (cumulative = sums), ESC returns. House gates green.

- [ ] **Step 4: Commit + decisions entry** — one D-entry covering Tasks 8+9 (leaderboard submit + screen). `git commit -m "feat: leaderboard screen with Highest/Cumulative tabs"`.

---

### Task 10: One-click updater

**Files:**
- Create: `CPP/game/update_check.hpp`, `CPP/game/update_check.cpp`
- Modify: `CPP/game/main.cpp` (check on title, `U` to update), title rendering site (banner)
- Test: `CPP/game/tests/unit/test_update_check.cpp` (version compare, pure)

**Interfaces:**
- Consumes: `GET /version`, `net::get`, `GAME_VERSION` (Task 3), Inno silent contract (Task 3).
- Produces:
```cpp
bool version_newer(const std::string& remote, const std::string& local);  // "2.1.0" > "2.0.0"; unparseable -> false
// Windows: downloads installer to %TEMP%, launches "/SILENT /NORESTART", returns true -> caller exits.
// Non-Windows: SDL_OpenURL(installer_url), returns false.
bool launch_update(const std::string& installer_url);
```

- [ ] **Step 1: Failing test**

`CPP/game/tests/unit/test_update_check.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "../../update_check.hpp"
TEST_CASE("version compare", "[update]") {
    REQUIRE(version_newer("2.1.0", "2.0.0"));
    REQUIRE(version_newer("2.10.0", "2.9.3"));
    REQUIRE_FALSE(version_newer("2.0.0", "2.0.0"));
    REQUIRE_FALSE(version_newer("1.9.9", "2.0.0"));
    REQUIRE_FALSE(version_newer("garbage", "2.0.0"));
}
```
Run → compile failure.

- [ ] **Step 2: Implement**

`version_newer`: `sscanf` both sides as `%d.%d.%d` (failure → false), tuple compare. `launch_update`:
```cpp
#ifdef _WIN32
// 1. blocking net::get(installer_url).get() -> write body to %TEMP%\ReactorDrone-Setup.exe (GetTempPathA)
// 2. ShellExecuteA(nullptr, "open", path, "/SILENT /NORESTART", nullptr, SW_SHOWNORMAL);
// 3. return true  (caller sets running=false so the installer can replace files)
#else
SDL_OpenURL(installer_url.c_str());
return false;
#endif
```
Blocking download accepted: set a "Downloading update..." status, render one frame, then call.

- [ ] **Step 3: Wire in**

On first entering PHASE_TITLE with `net::enabled()`: `pending_version = net::get(std::string(NET_BASE) + "/version")`. On ready + `version_newer(remote, GAME_VERSION)`: banner "Update vX.Y.Z available — press U", store url. `U` → `if (launch_update(url)) running = false;`. Render the banner on the title screen, house style.

- [ ] **Step 4: House gates + release walkthrough**

`python runTestsAll.py` green, canary clean. End-to-end (needs Task 4's release): install current version under Wine or real Windows, bump Worker `RELEASE_VERSION` + `INSTALLER_URL`, redeploy, launch → banner → U → silent upgrade → relaunches at new version. Uninstall via Add/Remove Programs. This is the spec's release-gate walkthrough.

- [ ] **Step 5: Commit + docs sync**

D-entry (updater semantics), update `agentProjectDocs/progress-tracker.md` Current Phase (distribution shipped; what was verified), per house handoff rules. `git commit -m "feat: one-click in-game updater via silent Inno reinstall"`.

---

## Release checklist (per release, post-plan)

1. Bump `GAME_VERSION` in `version.hpp`; commit.
2. `git tag vX.Y.Z && git push origin vX.Y.Z` → CI drafts the Release, pushes itch.
3. Publish the draft Release.
4. Update Worker `RELEASE_VERSION` + `INSTALLER_URL`; `npx wrangler deploy`. (Deliberately after publishing — no player is ever offered a 404 installer.)
