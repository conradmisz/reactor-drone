# Global Gameplay Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One anonymous per-run summary JSON (~3–5 KB) collected during play, POSTed at run end to the existing Cloudflare Worker, stored as one D1 row, rendered by a local Python report script. Spec: `agentProjectDocs/specs/telemetry.md`.

**Architecture:** A plain `telemetry::RunReport` struct owned by `main.cpp` scope (never the ECS), filled by polling player state each frame plus five write-only Blackboard counters published from systems (the `player.hit_bearing` precedent). One `POST /telemetry` + the missing `POST /score` fire from `bank_run_score`, the single run-end site. Backend: one new route, one new table, full JSON in a `body` column.

**Tech Stack:** C++17, nlohmann::json, libcurl via `net::post_json`, Cloudflare Worker + D1, Python + Pillow + wrangler for reporting.

## Global Constraints

- Zero warnings (`-Wall -Wextra -Wpedantic`); only Lua's `tmpnam` allowed.
- 100% ctest, both `^(Engine|ResourceManager)` and `^Game`.
- Determinism (Invariant 4): telemetry code takes **zero RNG draws** and writes **nothing** the sim reads. Replay canary: `SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000` twice → byte-identical summaries, zero network calls (`net::enabled()` is already false under `--stopframe`).
- No new components (Invariant 6): nothing enters ComponentStorage.
- No engine changes; `CPP/game/tests/unit/*.cpp` and `CPP/game/*.cpp` are GLOBed — no CMake edits needed.
- Build: `cmake -B CPP/build -S CPP && cmake --build CPP/build -j$(nproc)`. Game tests: `python runGameTests.py`. Single case: `./CPP/build/game/tests/game_unit_tests "[telemetry]"`.
- Backend deploys from `backend/` with `npx wrangler`; secrets: `GAME_KEY` (worker secret, local copy in `backend/GAME_KEY.local`), client key in `CPP/game/net/net_config.hpp` (`net::NET_BASE`, `net::NET_GAME_KEY`).
- Commit after every task with the trailer:
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` and
  `Claude-Session: https://claude.ai/code/session_01BRSWvgHV5J1LJm74bDCywZ`

---

### Task 1: Backend — `runs` table + `POST /telemetry`

**Files:**
- Modify: `backend/schema.sql`
- Modify: `backend/src/worker.js`
- Modify: `backend/test.sh`

**Interfaces:**
- Produces: `POST /telemetry` — headers `X-Game-Key`, JSON body ≤ 16384 bytes with required envelope fields `v` (int), `player_id` (uuid), `session_id` (string ≤ 64), `game_version` (string ≤ 32), `difficulty` (string ≤ 32), `prestige` (int 0–99), `ship` (int −1–99), `outcome` (`death|victory|quit|close`), `wave` (int 0–999), `score` (int 0–10 000 000), `dur_s` (number 0–86400). Everything else in the body is stored verbatim, unvalidated. Responses: 200 `{ok:true}`, 400 `{error:'bad_request'}`, 401 `{error:'unauthorized'}`.

- [ ] **Step 1: Add the `runs` table to `backend/schema.sql`** (append):

```sql
CREATE TABLE IF NOT EXISTS runs (
  id         INTEGER PRIMARY KEY,
  ts         INTEGER NOT NULL DEFAULT (unixepoch()),
  player_id  TEXT    NOT NULL,
  session    TEXT    NOT NULL,
  version    TEXT    NOT NULL,
  difficulty TEXT    NOT NULL,
  prestige   INTEGER NOT NULL,
  ship       INTEGER NOT NULL,
  outcome    TEXT    NOT NULL,
  wave       INTEGER NOT NULL,
  score      INTEGER NOT NULL,
  dur_s      INTEGER NOT NULL,
  body       TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_runs_ts ON runs(ts);
```

- [ ] **Step 2: Add the route to `backend/src/worker.js`.** First parameterize the body cap — replace the current `readJson`:

```js
// Reads and JSON-parses the body, refusing to buffer an oversized payload.
async function readJson(req, max = MAX_BODY_BYTES) {
  const len = req.headers.get('content-length');
  if (len && Number(len) > max) throw new Error('body_too_large');
  const text = await req.text();
  if (text.length > max) throw new Error('body_too_large');
  return JSON.parse(text);
}
```

Add next to `MAX_BODY_BYTES`:

```js
const MAX_TELEMETRY_BYTES = 16384; // one run report; ~3-5 KB typical
const OUTCOMES = new Set(['death', 'victory', 'quit', 'close']);
```

Insert this route before the `return json({ error: 'not_found' }, 404);` line:

```js
if (req.method === 'POST' && url.pathname === '/telemetry') {
  if (!safeEqual(req.headers.get('X-Game-Key'), env.GAME_KEY)) return json({ error: 'unauthorized' }, 401);
  const raw = await req.text();
  if (raw.length > MAX_TELEMETRY_BYTES) return json({ error: 'bad_request' }, 400);
  const b = JSON.parse(raw); // SyntaxError -> catch -> 400
  const str = (v, max) => typeof v === 'string' && v.length > 0 && v.length <= max;
  const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
  if (!int(b.v, 1, 99) || typeof b.player_id !== 'string' || !UUID_RE.test(b.player_id) ||
      !str(b.session_id, 64) || !str(b.game_version, 32) || !str(b.difficulty, 32) ||
      !int(b.prestige, 0, 99) || !int(b.ship, -1, 99) || !OUTCOMES.has(b.outcome) ||
      !int(b.wave, 0, 999) || !int(b.score, 0, 10_000_000) ||
      typeof b.dur_s !== 'number' || b.dur_s < 0 || b.dur_s > 86400)
    return json({ error: 'bad_request' }, 400);
  await env.DB.prepare(
    `INSERT INTO runs (player_id, session, version, difficulty, prestige, ship, outcome, wave, score, dur_s, body)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)`)
    .bind(b.player_id, b.session_id, b.game_version, b.difficulty, b.prestige, b.ship,
          b.outcome, b.wave, b.score, Math.round(b.dur_s), raw).run();
  return json({ ok: true });
}
```

Note: telemetry deliberately does NOT require a registered player row — a run report from a never-registered install is still data.

- [ ] **Step 3: Add failing-then-passing cases to `backend/test.sh`** (before `echo ALL PASS`):

