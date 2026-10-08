# Spec 5 (Idea A): The Room Remembers

Status: planned. Mic-based. Builds on: Spec 4 `LiveAnalyser`, Spec 1 Phantom, Spec 2 Dream Rooms, Spec 3 Tuner Duels, and the Idea Scheduler (`idea-scheduler-spec.md`).

## Premise
The game measures the player's *real* room with a clap, then the lab haunts that room: the Phantom answers inside it, Dream Rooms are warped copies of it, and the Tuner tries to "tune" its resonance.

## Educational slant (never announced)
RT60 and reverb, room modes and standing waves, impulse responses, rhythm ratios, resolving a dissonance to a stable tone, the idea that every space is an instrument.

## Requirements
- Mic only after the player arms it (existing `armMic`). Feedback and howl protection stay on. Output ducks while a clap is captured. Nothing is stored or sent; a clip is kept only if the player collects it.
- Fallback without a mic: a "synthetic room" drawn from the run seed, clapped with mouse clicks. All mechanics work with it.
- Needs: `LiveAnalyser` (onsets, decay/RT60 estimate, dominant room mode 40 to 250 Hz, pitch).

## Flow
1. **Presentation**: the Idea Scheduler presents it as "THE ROOM REMEMBERS". Moth asks the player to arm the mic and clap once.
2. **Clap census**: capture 3 s around the loudest onset. Estimate RT60 by Schroeder integration (T20 extrapolated), dominant mode from the pre-clap noise floor. Sonar ring animation expands from the click; result is shown as lab data ("RT60 0.42 s, mode 118 Hz, roughly a carpeted bedroom"). Retry if confidence is low.
3. **Phase one, the Phantom in your room** (needs Spec 1): the Phantom's calls play through a reverb whose decay matches the measured RT60. The player answers with claps; the judge uses inter-onset ratios (rhythm only).
4. **Phase two, the room warps**: a short Dream Room built from the measured numbers, then bent by the mutation level (RT60 x1.5, mode shifted a fifth up, etc.). The player finds the loud and quiet spots or the notch frequency, as in Spec 2, but with their own numbers.
5. **Phase three, the hum duel** (needs Spec 3): the Tuner snaps the dish to his grid. Break it by humming the note that resolves his chord into the room's mode; when pitch is within 30 cents of the target and the mode is within a semitone, the dish blooms. Mouse fallback: drag a pitch ring.
6. **Reveal**: Moth logs the room as "a carpeted bedroom" or "a stairwell", Sub names the mode, Echo repeats the RT60 as a delay time.

## Mutations across presentations (see scheduler spec)
Level n changes: which phases appear, RT60 scaling, mode offset, colours of the sonar ring, voice of the Phantom, tempo of clap rhythms, order of phases, and the shape of the room animation. Level 0 is only steps 1 and 2.

## Rewards (unique per level, picked by the scheduler's reward table)
Examples: a collected "room tone" WAV recorded from the measured room's reverb tail, a relic detune derived from the room's mode, a permanent reverb colour for the dish, a journal page about the player's room, a fact pack about acoustics.

## Data and persistence
- `RoomProfile { float rt60; float modeHz; float confidence; std::uint64_t stamp; }` saved in `resonance-acts.json` (numbers only).
- `IdeaProgress` entry for this idea (level, last presented, rewards given).

## Tests
- Synthetic exponential decays recover RT60 within 15% across 0.2 to 2.0 s; noise bursts give low confidence.
- Mode finder recovers a synthetic 118 Hz resonance in noise.
- Rhythm judge invariant to tempo scaling; rejects silence and noise.
- Mutation levels produce distinct parameter sets and bounded values.
- Persistence round trip; no audio data written.

## Risks
Noisy rooms (confidence gating and retry), speaker bleed (ducking, headphone advice), privacy (numbers only, documented), very dead or very live rooms (clamp RT60 to 0.1 to 4 s).
