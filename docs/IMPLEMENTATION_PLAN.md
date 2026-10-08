# Implementation plan: Phantom Accompanist, Tuner Duels, Dream Rooms, microphone ideas

Specs: `docs/specs/*.md`. Order requested: Phantom first, then the other two, then the microphone ideas. This plan starts after the 0.3.0 beta 1 builds finish and PR 9 is merged.

## Phase 0: finish what is in flight (gate for everything else)
1. Run 53 (beta 1 release) completes. If Linux was lost to runner shutdown, re-run the failed jobs once; the release job then publishes `v0.3.0-beta.1`.
2. Mark PR 9 ready and merge it only after the build is green.
3. Restart the working branch from the new `main` (`git checkout -B <branch> origin/main`), keep the same branch name.
4. Install dev packages locally (`libasound2-dev` ... see build.yml) and keep a warm build dir to avoid recompiling JUCE (about 15 min cold).

## Working rules for every phase
- Pure C++ headers first (logic + tests), then GUI wiring, then animations, then docs.
- Each phase ends with: local build, `ctest`, GUI smoke render (frames viewed), standalone launch under Xvfb for 12 s, then PR, then CI green, then release.
- New `Speaker`, `Anim`, `Kind` values are appended, never renumbered (persisted bitmasks).
- Progress fields are merged on save (OR/max), like existing `storyio`.
- Keep per-frame work allocation-free; audio thread only touches atomics and preallocated buffers.

## Phase 1: Phantom Accompanist, mouse and MIDI first (target: beta 2)
Steps:
1. `Source/Engine/Phantom.h`: motifs, `judge`, `makeCall`, `makeReply`, director; `Tests/PhantomTest.cpp` (new CTest target) covering every classification, transposition and tempo invariance, mode fit.
2. `Speaker::phantom`, `Anim::phantomTrail`, `Anim::bloom`; sigil in `StoryPanel`.
3. `PhantomSession` in `StoryPanel`: summon timer, call playback, answer capture (chamber clicks to degrees, MIDI note-ons), judge, reply, reward, persistence (`techniquesSeen`).
4. Chamber hook: map click x to scale degree while listening; HUD "LISTENING".
5. Docs: README section, release notes, journal entries.
Done when: playable with mouse, tests green, render smoke includes the new animations.

## Phase 2: Tuner Duels (target: beta 3)
1. `Source/Engine/TunerDuel.h` + `Tests/TunerDuelTest.cpp`: chords, resolutions, state machine, relics, difficulty by act.
2. Animations: grid overlay, shatter.
3. Chamber drag interaction (pick up unstable cell, drop on target ring) behind a `DuelSession` in `StoryPanel`.
4. Relic detune applied through existing scale/pitch path (cents offset on one degree), persisted.
5. Endings consider duels won.
Done when: all three round tiers playable, relics audible, tests green.

## Phase 3: Dream Rooms (target: beta 4; largest audio work)
1. `Source/Engine/DreamRooms.h` state machine and solve predicates + tests.
2. Audio: per-room bounded tweaks in `PostChain` (comb, Doppler ratio, reverb macro, low-shelf by position, phase offset) with smoothing and revert crossfade; offline test renders a block per room and asserts finite, bounded output.
3. Room renderers in `StoryFx`; cursor interaction; hint glow after 20 s.
4. Second lexicon mask (`lexicon2`) for new facts; journal replay.
Done when: five rooms solvable, offline audio test green, no allocations in `processBlock` (verified with the debug allocator hook).

## Phase 4: Microphone ideas (target: beta 5)
1. `LiveAnalyser` with synthetic-signal tests (pitch, onsets, RT60, mode). Ship behind the existing arm switch with the privacy text.
2. Phantom hum mode (Spec 1 + Mic Idea B step 1) and clap-rhythm mode (Mic Idea A step 2).
3. Clap census and RT60-matched reverb for Phantom calls (Mic Idea A steps 1-2).
4. Room Dream variants that start from measured RT60 and mode; Tuner duel by hum and by steady pitch (both ideas).
5. Choir finale for Act IX.
Done when: both ideas playable with a microphone and with mouse/MIDI fallbacks, tests green, feedback protection verified.

## Rough sizing (relative)
| Phase | Logic | Audio | UI/Anim | Risk |
|---|---|---|---|---|
| 1 Phantom | M | S | M | Fair judging |
| 2 Duels | M | S | M | Difficulty tuning |
| 3 Rooms | M | L | L | Audio artefacts, CPU |
| 4 Mic | L | M | M | Pitch robustness, feedback, privacy |

## Token and time efficiency
- One warm local build directory for the whole effort; build only affected targets between runs.
- Test logic in pure headers (fast, no JUCE) before touching GUI code.
- Verify visuals by rendering frames from the smoke test, not by launching the app.
- Batch edits per file with scripted replacements; run CI only per phase, not per commit.


---

# Addendum (2026-10-08): ideas A and B, the Idea Scheduler and the Music Facts engine

New specs: `idea-a-room-remembers-spec.md`, `idea-b-choir-of-one-spec.md`, `idea-scheduler-spec.md`, `music-facts-spec.md`.
Order changes: the Music Facts engine is self-contained, so it starts immediately and runs beside Phase 1. The Idea Scheduler lands right after Phase 1 so each later idea ships already wired into it.

## Delegation and model policy (token efficiency)
- Well-specified, mechanical, self-checking work goes to the cheapest model (Haiku): pure C++ headers with tests, data tables, generators, scripts, research and verification of curated facts. Anything with design judgement, audio-thread safety or cross-file integration stays with the main model.
- Every delegated task gets: the spec path, exact file list, the test that must pass, and a rule to report only a short summary. The main model builds and runs the tests itself before accepting the work.
- One warm build directory; delegated agents must not run full builds, only the pure-header test targets.

## Phase F: Music Facts engine (starts now)
1. `Source/Engine/MusicFacts.h/.cpp` generators for Tier A and C, the 30,000-slot ID map, affine permutation, scheduler of 1-in-5 demos, knowledge points. `Tests/MusicFactsTest.cpp` (new CTest target, pure C++).
2. `tools/factcheck.py` independent verifier; `tools/gen_facts.py` curated blob generator; `docs/data/curated_facts.json` seeded with researched, two-source-verified facts.
3. GUI: `FactToast`, second score in `ScoreHud`, demo playback via `StoryPanel`, 5-minute timer, win and skill-unlock bonuses, journal counters, persistence of the seen bitset and permutation counter.
4. Release as part of the next beta.

## Phase S: Idea Scheduler (after Phase 1)
1. `Source/Engine/IdeaScheduler.h` with genomes, operators, distance, rewards, timed and milestone modes, persistence struct; `Tests/IdeaSchedulerTest.cpp` including the 10-hour virtual session and 500-level mutation checks.
2. Journal scratch-off list UI and presentation banner ("A NEW PHENOMENON").
3. Wire the encounter mutex (one encounter at a time) across Phantom, quiz, banter, ideas, facts.
4. Ideas P and T get registered first; D, A and B register as they are built, with placeholders that present a short "coming soon" encounter disabled until their phase ships.

## Phases 2 to 4
Unchanged (Tuner Duels, Dream Rooms, microphone ideas), except each idea registers with the scheduler, implements `presentAtLevel(genome)` and `reward(level)`, and its mutation axes map to the genome fields.

## Order of work
F (parallel) and 1 Phantom, then S, then 2 Duels, 3 Dream Rooms, 4a `LiveAnalyser`, 4b Idea B, 4c Idea A, then a polish pass (balancing probabilities with the simulation, accessibility, docs).
