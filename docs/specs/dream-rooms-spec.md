# Spec 2: Dream Rooms (acoustic spaces that change the rules)

Status: planned. Build order: third (needs the most audio work). Depends on: nothing hard; benefits from the Phantom's reply voice.

## Goal
Occasionally the dish melts into a surreal architectural space whose *one altered law of sound* is the puzzle. Solving it gives a collectible "room tone", a lexicon entry, and a replayable toy in the journal.

## Educational slant
Comb filtering and flanging, Doppler shift, reverb time (RT60), room modes and standing waves, phase cancellation. Learned by exploring, never told.

## Entry and exit
- Triggered by a rare chance event (`ChanceEvents`, new `Kind::dreamRoom`), by act transitions (first time in an act), or from the journal.
- Lasts 45 to 75 s. Exit on solve, timeout, or Esc. Never starts during a Phantom call, duel, quiz or overlay.
- State machine in `Source/Engine/DreamRooms.h` (pure C++): `idle -> entering -> exploring -> solved/failed -> leaving`.

## The rooms (v1: five)
| Room | Altered law | Interaction | Solve condition |
|---|---|---|---|
| Comb Hall | Walls are teeth: a comb filter whose notch follows the cursor | Move cursor to sweep delay (0.5 to 20 ms) | Park on the notch that silences the hum cell for 2 s |
| Doppler Carousel | Cells orbit; pitch bends by radial velocity | Click a cell at closest approach (peak pitch) | Catch 3 cells at peak within tolerance |
| Cathedral of Long Tails | Huge reverb; every note returns late | Play notes (click) in the gaps between tails | Play 8 notes without overlapping a tail |
| Standing-Wave Corridor | Bass level depends on position (nodes and antinodes) | Walk the cursor along the corridor | Find the 2 loudest and 2 quietest spots |
| Phase Garden | Two copies of a tone drift out of phase | Drag a slider-like arc to align | Hold full-level alignment 3 s |

## Audio design
- Per-room processing is a **bounded** tweak on the existing post chain (`PostChain`): comb = short feedback delay with smoothed delay time; Doppler = per-voice pitch ratio from velocity; long tail = reverb size/decay macro; standing wave = low-shelf gain as a function of position; phase = delay on one of two detuned copies.
- All parameters smoothed (no zipper noise), limited to safe ranges, and reverted on exit with a short crossfade.
- Mono-safe; output limiter already present.

## Visuals
`StoryFx` gains a `Room` renderer: perspective walls/floor from simple geometry (stateless, function of age and room id), per-room motifs (teeth, orbit rings, arches, wave bands, twin sine wheels). Respect existing `Anim` timing conventions.

## Data model
- `Progress.roomsSolved` bitmask (u32) persisted; `Progress.roomTones` counted in collection.
- Each solve awards: lexicon-style entry (extend lexicon via new 64-bit mask `lexicon2` to avoid exceeding 64), `onReward(1.0)`, a recorded room tone through `SoundCollection`.

## Tests
- State machine transitions and timeouts; solve predicates with constructed inputs (e.g. comb notch within tolerance).
- Parameter ranges clamped; reversion restores baseline values.
- Render smoke test for each room frame; no empty frames.
- Persistence round trip.

## Risks
- Audio artefacts: smoothing, hard limits, offline test that renders a block per room and asserts finite, bounded output (no NaN, peak under 1.0).
- Difficulty: generous tolerances, a hint glow after 20 s.
- CPU: reuse existing buffers; no allocation in `processBlock`.

## Acceptance
Five rooms playable, each solvable in under a minute by a first-time player, audio bounded and glitch-free in the offline test, journal replay works.
