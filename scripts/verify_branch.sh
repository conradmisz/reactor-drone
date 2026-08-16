#!/usr/bin/env bash
# verify_branch.sh — everything about this branch that a machine can check.
#
# Touches NOTHING in production: the backend suite runs against a local
# `wrangler dev` + local D1, and saves/meta.json is backed up and restored.
#
#   scripts/verify_branch.sh           # all sections
#   scripts/verify_branch.sh 1 3 6     # only those sections
#
# What it CANNOT check is listed at the end; that part needs a human.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
WORK="${TMPDIR:-/tmp}/rd-verify.$$"
mkdir -p "$WORK"
PASS=0; FAIL=0; SKIP=0
ok()   { printf '  \033[32mPASS\033[0m %s\n' "$1"; PASS=$((PASS+1)); }
bad()  { printf '  \033[31mFAIL\033[0m %s\n' "$1"; FAIL=$((FAIL+1)); }
skip() { printf '  \033[33mSKIP\033[0m %s\n' "$1"; SKIP=$((SKIP+1)); }
hdr()  { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }
want() { [ $# -eq 0 ] || [ -z "${SECTIONS:-}" ] || grep -qw "$1" <<<"$SECTIONS"; }
SECTIONS="$*"

# saves/ is live user state; every game invocation below writes to it.
[ -f saves/meta.json ] && cp saves/meta.json "$WORK/meta.bak"
restore_saves() {
  [ -f "$WORK/meta.bak" ] && cp "$WORK/meta.bak" saves/meta.json
  rm -f saves/settings.json
}
cleanup() {
  restore_saves
  [ -f "$WORK/wrangler.pid" ] && kill "$(cat "$WORK/wrangler.pid")" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT

# ---------------------------------------------------------------- 1. build
if want 1; then
hdr "1. Dev build (warnings are errors here, except Lua's vendored tmpnam)"
cmake -B CPP/build -S CPP >/dev/null 2>&1
if cmake --build CPP/build -j"$(nproc)" >"$WORK/build.log" 2>&1; then ok "builds"; else bad "build failed (see $WORK/build.log)"; fi
W=$(grep -c 'warning:' "$WORK/build.log" || true)
LUA=$(grep 'warning:' "$WORK/build.log" | grep -c 'tmpnam' || true)
[ "$W" -eq "$LUA" ] && ok "no warnings beyond Lua tmpnam ($W total)" || bad "$((W-LUA)) unexpected warnings"
fi

# ---------------------------------------------------------------- 2. tests
if want 2; then
hdr "2. Test suite"
if ctest --test-dir CPP/build >"$WORK/ctest.log" 2>&1; then
  ok "ctest $(grep -o '[0-9]* tests passed' "$WORK/ctest.log" | head -1)"
else
  bad "ctest failed"; grep -E "Failed|\(Failed\)" "$WORK/ctest.log" | head -5 | sed 's/^/      /'
fi
./CPP/build/game/tests/game_unit_tests "[telemetry],[settings]" >"$WORK/tele.log" 2>&1 \
  && ok "telemetry + settings cases" || bad "telemetry/settings cases"
fi

# ------------------------------------------------------- 3. determinism
if want 3; then
hdr "3. Replay canary (determinism invariant #4)"
for i in 1 2; do
  SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000 >"$WORK/canary$i.txt" 2>&1
done
restore_saves
if diff -q "$WORK/canary1.txt" "$WORK/canary2.txt" >/dev/null; then ok "byte-identical across two runs"
else bad "DIVERGED"; diff "$WORK/canary1.txt" "$WORK/canary2.txt" | head -10 | sed 's/^/      /'; fi
grep -q "Frames: 3000" "$WORK/canary1.txt" && ok "reached stopframe 3000" || bad "did not reach stopframe"
fi

# ---------------------------------------------------------------- 4. data
if want 4; then
hdr "4. Game data + workflow files"
python3 -c "import json;json.load(open('assets/GameData.json'))" 2>/dev/null \
  && ok "GameData.json parses" || bad "GameData.json invalid"
python3 - <<'PY' && ok "every screen widget has the loader's required fields" || bad "a widget is missing a required field"
import json,sys
d=json.load(open('assets/GameData.json'))
need={'element_type','rect','label_text','z_order'}
bad=[(s,i) for s,v in d['screens'].items() for i,w in enumerate(v['widgets'])
     if not need.issubset(w) or (w['element_type'] in ('panel','label','button','checkbox') and 'style_id' not in w)]
sys.exit(1 if bad else 0)
PY
python3 -c "import yaml,sys;yaml.safe_load(open('.github/workflows/release.yml'))" 2>/dev/null \
  && ok "release.yml is valid YAML" || skip "PyYAML absent — skipped workflow lint"
for f in installer/package-win.sh installer/package-linux.sh installer/package-mac.sh CPP/fetch-win-deps.sh CPP/fetch-linux-deps.sh scripts/drive_ui.py; do
  [ -x "$f" ] && bash -n "$f" 2>/dev/null || python3 -m py_compile "$f" 2>/dev/null
done && ok "packaging scripts parse" || bad "a packaging script has a syntax error"
# bugs/011: the dashboard is ONE big JS template literal, so an escape meant for
# the browser gets eaten by the literal. That shipped a real newline inside a
# single-quoted string — a SyntaxError that killed the whole client script and
# left the page on "connecting..." forever, with every server-side check green.
# Extract the served <script> exactly as a browser would receive it and PARSE it.
( cd backend/src && node --input-type=module -e "
import('./dashboard.js').then(async m => {
  const fs = await import('node:fs');
  const h = m.DASHBOARD_HTML;
  const s = h.slice(h.indexOf('<script>')+8, h.lastIndexOf('</script>'));
  if (!s.length) process.exit(1);
  fs.writeFileSync(process.env.TMPDIR ? process.env.TMPDIR + '/dash_client.js' : '/tmp/dash_client.js', s);
})" && node --check "${TMPDIR:-/tmp}/dash_client.js" ) >/dev/null 2>&1 \
  && ok "dashboard client script parses in a browser" \
  || bad "dashboard client script has a syntax error — the page will hang on 'connecting'"
grep -q "$(grep -o '"[0-9.]*"' CPP/game/version.hpp | tr -d '"')" backend/wrangler.jsonc \
  && ok "version.hpp matches wrangler RELEASE_VERSION" || bad "version.hpp and wrangler.jsonc disagree"
fi

# ------------------------------------------------------------- 5. backend
if want 5; then
hdr "5. Backend against a LOCAL wrangler dev + local D1 (never production)"
( cd backend
  find .wrangler -name "*.sqlite*" -delete 2>/dev/null
  npx wrangler d1 execute reactor-drone-db --local --file=schema.sql >/dev/null 2>&1
  npx wrangler dev --local --port 8787 >"$WORK/dev.log" 2>&1 &
  echo $! > "$WORK/wrangler.pid" )
for _ in $(seq 1 40); do curl -s -m 2 http://127.0.0.1:8787/version >/dev/null 2>&1 && break; sleep 1; done
if curl -s -m 3 http://127.0.0.1:8787/version >/dev/null 2>&1; then
  ok "local worker up"
  ( cd backend && set -a && . ./.dev.vars && set +a \
    && BASE=http://127.0.0.1:8787 KEY="$(cat GAME_KEY.local)" DASH="$DASH_PASS" bash ./test.sh ) >"$WORK/api.log" 2>&1 \
    && ok "test.sh ALL PASS (register/score/top/version/telemetry/feedback/dashboard/stats)" \
    || { bad "test.sh failed"; tail -5 "$WORK/api.log" | sed 's/^/      /'; }
  # tech-center route table must list every route worker.js actually serves —
  # the reference half of the dashboard is only allowed to exist because this
  # check stops it rotting (specs/dashboard-telemetry-and-status.md).
  ROUTES_OK=1
  for rt in $(grep -oE "url\.pathname === '[^']+'" backend/src/worker.js | grep -oE "/[a-z]+" | sort -u); do
    grep -q "$rt</code>" backend/src/dashboard.js || { ROUTES_OK=0; bad "route $rt missing from tech center"; }
  done
  [ "$ROUTES_OK" = 1 ] && ok "tech-center route table matches worker.js"
  for t in players scores runs feedback subscribers; do
    ( cd backend && npx wrangler d1 execute reactor-drone-db --local --command "SELECT 1 FROM $t LIMIT 1" ) >/dev/null 2>&1 \
      && ok "table $t exists" || bad "table $t missing"
  done
else
  bad "local worker never came up (see $WORK/dev.log)"
fi
[ -f "$WORK/wrangler.pid" ] && kill "$(cat "$WORK/wrangler.pid")" 2>/dev/null; rm -f "$WORK/wrangler.pid"
fi

# ------------------------------------------------- 6. portable linux build
if want 6; then
hdr "6. Portable Linux build — runs as an installed copy would"
cmake -B CPP/build-portable -S CPP -DRD_PORTABLE=ON -DBUILD_TESTING=OFF \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$ROOT/CPP/linux-deps" >/dev/null 2>&1
if cmake --build CPP/build-portable -j"$(nproc)" >"$WORK/pbuild.log" 2>&1; then ok "portable build"; else bad "portable build failed"; fi
if ./installer/package-linux.sh >"$WORK/pkg.log" 2>&1; then ok "package-linux.sh staged"; else bad "package-linux.sh failed"; fi
FAKE="$WORK/install"; HOMEDIR="$WORK/home"
rm -rf "$FAKE" "$HOMEDIR"; mkdir -p "$FAKE" "$HOMEDIR"
cp -r installer/stage-linux/* "$FAKE"/
# alien cwd + scratch HOME: the compiled-in-source-path trap would fail here
( cd / && HOME="$HOMEDIR" XDG_DATA_HOME="$HOMEDIR/.local/share" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 90 "$FAKE/run.sh" \
    --seed 42 --keys 5:SPACE --stopframe 600 ) >"$WORK/portable.log" 2>&1
grep -q "Frames: 600" "$WORK/portable.log" \
  && ok "staged build runs from an alien cwd with a scratch HOME" \
  || { bad "staged build did not complete"; tail -3 "$WORK/portable.log" | sed 's/^/      /'; }
[ -f "$HOMEDIR/.local/share/conradm/ReactorDrone/saves/meta.json" ] \
  && ok "save landed in XDG, not the source tree" || bad "save did not land in XDG"
[ "$(strings "$FAKE/ReactorDrone" | grep -c "$ROOT/assets")" -eq 0 ] \
  && ok "no build-machine asset path baked into the portable binary" \
  || bad "portable binary still references $ROOT/assets"
fi

# ------------------------------------------------------ 7. windows build
if want 7; then
hdr "7. Windows cross-build (+ wine smoke test)"
if ! command -v x86_64-w64-mingw32-g++ >/dev/null; then skip "mingw absent"; else
  CPP/fetch-win-deps.sh >/dev/null 2>&1
  cmake -S CPP -B CPP/build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
        -DCMAKE_PREFIX_PATH="$ROOT/CPP/win-deps" -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release -DRD_PORTABLE=ON >/dev/null 2>&1
  if cmake --build CPP/build-win -j"$(nproc)" >"$WORK/win.log" 2>&1; then ok "ReactorDrone.exe builds"
  else bad "windows build failed"; tail -5 "$WORK/win.log" | sed 's/^/      /'; fi
  EXE=$(find CPP/build-win -name ReactorDrone.exe | head -1)
  if [ -n "$EXE" ]; then
    ./installer/package-win.sh >"$WORK/winpkg.log" 2>&1 && ok "package-win.sh staged" || bad "package-win.sh failed"
    [ -f installer/stage/PRIVACY.md ] && ok "PRIVACY.md staged for the installer" || bad "PRIVACY.md not staged"
    # One strings snapshot, then grep the FILE. Never `strings ... | grep -q`
    # under `set -o pipefail`: grep -q exits at the first match and closes the
    # pipe, strings takes SIGPIPE, and pipefail reports the pipeline as failed —
    # so a passing check reads as a failure. A verification script that
    # misreports is worse than no check at all.
    strings "$EXE" > "$WORK/exe.strings"
    # THE trap that already bit once: a stale localhost NET_BASE in the artifact
    if grep -qF '127.0.0.1' "$WORK/exe.strings"; then bad "exe contains 127.0.0.1 — DO NOT SHIP"
    else ok "no localhost NET_BASE in the exe"; fi
    if grep -qF 'reactor-drone-api' "$WORK/exe.strings"; then ok "exe points at the production API"
    else bad "exe has no API URL"; fi
    if command -v wine >/dev/null; then
      ( cd installer/stage && WINEDEBUG=-all SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
          timeout 180 wine ReactorDrone.exe --seed 42 --keys 5:SPACE --stopframe 300 ) >"$WORK/wine.log" 2>&1
      grep -q "Frames: 300" "$WORK/wine.log" && ok "exe runs under wine to frame 300" \
        || skip "wine run inconclusive (see $WORK/wine.log) — CI/real Windows is authoritative"
    else skip "wine absent"; fi
  fi
fi
fi

restore_saves
hdr "SUMMARY"
printf '  %d passed, %d failed, %d skipped\n' "$PASS" "$FAIL" "$SKIP"
cat <<'MANUAL'

  A machine cannot check these — they need you:
    - PLAY IT in a window. Tests passing is not a playtest (CLAUDE.md).
    - macOS: no Mac here. The first `v*` tag push runs build-mac on both
      arches; watch those two jobs.
    - Windows installer: ISCC.exe is Windows-only, so the .exe installer
      itself is only built in CI.
    - Production: after `wrangler d1 execute --remote --file schema.sql`
      and `wrangler deploy`, submit one real run and one real feedback
      report, then check /dashboard.
MANUAL
[ "$FAIL" -eq 0 ]
