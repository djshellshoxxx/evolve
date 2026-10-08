# Spec 3: The Tuner Duels (tension and resolution bosses)

Status: planned. Build order: second. Depends on: Storyline (Tuner, acts, endings); no new audio systems.

## Goal
The Tuner becomes a recurring opponent. He quantises the dish onto a rigid grid; the player breaks it by resolving his dissonances correctly.

## Educational slant
Voice leading, tension and release, dominant and tonic function, deceptive cadences, and the idea that imperfect tunings have character.

## Flow
1. **Trigger**: once per act from Act 2 onwards, after the act's first twist or 6 minutes of unpaused play, whichever is later. Never during Phantom calls, Dream Rooms or quizzes. Skippable once per run with "ignore" (he gloats).
2. **Quantise**: a grid overlays the dish (`StoryFx::Anim::grid`), cells snap pitches to 12-TET, timbre dulls (existing steering command toward low divergence).
3. **Phases**
   - *Tension*: the Tuner holds a chord with 1 to 3 unstable tones (tritone, leading tone, seventh). A tension meter rises.
   - *Resolve*: the player drags each unstable cell to its resolution (leading tone up a semitone, seventh down a step, tritone resolves inward or outward). Correct drops land with a pure chord; wrong ones raise tension and add a rub.
   - *Cadence*: when stable, a V-I (or deceptive V-vi in late acts) lands; one grid layer breaks.
4. **Rounds** by act: 1 round (Acts 2-3), 2 (4-6), 3 with deceptive and borrowed chords (7-9).
5. **Outcome**
   - *Win*: grid shatters, reward, a **wolf-interval relic** that detunes one scale degree for the run by 10 to 40 cents (a lasting, audible, harmless colour).
   - *Lose* (meter full): dish sounds sterile for 45 s, Tuner gloats, no score loss beyond missing the reward.

## Data model
- `Source/Engine/TunerDuel.h` (pure C++): `Chord`, `Tension`, `resolutionsFor(chord, mode)`, `isResolved(chord)`, `Round`, `Duel` state machine, `makeRound(Rng&, act)`.
- `Progress.duelsWon` (u32), `Progress.relics` (u32 bitmask, 8 relics); relic table in the header, each with a cents offset and a name from music history (Pythagorean comma, syntonic comma, Werckmeister, Kirnberger, Meantone, Just 5th, Septimal, Quarter-tone).
- Endings: `endingFor` gets a duel-aware variant (more duels won favours "RESOLVED").

## UI
- Panel shows round, tension meter, and the Tuner's lines (existing say/queue).
- Chamber drag: pick up an unstable cell (highlighted red), drop on a target ring at the resolution pitch.
- Relic shown in the status line and journal.

## Tests
- Every chord type has at least one valid resolution; resolutions land on chord tones of the target.
- Tension monotonic under wrong moves; win and lose reachable; relic cents bounded and persisted.
- Deterministic rounds for a given seed; difficulty scales with act.
- Render smoke for grid and shatter animations.

## Risks
- Music theory strictness: accept any resolution that lands on a tone of the target chord; teach rules through reaction, not rejection.
- Frustration: three tries per round, hints after the second failure.

## Acceptance
All three round tiers playable; relic persists and audibly alters tuning; tests green.
