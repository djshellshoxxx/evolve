# Chance minigames: Pulse and Interval Hunt

Engines in `Source/Engine/Minigames.h` (pure C++, `Tests/MinigamesTest.cpp`); UI is `EncounterOverlay::Kind::pulse` (4) and `hunt` (5). Rolled each minute after the other encounters at 1/59 and 1/67; no prerequisites.

- **Pulse:** 8 beats at a seeded 75-120 bpm. Only the first 4 click; tap (click or Space) within 120 ms of every beat, 6 hits wins. A tap inside 250 ms but outside 120 ms is judged early/late and spends that beat. Pays 250 per hit plus 100 per perfect (50 ms) hit.
- **Interval Hunt:** 5 questions; the colony plays a low note then a higher one, pick the interval from 4 options (keys 1-4, R replays). Pool widens with the act. 4 of 5 wins. Pays 250 per correct answer.
- A win grants a bonus fact. Leaving early keeps what was earned. The finish mask carries hits (bits 0-7) and perfects (bits 8-15).

## Seek and Chord Flash (kinds 6 and 7, 1/71 and 1/79 per minute)
- **Seek:** a hidden pitch on a 24-semitone line. Move the mouse; pings speed up as you near it; click within 6% to lock in. 3 rounds of 12 s, 2 found wins. Pays 250 per find.
- **Chord Flash:** 8 chords (major, minor, diminished, augmented), 4 s each, keys 1-4; a timeout is wrong. 6 of 8 wins. Pays 250 per correct answer.
