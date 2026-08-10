# Design record — `docs/features.html`

Rebuilt 2026-08-10 (D135) after an owner interview. The file's own HOW TO
UPDATE header covers the mechanics of editing; this covers *why it is the way
it is*. The original 2026-08-09 page and its decisions are in git history.

## What it is

A **player's guide**. Someone reads it and knows how to play. Not a spec sheet,
not a portfolio piece. Engineering content is deliberately absent.

## Interview decisions (2026-08-10)

| Question | Decision |
| --- | --- |
| Complaint with old page | The generic neon aesthetic (empty media slots implicitly) |
| Scope | Full visual redesign; keep content, structure, prose voice |
| Direction | **Refined neon arcade** — evolve the identity: flat near-black ground, accent used sparingly, bespoke type. (Rejected: military tech-manual, art-first showcase) |
| Display face | **Orbitron** (owner pick over Chakra Petch/Michroma); body Space Grotesk, readouts JetBrains Mono |
| Dependencies | Vendor locally, page works offline — no CDN |
| Media | Real captures; stills + a few animated clips |
| Updatability | **JSON blob in the file**, rendered by vanilla JS (rejected: Python generator, hand-authored HTML) |
| Motion | Purposeful + bespoke: native CSS scroll reveals, rail charge bar, palette crossfade. No JS animation libs |
| Voice / numbers / structure | Keep current prose voice, keep the relative-numbers rule, keep the 12 sections |

## The relative-numbers rule (unchanged)

Balance values in `assets/GameData.json` are provisional, so the page says
"costs climb steeply", never "50 credits". Exact numbers only for structural
facts (30 waves, 9 arenas / 4 themes, 10 enemy types, 8 upgrades, 4 items,
4 consumables, 3 actives, 2 ships, bosses 10/20/30, shop every 5th wave,
1 item + 1 consumable). The bestiary gauges are relative 0–5 boxes, not stats.

## Design system

- **Tokens:** each arena authors 5 colours (`--neon --neon-2 --hazard --ground
  --ground-2`), anchored to `assets/generator/v2/palette.py`. Ink, lines,
  surfaces, edges and glows are all **derived** with `color-mix(in oklab …)`,
  so a palette change is a 5-line edit and all four palettes hold their weight.
  The colour tokens are `@property`-registered `<color>`s, so switching arenas
  crossfades instead of snapping.
- **Type roles:** Orbitron for display (h1/h2/brand), Space Grotesk body at
  ~68ch, JetBrains Mono for every readout. Vendored variable woff2, latin
  subsets, ~65 KB total (`docs/fonts/`, OFL — see LICENSE-OFL.txt).
- **Layout variety by section** (the anti-uniform-grid rule): bestiary rows
  with gauges for the archetypes, cards for moons/specialists/upgrades/gear,
  arena bands each painted in *its own* palette regardless of the switcher,
  tables for controls/reference. Sharp corners throughout; radius only on kbd.
- **Motion:** scroll-driven reveals + the rail's reactor-charge scroll bar,
  both behind `@supports (animation-timeline: …)` and
  `prefers-reduced-motion`. Browsers without support get the full static page.
- **The wave ladder is content:** 30 ticks banded by `waveBands` in the JSON
  blob, boss waves marked; it lights up with the selected palette.

## Media pipeline

Real captures in `docs/media/`, made headlessly — `SDL_VIDEODRIVER=dummy`
renders fine and `--screenshot N` writes BMPs. `docs/media/capture.sh` holds
both a generic probe mode and the exact `scenario` session used for the
shipped shots (seed 42; waves 1-5, shop open at frame 8000, buys, wave-10
boss ~14400).

To make scripted runs progress, capture sessions temporarily buff
`assets/GameData.json` — weapon damage 400 / fire rate 12-40 / spread 6.283
for the rush profile, start_health 400, contact damage 0.5, currency ×~20 —
then restore with `git checkout assets/GameData.json`. Consequence: **HUD
numbers in the shipped media are capture-buffed** (fat credit counts, /425
hull). Recapture after balance settles. Stills are PNG; clips are looping
animated WebP built with ImageMagick (`convert -delay 3.33 … -loop 0 out.webp`)
because ffmpeg is not installed here.

## Verification (2026-08-10)

Headless Chromium via Playwright (`~/.cache/ms-playwright`, installed for this
purpose — headless Firefox hangs on this machine): zero console errors, four
palettes, 1440px and 390px viewports, full-page scroll, bestiary/bands/tables
all rendered from the blob, both clips loading. Full-page screenshots show
reveal-dimmed sections below the fold; that is the scroll-animation pre-entry
state, not a bug.

## Known gaps

- Media HUD shows capture-buff numbers (see above).
- No print stylesheet, no presskit block (owner declined both).
- The `LEVELS` tab visible in the shop screenshots is a debug page; harmless
  in the shot, worth recropping if it ever confuses anyone.
