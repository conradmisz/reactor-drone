# In-Game Feedback Reports Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A feedback form (subject, body, optional tags, optional from) reachable from the pause menu and the main menu, POSTing to the existing Worker with auto-attached build/player/run context, stored in a `feedback` D1 table with a server-side timestamp. Spec: `agentProjectDocs/specs/feedback-reports.md`.

**Architecture:** A `PHASE_FEEDBACK` screen reusing PHASE_NAME_ENTRY's proven plumbing — `SDL_StartTextInput` + `ui.text_input` blackboard chars, single in-flight future, by-name widget rewrites. Entered from the title like the leaderboard (CLEAR_TO) or pushed over the pause screen (CMD_PUSH); a `fb_from_pause` flag routes ESC back to the right place. Backend: one flat-column table (the table IS the future AI export) + one validated route.

**Tech Stack:** C++17/SDL3, nlohmann::json, `net::post_json`, Cloudflare Worker + D1.

## Global Constraints

- Zero warnings (`-Wall -Wextra -Wpedantic`); only Lua's `tmpnam` allowed.
- ctest 8/8; replay canary (`--seed 42 --keys 5:SPACE --stopframe 3000` twice) byte-identical — the screen is unreachable when `net::enabled()` is false, so headless never sees it.
- No new components (Invariant 6); no engine changes.
- SUBJECT ≤120, BODY ≤4000, TAGS ≤200, FROM ≤60; printable ASCII everywhere except BODY, which also allows `\n`.
- Submitting is consent — NOT gated on `settings.analytics`. Gated on `net::enabled()` only.
- After any GameData.json edit: `python3 -c "import json;json.load(open('assets/GameData.json'))"`.
- Commit after every task with the session trailer used all day.

---

### Task 1: Backend — `feedback` table + `POST /feedback`

**Files:**
- Modify: `backend/schema.sql`, `backend/src/worker.js`, `backend/test.sh`

**Interfaces:**
- Produces: `POST /feedback` — headers `X-Game-Key`; JSON body ≤8192 bytes with required `subject` (string 1..120), `body` (string 1..4000), `player_id` (uuid), `version` (string ≤32), `platform` (`win|linux|mac`), `session_id` (string ≤64), `in_run` (bool); optional-but-typed `tags` (string ≤200, default ''), `from_name` (string ≤60, default ''), `pilot` (string ≤40, default ''); run-state `wave`/`score`/`ship`/`prestige` (ints) + `difficulty` (string ≤32) required when `in_run`, must be absent-or-null when not. Responses 200 `{ok:true}` / 400 / 401.

- [ ] **Step 1: schema.sql** — append:

```sql
CREATE TABLE IF NOT EXISTS feedback (
  id         INTEGER PRIMARY KEY,
  ts         INTEGER NOT NULL DEFAULT (unixepoch()),
  subject    TEXT    NOT NULL,
  body       TEXT    NOT NULL,
  tags       TEXT    NOT NULL DEFAULT '',
  from_name  TEXT    NOT NULL DEFAULT '',
  player_id  TEXT    NOT NULL,
  pilot      TEXT    NOT NULL DEFAULT '',
  version    TEXT    NOT NULL,
  platform   TEXT    NOT NULL,
  session    TEXT    NOT NULL,
  in_run     INTEGER NOT NULL,
  wave INTEGER, score INTEGER, ship INTEGER, prestige INTEGER, difficulty TEXT
);
CREATE INDEX IF NOT EXISTS idx_feedback_ts ON feedback(ts);
```

- [ ] **Step 2: worker.js route** — beside `/telemetry`. Add `const MAX_FEEDBACK_BYTES = 8192;` and `const PLATFORMS = new Set(['win','linux','mac']);` next to the other constants, then before the 404:

