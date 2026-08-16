# Handoff — Reactor Drone

You are picking up mid-flight. **Gameplay pack v2.3 (spec:
`agentProjectDocs/specs/gameplay-pack-v2.3.md`, plan:
`plans/gameplay-pack-plan.md`) is built through tier 8 of 9.** Tiers 0-8 are
committed on `feature/distribution`. Tier 9 (balance + WINDOWED PLAYTEST +
release v2.3.0) is what remains, and its playtest needs a human.

- Repo: **https://github.com/conradmisz/reactor-drone**, branch `feature/distribution`
  (worktree `~/Documents/GameEngines/reactor-drone-v2-distribution`).
- **Next free decision id: see CLAUDE.md** (D221-D225 are the pack's).

## Read first

1. `CLAUDE.md` — rules + the CURRENT canary line (re-baselined twice: D223, D224).
2. `agentProjectDocs/bugs/003-verification-traps.md` — how checks lie here.
3. `agentProjectDocs/progress-tracker.md` and `decisions.md` D220-D225.
4. `ENGINE.md` §4 — **entity iteration is SORTED now (D224)**; §5 traps.

## Re-establish the baseline before changing anything

```bash
cmake -B CPP/build -S CPP && cmake --build CPP/build -j$(nproc)   # zero warnings (Lua tmpnam exempt)
python3 runTestsAll.py                                            # 8/8, 409 unit cases
rm -f saves/settings.json
printf '{"best_wave":5,"lifetime_score":1305,"prestige":0,"runs_played":4}\n' > saves/meta.json
bash gate.sh .canary-baseline.txt
# Expected, TWICE, byte-identical:
# Shutting down. Frames: 3000  Final score: 160  Units: 20  Wave: 2  Phase: 1
```

## What the pack added (do not rebuild)

| Tier | Shipped |
|---|---|
| 1 | Scrap on meta.json (banked in `bank_run_score`); 4-ship roster with stats (Falcon/Owl/Gryphon + locked Gatling); first-class `WeaponDef`s (55 Iron/Moonshot/Flak/Hailstorm); equipped loadout persists (D222) |
| 2 | Per-weapon projectile size/pierce (Moonshot = wide piercing crescent); ship specials (`ship_specials.hpp`): Falcon equip_cd 0.75, Owl phoenix veil, Gryphon ram dash |
| 3 | Right-mouse secondaries (`secondary_fire.{hpp,cpp}`): charge shot / crescent burst / lava stream+Burn / blizzard+Chill; `hud_sec` gauge; `RMB` in `--keys` |
| 4 | Seeded arena shuffle (no Prism opener, Singularity pinned); player-enemy solidity (dash + veil excepted); early drop floor; sustain 1→4 pickups at wave 16, 1.5x size |
| 5 | Boss 2-phase enrage at 50% + unstick slide; reward waits for adds; actives trimmed to Heat-Seeking Missiles (reward_choices 1) |
| 6 | Hangar (run_setup): live-drone preview, aligned pip stat sheet (`hangar_stats.hpp`), weapon/ship cycle, BUY with scrap, green LAUNCH bottom-left; run_stats flight report before the prestige offer |
| 7 | Paints: 3 ship-granted + Gold/Crimson/Arctic (own atlases); per-item slots on meta.json; cosmetic shop + inventory screens. **Engine: `entities_with_component` SORTED (D224)** |
| 8 | GEAR/LEVELS retired (D225); ESC-in-shop fix; rear tracer trail; main-menu LEADERBOARD button; site disclaimers (docs/features.html) |

## Playtest #1 happened (2026-08-16) — batch D227

Nine items, all fixed and committed. It found two bugs no gate had: contact
damage had been DEAD since tier 4 (separation ran before collision), and the
separation's 2px bonus shove was a repulsion field. Full account in
`agentProjectDocs/bugs/012`. The canary was re-baselined a THIRD time and now
ends in a death run at wave 1.

## What tier 9 still needs (in order)

0. **PLAYTEST #2** — the D227 fixes are unjudged: bolts vs tracers, the charge
   bar and its 76px slug, the pause pip column, the hangar preview, and whether
   solid enemies make the game too punishing (a standing bot now dies in wave 1).

1. **The rest of the original list — still not played by a human.** Judge:
   weapon feel (all 4 primaries + 4 secondaries), collision bounce, veil/ram
   specials, boss enrage difficulty, scrap pacing (5/wave, +25 boss, +100 win;
   Owl 400 / Gryphon 800 / paints 150-250), hangar/shop/inventory click-through
   (drive_ui XTest CLICKS don't register under COSMIC — hover only — so this
   must be real mouse), the flight report on death AND victory, LEADERBOARD
   button, ESC-in-shop, rear trail read.
2. Tune the tables the playtest complains about (all in GameData.json:
   `scrap`, ship `scrap_cost`, weapon stats, `boss` enrage knobs, `sustain`).
3. Then release: GAME_VERSION + backend RELEASE_VERSION/INSTALLER_URL → 2.3.0,
   tag `v2.3.0`, watch CI (both build-mac jobs have still never run on real
   hardware).

## Standing cautions

- The canary line moved TWICE, deliberately (D223 tier 4, D224 tier 7). It must
  not move again this pack — if it does, that is a regression, not a tune.
- `verify_branch.sh` sections 5-7 exercise backend/portable/Windows; run the
  full script before the release cut.
- Owl and locked-Gatling share the violet atlas; Gatling needs its own art at
  its release (TODO in spec, with "more boss items" and the 4th-drone launch).
