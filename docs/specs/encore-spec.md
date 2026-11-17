# Encore: the Echo Chorale

Post-ending mini-game. Engine: `Source/Engine/Encore.h` (pure C++, tests in `Tests/EncoreTest.cpp`). UI: `EncounterOverlay::Kind::chorale`.

- The colony sings a phrase on four pads (four scale steps spread across the run's mode); the player echoes it by clicking or pressing 1-4.
- 12 rounds; the phrase grows by one note per round and the tempo tightens (floor 0.26 s). 3 lives: a wrong pad or a 6 s pause costs a life and replays the same phrase.
- Offered once, 30 s of play after the finale, and afterwards as a 1-in-41 per-minute encounter once any ending has been seen. Escape keeps rounds already cleared.
- Reward: 400 points per round cleared, plus 2000 per life kept on a full clear, and a bonus fact on a full clear.
- Persistence: `Progress::encoreBest` (key `encoreBest`, max-merged across instances) drives the rank shown in the story status line: APPRENTICE, CHORISTER, SOLOIST, CONDUCTOR, MAESTRO. Missing key loads as 0, so existing files are unaffected.
- `EncounterOverlay::Kind` is append-only; `chorale` is value 3. The finish mask carries rounds in bits 0-7 and lives in bits 8-15.
