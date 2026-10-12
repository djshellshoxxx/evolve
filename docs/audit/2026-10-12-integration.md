# Evolve integration audit — 2026-10-12

## Branch disposition

- `fix/ci-configure-empty-arg-20261012`: merged via PR #15 on `main`.
- `claude/focused-dirac-wb88c7`, `claude/gracious-gates-kpf63o`, `feature/lab-games-orbs`, `fix/pinned-build-lifetime-audit`: no commits ahead of `main`. No merge needed.
- `feature/deep-progression`, `feature/evolution-steering-play`, `feature/haunted-lab-secrets`, `feature/steering-intensity`: diverged older branches; many changes overlap existing `main` features. Do not wholesale merge without a semantic diff.
- `claude/relaxed-maxwell-lbbb6g`: six distinct commits ahead, containing `DreamRooms.h`, `Phantom.h`, `TunerDuel.h` and tests, plus CMake edits. These are not wired into the current plugin editor/gameplay, so merging alone would imply feature completion incorrectly. Port and integrate separately after interface contracts and compiler tests.

## Existing main-branch audit status

`docs/audit/STATUS.md` identifies unresolved items F-04/F-05/F-06 (export duration, instrument-only gating, stereo output), F-08 (memory control), F-12 (XY-pad context menu), F-18/F-19 (score state and duplicate high scores), W9 (payload acknowledgement), W11 (game clock semantics), plus profiling candidates. These were not addressed by this branch.

## Chance-only games

This branch implements three playable secret variants via the existing overlay and score/reward plumbing:
- Phantom Monte: independent 1/257 per-play-minute offer; extra sample and score when winning.
- Eclipse Slots: independent 1/389 per-play-minute offer; extra special orbs.
- Spectral Dice: independent 1/601 per-play-minute offer; extra sample and score when winning.

Each uses the existing game interactions with a distinct resolution title and bonus reward. They are gameplay variants rather than three completely new interaction types.

## Release gate

Run GitHub Actions Windows, macOS, Linux builds and CTest for PR #16. Merge only after the latest head succeeds. A Windows beta release requires a successful packaged Windows artifact and then the existing release workflow with an appropriate beta suffix. Do not publish unverified Windows binaries.