```js
      // Explicit player action — deliberately not gated on any consent flag
      // beyond the submit itself. Flat columns: this table is the AI export.
      if (req.method === 'POST' && url.pathname === '/feedback') {
        if (!safeEqual(req.headers.get('X-Game-Key'), env.GAME_KEY)) return json({ error: 'unauthorized' }, 401);
        const len = req.headers.get('content-length');
        if (len && Number(len) > MAX_FEEDBACK_BYTES) return json({ error: 'bad_request' }, 400);
        const raw = await req.text();
        if (raw.length > MAX_FEEDBACK_BYTES) return json({ error: 'bad_request' }, 400);
        const b = JSON.parse(raw); // SyntaxError -> catch -> 400
        // Body allows \n; everything else is single-line printable ASCII.
        const line = (v, min, max) => typeof v === 'string' && v.length >= min && v.length <= max &&
                                      (v === '' || NAME_RE.test(v));
        const text = (v, min, max) => typeof v === 'string' && v.length >= min && v.length <= max &&
                                      /^[\x20-\x7e\n]*$/.test(v) && v.trim().length >= min;
        const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
        if (!line(b.subject, 1, 120) || b.subject.trim().length === 0 || !text(b.body, 1, 4000) ||
            !line(b.tags ?? '', 0, 200) || !line(b.from_name ?? '', 0, 60) ||
            typeof b.player_id !== 'string' || !UUID_RE.test(b.player_id) ||
            !line(b.pilot ?? '', 0, 40) || !line(b.version, 1, 32) ||
            !PLATFORMS.has(b.platform) || !line(b.session_id, 1, 64) ||
            typeof b.in_run !== 'boolean')
          return json({ error: 'bad_request' }, 400);
        if (b.in_run && (!int(b.wave, 0, 999) || !int(b.score, 0, 10_000_000) ||
                         !int(b.ship, -1, 99) || !int(b.prestige, 0, 99) || !line(b.difficulty, 1, 32)))
          return json({ error: 'bad_request' }, 400);
        await env.DB.prepare(
          `INSERT INTO feedback (subject, body, tags, from_name, player_id, pilot, version, platform,
                                 session, in_run, wave, score, ship, prestige, difficulty)
           VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)`)
          .bind(b.subject.trim(), b.body, b.tags ?? '', b.from_name ?? '', b.player_id, b.pilot ?? '',
                b.version, b.platform, b.session_id, b.in_run ? 1 : 0,
                b.in_run ? b.wave : null, b.in_run ? b.score : null, b.in_run ? b.ship : null,
                b.in_run ? b.prestige : null, b.in_run ? b.difficulty : null).run();
        return json({ ok: true });
      }
```

- [ ] **Step 3: test.sh cases** (before the dashboard block; reuse `$U1`):

```bash
# feedback
FB="{\"subject\":\"Boss too hard\",\"body\":\"wave 9 boss\\nmelts me\",\"tags\":\"balance,boss\",\"from_name\":\"conrad\",\"player_id\":\"$U1\",\"pilot\":\"testconrad\",\"version\":\"2.0.0\",\"platform\":\"linux\",\"session_id\":\"s1\",\"in_run\":true,\"wave\":9,\"score\":4200,\"ship\":1,\"prestige\":0,\"difficulty\":\"Normal\"}"
[ "$(c -XPOST "$BASE/feedback" -H "$J" -d "$FB")" = 401 ]                                # no key
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "$FB")" = 200 ]
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"menu note\",\"body\":\"more ships pls\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s2\",\"in_run\":false}")" = 200 ]   # optionals absent
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s\",\"in_run\":false}")" = 400 ]  # empty subject
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"s\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"amiga\",\"session_id\":\"s\",\"in_run\":false}")" = 400 ]  # bad platform
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"s\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s\",\"in_run\":true}")" = 400 ]  # in_run without run state
```

