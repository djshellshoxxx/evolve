# Spec 4: Two microphone ideas that combine all three systems

Both ideas use the **Phantom** (call and response), the **Dream Rooms** (acoustic spaces) and the **Tuner Duels** (tension and resolution) together, with the microphone as the instrument. They share one new component.

## Shared component: LiveAnalyser
Real-time, allocation-free analysis of the microphone stream, running in the audio callback and publishing results through lock-free atomics (same pattern as `mic.level()` and `howlFrequency()`).

- `Source/Engine/LiveAnalyser.h/.cpp`, pure C++ where possible so tests can feed synthetic signals.
- **Pitch**: YIN (or McLeod) on 2048-sample windows with hop 512; output `pitchHz`, `confidence`, `cents` from the nearest scale degree. Octave-error guard and median smoothing.
- **Onsets**: spectral flux with adaptive threshold; output onset timestamps (sample clock).
- **Level and spectrum**: RMS, spectral centroid, a coarse 8-band energy vector.
- **Decay estimate**: after a loud onset (a clap), fit the log-energy decay to estimate RT60 (Schroeder integration on the captured burst).
- **Dominant room mode**: peak of a long-window spectrum of the room's noise floor between 40 and 250 Hz.
- Safety and privacy (non-negotiable): mic only active when the player arms it (existing `armMic`); the existing feedback and howl protection stays on; audio is never written to disk or sent anywhere except a clip the player explicitly collects; analysis outputs are numbers only.

Tests: synthetic sines recover pitch within 5 cents across 80 to 1000 Hz; noise gives low confidence; click trains give onsets within 10 ms; synthetic exponential decays recover RT60 within 15%; no allocations in the callback (checked by a debug allocator hook in the test build).

---

## Mic Idea A: The Room Remembers (clap cartography)

**Pitch:** the game maps the player's *real* room and then haunts it.

1. **Clap census** (during an act opening or on request): "Clap once, then be still." The analyser captures the onset and decay and estimates the room's RT60 and dominant mode. A visual sonar ring expands from the click.
2. **The Phantom answers in your room:** its calls are played through a reverb whose tail matches your measured RT60, so it sounds like it is in the room with you. The player responds by clapping rhythms; the judge uses onset ratios (Spec 1) instead of notes.
3. **Dream Rooms are warped copies of your room:** each room starts from your measured RT60 and mode and bends one parameter further (a hall that is twice as long, a corridor whose mode is your own note). Solving them teaches RT60 and room modes using *your* numbers.
4. **Tuner duel by hum:** the Tuner "tunes" your room mode to his grid; to break it, hum the note that resolves his chord (analyser pitch, within 30 cents) while the room's mode resonates with it. Singing into the mode makes the dish bloom, a real acoustic effect the player can hear.
5. **Reveals:** the Moth reports your RT60 and mode as lab data ("your room is a carpeted bedroom, 0.4 s"), and Sub names your room's hum as a pet.

Educational: RT60, room modes, impulse response, rhythm ratios, resolution to a stable tone.
Surreal: the lab leaks into the player's real space; the room becomes a character.

## Mic Idea B: The Choir of One (voice as colony)

**Pitch:** the player's own voice becomes the dish's chorus.

1. **Hum duet:** the Phantom sings a motif; the player hums or sings it back. The analyser tracks pitch live and the Phantom *harmonises in real time* (a diatonic third, then a sixth, then a drone fifth below), pitch-shifting copies of the player's own voice through the colony voices.
2. **Voice cells:** each sustained note spawns a cell that carries the player's formant colour (spectral centroid maps to cell hue and brightness). Sing different vowels and the dish changes species.
3. **Dream Rooms with a voice:** in the Comb Hall your voice is flanged as you sweep; in the Doppler Carousel moving your pitch up and down makes cells orbit faster; in the Long Tails room you sing in the gaps; in the Standing-Wave Corridor, pitch decides which nodes light up; in the Phase Garden you hold a tone against its drifting twin until they lock (beats slow and stop).
4. **Tuner duel by ear:** the Tuner drifts the dish flat or sharp; the player must hold a steady pitch (within 15 cents) against the drift while his interval resolves, learning to hear beats. A held steady note for 4 s breaks a grid layer.
5. **Finale:** in Act IX the whole choir (your harmonised voice plus the cast) performs the ending cadence; the final chord is built from what the player sang.

Educational: pitch matching, harmony by thirds and sixths, vowel formants and timbre, beats and intonation, vocal range.
Surreal: your voice splits into a ghost ensemble and the cells wear your timbre.

## Shared risks and mitigations
- Feedback: speakers into the mic. Keep existing howl protection; duck output while analysing claps; recommend headphones in the arming dialog.
- Accessibility: every mic interaction has a mouse or MIDI fallback.
- Noisy rooms: confidence gating; the Phantom politely asks to try again.
- Privacy: see Shared component. Document in the README and the first-arm dialog.
