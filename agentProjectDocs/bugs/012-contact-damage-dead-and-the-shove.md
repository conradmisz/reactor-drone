---
id: 012
title: Contact damage was dead for the whole v2.3 pack, and the "solidity" fix was a repulsion field
status: resolved
severity: high
resolved: 2026-08-16
---

## Summary

Two bugs in one piece of code, both introduced by this pack's tier 4
("the drone no longer passes through enemies"), both found only by the owner
**playing the game** — every automated gate stayed green through both.

**Bug A — contact damage never fired.** The player/enemy separation ran at
`main.cpp` ~L3016; `CollisionSystem` runs at ~L3091. The drone was pushed clear
before collision looked, so `CollidedWith` never listed the enemy,
`PlayerDamageSystem` never saw it, and **bumping an enemy did no damage at all**
from tier 4 until this fix. Order is now: move -> collide -> damage -> separate.

**Bug B — the separation was a repulsion field.** It pushed by
`overlap + 2px` on *every* contact frame, so a standing drone was walked
through the swarm, eating contact damage from body after body. Discovered only
because fixing bug A made the replay canary die instantly.

## Measured, not guessed

Scripted canary (`--seed 42`, fire-only, never moves), final score:

| Build | Score |
| --- | --- |
| solidity disabled entirely | 115 |
| solidity with the `+2px` bonus shove | **0** |
| solidity separating by exact overlap | 10 |

Four seeds were checked before blaming the arena shuffle: seeds 42/7/11/99 all
died in wave 1, including seed 99, which opens on plain Core. The shuffle was
ruled out that way, not by argument.

## Resolution

Separation moved after collision+damage; separation is now exactly the overlap;
the spec's "small bounce" fires only on the frame a **dash** ends on top of an
enemy. Canary re-baselined a third time (D227) to
`Frames: 3000  Final score: 10  Units: 0  Wave: 1  Phase: 2`.

## Why the gates missed it

Nothing asserts "an enemy touching the drone removes hull". The canary summary
line (score/units/wave/phase) *did* change when contact damage died — but that
same tier deliberately re-baselined the canary for the arena shuffle (D223), so
the regression hid inside an expected change. **A tier that re-baselines the
canary cannot also rely on the canary.** Consider a unit test on
PlayerDamageSystem the next time this area is touched.

## Still open

- The canary now ends in a death run (Phase 2, wave 1), so it exercises little
  past the first wave. Scripting movement into it would restore coverage but
  changes the documented command in CLAUDE.md — owner's call.
- Whether solidity's difficulty jump is right for a player who moves is
  unjudged; playtest #2 is the test.