```bash
T="{\"v\":1,\"player_id\":\"$U1\",\"session_id\":\"s1\",\"game_version\":\"2.0.0\",\"difficulty\":\"Normal\",\"prestige\":0,\"ship\":0,\"outcome\":\"death\",\"wave\":7,\"score\":1200,\"dur_s\":301.5,\"heat\":{},\"waves\":[]}"
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -d "$T")" = 401 ]                          # no key
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "$T")" = 200 ]
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d '{"v":1}')" = 400 ]  # missing fields
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "{\"v\":1,\"player_id\":\"$U1\",\"session_id\":\"s1\",\"game_version\":\"2.0.0\",\"difficulty\":\"Normal\",\"prestige\":0,\"ship\":0,\"outcome\":\"rage\",\"wave\":7,\"score\":1,\"dur_s\":1}")" = 400 ]  # bad outcome
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "$(python3 -c 'print("{\"pad\":\""+"x"*17000+"\"}")')")" = 400 ]  # oversized
```

- [ ] **Step 4: Migrate and deploy.** From `backend/`:

```bash
npx wrangler d1 execute reactor-drone-db --remote --file schema.sql
npx wrangler deploy
```

- [ ] **Step 5: Run the backend tests against the live worker:**

```bash
cd backend && BASE="https://reactor-drone-api.conradmiszczak.workers.dev" KEY="$(cat GAME_KEY.local)" ./test.sh
```

Expected: `ALL PASS`. (The register/score lines re-run against existing rows: `/register` upserts and `/score` appends, so re-runs stay green; the `/top` greps check `>=` the seeded values — if the exact-value greps fail on re-run, that is pre-existing test.sh behavior, verify the four NEW lines pass.)

- [ ] **Step 6: Commit** — `git add backend/ && git commit -m "feat(backend): /telemetry route + runs table"`

---

### Task 2: Client telemetry module

**Files:**
- Create: `CPP/game/telemetry.hpp`
- Create: `CPP/game/telemetry.cpp`
- Test: `CPP/game/tests/unit/test_telemetry.cpp` (GLOBed in automatically)

