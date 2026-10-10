# Spec 8: The Music Facts engine (30,000 educational facts)

Status: planned (implementation starts now, with cheap-model helpers). Pure C++ core (`Source/Engine/MusicFacts.h/.cpp`), presented by a small overlay and demonstrated through the existing note/chord/effect commands.

## Behaviour
- Every 5 minutes of unpaused play, one fact is shown as text for about 6 seconds, then fades. Also granted as a bonus when the player wins a game or unlocks a skill.
- Each fact scores **1 to 4 knowledge points** (seeded random), shown as a second score under the main score ("KNOWLEDGE").
- Exactly **1 in 5** facts is demonstrable (a shuffled block of five with one demo). A demonstrable fact plays its sound as the text appears and awards **+9344** to the main score.
- Facts are never repeated until all 30,000 have been seen.

## The honest numbers and the quality bar
30,000 facts is only possible with correctness if most are **computed**, not hand-written. Tiers, all inside one 30,000-slot ID space:
- **Tier A, computed (about 20,000)**: generated from small tables by templates, correct by construction and re-verified by an independent script. Examples: frequency, wavelength and name of every MIDI note in three tunings; every piano key; each interval, chord and scale spelled in all 12 roots (with demo); ear-training prompts from each of the 88 keys; harmonic-series partials; BPM to delay-time tables for each note value; sample-rate and bit-depth facts; transposing-instrument written/concert pairs; key signatures and circle-of-fifths steps; dB, inverse-square and SPL tables; Doppler examples at stated speeds.
- **Tier C, drills (about 7,000)**: computed "skill" facts that teach recognition, e.g. "This is a minor seventh above Eb: Db. Listen." with a demo.
- **Tier B, curated trivia (3,000 target, grows over releases)**: hand-written facts about instruments, music theory, sound design, famous musicians and algorithms, **each with at least two independent sources** recorded in the data file (sources are not shipped in the binary). Unfilled curated slots are backed by Tier A/C so the game always has exactly 30,000 valid facts; each release replaces filler slots with verified curated ones.
- Anything uncertain is excluded. Contentious claims (for example "432 Hz is healing", acoustic myths) are only allowed as correctly framed myth-busting.
- Famous-song demonstrations use **public-domain** melodies or short generic illustrations of a technique (no copyrighted melodies).

## Storage and lookup (fastest and smallest)
- ID space `[0, 30000)`. A fixed table maps ID ranges to **generators**; a generator turns `(range, local index)` into text + demo recipe in O(1) with no storage beyond its small tables.
- Curated facts live in one compact UTF-8 blob plus a `uint32` offset index, embedded at build time from `docs/data/curated_facts.json` by `tools/gen_facts.py` (about 300 KB raw for 3,000 facts). No JSON parsing at runtime.
- **No-repeat order without storage**: an affine permutation `slot = (a * k + b) mod 30000` with `gcd(a, 30000) = 1` and `a, b` from the run seed visits every ID exactly once per pass. Progress is a single counter `k`.
- **Seen set**: a 30,000-bit bitset (3,750 bytes) persisted in `resonance-acts.json` (base64), used for the journal count ("FACTS 1,204 / 30,000").
- Memory at runtime: well under 1 MB including tables.

## Demonstration recipes
`Demo { kind; notes[]; durationMs; fxId }` with kinds: `note`, `pianoKey`, `interval`, `chord`, `scale`, `octavePair`, `keySound` (play a key so the player memorises it), `circleOfFifths`, `ghostNote`, `instrumentColour`, `effect` (bass drop, sweep, riser, gate, sidechain, tape stop, filter open), `songMotif` (public domain), `dopplerPass`, `beats` (two close tones). Playback reuses `StoryPanel::playNotes` and existing commands (`noteBurst`, gator, trip delay, knob gestures) so no new audio engine is required. Text reveals typewriter-style with a short chime keyed to the run's mode; the demo plays on the text's first line.

## Presentation
- `FactToast` overlay (top-centre of the chamber): typed text, category tag, knowledge points flying to the second score, fade out. Skippable by click.
- `ScoreHud` gains a "KNOWLEDGE n" line under the main score.
- Journal: counts by category, "last 20 facts" replay with demo buttons, favourites.

## Validation pipeline
- `tools/factcheck.py`: independently recomputes Tier A/C facts (frequencies from `440 * 2^((n-69)/12)`, spellings from interval tables, delays from `60000/bpm * ratio`, wavelengths from `343 / f`) and diffs against the generator's output (exported by a test binary).
- C++ tests: exactly 30,000 IDs; text unique; length bounds; every demo valid (notes in range 21..108, durations bounded); deterministic by seed; permutation visits every ID once; 1-in-5 demo cadence holds; knowledge points in 1..4; +9344 only on demos.
- Curated data: every entry needs `claim`, `category`, `src1`, `src2`, `checkedBy`; a lint script rejects entries missing two sources.

## Risks
Repetitive feel from templated facts (mitigated by varied phrasings per template, interleaving categories, and curated growth), copyright (public-domain only), false claims (two-source rule, computed proofs, myth-busting framing), audio overlap (demos queue behind active encounters and defer while a quiz or idea encounter is active).

## As built (2026-10-08, phase F core)
- `Source/Engine/MusicFacts.h/.cpp`, `Tests/MusicFactsTest.cpp` (CTest target `MutagenMusicFactsTest`), `tools/factcheck.py`, `docs/data/curated_facts.json` (89 reviewed facts, not yet embedded).
- 30,000 unique facts; 8,480 (28%) are demonstrable. Real mix: about 6,000 computed Tier A, about 6,700 Tier C drills, about 9,200 octave-placed variants, about 7,100 ear-training pair drills; slots 27,000 to 29,999 are reserved for curated facts through `curatedText()` (currently empty, so filler is used).
- `tools/factcheck.py` independently recomputes 16 fact families (about 30,000 ids): zero mismatches. It caught and led to fixes of three real bugs in the C++ (scale spelling, interval drill spelling, trumpet concert-pitch offset).
- **No-repeat rule, as built**: the exact 1-in-5 demo cadence always holds, and a fact never repeats before every fact of its own kind has been shown. Because 28% of facts are demonstrable, non-demo facts run out first (after about 26,900 draws, roughly 2,240 hours at one fact per 5 minutes) and then start a new pass on their own; the cadence is never broken.
- Scale demos carry the first six tones; playback calls `scaleSemitones(fxId)` for the whole scale.

## Update (2026-10-09)
- The 89 reviewed curated facts are embedded (`Source/Engine/MusicFactsCurated.inc`, generated by `tools/gen_facts.py`, validated by `tools/lint_curated.py`); ids 27000 to 27088. Fifteen carry demos.
- 500 sound-design demonstration facts (ids 29500 to 29999): gate, sidechain, riser, filter sweep, bass drop, tape stop, echo throw, reverse swell. Every number in them is computed and re-derived by `tools/factcheck.py`.
- The colony plays those demos with its own voices (gated chords, rising and falling glides, a decelerating tape stop, an echo throw), since the engine has no dedicated filter or tape commands.
- 8,995 of 30,000 facts are demonstrable (29%).
