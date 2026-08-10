#!/bin/bash
# ponytail: this script + one transient systemd timer is the whole scheduler.
# Add a real unit file only if it needs to survive reboots or run more than once.
cd /home/conrad/Documents/GameEngines/reactor-drone-v2 || exit 1
exec /home/conrad/.local/bin/claude -p --permission-mode bypassPermissions \
  --model claude-opus-5 \
  "Read HANDOFF.md and follow it. Re-establish the baseline first (build with zero warnings, python3 runTestsAll.py 8/8, the twice-identical headless replay canary). Then work through ALL iteration-5 lanes in HANDOFF.md to completion: implement each feature, keep the build warning-free, keep all tests green, and keep the replay canary byte-identical. Follow CLAUDE.md rules exactly - update ENGINE.md in the same commit as any engine change, append decisions from D105 onward to agentProjectDocs/decisions.md, and update progress-tracker.md as you go. Commit each lane on its own branch off master and merge into master when its tests pass. Do not push. Work autonomously; do not ask questions - if a choice is ambiguous, pick the option most consistent with existing code and record it as a decision. Finish by writing a summary of what shipped, what is left, and exactly which verification commands you ran with their output to HANDOFF-RESULT.md."
