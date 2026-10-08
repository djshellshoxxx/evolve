# Spec 1: The Phantom Accompanist (call and response)

Status: planned. Build order: first. Depends on: Spec 4 groundwork (live input analyser), but ships with a mouse/MIDI-only mode first.

## Goal
The colony answers the player. A new character, the Phantom (Echo's unreleased twin), sings a short motif; the player answers by clicking cells, playing MIDI keys, or (once the analyser lands) humming. The Phantom reacts to *how* the answer relates to the call. Rewards musical variation over exact copying.

## Educational slant (never announced)
Call and response, motif development (repetition, inversion, augmentation, sequence), interval recognition, rhythm matching. The lexicon gains entries when the player first uses a technique.

## Surreal framing
Echo's twin exists only as a delayed shadow. The dish hushes, a ghost baton cursor appears, cells glow in sequence as the Phantom sings. Its mood is visible in the dish colour and drift.

## Flow
1. **Summon**: a Phantom encounter starts on a timer (every 4 to 8 min of unpaused play, seeded per run), never during a quiz, banter, duel or Dream Room.
2. **Call**: 3 to 5 notes drawn from the run's mode (`RunStory::scaleNotes`), rhythm drawn from a small pattern bank. Played through `StoryPanel::playNotes`, with matching cell glows from `StoryFx`.
3. **Window**: the player has `callDuration * 2 + 2 s` to answer. The answer is captured as a list of (time, note) events from three sources: chamber clicks mapped to scale degrees, MIDI note-ons, and (later) the live analyser.
4. **Judge**: compare *intervals and inter-onset ratios*, not absolute pitch or time. Classify:
   - `echo`: same contour and rhythm (score 0.3).
   - `inversion`: contour mirrored (0.8).
   - `augmentation`/`diminution`: rhythm scaled by 2x or 0.5x with same intervals (0.8).
   - `sequence`: same intervals transposed (0.7).
   - `variation`: shares at least half the intervals, differs elsewhere (1.0).
   - `answer`: fits the mode, different shape, ends on a stable degree (0.9).
   - `noise`: out of mode or fewer than 2 notes (0.0).
   - `silence`: no answer.
5. **React**: Phantom replies with a harmonised or developed version of the answer (e.g. thirds above it). Good answers bloom colour and trigger a small mutation toward appeal; silence or noise makes the colony drift flat for 30 s (small `knobGesture` pitch down) and the Phantom speaks sadly.
6. **Reward**: `onReward` with the classification score, a lexicon-style "technique" bit per classification first seen, a collected sound of the exchange.

## Data model
- `Source/Engine/Phantom.h` (pure C++): `Motif { std::vector<int> degrees; std::vector<float> beats; }`, `Phrase` from player, `Classification`, `judge(const Motif& call, const Phrase& answer, const Mode&)`, `makeCall(Rng&, int act)`, `makeReply(const Phrase&, Classification, Rng&)`, `Director` (timing, seeded).
- `Progress` gains `std::uint32_t techniquesSeen` (bit per Classification) persisted in `resonance-acts.json`.
- Difficulty by act: Act 1-2 three notes diatonic steps; Act 3-5 four notes with leaps; Act 6-9 five notes, syncopated rhythm, accidentals allowed.

## UI
- Panel strip shows "THE PHANTOM" with a baton sigil (new `Speaker::phantom`; append, do not renumber).
- `StoryFx` gets `Anim::phantomTrail` (glow follows each sung note) and `Anim::bloom`.
- Chamber click handler: when `PhantomSession` is listening, a click on the dish maps x position to a scale degree (the click also still mutates as normal).

## Audio
Reuse `CommandType::noteBurst` / `noteRelease`. Reply voices use the colony's own voices, so it always sounds like the dish.

## Tests (pure C++, `ProgressionTest` + new `PhantomTest`)
- Every classification reachable with constructed inputs; invariance to transposition and tempo.
- Calls always lie in the mode; length matches act difficulty; no two consecutive calls identical.
- Silence and noise never score; scores bounded.
- Persistence round trip of `techniquesSeen`.
- GUI smoke: new animations render and clear.

## Risks and mitigations
- Matching feels unfair: judge only intervals and ratios, generous tolerance (+-25% on rhythm), and always give partial credit for mode-fitting answers.
- Chamber click ambiguity: only active during the window; HUD shows "LISTENING".
- Latency: judge on onset times from the UI thread clock; tolerance covers jitter.

## Acceptance
Playable end to end with mouse only; hum mode works when Spec 4's analyser is present; all tests green; no allocations on the audio thread.