**Interfaces:**
- Produces (consumed by Task 4's main.cpp hooks):
  - `telemetry::RunReport` — plain struct, all fields below.
  - `int telemetry::heat_bin(float x, float y, float cx, float cy, float radius)` → 0..1023.
  - `void telemetry::frame_sample(RunReport&, double dt, int wave, float hull, float shield, long long currency, float px, float py, int arena_idx, float cx, float cy, float radius)` — call once per sim frame.
  - `std::string telemetry::b64(const uint8_t*, size_t)`.
  - `std::string telemetry::serialize(const RunReport&)` — the POST body.

- [ ] **Step 1: Write the failing test** `CPP/game/tests/unit/test_telemetry.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "telemetry.hpp"

TEST_CASE("heat_bin maps the arena circle's bounding square to 32x32", "[telemetry]") {
    // Arena: centre (1000, 1000), radius 800 -> square x,y in [200, 1800]
    REQUIRE(telemetry::heat_bin(200.0f, 200.0f, 1000.0f, 1000.0f, 800.0f) == 0);           // min corner
    REQUIRE(telemetry::heat_bin(1799.9f, 1799.9f, 1000.0f, 1000.0f, 800.0f) == 1023);      // max corner
    REQUIRE(telemetry::heat_bin(1000.0f, 1000.0f, 1000.0f, 1000.0f, 800.0f) == 16 * 32 + 16);
    REQUIRE(telemetry::heat_bin(-5000.0f, 9000.0f, 1000.0f, 1000.0f, 800.0f) == 31 * 32 + 0); // clamped
}

TEST_CASE("b64 encodes RFC 4648 vectors", "[telemetry]") {
    auto enc = [](const char* s) {
        return telemetry::b64(reinterpret_cast<const uint8_t*>(s), std::string(s).size());
    };
    REQUIRE(enc("Man") == "TWFu");
    REQUIRE(enc("Ma") == "TWE=");
    REQUIRE(enc("M") == "TQ==");
    REQUIRE(enc("") == "");
}

TEST_CASE("frame_sample opens waves, accumulates damage and econ, bins heat", "[telemetry]") {
    telemetry::RunReport r;
    // Wave 1 opens with the drone at full state.
    telemetry::frame_sample(r, 0.1, 1, 100.0f, 50.0f, 0, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves.size() == 1);
    REQUIRE(r.waves[0].wave == 1);
    REQUIRE(r.waves[0].hp == 100.0f);
    REQUIRE(r.waves[0].units == 0);
    // Take 30 hull damage, earn 10 units.
    telemetry::frame_sample(r, 0.1, 1, 70.0f, 50.0f, 10, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves[0].damage_taken == 30.0f);
    REQUIRE(r.earned == 10);
    REQUIRE(r.spent == 0);
    // Spend 6 (currency 10 -> 4): spent, not negative earned.
    telemetry::frame_sample(r, 0.1, 1, 70.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.spent == 6);
    // A hull INCREASE (shop hull upgrade) is not damage.
    telemetry::frame_sample(r, 0.1, 1, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves[0].damage_taken == 30.0f);
    // Wave 2 opens carrying the CURRENT state; wave 1 kept its seconds.
    telemetry::frame_sample(r, 0.1, 2, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves.size() == 2);
    REQUIRE(r.waves[0].seconds > 0.39);  // 4 frames x 0.1s landed on wave 1
    REQUIRE(r.waves[1].units == 4);
    // 0.5 s at one spot -> at least 2 samples in the centre bin of arena 0.
    for (int i = 0; i < 5; ++i)
        telemetry::frame_sample(r, 0.1, 2, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.heat.count(0) == 1);
    REQUIRE(r.heat.at(0)[16 * 32 + 16] >= 2);
    REQUIRE(r.dur_s > 0.89);
}

TEST_CASE("serialize emits envelope, sections and base64 heat", "[telemetry]") {
    telemetry::RunReport r;
    r.player_id = "11111111-aaaa-bbbb-cccc-000000000001";
    r.session_id = "s1"; r.game_version = "2.0.0"; r.difficulty = "Normal";
    r.outcome = "death"; r.seed = 42; r.ship = 1; r.wave = 7; r.score = 1200;
    r.died = true; r.death_x = 3.0f; r.death_y = 4.0f; r.death_wave = 7; r.killed_by = "enemy:2";
    telemetry::frame_sample(r, 0.3, 7, 10.0f, 0.0f, 5, 1000.0f, 1000.0f, 2, 1000.0f, 1000.0f, 800.0f);
    r.upg_counts[3] = 2;
    r.consumables_used["REPAIR KIT"] = 1;
    r.ui["shop"] = 3;
    const auto j = nlohmann::json::parse(telemetry::serialize(r));
    REQUIRE(j["v"] == 1);
    REQUIRE(j["player_id"] == "11111111-aaaa-bbbb-cccc-000000000001");
    REQUIRE(j["outcome"] == "death");
    REQUIRE(j["death"]["x"] == 3.0f);
    REQUIRE(j["death"]["killed_by"] == "enemy:2");
    REQUIRE(j["death"].contains("bin"));
    REQUIRE(j["heat"]["2"].is_string());               // arena idx -> base64 grid
    REQUIRE(j["econ"]["upg_counts"][3] == 2);
    REQUIRE(j["econ"]["consumables_used"]["REPAIR KIT"] == 1);
    REQUIRE(j["ui"]["shop"] == 3);
    REQUIRE(j["waves"][0]["wave"] == 7);
    REQUIRE(j["combat"]["shots"] == 0);
    // No death section when the run didn't end in one.
    telemetry::RunReport alive;
    alive.player_id = r.player_id; alive.session_id = "s1";
    alive.game_version = "2.0.0"; alive.difficulty = "Normal";
    REQUIRE(nlohmann::json::parse(telemetry::serialize(alive)).contains("death") == false);
}
```

- [ ] **Step 2: Run it to verify it fails** (build error — header missing):

```bash
cmake --build CPP/build -j$(nproc) 2>&1 | grep -m1 'telemetry.hpp'
```

Expected: `fatal error: telemetry.hpp: No such file or directory` (or similar).

- [ ] **Step 3: Write `CPP/game/telemetry.hpp`:**

```cpp
#ifndef TELEMETRY_HPP
#define TELEMETRY_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * telemetry — the per-run summary report (specs/telemetry.md).
 *
 * A plain struct owned by main.cpp scope, deliberately outside the ECS
 * (Invariant 6) and write-only with respect to the simulation (Invariant 4):
 * filling it takes zero RNG draws and nothing in the sim ever reads it. One
 * report is serialized and POSTed once, at bank_run_score.
 */
namespace telemetry {

constexpr int HEAT_DIM = 32;  // 32x32 occupancy grid over the arena's bounding square

struct WaveStat {
    int wave = 0;             ///< 1-based, as the HUD counts
    float hp = 0.0f;          ///< hull when the wave opened
    float shield = 0.0f;
    int units = 0;            ///< currency held when the wave opened
    float seconds = 0.0f;     ///< sim time spent on this wave
    float damage_taken = 0.0f;///< sum of hull DECREASES during the wave
};

struct RunReport {
    // Envelope.
    int v = 1;
    std::string game_version, player_id, session_id, difficulty;
    std::string outcome = "close";   ///< death|victory|quit|close; close = window shut mid-run
    unsigned seed = 0;
    int ship = -1, prestige = 0;
    bool resumed = false;
    int wave = 0;
    long long score = 0;
    double dur_s = 0.0;              ///< sim seconds (PLAYING + INTERMISSION)

    // Sections.
    std::vector<WaveStat> waves;
    bool died = false;
    float death_x = 0.0f, death_y = 0.0f;
    int death_wave = 0;
    int death_bin = 0;               ///< heat_bin() of the death position
    std::string killed_by;           ///< "enemy:<kind>" | "shot" | "hazard" | ""
    std::map<int, std::array<uint8_t, HEAT_DIM * HEAT_DIM>> heat;  ///< arena idx -> grid
    long long earned = 0, spent = 0; ///< sums of currency deltas, split by sign
    int upg_counts[8] = {0};
    std::string item_equipped;
    std::map<std::string, int> consumables_used;
    std::map<std::string, int> ui;   ///< screen -> open count
    long long shots = 0, hits = 0, dashes = 0, bombs = 0;

    // Accumulator internals (not serialized).
    float last_hull = -1.0f;
    long long last_currency = -1;
    double sample_accum = 0.0;
};

/// Grid index for a world position, over the bounding square of the arena
/// circle (centre cx,cy radius r). Out-of-square positions clamp to the edge.
int heat_bin(float x, float y, float cx, float cy, float radius);

/// One sim frame of observation: duration, wave open/close, hull-decrease
/// accumulation, currency-delta split, and 4 Hz position binning. Pure
/// function of its arguments + the report — unit-tested with no ECS.
void frame_sample(RunReport& r, double dt, int wave, float hull, float shield,
                  long long currency, float px, float py, int arena_idx,
                  float cx, float cy, float radius);

/// RFC 4648 base64 (with padding).
std::string b64(const uint8_t* data, size_t n);

/// The POST body. Sections always present except `death` (only when died).
std::string serialize(const RunReport& r);

}  // namespace telemetry

#endif  // TELEMETRY_HPP
```

- [ ] **Step 4: Write `CPP/game/telemetry.cpp`:**

```cpp
#include "telemetry.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace telemetry {

int heat_bin(float x, float y, float cx, float cy, float radius) {
    const float side = 2.0f * radius;
    int gx = static_cast<int>((x - (cx - radius)) / side * HEAT_DIM);
    int gy = static_cast<int>((y - (cy - radius)) / side * HEAT_DIM);
    gx = std::clamp(gx, 0, HEAT_DIM - 1);
    gy = std::clamp(gy, 0, HEAT_DIM - 1);
    return gy * HEAT_DIM + gx;
}

void frame_sample(RunReport& r, double dt, int wave, float hull, float shield,
                  long long currency, float px, float py, int arena_idx,
                  float cx, float cy, float radius) {
    r.dur_s += dt;

    // Wave watcher: a change of the HUD's 1-based wave number opens a stat row
    // carrying the player's state at that moment.
    if (wave > 0 && (r.waves.empty() || r.waves.back().wave != wave)) {
        WaveStat w;
        w.wave = wave; w.hp = hull; w.shield = shield;
        w.units = static_cast<int>(currency);
        r.waves.push_back(w);
    }
    if (!r.waves.empty()) {
        r.waves.back().seconds += static_cast<float>(dt);
        // Hull decreases are damage; increases (shop hull upgrades) are not.
        if (r.last_hull >= 0.0f && hull < r.last_hull)
            r.waves.back().damage_taken += r.last_hull - hull;
    }
    r.last_hull = hull;

    if (r.last_currency >= 0) {
        const long long d = currency - r.last_currency;
        if (d > 0) r.earned += d; else r.spent -= d;
    }
    r.last_currency = currency;

    // 4 Hz occupancy sampling. Saturating u8 bins: 255 caps a ~64 s camp in
    // one cell per run, which is plenty of signal for a global heatmap.
    r.sample_accum += dt;
    while (r.sample_accum >= 0.25) {
        r.sample_accum -= 0.25;
        auto& grid = r.heat[arena_idx];           // zero-initialised std::array
        uint8_t& cell = grid[static_cast<size_t>(heat_bin(px, py, cx, cy, radius))];
        if (cell < 255) ++cell;
    }
}

std::string b64(const uint8_t* data, size_t n) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t a = data[i];
        const uint32_t b = i + 1 < n ? data[i + 1] : 0;
        const uint32_t c = i + 2 < n ? data[i + 2] : 0;
        const uint32_t v = (a << 16) | (b << 8) | c;
        out.push_back(T[(v >> 18) & 63]);
        out.push_back(T[(v >> 12) & 63]);
        out.push_back(i + 1 < n ? T[(v >> 6) & 63] : '=');
        out.push_back(i + 2 < n ? T[v & 63] : '=');
    }
    return out;
}

std::string serialize(const RunReport& r) {
    nlohmann::json j{
        {"v", r.v},
        {"game_version", r.game_version},
        {"player_id", r.player_id},
        {"session_id", r.session_id},
        {"difficulty", r.difficulty},
        {"outcome", r.outcome},
        {"seed", r.seed},
        {"ship", r.ship},
        {"prestige", r.prestige},
        {"resumed", r.resumed},
        {"wave", r.wave},
        {"score", r.score},
        {"dur_s", r.dur_s},
    };
    j["waves"] = nlohmann::json::array();
    for (const WaveStat& w : r.waves)
        j["waves"].push_back({{"wave", w.wave}, {"hp", w.hp}, {"shield", w.shield},
                              {"units", w.units}, {"seconds", w.seconds},
                              {"damage_taken", w.damage_taken}});
    if (r.died)
        j["death"] = {{"x", r.death_x}, {"y", r.death_y},
                      {"wave", r.death_wave}, {"killed_by", r.killed_by},
                      {"bin", r.death_bin}};
    j["heat"] = nlohmann::json::object();
    for (const auto& [arena, grid] : r.heat)
        j["heat"][std::to_string(arena)] = b64(grid.data(), grid.size());
    j["econ"] = {{"earned", r.earned}, {"spent", r.spent},
                 {"upg_counts", r.upg_counts}, {"item", r.item_equipped},
                 {"consumables_used", r.consumables_used}};
    j["ui"] = r.ui;
    j["combat"] = {{"shots", r.shots}, {"hits", r.hits},
                   {"dashes", r.dashes}, {"bombs", r.bombs}};
    return j.dump();
}

}  // namespace telemetry
```

- [ ] **Step 5: Build and run the test:**

```bash
cmake --build CPP/build -j$(nproc) && ./CPP/build/game/tests/game_unit_tests "[telemetry]"
```

Expected: all assertions pass, zero warnings in the build log.

- [ ] **Step 6: Commit** — `git add CPP/game/telemetry.* CPP/game/tests/unit/test_telemetry.cpp && git commit -m "feat: telemetry RunReport, heat binning, serialization"`

---

### Task 3: Combat counters published from systems

**Files:**
- Modify: `CPP/game/player_fire_system.hpp` + `.cpp` (const removal + `tm.shots`)
- Modify: `CPP/game/projectile_hit_system.hpp` + `.cpp` (const removal + `tm.hits`)
- Modify: `CPP/game/dash_system.hpp` (`tm.dashes`)
- Modify: `CPP/game/active_items.cpp` (`tm.bombs`)
- Modify: `CPP/game/player_damage_system.cpp` (`tm.last_hit_by`)
- Test: `CPP/game/tests/unit/test_dash.cpp` (one new case)

**Interfaces:**
- Produces Blackboard keys read by Task 4: `tm.shots`, `tm.hits`, `tm.dashes`, `tm.bombs` (all `double`, monotonically incremented, reset by main at run start via `blackboard.set<double>(key, 0.0)`), and `tm.last_hit_by` (`std::string`).
- Determinism: every write is observation-only — nothing in the sim reads a `tm.*` key. Same contract as `player.hit_bearing` (documented at its publish site in `player_damage_system.cpp`).

Each increment is this one-line idiom (adjust the key):

```cpp
// telemetry: write-only observation, nothing in the sim reads tm.* (the
// player.hit_bearing precedent) — cannot move the replay canary.
blackboard.set<double>("tm.shots", blackboard.get_or<double>("tm.shots", 0.0) + 1.0);
```

- [ ] **Step 1: Write the failing test** — add to `CPP/game/tests/unit/test_dash.cpp` a case alongside its existing setup (copy the fixture pattern already in that file for `tick_dash`; it builds a player with `ShipState` + `Velocity` and calls `tick_dash(storage, em, blackboard, cfg, state, /*key_down=*/true, dt)`):

```cpp
TEST_CASE("a triggered dash bumps the tm.dashes counter", "[dash][telemetry]") {
    // ... same entity/config setup as the file's first dash-trigger case ...
    REQUIRE(blackboard.get_or<double>("tm.dashes", 0.0) == 0.0);
    tick_dash(storage, em, blackboard, cfg, state, true, 0.016f);
    REQUIRE(blackboard.get_or<double>("tm.dashes", 0.0) == 1.0);
}
```

- [ ] **Step 2: Run to verify it fails:** `cmake --build CPP/build -j$(nproc) && ./CPP/build/game/tests/game_unit_tests "[dash][telemetry]"` — expected: FAIL (counter stays 0).

- [ ] **Step 3: `dash_system.hpp`** — in `tick_dash`'s trigger block, immediately after `--ship.dash_charges;`, add the increment idiom with key `"tm.dashes"`. Run the test again: PASS.

- [ ] **Step 4: `player_fire_system`** — change `const Blackboard& blackboard` to `Blackboard& blackboard` in both `player_fire_system.hpp:24` and the matching definition in `player_fire_system.cpp` (the call site `main.cpp:1992` passes a mutable `blackboard` already — no change there). In the `.cpp`, at the point a projectile entity is created (one increment per projectile, so a multi-barrel volley counts each shot), add the idiom with key `"tm.shots"`.

- [ ] **Step 5: `projectile_hit_system`** — same const removal in `projectile_hit_system.hpp:24` + `.cpp` (call site `main.cpp:1994` already mutable). At the site where a player projectile lands on an entity with `EnemyTag` (the existing `has_component<EnemyTag>(other)` branch around `projectile_hit_system.cpp:66`), add the idiom with key `"tm.hits"`.

- [ ] **Step 6: `active_items.cpp`** — at the missile detonation (the `burst(...)` call after the AoE `DamageEvent` loop, ~line 171), add the idiom with key `"tm.bombs"`. The function already takes `Blackboard& blackboard`.

- [ ] **Step 7: `player_damage_system.cpp`** — in the contact loop, next to the existing `player.hit_bearing` publish, classify what hit the drone (add `#include "enemy_components.hpp"` if not present — it defines `EnemyShot`, `EnemyBehavior`, `EnemyTag`):

```cpp
// telemetry: what last hurt the drone, read only at death (write-only, the
// hit_bearing precedent). "enemy:0" = a default enemy with no EnemyBehavior.
if (storage.has_component<EnemyShot>(other)) {
    blackboard.set<std::string>("tm.last_hit_by", "shot");
} else if (auto eb = storage.get_component<EnemyBehavior>(other); eb.has_value()) {
    blackboard.set<std::string>("tm.last_hit_by",
                                "enemy:" + std::to_string(eb->get().kind));
} else if (storage.has_component<EnemyTag>(other)) {
    blackboard.set<std::string>("tm.last_hit_by", "enemy:0");
} else {
    blackboard.set<std::string>("tm.last_hit_by", "hazard");
}
```

- [ ] **Step 8: Full build + game tests:** `cmake --build CPP/build -j$(nproc) 2>&1 | grep warning:` (expect only Lua `tmpnam`), then `python runGameTests.py` — all pass.

- [ ] **Step 9: Commit** — `git commit -am "feat: tm.* combat counters published from systems"`

---

### Task 4: main.cpp integration — collect, finalize, POST

**Files:**
- Modify: `CPP/game/main.cpp`

**Interfaces:**
- Consumes: everything Task 2 + Task 3 produced; `net::post_json` / `net::NET_BASE` / `net::NET_GAME_KEY`; `generate_uuid()` from `meta_save.hpp`; `game_version()` — check `CPP/game/version.hpp` for the exact symbol (it holds the release version string) and use it for `tm.game_version`.
- Consumes (Task 5, forward reference): `settings.analytics` — a `bool` on `SettingsSave`. Until Task 5 lands, guard the POST with `settings.analytics` anyway by adding the field in this task if executing out of order is needed — otherwise execute Task 5 first or reference `settings.screen_shake`'s pattern. **Recommended execution order: Task 5 before Task 4.**

All edits below are in `main.cpp`. Line numbers are as of commit `5259039` — re-locate by the quoted anchors, not the numbers.

- [ ] **Step 1: Declarations** (near `std::future<net::Response> pending_register;`, ~line 855):

```cpp
#include "telemetry.hpp"   // (top of file, with the other game includes)
```

```cpp
telemetry::RunReport tm;
const std::string session_id = generate_uuid();   // one per launch
std::vector<std::future<net::Response>> tm_inflight;  // polled each frame, drained at exit
```

- [ ] **Step 2: Reset at run start** — in `start_run`, next to `run_banked = false;` (the `// Lane F: a new run to bank at its end` line inside `start_run`, NOT the restart at ~2187):

```cpp
// Telemetry: a fresh report per run. Counters are Blackboard-published by
// the systems (Task 3), so they reset here too.
tm = telemetry::RunReport{};
tm.game_version = GAME_VERSION;   // version.hpp (already included by main.cpp)
tm.player_id = meta.player_id;
tm.session_id = session_id;
tm.seed = config.seed;
tm.ship = selected_ship;
tm.prestige = meta.prestige;
tm.resumed = (resume != nullptr && resume->present);
for (const char* k : {"tm.shots", "tm.hits", "tm.dashes", "tm.bombs"})
    blackboard.set<double>(k, 0.0);
blackboard.set<std::string>("tm.last_hit_by", "");
```

After the difficulty label is chosen (`blackboard.set<std::string>("difficulty", label);`), add `tm.difficulty = label;`.

Also reset at the in-place restart site (~2187, `run_banked = false;   // Lane F: a new run to bank at its end` after game-over/victory `advance`): duplicate the same reset block there (difficulty: `tm.difficulty = blackboard.get_or<std::string>("difficulty", "Normal");`).

- [ ] **Step 3: Per-frame sampler** — immediately after the `// === END HOOK: dash ===` line (inside the sim-phase block), add:

```cpp
// === HOOK: telemetry === (specs/telemetry.md)
// Pure observation: reads player state, writes only the report. Zero RNG
// draws, so the replay canary cannot move (Invariant 4).
{
    float hull = 0.0f, shield = 0.0f, px = 0.0f, py = 0.0f;
    long long currency = 0;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        if (auto h = component_storage.get_component<Health>(p); h.has_value())
            hull = h->get().current;
        if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
            shield = s->get().shield;
            currency = s->get().currency;
            // A consumable slot emptying during play = one use.
            static int prev_consumable = -1;
            const int cur = s->get().consumable_id;
            if (prev_consumable >= 0 && cur < 0)
                tm.consumables_used[blackboard.get_or<std::string>(
                    "ship.consumable_name", "?")]++;
            prev_consumable = cur;
        }
        if (auto pos = component_storage.get_component<Position>(p); pos.has_value()) {
            px = pos->get().x; py = pos->get().y;
            if (auto sz = component_storage.get_component<Size>(p); sz.has_value()) {
                px += sz->get().width * 0.5f; py += sz->get().height * 0.5f;
            }
        }
    }
    const int cur_wave = blackboard.get_or<int>("wave", 0);
    telemetry::frame_sample(
        tm, blackboard.get_or<double>("delta_time", 0.0), cur_wave, hull, shield,
        currency, px, py, active_arena_index(config.arenas, std::max(cur_wave, 1)),
        config.arena.center_x, config.arena.center_y, config.arena.radius);
}
// === END HOOK: telemetry ===
```

Note: `static int prev_consumable` is fine — one player, and a stale value across runs self-heals on the first frame (reset to the live slot). If the executor prefers, hoist it next to `tm` as a plain local; either is acceptable.

- [ ] **Step 4: Death capture** — at the run-end site (~1883, `bank_run_score(blackboard);   // Lane F: the run ended, so it counts`), BEFORE the bank call, add:

```cpp
if (phase == PHASE_GAMEOVER) {   // adjust to the actual phase test at this site
    tm.died = true;
    tm.death_wave = blackboard.get_or<int>("wave", 0);
    tm.killed_by = blackboard.get_or<std::string>("tm.last_hit_by", "");
    for (Entity p : component_storage.entities_with_component<PlayerTag>())
        if (auto pos = component_storage.get_component<Position>(p); pos.has_value()) {
            tm.death_x = pos->get().x; tm.death_y = pos->get().y;
        }
    tm.death_bin = telemetry::heat_bin(tm.death_x, tm.death_y, config.arena.center_x,
                                       config.arena.center_y, config.arena.radius);
}
```

Read the surrounding code first: the site knows whether the run ended in death or victory (it transitions to `PHASE_GAMEOVER` or `PHASE_VICTORY`). Capture death info only on the death path.

- [ ] **Step 5: Outcome + POST in `bank_run_score`** — change the lambda signature to take the outcome:

```cpp
auto bank_run_score = [&](const Blackboard& bb, const char* outcome) {
```

and update every call site (find them all: `grep -n 'bank_run_score(' CPP/game/main.cpp`):
- death/victory site (~1883): `bank_run_score(blackboard, phase == PHASE_VICTORY ? "victory" : "death");` — match the actual phase logic there.
- quit-from-pause site(s): `"quit"`.
- window-close site (~2407): `"close"`.

Inside the lambda, after `meta_write(meta_save_path(), meta);`, add:

```cpp
// Telemetry + leaderboard: the one run-end site (specs/telemetry.md).
// net::enabled() is false under --stopframe, so headless runs POST nothing.
tm.outcome = outcome;
tm.wave = wave_spawner.current_wave_index() + 1;
tm.score = bb.get_or<int>("score", 0);
tm.shots = static_cast<long long>(bb.get_or<double>("tm.shots", 0.0));
tm.hits = static_cast<long long>(bb.get_or<double>("tm.hits", 0.0));
tm.dashes = static_cast<long long>(bb.get_or<double>("tm.dashes", 0.0));
tm.bombs = static_cast<long long>(bb.get_or<double>("tm.bombs", 0.0));
tm.item_equipped = bb.get_or<std::string>("ship.item_name", "");
for (Entity p : component_storage.entities_with_component<PlayerTag>())
    if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
        for (int i = 0; i < 8; ++i) tm.upg_counts[i] = s->get().upg_counts[i];
tm.ui["minimap_on"] = settings.minimap ? 1 : 0;
tm.ui["shake_on"] = settings.screen_shake ? 1 : 0;
if (net::enabled() && settings.analytics) {
    tm_inflight.push_back(net::post_json(std::string(net::NET_BASE) + "/telemetry",
                                         telemetry::serialize(tm), net::NET_GAME_KEY));
    if (meta.registered)
        tm_inflight.push_back(net::post_json(
            std::string(net::NET_BASE) + "/score",
            nlohmann::json{{"player_id", meta.player_id},
                           {"score", bb.get_or<int>("score", 0)}}.dump(),
            net::NET_GAME_KEY));
}
```

NOTE: `bank_run_score` must remain defined AFTER `wave_spawner`, `settings`, and `tm` exist — it already sits after `wave_spawner` (it calls `current_wave_index()`); if `settings` (loaded at ~1047) comes later, capture what's needed or move the settings load earlier. Verify order when editing; `settings` is loaded at ~1047 which is AFTER the lambda at ~866 — so move the `SettingsSave settings = settings_load(...)` + the two `blackboard.set` lines from ~1047 up to just before `bank_run_score`'s definition (they have no dependencies beyond `blackboard`).

- [ ] **Step 6: Poll + drain the in-flight futures** — once per frame, near the `pending_register` poll or in the main loop's top:

```cpp
// Reap finished telemetry/score POSTs; nothing reads the responses.
tm_inflight.erase(
    std::remove_if(tm_inflight.begin(), tm_inflight.end(),
                   [](std::future<net::Response>& f) {
                       return f.wait_for(std::chrono::seconds(0)) ==
                              std::future_status::ready;
                   }),
    tm_inflight.end());
```

And after the main loop exits (before teardown/`return`):

```cpp
// Drain in-flight POSTs so the vector's destructor can't stall shutdown
// invisibly. ponytail: bounded 2s grace, then the future destructors block
// up to CURLOPT_TIMEOUT (8s) worst case; a detach-capable client is the
// upgrade path if exit latency ever matters.
for (auto& f : tm_inflight)
    if (f.valid()) f.wait_for(std::chrono::seconds(2));
```

- [ ] **Step 7: Wire the missing `/score` verification.** Build, then confirm both POSTs by a real run (needs a display): start the game, die on wave 1, then:

```bash
cd backend && npx wrangler d1 execute reactor-drone-db --remote --json \
  --command "SELECT outcome, wave, score, length(body) FROM runs ORDER BY id DESC LIMIT 1"
npx wrangler d1 execute reactor-drone-db --remote --json \
  --command "SELECT score, ts FROM scores ORDER BY ts DESC LIMIT 1"
```

Expected: a `death` row with your wave/score and a matching `scores` row (if this install is registered).

- [ ] **Step 8: Replay canary** — run twice, diff summaries, and confirm zero network calls (net is disabled under `--stopframe`; additionally `strace`-free check is not needed — `net::enabled()` short-circuits before curl):

```bash
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000 > /tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2-distribution/d7902a94-89dd-429d-aad9-6cff8981f7e4/scratchpad/c1.txt
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000 > /tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2-distribution/d7902a94-89dd-429d-aad9-6cff8981f7e4/scratchpad/c2.txt
diff /tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2-distribution/d7902a94-89dd-429d-aad9-6cff8981f7e4/scratchpad/c1.txt /tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2-distribution/d7902a94-89dd-429d-aad9-6cff8981f7e4/scratchpad/c2.txt && echo CANARY-OK
```

- [ ] **Step 9: Full tests + warning grep:** `python runTestsAll.py`; build log grep for `warning:`.

- [ ] **Step 10: Commit** — `git commit -am "feat: telemetry collection in main + /telemetry and /score POSTs at run end"`

---

### Task 5: Consent — settings bool, options toggle, disclosure, PRIVACY.md

(Recommended to execute BEFORE Task 4 — Task 4's POST guard reads `settings.analytics`.)

**Files:**
- Modify: `CPP/game/settings_save.hpp`
- Modify: `assets/GameData.json` (settings screen + name_entry screen)
- Modify: `CPP/game/main.cpp` (toggle handler + widget sync)
- Create: `PRIVACY.md`
- Modify: `installer/package-win.sh` (stage PRIVACY.md)
- Test: `CPP/game/tests/unit/test_telemetry.cpp` (one settings round-trip case)

**Interfaces:**
- Produces: `SettingsSave::analytics` (`bool`, default `true`), read by Task 4's POST guard. No Blackboard publish — main.cpp is the only reader.

- [ ] **Step 1: Failing test** — append to `test_telemetry.cpp`:

```cpp
#include <cstdio>
#include "settings_save.hpp"

TEST_CASE("analytics setting round-trips and defaults true", "[telemetry][settings]") {
    REQUIRE(SettingsSave{}.analytics == true);            // default-on
    const std::string p = "/tmp/rd_test_settings.json";
    SettingsSave s; s.analytics = false;
    REQUIRE(settings_write(p, s));
    REQUIRE(settings_load(p).analytics == false);
    std::remove(p.c_str());
    REQUIRE(settings_load("/nonexistent/x.json").analytics == true);  // garbage-tolerant
}
```

Run: `./CPP/build/game/tests/game_unit_tests "[settings]"` after build — FAIL (no member `analytics`).

- [ ] **Step 2: `settings_save.hpp`** — add `bool analytics = true;` to the struct, a parse clause matching the existing `screen_shake` pattern (`if (j.contains("analytics") && j["analytics"].is_boolean()) s.analytics = j["analytics"].get<bool>();`), and add `{"analytics", s.analytics}` to the JSON written by `settings_write`. Update the file's doc comment: three toggles now. Build + run the test: PASS.

- [ ] **Step 3: `assets/GameData.json` settings screen** — after the MINIMAP checkbox widget (~line 4596), insert an ANALYTICS row, and move the caption label (currently `"y": 216`) down to `"y": 160`:

```json
{
  "element_type": "label",
  "rect": { "x": 204, "y": 208, "w": 280, "h": 44 },
  "label_text": "ANALYTICS",
  "style_id": "subtitle",
  "z_order": 10
},
{
  "element_type": "checkbox",
  "rect": { "x": 500, "y": 208, "w": 96, "h": 44 },
  "label_text": "",
  "style_id": "default_button",
  "z_order": 10,
  "value": 1,
  "on_click_fn": "on_toggle_analytics",
  "name": "settings_analytics"
}
```

- [ ] **Step 4: `main.cpp` toggle wiring** — three edits, all following the existing two-toggle pattern:
  1. Where `shake_w`/`mini_w` caches are declared, add `analytics_w` + `analytics_w_resolved` with the same types.
  2. In `sync_settings_widgets` (~1052), add a third block setting `settings_analytics`'s `UIState.value` from `settings.analytics`.
  3. Extend the click handler (~2058) to cover three toggles — replace the two-way ternary with:

```cpp
} else if (menu_click == "on_toggle_shake" || menu_click == "on_toggle_minimap" ||
           menu_click == "on_toggle_analytics") {
    // UISystem already flipped the checkbox's UIState.value — read it back
    // as the truth, persist, and publish to the apply sites. Analytics has
    // no apply site: main.cpp's POST guard reads the struct directly.
    blackboard.remove(UISystem::UI_CLICK_KEY);
    Entity w = 0; bool* target = nullptr; const char* bb_key = nullptr;
    if (menu_click == "on_toggle_shake") {
        w = widget_by_name("settings_shake", shake_w, shake_w_resolved);
        target = &settings.screen_shake; bb_key = "settings.screen_shake";
    } else if (menu_click == "on_toggle_minimap") {
        w = widget_by_name("settings_minimap", mini_w, mini_w_resolved);
        target = &settings.minimap; bb_key = "settings.minimap";
    } else {
        w = widget_by_name("settings_analytics", analytics_w, analytics_w_resolved);
        target = &settings.analytics;
    }
    if (auto st = component_storage.get_component<UIState>(w); st.has_value()) {
        const bool on = st->get().value >= 0.5f;
        *target = on;
        if (bb_key != nullptr) blackboard.set<bool>(bb_key, on);
        settings_write(settings_save_path(), settings);
    }
}
```

- [ ] **Step 5: Disclosure line on the name-entry screen** — in `assets/GameData.json`'s `name_entry` screen, after the `name_entry_msg` widget (rect `x:204 y:300 w:392 h:24`), add:

```json
{
  "element_type": "label",
  "rect": { "x": 204, "y": 244, "w": 392, "h": 24 },
  "label_text": "Anonymous gameplay stats are sent to improve the game (SETTINGS to opt out)",
  "style_id": "caption",
  "z_order": 10
}
```

Verify no rect collision with existing name_entry widgets (read the screen's widget list; adjust y downward if occupied).

- [ ] **Step 6: `PRIVACY.md`** at repo root:

```markdown
# Reactor Drone — Privacy

Reactor Drone sends two kinds of data to its server (a Cloudflare Worker):

- **Leaderboard**: a random player ID generated on your machine, the pilot
  name you chose, and your run scores.
- **Gameplay analytics** (anonymous): per-run summaries — score, wave
  reached, difficulty, ship, where the drone flew and died (as a coarse
  32x32 grid), what was bought and used, which menus were opened, and
  aggregate combat counters. No account, no email, no hardware IDs, no
  IP-based profiles. Reports are keyed only by the same random ID.

Opt out any time: SETTINGS -> ANALYTICS off. The leaderboard only receives
scores if you registered a pilot name (ESC on the name screen skips it).

Delete your data: the ID lives in `saves/meta.json`; deleting it severs the
link to past reports. For removal of server rows, open an issue on the
GitHub repository with your pilot name.
```

- [ ] **Step 7: Stage it in the installer** — in `installer/package-win.sh`, where files are copied into `installer/stage/` (read the script; it stages the exe + assets), add `cp "$ROOT/PRIVACY.md" "$STAGE/"` following the script's existing variable names. The `.iss` needs no edit — `[Files]` already ships `stage\*` recursively.

- [ ] **Step 8: Build, run `[telemetry]` + `[settings]` tests, then a quick visual check** (needs a display): open SETTINGS, toggle ANALYTICS off, quit, confirm `saves/settings.json` contains `"analytics":false`, relaunch, confirm the checkbox shows off.

- [ ] **Step 9: Commit** — `git add -A && git commit -m "feat: analytics consent - settings toggle, disclosure line, PRIVACY.md"`

---

### Task 6: `analytics/report.py`

**Files:**
- Create: `analytics/report.py`
- Create: `analytics/.gitignore` (containing `out/`)

**Interfaces:**
- Consumes: the `runs` table via `npx wrangler d1 execute reactor-drone-db --remote --json` (run from `backend/`, where wrangler + the D1 binding live). Pillow (already a dev dependency for `assets/generator/v2/make_sprites.py`).
- Produces: `analytics/out/heat_<arena>.png`, `analytics/out/death_<arena>.png`, and a stdout report.

- [ ] **Step 1: Write `analytics/report.py`:**

```python
#!/usr/bin/env python3
"""Telemetry report: heatmaps + tables from the runs table.

Usage: python analytics/report.py            # full report into analytics/out/
Needs: backend/ wrangler auth (npx wrangler login), Pillow.
"""
import base64
import collections
import json
import pathlib
import subprocess

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = pathlib.Path(__file__).resolve().parent / "out"
DB = "reactor-drone-db"
DIM = 32
SCALE = 16  # 32x32 -> 512x512 PNG


def q(sql):
    r = subprocess.run(
        ["npx", "wrangler", "d1", "execute", DB, "--remote", "--json", "--command", sql],
        capture_output=True, text=True, cwd=ROOT / "backend", check=True)
    return json.loads(r.stdout)[0]["results"]


def heat_png(grid, path, tint):
    peak = max(grid) or 1
    img = Image.new("RGB", (DIM, DIM))
    img.putdata([tuple(int(c * v / peak) for c in tint) for v in grid])
    img.resize((DIM * SCALE, DIM * SCALE), Image.NEAREST).save(path)


def main():
    OUT.mkdir(exist_ok=True)
    rows = q("SELECT outcome, wave, score, dur_s, body FROM runs")
    if not rows:
        print("no runs yet")
        return
    bodies = [json.loads(r["body"]) for r in rows]

    # --- outcome split -------------------------------------------------
    n = len(rows)
    print(f"runs: {n}")
    for oc, c in collections.Counter(r["outcome"] for r in rows).most_common():
        print(f"  {oc:8} {c:5}  {100 * c / n:.0f}%")

    # --- wave-reached histogram (death walls show as spikes) -----------
    print("\nwave reached (deaths only):")
    deaths = collections.Counter(r["wave"] for r in rows if r["outcome"] == "death")
    for w in sorted(deaths):
        print(f"  w{w:3} {'#' * deaths[w]} {deaths[w]}")

    # --- killer table --------------------------------------------------
    print("\nkilled by:")
    for k, c in collections.Counter(
            b["death"]["killed_by"] for b in bodies if "death" in b).most_common():
        print(f"  {k or '?':12} {c}")

    # --- occupancy + death heatmaps per arena --------------------------
    occ = collections.defaultdict(lambda: [0] * (DIM * DIM))
    dth = collections.defaultdict(lambda: [0] * (DIM * DIM))
    for b in bodies:
        for arena, b64grid in b.get("heat", {}).items():
            g = base64.b64decode(b64grid)
            acc = occ[arena]
            for i, v in enumerate(g):
                acc[i] += v
    # Death positions bin with the same transform the client used; the
    # arena circle is shared geometry, so read centre/radius from any
    # client build - hardcoded here to match GameData.json's arena.
    # (If heat_bin's constants change client-side, update these.)
    for b in bodies:
        d = b.get("death")
        if not d:
            continue
        # bin against the LAST arena the run visited
        arenas = sorted(b.get("heat", {}).keys())
        if arenas:
            dth[arenas[-1]][min(DIM * DIM - 1, max(0, d.get("bin", 0)))] += 1
    for arena, grid in occ.items():
        heat_png(grid, OUT / f"heat_{arena}.png", (0, 255, 128))
    for arena, grid in dth.items():
        heat_png(grid, OUT / f"death_{arena}.png", (255, 40, 40))
    print(f"\nheatmaps -> {OUT}/")

    # --- economy: purchase rate vs survival ----------------------------
    print("\nupgrade row: buy-rate, mean wave reached (buyers vs non-buyers):")
    for i in range(8):
        buyers = [b for b in bodies if b["econ"]["upg_counts"][i] > 0]
        rest = [b for b in bodies if b["econ"]["upg_counts"][i] == 0]
        mw = lambda xs: sum(x["wave"] for x in xs) / len(xs) if xs else 0.0
        print(f"  upg{i}: {len(buyers):4}/{n}  wave {mw(buyers):5.1f} vs {mw(rest):5.1f}")

    # --- UI funnel -----------------------------------------------------
    print("\nui opens (mean per run):")
    keys = sorted({k for b in bodies for k in b.get("ui", {})})
    for k in keys:
        print(f"  {k:12} {sum(b.get('ui', {}).get(k, 0) for b in bodies) / n:.2f}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run it** against whatever live rows exist (Task 1's test.sh rows + Task 4's live run):

```bash
python analytics/report.py
```

Expected: the tables print; `analytics/out/*.png` exist for any arena with heat data.

- [ ] **Step 3: Commit** — `git add analytics/ && git commit -m "feat: analytics/report.py - heatmaps + balance tables from D1"`

---

### Task 7: Verification + context sync

**Files:**
- Modify: `agentProjectDocs/progress-tracker.md`, `agentProjectDocs/decisions.md`, `agentProjectDocs/project-overview.md`, `CLAUDE.md` (next decision id), `agentProjectDocs/specs/telemetry.md` (status → Done)

- [ ] **Step 1: Full verification sweep:**

```bash
cmake --build CPP/build -j$(nproc) 2>&1 | tee /tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2-distribution/d7902a94-89dd-429d-aad9-6cff8981f7e4/scratchpad/build.log | grep warning:   # only Lua tmpnam
python runTestsAll.py                                        # engine + game, 100%
# canary (twice, diff) — as Task 4 Step 8
cd backend && BASE="https://reactor-drone-api.conradmiszczak.workers.dev" KEY="$(cat GAME_KEY.local)" ./test.sh
```

- [ ] **Step 2: Live end-to-end** (needs a window): one real run to death with ANALYTICS on → confirm the `runs` row and `scores` row (Task 4 Step 7 queries); one run with ANALYTICS off → confirm no new row.

- [ ] **Step 3: Context sync:**
- `decisions.md`: append **D196** — global telemetry: per-run summary blob over event streams (D1 100K rows/day; every question asked is a per-run aggregate), on-by-default consent with an Options toggle, Blackboard `tm.*` counters under the hit_bearing write-only precedent, `/score` wired at bank_run_score (was never called). Rejected: Workers Analytics Engine (new binding + no join to players for zero benefit at this scale), per-event rows, offline retry queue (YAGNI until online-vs-total counts diverge).
- `CLAUDE.md`: bump "next free id" to **D197**.
- `progress-tracker.md`: Current Phase entry — what shipped, what was verified (name each verification actually run; a window playtest of the toggle + one live death is required before claiming done), and the standing note that the two POSTs now fire.
- `project-overview.md` → Features: one line.
- `specs/telemetry.md`: Status → Done.

- [ ] **Step 4: Final commit** — `git add -A && git commit -m "docs: telemetry shipped - D196, tracker, overview"`

---

## Execution notes

- **Order:** 1 → 2 → 3 → 5 → 4 → 6 → 7 (Task 5 before 4: the POST guard reads `settings.analytics`).
- Line numbers reference commit `5259039`; always re-anchor by the quoted code.
- Anything needing a real window (Task 5 Step 8, Task 7 Step 2) is flagged; everything else is headless-verifiable.
