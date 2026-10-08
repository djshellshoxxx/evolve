# Spec 6 (Idea B): The Choir of One

Status: planned. Mic-based. Builds on: Spec 4 `LiveAnalyser`, Spec 1 Phantom, Spec 2 Dream Rooms, Spec 3 Tuner Duels, and the Idea Scheduler.

## Premise
The player's own voice becomes the colony's chorus: the Phantom sings with them, the cells wear their timbre, and the finale is a chord they built.

## Educational slant (never announced)
Pitch matching, harmony in thirds and sixths, vowel formants and timbre, beats and intonation, vocal range, drones.

## Requirements
- Mic armed by the player; feedback protection on; headphones recommended in the arm dialog. Nothing stored or sent; a clip only if collected.
- Mouse/MIDI fallback: a held key or drag acts as the "voice" with the same analyser output fields (pitch, onset, brightness).
- Needs `LiveAnalyser`: pitch (YIN), confidence, cents from scale degree, RMS, spectral centroid, 8-band energy.

## Flow
1. **Presentation** by the Idea Scheduler as "THE CHOIR OF ONE".
2. **Hum duet**: the Phantom sings a motif; the player hums or sings it back. Pitch is tracked live; the Phantom harmonises in real time with a diatonic third above, then a sixth, then a drone fifth below. Harmony notes are produced by the colony voices (pitch-targeted `noteBurst`) following the tracked pitch with a 120 ms lag so it never fights the singer.
3. **Voice cells**: each held note (confidence above 0.6 for at least 400 ms) spawns a cell in the dish. Spectral centroid maps to hue and brightness; vowels (centroid and band balance) pick the species. The dish visibly takes on the player's colour.
4. **Dream Room with a voice** (needs Spec 2): one room per presentation, driven by voice. Comb Hall: pitch sweeps the notch. Doppler Carousel: rising pitch speeds the orbits. Long Tails: sing inside the gaps. Standing-Wave Corridor: pitch picks which nodes light. Phase Garden: hold a tone against its drifting twin until the beats slow and stop.
5. **Steady-pitch duel** (needs Spec 3): the Tuner drifts the dish flat or sharp; the player holds a steady pitch (within 15 cents) for 4 s to break a grid layer. Beat visuals make the intonation audible.
6. **Finale** (Act IX or when mutation level is high): a choir chord built from notes the player sang during the session, voiced by cast and colony together, collected as a sound.

## Mutations across presentations
Level n changes: harmony interval set (third/sixth/fourth/tenth/clusters), response lag, which room, drone interval, voice colour mapping, number of voices, tempo, and presentation (ring choir, spiral choir, column choir). Level 0 is steps 1 and 2 only.

## Rewards (unique per level)
A "choir take" WAV of the harmonised voice (collected), a new colony voice patch tinted by the player's timbre, a vocal-range journal page, a permanent harmony relic, a fact pack on voice and singing.

## Data and persistence
`VoiceProfile { float lowHz, highHz; float meanCentroid; int notesSung; }` (numbers only) plus the idea's `IdeaProgress` entry.

## Tests
- Synthetic sines and harmonic stacks: pitch within 5 cents from 80 to 1000 Hz; octave errors rejected.
- Harmoniser output stays in the run's mode for 1000 random inputs; lag bounded.
- Steady-pitch judge: pass at <=15 cents for 4 s, fail otherwise; robust to vibrato below 40 cents.
- Cell spawn gating (confidence and duration thresholds).
- Mutation distinctness and bounds; persistence round trip.

## Risks
Pitch tracker errors (confidence gating, median smoothing), feedback loop from harmony through speakers (existing howl guard plus a gate that ignores energy at the harmony frequency), singers with narrow range (adaptive target transposition), privacy (documented).