- [ ] **Step 4: verify locally** — reset local D1 (`find .wrangler -name "*.sqlite*" -delete`; apply `schema.sql`), `npx wrangler dev --local --port 8765`, run `BASE=http://127.0.0.1:8765 KEY=$(cat GAME_KEY.local) ./test.sh` → ALL PASS. Then `SELECT subject, in_run, wave, ts FROM feedback` shows both rows, one with wave 9, one with NULL, both with a real `ts`.

- [ ] **Step 5: commit** — `git add backend/ && git commit -m "feat(backend): /feedback route + feedback table"`.

---

### Task 2: GameData.json — feedback screen + two FEEDBACK buttons

**Files:**
- Modify: `assets/GameData.json`

**Interfaces:**
- Produces widget names main.cpp reads/rewrites: `fb_subject`, `fb_body`, `fb_tags`, `fb_from` (value labels), `fb_msg` (status line), buttons `pause_feedback` (`on_feedback_click`), `menu_feedback` (`on_feedback_click`). Screen key: `feedback`.

- [ ] **Step 1: the `feedback` screen** — add to `"screens"` (window is 980x660, UI y is bottom-up; panel matches name_entry's 180/64/620/472):

```json
"feedback": {
  "widgets": [
    { "element_type": "panel", "rect": { "x": 180, "y": 64, "w": 620, "h": 472 }, "label_text": "", "z_order": 9 },
    { "element_type": "label", "rect": { "x": 204, "y": 452, "w": 392, "h": 44 }, "label_text": "FEEDBACK", "style_id": "title", "z_order": 10 },
    { "element_type": "panel", "rect": { "x": 204, "y": 440, "w": 552, "h": 2 }, "label_text": "", "z_order": 10 },
    { "element_type": "label", "rect": { "x": 204, "y": 396, "w": 100, "h": 24 }, "label_text": "SUBJECT", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "name": "fb_subject", "rect": { "x": 316, "y": 396, "w": 440, "h": 24 }, "label_text": "_", "style_id": "subtitle", "z_order": 10 },
    { "element_type": "label", "rect": { "x": 204, "y": 356, "w": 100, "h": 24 }, "label_text": "BODY", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "name": "fb_body", "rect": { "x": 316, "y": 260, "w": 440, "h": 120 }, "label_text": "", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "rect": { "x": 204, "y": 216, "w": 100, "h": 24 }, "label_text": "TAGS", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "name": "fb_tags", "rect": { "x": 316, "y": 216, "w": 440, "h": 24 }, "label_text": "", "style_id": "subtitle", "z_order": 10 },
    { "element_type": "label", "rect": { "x": 204, "y": 176, "w": 100, "h": 24 }, "label_text": "FROM", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "name": "fb_from", "rect": { "x": 316, "y": 176, "w": 440, "h": 24 }, "label_text": "", "style_id": "subtitle", "z_order": 10 },
    { "element_type": "label", "name": "fb_msg", "rect": { "x": 204, "y": 128, "w": 552, "h": 24 }, "label_text": "TAB next field - ENTER send - ESC back", "style_id": "caption", "z_order": 10 },
    { "element_type": "label", "rect": { "x": 204, "y": 96, "w": 552, "h": 24 }, "label_text": "Sent with game version, platform and (mid-run) wave/score.", "style_id": "caption", "z_order": 10 }
  ]
}
```

- [ ] **Step 2: pause row 4→5 buttons** — the row at y=36 (x/w: resume 164/110, save 286/110, menu 408/110, quit 530/106) becomes five at w=86, x = 164, 258, 352, 446, 540. Rewrite the four existing rects and insert after `pause_quit`:

```json
{ "element_type": "button", "name": "pause_feedback", "rect": { "x": 540, "y": 36, "w": 86, "h": 44 },
  "label_text": "FEEDBACK", "style_id": "default_button", "z_order": 10, "on_click_fn": "on_feedback_click" }
```

(order after re-rect: resume 164, save 258, menu 352, quit 446, feedback 540. UIRenderSystem overflow-shrink handles the longer caption.)

- [ ] **Step 3: main-menu QUIT row split** — `menu_quit` is x 204 w 392 at y 180; change to w 188 and insert beside it:

```json
{ "element_type": "button", "name": "menu_feedback", "rect": { "x": 408, "y": 180, "w": 188, "h": 44 },
  "label_text": "FEEDBACK", "style_id": "default_button", "z_order": 10, "on_click_fn": "on_feedback_click" }
```

- [ ] **Step 4: validate** — `python3 -c "import json;json.load(open('assets/GameData.json'))"` then dump both screens' widget y/x to confirm no rect collisions (the settings-screen dump one-liner from Task 5 of the telemetry work).

- [ ] **Step 5: commit** — `git add assets/GameData.json && git commit -m "feat(ui): feedback screen + FEEDBACK buttons (pause, title)"`.

---

### Task 3: main.cpp — PHASE_FEEDBACK state machine + POST

**Files:**
- Modify: `CPP/game/main.cpp`

**Interfaces:**
- Consumes: Task 1's `POST /feedback` body shape; Task 2's widget names; existing `session_id`, `meta`, `selected_ship`, `run_difficulty` label on the blackboard (`"difficulty"`), `wave`/`score` blackboard ints, `widget_by_name`, `net::post_json`, `abandon_future`, `GAME_VERSION`.
- Produces: `RD_PLATFORM` string constant; `on_feedback_click` handled in BOTH the pause-click block (~1599) and the menu-click block (~2280s).

- [ ] **Step 1: constants** — extend the Phase enum: `PHASE_FEEDBACK = 8`; add `constexpr const char* SCREEN_FEEDBACK = "feedback";` beside the other SCREEN_*; near the telemetry state add:

```cpp
#if defined(_WIN32)
    constexpr const char* RD_PLATFORM = "win";
#elif defined(__APPLE__)
    constexpr const char* RD_PLATFORM = "mac";
#else
    constexpr const char* RD_PLATFORM = "linux";
#endif
```

- [ ] **Step 2: state** (beside `name_buf`):

```cpp
    // Feedback form (specs/feedback-reports.md). One live future, name_entry's
    // plumbing. fb_fields: 0=subject 1=body 2=tags 3=from.
    std::string fb_fields[4];
    int fb_focus = 0;
    bool fb_from_pause = false;      // routes ESC: pop-to-pause vs back-to-title
    int fb_prev_phase = PHASE_TITLE; // restored on exit when fb_from_pause
    std::string fb_msg;              // '' = show the default hint line
    std::future<net::Response> pending_feedback;
    nlohmann::json fb_ctx;           // run context captured AT OPEN, not at send
```

- [ ] **Step 3: opening — pause entry.** In the pause-click chain (after `on_save_run_click`'s block, before `on_quit_click`), add:

```cpp
            } else if (pause_click == "on_feedback_click") {
                blackboard.remove(UISystem::UI_CLICK_KEY);
                fb_from_pause = true;
                fb_prev_phase = phase;
                // Run context is captured at OPEN — the numbers the player is
                // looking at — not at send, when the form may outlive the wave.
                fb_ctx = {{"in_run", true},
                          {"wave", blackboard.get_or<int>("wave", 0)},
                          {"score", blackboard.get_or<int>("score", 0)},
                          {"ship", selected_ship},
                          {"prestige", meta.prestige},
                          {"difficulty", blackboard.get_or<std::string>("difficulty", "Normal")}};
                fb_focus = 0; fb_msg.clear();
                phase = PHASE_FEEDBACK;   // sim block only runs in PHASE_PLAYING
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                            std::string(SCREEN_FEEDBACK));
                SDL_StartTextInput(window.get());
            }
```

Gate the button's visibility where the pause screen opens (the `CMD_PUSH(SCREEN_PAUSE)` site, ~1564): after the SAVE-label reset, rewrite `pause_feedback`'s label to `""` when `!net::enabled()` — offline hides the caption; clicks on an empty button are ignored by checking `net::enabled()` in the handler too (`if (... && net::enabled())`).

- [ ] **Step 4: opening — title entry.** In the menu-click chain (beside `on_records_click`), add the same block with `fb_from_pause = false;`, `fb_ctx = {{"in_run", false}};`, `phase = PHASE_FEEDBACK;` and `CMD_CLEAR_TO(SCREEN_FEEDBACK)` instead of PUSH. Hide/no-op exactly like the leaderboard's `L` (gated on `net::enabled()`).

- [ ] **Step 5: the PHASE_FEEDBACK frame block** — beside the `PHASE_NAME_ENTRY` block (~2466), same position in the frame:

```cpp
            } else if (phase == PHASE_FEEDBACK) {
                constexpr size_t FB_CAPS[4] = {120, 4000, 200, 60};
                const std::string typed =
                    blackboard.get_or<std::string>("ui.text_input", std::string());
                std::string& cur = fb_fields[fb_focus];
                for (char c : typed) {
                    if (cur.size() >= FB_CAPS[fb_focus]) break;
                    if (c >= 0x20 && c < 0x7F) cur.push_back(c);
                }
                if (blackboard.get_or<bool>("ui.backspace_pressed", false) && !cur.empty())
                    cur.pop_back();
                if (blackboard.get_or<bool>("ui.tab_pressed", false))
                    fb_focus = (fb_focus + 1) % 4;

                // Poll last frame's submit before reading enter/escape (the
                // name_entry ordering — a same-frame response never races input).
                if (pending_feedback.valid() &&
                    pending_feedback.wait_for(std::chrono::seconds(0)) ==
                        std::future_status::ready) {
                    const net::Response resp = pending_feedback.get();
                    if (resp.status == 200) {
                        for (auto& f : fb_fields) f.clear();
                        fb_msg = "Thanks - received!";
                    } else {
                        // Typed content is kept: a retry must not lose their words.
                        fb_msg = "Couldn't send - check your connection";
                    }
                }

                const bool enter = blackboard.get_or<bool>("ui.enter_pressed", false);
                if (enter && fb_focus == 1) {
                    if (cur.size() < FB_CAPS[1]) cur.push_back('\n');   // ENTER in BODY = newline
                } else if (enter && !pending_feedback.valid()) {
                    if (fb_fields[0].find_first_not_of(' ') == std::string::npos)
                        fb_msg = "Subject is required";
                    else if (fb_fields[1].find_first_not_of(" \n") == std::string::npos)
                        fb_msg = "Body is required";
                    else {
                        nlohmann::json j = fb_ctx;
                        j["subject"] = fb_fields[0]; j["body"] = fb_fields[1];
                        j["tags"] = fb_fields[2];    j["from_name"] = fb_fields[3];
                        j["player_id"] = meta.player_id;
                        j["pilot"] = meta.registered ? meta.player_name : "";
                        j["version"] = GAME_VERSION; j["platform"] = RD_PLATFORM;
                        j["session_id"] = session_id;
                        fb_msg = "Sending...";
                        pending_feedback = net::post_json(
                            std::string(net::NET_BASE) + "/feedback", j.dump(),
                            net::NET_GAME_KEY);
                    }
                }

                if (blackboard.get_or<bool>("ui.escape_pressed", false)) {
                    abandon_future(std::move(pending_feedback));
                    SDL_StopTextInput(window.get());
                    for (auto& f : fb_fields) f.clear();
                    fb_msg.clear();
                    if (fb_from_pause) {
                        phase = fb_prev_phase;                      // back under the pause screen
                        blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                    } else {
                        phase = PHASE_TITLE;
                        blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                    std::string(SCREEN_MAIN_MENU));
                    }
                }
            }
```

Blackboard keys to verify at execution: `ui.tab_pressed` / `ui.enter_pressed` — grep InputSystem; if TAB isn't published, publish is a 2-line addition in `input_system.cpp` following `ui.backspace_pressed`'s exact pattern (write-only UI key, no sim reader — canary-safe).

- [ ] **Step 6: widget rewrites** — beside the name_entry widget-rewrite block, add per-frame rewrites when `phase == PHASE_FEEDBACK`: each field label gets its text plus `_` appended on the focused one; `fb_body` shows the LAST 3 lines that fit (split on `\n`, take tail) — full text still sends; `fb_msg` shows `fb_msg` when non-empty else the TAB/ENTER/ESC hint. Cache entities via `widget_by_name` with `fb_*_w`/`fb_*_w_resolved` statics like `shake_w`.

- [ ] **Step 7: ESC exclusion + resync** — add `phase != PHASE_FEEDBACK` to the pause-toggle ESC condition (~1562, beside `PHASE_NAME_ENTRY`/`PHASE_LEADERBOARD`), and add `PHASE_FEEDBACK → SCREEN_FEEDBACK` to the screen-resync ternary at ~1307 if it enumerates phases (read it first).

- [ ] **Step 8: build + tests** — warning-free build, ctest 8/8.

- [ ] **Step 9: commit** — `git add CPP/game/ && git commit -m "feat: feedback form screen wired to /feedback from pause + title"`.

---

### Task 4: PRIVACY.md + live walkthrough

**Files:**
- Modify: `PRIVACY.md`

- [ ] **Step 1: PRIVACY.md** — after the analytics bullet add:

```markdown
- **Feedback reports** (only when you press send): the subject, message,
  tags and name you typed, plus game version, platform, and — if sent
  mid-run — the current wave/score/ship/difficulty. Keyed by the same
  random ID.
```

- [ ] **Step 2: walkthrough on DISPLAY=:1 against local wrangler dev** — repoint `net_config.hpp` to `127.0.0.1:8765`, rebuild (the localhost-trap discipline: revert AND rebuild AND `strings`-check after). Back up `saves/meta.json` first; restore after. Drive with `--clicks` for buttons and `xdotool type`/`key` for text (scripted `--keys` has no letter vocabulary — the D197 technique):
  1. title → FEEDBACK → type subject/TAB/body/TAB/tags/TAB/from → ENTER → "Thanks - received!" → row lands with `in_run=0`, NULL wave.
  2. start a run → ESC → FEEDBACK → submit → row with `in_run=1` and the live wave/score.
  3. kill wrangler dev → submit → "Couldn't send" AND the typed text still on screen; ESC exits cleanly.
  4. Screenshot each state; read them.
- [ ] **Step 3: revert net_config, rebuild, `strings` check** (zero `127.0.0.1` refs), restore `saves/meta.json`, delete test logs.
- [ ] **Step 4: commit** — `git add PRIVACY.md && git commit -m "docs: feedback reports privacy line"`.

---

### Task 5: Verification + context sync

- [ ] **Step 1: canary** — twice, byte-identical, AND identical to the session baseline (`$S/canary1.txt`).
- [ ] **Step 2: full ctest + warning grep.**
- [ ] **Step 3: docs** — decisions.md D200 (entry points, capture-at-open, consent-by-action, flat-column AI-export rationale, what was rejected), tracker update, CLAUDE.md id bump to D201.
- [ ] **Step 4: commit** — `git add agentProjectDocs CLAUDE.md && git commit -m "docs: feedback reports decision + tracker"`.

## Execution notes

- Prod deploy of the `feedback` table rides the SAME pending migration as telemetry: `wrangler d1 execute reactor-drone-db --remote --file schema.sql && wrangler deploy` — one command covers both once Conrad runs it.
- The pause-freeze check keys off `stack.back() == SCREEN_PAUSE`; feedback-from-pause avoids the issue entirely by leaving PHASE_PLAYING (the sim block is phase-gated), so do NOT touch `menu_paused`.
