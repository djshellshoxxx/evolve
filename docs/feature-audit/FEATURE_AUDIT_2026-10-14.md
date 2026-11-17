# evolve feature audit, 2026-10-14

Base: `main` @ `2113964ce5959a6f9a7ead36f78d3c05ff9801ce` (PR #21 merge). Audit only; no application code changed.

## Method and limits
- Read AGENTS.md, README, PROGRESS.md, `docs/specs/*`, `docs/audit/*`, `docs/IMPLEMENTATION_PLAN.md`, CMakeLists.txt, `.github/workflows/build.yml`; grepped `Source/` and `Tests/` for TODO/FIXME/stub/placeholder (only hit: a comment at `Source/GUI/FxRackView.cpp:68`) and for each spec'd module.
- **Not run:** no build, ctest, or app launch (JUCE cold build ~15 min was out of scope). Nothing here claims execution. Windows/macOS/DAW/screen-reader behaviour is unverified.
- Existing audits (`docs/audit/STATUS.md`, `integration-followup-20261012.md`) were trusted for items they mark fixed; I only re-checked spec'd-vs-present files.

## Headline
The 31 original requirements are implemented (`docs/audit/features-and-controls.md` §3). The F-xx host/UX gaps were addressed in PR #16/#18. **What is still missing is the second wave of spec'd features: the Idea Scheduler, the microphone `LiveAnalyser`, Ideas A and B, and mic modes of Phantom/Duels/Rooms.** Several docs are also stale.

## Traceability matrix
| Feature | Spec/doc | Status | Evidence |
|---|---|---|---|
| Req 1-31 (divergence, interaction, score, gesture knobs, enzyme/catalyst/radiate/heat/water, fractal ghost) | PROGRESS.md §1 | Implemented; outcome sound quality not auditioned | features-and-controls.md §3 |
| Mic input + howl protection (req 14) | PROGRESS §1 | Implemented | `Source/Engine/Ingest.h`, `PluginProcessor.h:284-286` (`armMic`) |
| Radio entropy (req 15) | PROGRESS §1 | Partial by design: only audio-input LSBs, no radio/SDR source | `Source/Engine/Entropy.h:21-22`, README:345 |
| Story acts, Phantom, Tuner, Dream Rooms | specs 1-3 | Implemented, tested (pure-header tests) | `Tests/{Phantom,TunerDuel,DreamRooms}Test.cpp`, `EncounterOverlay.cpp` |
| Music Facts engine | music-facts-spec | Implemented; curated list embedded (~89 facts in `.inc`) | `Source/Engine/MusicFacts.cpp`, `MusicFactsCurated.inc`, `Tests/MusicFactsTest.cpp` |
| Host: programs, bypass, latency, A/B, import, shortcuts, Esc | incomplete-feature-completion spec | Implemented per STATUS.md; native QA unverified | docs/audit/STATUS.md |
| CLAP build | PROGRESS S6 | **Implemented but PROGRESS.md still shows unchecked** | CMakeLists.txt:75-90, build.yml:88 |
| **Idea Scheduler** (`IdeaScheduler.h`, journal scratch-off, encounter mutex) | idea-scheduler-spec | **Missing** | No such file; `grep IdeaScheduler Source` empty |
| **LiveAnalyser** (YIN pitch, onsets, RT60, room mode) | microphone-ideas-spec | **Missing** | No `LiveAnalyser.*` in `Source/Engine` |
| **Idea A: The Room Remembers** (clap census, room-matched reverb) | idea-a spec | **Missing** | depends on LiveAnalyser |
| **Idea B: Choir of One** | idea-b spec | **Missing** | depends on LiveAnalyser |
| Phantom hum/clap modes, Tuner duel by hum, mic-seeded Dream Rooms | Phase 4 of IMPLEMENTATION_PLAN | **Missing** (mouse/MIDI modes only) | `EncounterOverlay.cpp:131` text is mouse-only |
| Echo Chorale encore | PR #23 (open, draft) | **Unmerged**; tracked separately | PR #23, head 6e9c9e9 |
| Accessibility (F-31), High-DPI (F-32), device unplug (F-25) | STATUS.md | Implemented in part; native verification pending | STATUS.md |
| DSP profiling | spec §D | Not done (estimates only) | STATUS.md "Performance" |

## Findings
1. **P1: Mic-based second wave is entirely absent.** Specs 4, A, B and the scheduler are "Status: planned" and no code exists. `LiveAnalyser` is the shared prerequisite; the plan puts it in Phase 4a. Existing mic plumbing (`armMic`, howl detector, `mic.level()`) provides the pattern to copy.
2. **P1: Encounters are scheduled by independent per-minute dice, not a scheduler.** Integration follow-up: Dream Room 1/97, Phantom 1/131, Tuner 1/173 per minute, plus three secret games (1/257, 1/389, 1/601) with no shared mutex or cycle/weighting as in idea-scheduler-spec. Adding Ideas A/B would multiply uncoordinated rolls.
3. **P2: Stale docs mislead.** PROGRESS.md:777-780 shows S6 (CLAP, release CI) and S7 unchecked though CLAP and release jobs exist; `docs/IMPLEMENTATION_PLAN.md` still says "starts after 0.3.0 beta 1"; specs still say "planned" for Phantom/Duels/Rooms/Facts though shipped; RELEASE_NOTES.md:31 lists "Source selector does nothing" and "Shift/Ctrl fine-drag not implemented" as known issues, both fixed (F-02, F-10). README Testing (README:352-383) names only two tests; CMake defines 11.
4. **P2: Radio entropy only indirect.** Requirement 15 says radio noise; implementation relies on whatever the audio input carries. Acceptable if documented as such (README does), otherwise a gap.
5. **P2: Test/CI coverage.** CI runs `ctest` on Windows/Linux/macOS (build.yml:76-77) but no ASAN/TSAN/clang-tidy/format check, no allocation-in-`processBlock` check (required by AGENTS.md and the plan), no deterministic DSP reference renders beyond DivergenceTest. `MutagenDivergenceTest` is defined (CMakeLists.txt:215) but not registered via `add_test` (no match in the add_test list), so CI never runs it.
6. **P3: Monolith size.** `Colony.cpp` 2319 lines, `PluginProcessor.cpp` 1615, `MusicFacts.cpp` 1270, `PluginEditor.cpp` 1079, `CultureChamber.cpp` 1224. AGENTS.md asks for incremental seam extraction with characterization tests; none queued.
7. **P3: Unverified native behaviour:** screen reader, 125/150/200% DPI, multi-monitor, device unplug, DAW/listening QA (STATUS.md, RELEASE_NOTES.md:1-3).
8. **P3: Profiling not done** for the resonator/grain `cos`/`fmod`, PostChain EQ allocation, O(N^2) fitness sharing, 40 Hz repaints.

## Branch / PR inventory
- Open: PR #23 (draft, Echo Chorale; reports 11 CTest targets pass on Linux; Windows/macOS unverified).
- Branches: `main`, this audit branch only (no stale remote branches remain; older feature branches were squashed into main per integration-followup).

## Backlog
| Pri | Item | Acceptance | Depends on |
|---|---|---|---|
| P0 | none found (no stub/broken shipped feature located by static read) | | |
| P1 | `LiveAnalyser` + `Tests/LiveAnalyserTest.cpp` | Sines 80-1000 Hz within 5 cents; noise low confidence; clicks within 10 ms; decay RT60 within 15%; zero allocation in callback; only active when armed | existing `armMic` |
| P1 | `IdeaScheduler.h` + test (10-hour virtual session, 500-level mutation) and one-encounter mutex across Phantom/Tuner/Room/secret games | Replaces per-minute dice; persisted state merges | none |
| P1 | Phantom hum/clap mode, Tuner hum duel | Mouse/MIDI fallback remains | LiveAnalyser, scheduler |
| P1 | Idea A, Idea B | per specs | above |
| P2 | Register `MutagenDivergenceTest` in ctest (or document why not) | CI runs it | none |
| P2 | Refresh PROGRESS.md, IMPLEMENTATION_PLAN.md, RELEASE_NOTES known issues, README Testing | docs match code | none |
| P2 | Add sanitizer CI job and an allocation-hook test for `processBlock` | green on Linux | none |
| P2 | Merge or close PR #23 after CI on all three OSes | | |
| P3 | Profiling baseline, then optimise; seam extraction of Colony/Processor | per AGENTS.md | baseline tests |
| P3 | Native QA: Windows DAW + screen reader + DPI | manual checklist | Windows runner |

## Immediate next steps
1. Confirm priority: scheduler first or `LiveAnalyser` first (scheduler is smaller and unblocks wiring).
2. Fix the doc drift (finding 3) and the unregistered test (finding 5) in one small PR.
3. Land PR #23 after CI before adding more encounters.
