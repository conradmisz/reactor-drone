# Design record — `docs/features.html`

Decisions behind the player-facing features page, captured 2026-08-09 so a later
session doesn't re-litigate them. The file's own `HOW TO UPDATE` header covers
mechanics of editing; this covers *why it is the way it is*.

## What it is

A **player's guide**. Someone reads it and knows how to play. Not a spec sheet,
not a portfolio piece. Engineering content (ECS, determinism, tests, file
layout) is deliberately absent and would go on a separate page if ever wanted.

## Interview decisions

| Question | Decision |
| --- | --- |
| Audience | Players / a showcase page — sells the game, teaches the game |
| Media | Placeholder slots now, real screenshots/clips dropped in later |
| Interactivity | Arena palette switcher only. No canvas toys, no data explorers |
| Staying current | Hand-authored; owner returns and says "the game updated", an agent edits it |
| Hosting | `docs/features.html` in the repo. A shareable hosted copy is a later, optional step |
| Depth | Gameplay and features overview — how to play, not how it's built |
| Structure | One long scroll with a sticky nav rail |
| Visual style | Neon-forward chrome, high-contrast readable body text |
| Reveal | Full reveal of all content, but tuning described in **relative** terms |
| Extras kept | Quick-start card, phone support, plain voice with a thin in-universe hero line |
| Extras dropped | A "what's not implemented yet" section |

## The relative-numbers rule

The single most important maintenance rule. Balance values in
`assets/GameData.json` are explicitly provisional — no full run has been played,
and the shop catalogue says so in its own comments. So the page says "costs climb
steeply each purchase", never "50 credits". Structural facts that rarely move
(20 waves, 4 arenas, 3 enemy archetypes, 6 upgrades, 4 items, 4 consumables,
1 item + 1 consumable slot) are stated exactly.

## Design system

- **Single committed theme.** The game is dark-only neon, so the page is too —
  no light mode, but every colour is painted explicitly rather than inherited,
  so it holds on any host background.
- **Four palettes** in `[data-arena]` token blocks, copied from
  `assets/generator/v2/palette.py`. If the palettes change there, change them
  here. The switcher sets one attribute on `<html>`; all colour flows from tokens.
- **Type roles:** heavy uppercase grotesque for display, system sans for body at
  ~66ch, monospace for every readout (keycaps, chips, wave numbers, captions).
  The game's own `assets/fonts/default.ttf` is pulled in via `@font-face` for
  readouts with a safe fallback stack.
- **The wave ladder in the rail is content, not decoration** — 20 ticks banded
  into four arenas of five, lighting up with the selected palette. Numbering in
  the run-loop section is likewise a real sequence.

## Known gaps

- **Never visually verified.** Headless Firefox failed to screenshot it on this
  machine; the markup and anchors were verified programmatically only.
- **All 7 media slots are placeholders.** Each carries a caption describing the
  moment it wants.
- Content reflects the game as of 2026-08-09: difficulty select (Normal/Hard),
  main menu, pause screen, 4 arenas, 20 waves, shop + gear.
  The `dash`, `minimap`, `boss`, `sustain` and `actives` blocks in
  `GameData.json` are inert iteration-3 scaffolding with HOOK comments in
  `main.cpp` — deliberately undocumented until they actually ship.
