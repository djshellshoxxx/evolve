# Hidden skill levels

Five concealed levels unlock through lifetime **skills earned**, separately from the eight research levels.

| Lifetime skills earned | Secret level | Reward after discovery |
| ---: | --- | --- |
| 5 | Ghost Culture | +5 score per subsequently earned skill |
| 10 | Echo Genome | +1 collectible game sound per subsequently earned skill |
| 20 | Prism Colony | +1 special orb per subsequently earned skill |
| 35 | Phantom Resonance | +25 score per subsequently earned skill |
| 50 | The Fifth Specimen | +1 rainbow orb per subsequently earned skill |

## Discovery and persistence

When a positive game-skill award crosses a milestone for the first time, the game rolls an independent, unbiased 50/50 discovery chance for that milestone. Each milestone is checked **once** in the lifetime record, even if the character later loses game skills. Successful discoveries persist across launches; failed rolls do not reroll. A multi-skill award may cross more than one threshold, which gives each crossed milestone its own independent 50/50 roll.

The save file stores `earnedGameSkills`, `hiddenAttemptedMask`, and `hiddenUnlockedMask`; the latter cannot contain bits that were not attempted. Legacy saves start their earned counter from currently held game skills without automatically claiming old milestones.

The five secret perks augment skill awards without replacing existing rewards. They never launch an optional game, and the separate rare-offer modifier for chance-based games remains unchanged.

## QA checks

- Award four skills: no attempt; award the fifth: only Ghost Culture is attempted.
- At each exact threshold, winning and losing 50/50 rolls set attempted / unlocked status correctly.
- Cross several thresholds in a single award: each milestone receives a separate result.
- Lose and regain skills: previously attempted milestones never reroll.
- Save and relaunch: counters, attempts, successful unlocks and their perks persist.
- Open a legacy save: existing skills and progression remain intact.
- Unlock all five: subsequent awards grant each applicable bonus once per earned skill.
- Verify no additional games are queued and no existing research levels are modified.

Implementation: `Source/Engine/ProgressionSystem.h/.cpp` and `Source/LabGameFlow.cpp`.
