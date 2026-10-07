# MUTAGEN

> **Project status:** experimental open-source audio/software-art project. MUTAGEN is not currently part of the Circuit Drift Labs commercial product line.


A sound colony you play like a game.

MUTAGEN is a VST3 / AU / standalone instrument built around a population of small
synthetic organisms. Each one is a cell with a genome, a lifespan and a voice. They
breed, compete, infect each other, form symbioses and die, and what you hear is the
population, not a patch. You do not edit a sound here — you keep an ecosystem
interesting, and it scores you on how well you do it.

---

## The problem this version exists to solve

The previous build had one fatal flaw: **every colony ended up sounding the same.**
Changing the seed changed the route and not the destination.

There were five reasons, and all five are gone:

| | What was wrong | What replaced it |
|---|---|---|
| 1 | One global attractor genome that every cell was dragged toward, every tick | Islands — 2–8 semi-isolated niches per world, each with a target that wanders independently |
| 2 | Fitness defined as *closeness to that attractor*, so being average was optimal | Fitness sharing, novelty search and a MAP-Elites archive — crowding is now penalised |
| 3 | Every founder derived from one seed genome computed from the source audio | Founders drawn from the islands, and the islands are re-rolled per run |
| 4 | The diversity-rescue immigrant was blended 60/40 **back toward** the thing it was rescuing you from | Re-seeding from the most behaviourally *distant* stored elite |
| 5 | Cells had no modulation at all — `renderAdd` was a pure function of the genome, so similar genomes were sample-identical | A six-lane LFO bank per cell, spread from ~0.003 Hz to 26 Hz |

And one more that mattered as much as all of them combined: **the default source
material was pink noise.** The grain species granulates the seed directly, so a noise
seed gives noise regardless of how cleverly anything evolves. The default is now a
tonal, harmonically rich seed.

The improvement is measured, not asserted — see [Testing](#testing).

---

## Running it

```bash
cmake -B build                      # fetches JUCE 8.0.6 on first run
cmake --build build --config Release --target MUTAGEN_Standalone
```

The VST3 is copied to your system plugin folder automatically. A CLAP build is
produced too (via clap-juce-extensions); pass `-DMUTAGEN_BUILD_CLAP=OFF` to skip it. On this machine a
fully parallel build occasionally dies with `CL.exe exited with code -1`; `-j 1`
always completes.

---

## Playing it

### The chamber

The big panel is the culture chamber, and every gesture in it is real — the ripple you
see corresponds to a mutation command that was actually sent.

| Input | What happens |
|---|---|
| **Left click** | An additive mutation burst at the click, with a ripple that spreads outward. Cells swell as the wave passes through them. |
| **Left drag** | A continuous wake. The faster you move, the harder it mutates — a slow drag nudges, a fast slash across the chamber causes a mass mutation. |
| **Right click / drag** | Subtractive damage. Strips partials, density and noise colour, weakens, and eventually kills. This is also **the cure for a noise lock**. |
| **Ctrl + right click** | The inspect / isolate / preserve / infect menu. |
| **Double click** | On a cell: send its family to the Breeding Lab. On empty space: seed a burst. |
| **Drop an audio file** | The colony eats it (see [Feeding it](#feeding-it)). |

### The buttons

Every one of these re-rolls its **amount and its effect** on each press. Pressing the
same button twice does not do the same thing twice.

- **ADD ENZYME** — sparkles, and a subtraction that is *usually* an improvement. An
  enzyme digests, so the common case strips the hiss and the clutter. About one press
  in twelve eats something the sound needed.
- **ADD CATALYST** — a fast pitch wobble that decays over a couple of seconds. The
  wobble is the loud part; what it quietly removes on the way out is the actual effect.
- **ADD HEAT / ADD WATER** — speeds up or slows down one randomly chosen thing: an
  oscillator, a modulation lane, or the loop. Which one it catches is the roll.
- **RADIATE** — the only one with fixed, stated odds, because it is the only one that
  can cost you the whole run. **5%** kills the colony and zeroes your score. **10%** is a
  beneficial mutation. The other **85%** is a shrug. All three flash the screen red and
  leave a Geiger-counter click ticking over the loop for several seconds afterwards.

### The knobs

**PITCH**, **LFO** and **OSC** are not parameters. They have no value to read back and
they spring to centre when released, because there is no setting to return to. What
they send is a gesture: the direction you turned, how fast, and when. Up tends to add,
down tends to strip — but roughly one turn in six does the opposite, and a fast turn is
more likely to misbehave than a slow one.

### Feeding it

Drop as many audio files onto the window as you like. Each one is **stitched** into the
colony's working material, not mixed into it — summing uncorrelated recordings is the
definition of noise, so instead each drop replaces crossfaded segments of the digest
buffer while leaving the rest intact. The share it takes shrinks with every drop
(0.60 / (1 + 0.55 n)), so the newest sample can never completely displace what the
colony already is. It takes *some* of the dropped sound's form, which is the point.

**ARM MIC** does the same thing with a live capture, and it is defended in three layers:

1. The output is **muted while capturing**. An open loop cannot howl. This is the default.
2. A howl detector runs whenever the mic is armed. It requires narrow-band *and*
   persistent-in-the-same-band *and* growing, all at once, for 0.45 s — music is
   regularly one or two of those and almost never all three. On a hit it aborts the
   capture, notches the offending frequency and ducks.
3. A hard output ceiling underneath both.

### Presets, files and the manual

The header strip carries the chrome every plugin in this range has: the name on the
left, and on the right the **FILE** menu, the preset selector, the **A/B** compare
pair, the **options gear** and the **manual**.

- **Presets** are the *environment*, not the organism — the conditions the colony
  lives in. Loading one changes the world and lets the running colony react to it,
  rather than replacing the thing that has been evolving. Twenty-odd factory presets in
  six categories, plus your own in `%APPDATA%/MUTAGEN/Presets`.
- **A/B** holds two whole parameter states and flips between them under the same living
  colony. The first switch copies the slot you are leaving into the empty one, so there
  is always something to compare against.
- **RANDOM** re-rolls everything. The first press randomises from wherever you are;
  every press after that resets to defaults first, so press two is a genuinely new world
  rather than a drift away from press one. Output level, dry/wet, CPU budget and plugin
  role are left alone — they are your setup, not the sound.
- **RESET** returns every parameter, the history, the Breeding Lab, the source, the MIDI
  mappings and the colony itself to factory state.
- **Save / Open Run** (`.mutagen`) is the whole thing: parameters, the living colony, its
  history, the score and the run statistics. **Export Audio** records what the colony
  plays next to a WAV.
- **Right-click any control** for *Set Value*, *Reset to Default* and *Map to MIDI*.
  MIDI learn claims the first CC that moves, and that same message sets the value so the
  gesture lands immediately. One parameter answers to one CC; mappings travel with the
  plugin state.
- **Hover anything** for a sentence explaining it. Turn tooltips off in Options if you
  would rather not see them.
- **Options** holds the interface switches, the MIDI mappings and — in the standalone —
  the audio device, sample rate, buffer size and MIDI inputs. Inside a host that section
  says plainly that the host owns the soundcard rather than showing controls that would
  do nothing.
- **Help** is the whole manual, in the plugin, with the version number in its footer.

---

## The score

The score is meaningless by design — nothing in the engine reads it back — but it is
what turns the instrument into a game, so the rules are legible from the number alone:

- It climbs **faster the more the sound varies**. The rate is driven by the measured
  long-window movement of the spectrum, with novelty and archive coverage as bonuses.
- It **stops dead** when the sound collapses into noise, and tells you why.
- **Right-clicking to strip elements** is how you clear that, and acting while frozen
  builds combo at double rate — it is the move the game wants you to learn.
- The rate starts **sagging before** the freeze actually bites, so there is warning.
- Beauty is worth a little, but only a little. Weight it heavily and the optimal
  strategy becomes one consonant drone, which is the opposite of the point.

High scores live in `%APPDATA%/MUTAGEN/scores.json`. **SAVE RUN** writes a `.mutagen`
file (colony + score + statistics) and files the run on the board.

When the colony sounds genuinely good *and* you are scoring well, held for seven
continuous seconds, a **fractal ghost** flashes briefly over the chamber. It then sits
behind a 30–70 second random cooldown. The scarcity is deliberate: a reward that turns
up whenever things are going fine stops reading as a reward within about a minute.

---

## Field Journal and long-term progression

The **JOURNAL** button opens a persistent research record that sits above individual
runs. It does not alter the audio thread or invalidate old organisms. It records what
you actually do across sessions: generations observed, timbres discovered, worlds
visited, noise rescues, outside samples digested, radiation exposure, breeding,
steering, score, anomalies, relics and combo peaks.

Progress unlocks eight story chapters, six researcher characters, ten research
protocols, a rotating challenge deck, an anomaly catalogue, relics, lineage records
and the world atlas. Late play reveals three non-exclusive directives — **PRESERVE**,
**ACCELERATE** and **RELEASE** — with the journal recommending one from the way you
have actually played. No ending deletes or permanently locks the others.

The progression file is separate from presets, scores and `.mutagen` saves, so deleting
or moving a run does not rewrite the research history.

---

## Hidden Laboratory

MUTAGEN now contains a second layer that is intentionally discovered rather than
presented as a normal feature panel. Exploratory click sequences across different GUI
regions can reveal **100 persistent creatures**. The exact sequences are not documented
in the user manual; ordinary experimentation is enough to find them.

Discovered creatures are recorded in the Field Journal and can later reappear as
short-lived animations that leave the plugin window where the host and operating
system permit desktop overlays. Hidden events also have procedural sound signatures.

Every whole-million score crossing unlocks a numbered **Milestone Artifact** with a
unique procedural sound recipe, animation recipe and replayable skill. Unlocked skills
can be replayed from the Journal without manufacturing extra score.

The story begins strange at chapter zero and becomes less reliable over time: MOTH edits
its own notes, researchers disagree about events, specimens can appear before their
recorded discovery, and late Visitor material implies the operator may be part of the
environment being measured.

A few extremely rare events can touch system hardware on Windows. They are deliberately
kept off the audio thread. Microphone capture uses the existing muted-output/howl-guard
path and inserts a silent gap before reversed, granularly stretched playback. Optical
drive requests are ignored on systems without a usable drive, and modern Windows
machines may route system beeps through normal audio rather than a motherboard speaker.

---

## The Resonance Acts

A second storyline runs in the narrator strip under the chamber. Seven new
characters share the lab: **Dr. Cadence Mirelle** (acoustician), **Fourier** (the
spectrum analyser), **Lyra Kestrel** (composer), **Sub** (an organism living below
20 Hz), **Echo** (a delay line), **Nyquist** (the gatekeeper at half the sample rate)
and **The Tuner**, who wants every sound in the building at exactly 440 Hz.

Every line they speak is one true fact about acoustics, sound design or music
theory, and the colony *performs* it while they speak: the cells sing the octave,
the fifth, the dominant seventh; a tempo gate opens at 120 BPM; a delay starts
repeating itself. Facts heard are counted as **lexicon**, and lexicon (plus sounds
collected and ear checks passed) is what opens the next act. There are eight acts,
sixty lexicon entries and sixteen ear checks.

No two runs are the same. Each run draws its own **key and mode** (twelve roots,
ten modes from Lydian to whole-tone), its own **twist** (eight, such as *Echo was
always alone* or *The 432 decree*), its own event order and its own pacing.

The lab games (roulette, three-card monte, slots, dice, twenty-one, scratch cards)
are offered during play. Instrument rewards are real sounds: each one is recorded
into your collection, and whoever hands it over tells you how that instrument works.

## Collecting and exporting sounds

**COLLECT** records the last three seconds of the colony. Story twists, new acts,
newly discovered creatures and lab-game instrument rewards are collected
automatically. Each sound is prepared like a sample-library one-shot: DC offset
removed, peak-normalised to -1 dBFS and given 10 ms fades so it never clicks. It is
then written straight away as a **24-bit WAV** to

| OS | Folder |
|---|---|
| Windows | `%APPDATA%\MUTAGEN\Collected Sounds` |
| macOS | `~/Library/MUTAGEN/Collected Sounds` |
| Linux | `~/.config/MUTAGEN/Collected Sounds` |

**EXPORT WAV** copies the whole collection to a folder you choose.

## Downloads

Builds are on the [Releases](https://github.com/djshellshoxxx/evolve/releases) page:

| Platform | Contents |
|---|---|
| Windows x64 | `MUTAGEN.exe` (standalone, no installer or redistributable needed), `MUTAGEN.vst3`, `MUTAGEN.clap` |
| Linux x64 | `MUTAGEN` (standalone), `MUTAGEN.vst3`, `MUTAGEN.clap` |
| macOS (universal) | `MUTAGEN.app`, `MUTAGEN.component` (AU), `MUTAGEN.vst3`, `MUTAGEN.clap` (unsigned) |

Plugin folders: VST3 goes in `C:\Program Files\Common Files\VST3`, `~/.vst3` or
`~/Library/Audio/Plug-Ins/VST3`. CLAP goes in `C:\Program Files\Common Files\CLAP`,
`~/.clap` or `~/Library/Audio/Plug-Ins/CLAP`.

## How it stays interesting

Five mechanisms from the quality-diversity literature, all running at once:

- **Islands.** 2–8 niches per world, each with its own target genome performing its own
  random walk, plus occasional jumps so they cannot converge by accident.
- **Fitness sharing.** Cells within 0.18 of each other in behaviour space divide their
  fitness through a triangular kernel. A crowd is always worth less per head than an
  empty niche, so converging is actively penalised.
- **Novelty search.** k-nearest-neighbour distance to a rolling 192-entry archive of
  behaviours the colony has already produced. Only genuinely new behaviours enter it —
  otherwise a static colony fills the archive with copies of itself and everything reads
  as novelty zero.
- **MAP-Elites.** A 6×5×4 grid over brightness, density and noise, keeping the best
  individual per bin. Population rescues re-seed from the most *distant* stored elite.
- **Dispersal.** An explicit repulsive force away from the colony mean. Selection is an
  attractive force; something has to push back.

Plus a **stagnation detector**: two exponential averages fifteen times apart in time
constant, and when they agree that nothing has changed for twelve seconds, hypermutation
fires and a third of the colony migrates between islands.

### Movement

Every cell carries six LFOs spread logarithmically from about **0.003 Hz — one cycle
every six minutes — up to 26 Hz**, so a colony is never wobbling at one identifiable
speed. Summing octave-spaced sources is the Voss–McCartney construction, so the combined
drift is 1/f rather than white or Brownian: the documented perceptual sweet spot between
too twitchy to follow and too smooth to notice.

The lanes are routed to pitch, amplitude, formant, brightness, pan, density, resonance,
grain rate and detune **by the world**, so a world has a consistent *kind* of movement
while every cell in it moves differently. `enforceMovementFloor()` runs after every
mutation: a lineage may become subtle, but it may not become still, because no selection
pressure can see a gene that currently does nothing.

### Worlds

Each run re-rolls the *rules*, not just the starting genomes: tuning system and root,
partial palette (harmonic, odd, stretched, golden, subharmonic, formantic), the register
the colony occupies, how many voices are audible at once, the tempo of life, modulation
routing, niche count, what fitness means, and a colony-wide tilt and formant applied
*after* the sum — which is the one part of a world's identity that averaging cannot
erase. Worlds get names like `VELVET CHORUS` and `BROKEN FURNACE`.

### Randomness

The RNG is seeded from a 256-bit entropy pool mixing `std::random_device`,
high-resolution timer jitter (the *delta* between reads, not the reading), your mouse
gestures, and the audio input's own noise floor. Point an untuned radio, an SDR or a
hissing preamp at the input and the bottom bit of the converter is a physical noise
source. Those bits are von Neumann debiased (01→0, 10→1, 00/11 discarded), so a biased
or silent source contributes nothing rather than contributing zeros.

---

## Testing

```bash
cmake --build build --config Release --target MutagenDivergenceTest
./build/MutagenDivergenceTest_artefacts/Release/MutagenDivergenceTest.exe
```

Six colonies run offline for 45 seconds each with nobody touching anything. The test
reports how far apart they end up, whether each one keeps moving, and whether any of
them parks in noise. It also calibrates the analyser against known signals on every run,
so the constants in `Descriptors.cpp` are measurements rather than guesses.

Current results:

There is also a fast offscreen GUI smoke test for the floating score HUD:

```bash
ctest --test-dir build -C Release -R MutagenGuiSmokeTest --output-on-failure
```

It guards the chamber's input routing: ordinary chamber clicks must pass through the
full-size HUD overlay, while the score card and open high-score sheet remain clickable.

```
  sine 220           raw 0.167     harmonic tone   raw 0.130
  detuned saws       raw 0.126     white noise     raw 0.843

  SPECTRUM distance  mean 0.34   (0 would mean the runs converged)
  genome distance    mean 0.24
  every run keeps moving,  no run parks in noise
```

### How "is this noise?" is measured

Two measurements, because one is not enough.

The first is **spectral flatness on a whitened spectrum**: divide the spectrum by its own
smoothed envelope and ask how far what is left departs from flat. Energy concentrated in
partials is tonal; energy smeared between them is noise. Whitening is what makes a world's
tilt cancel, so a bright world is not reported as a noisy one.

On its own that measurement is **wrong for this instrument**, and for a long time it was
reporting every colony as pure noise — all six runs pinned at 1.000, which left the visuals
permanently two-thirds grey and the score one frame from freezing on colonies that were
perfectly musical. The cause was not density and not the crowd. Every cell here is
deliberately frequency-modulated — six LFO lanes from 0.003 Hz to 26 Hz, which is the point
of the whole instrument — and a partial carrying a few hundred cents of vibrato sweeps
across a dozen analysis bins inside one 46 ms frame. Integrated over the frame it deposits
its energy evenly across that span, which to any single-frame spectral statistic is exactly
what broadband noise looks like. The density sweep pins the blame precisely: with the
modulation bank muted and nothing else changed, one spectral cell drops from 0.613 to 0.432
and a two-cell colony from 0.591 to 0.305.

No choice of window fixes that — a longer window resolves partials better but smears
modulation worse, and a shorter one does the reverse. So the second measurement asks a
question modulation does not disturb: **does the waveform repeat?** A vibrato'd note is
still locally periodic; noise repeats at no lag at all. It is the autocorrelation, taken as
the inverse transform of the power spectrum we already have, divided by the analysis
window's own self-overlap so that a low note is not scored as less periodic than a high one
merely for having a longer period.

Periodicity carries the larger weight, because flatness's failure mode here is a false
*positive* — calling healthy material noise — and that is the error that breaks the game.
The scale that results has real range, and the test asserts both ends of it:

```
  vibrato tone    rawFlat 0.151   rawPeri 0.969   ->  noisiness 0.012   (musical)
  harmonic tone   rawFlat 0.130   rawPeri 1.000   ->  noisiness 0.003   (musical)
  pink noise      rawFlat 0.846   rawPeri 0.238   ->  noisiness 0.746   (fully grey)
  white noise     rawFlat 0.843   rawPeri 0.119   ->  noisiness 0.916   NOISE-LOCKED
  live colonies                                       noisiness 0.29 - 0.32
```

A metric that returns zero for everything passes "no run parks in noise" perfectly and is
useless, so the test checks the converse too: white noise must still lock, and a wobbling
note must not.

---

## Layout

```
Source/Engine/
  Entropy.*        entropy pool: random_device, timer jitter, audio LSBs, gestures
  WorldSeed.h      the per-run rules
  Genome.*         34 traits, 14 of them modulation
  ModBank.h        six LFO lanes per cell, 0.003 Hz .. 26 Hz, Voss-McCartney summed
  Cells.*          the three species and their DSP
  Novelty.h        behaviour descriptors, novelty archive, MAP-Elites, stagnation
  Descriptors.*    RT-safe flatness / centroid / flux / roughness / appeal
  Colony.*         the ecology, the guards, the gestures, the voicing
  ScoreSystem.*    score, combo, events, high scores
  Ingest.*         sample stitching and the microphone feedback guard
  PostChain.*      the post-colony rack (filter, EQ, gator, glitch)

Source/GUI/
  MutagenLookAndFeel.* the house visual identity - palette, type, knobs, meters
  TopBar.*         the house header (presets, A/B, options, help) + the run verbs
  CultureChamber.* the play surface
  WaveField.*      the 2-D wave simulation behind it
  ScoreHud.*       score, meters, event feed, high-score table, fractal ghost
  GameBar.*        the verbs and the gesture knobs
  ParamControl.*   the right-click contract: set value, reset, map to MIDI
  HelpView.*       the manual
  OptionsView.*    tooltips, MIDI mappings, audio/MIDI devices
  DeviceSetupHook.h  how the options page reaches the standalone's soundcard

Source/
  Parameters.*     the parameter surface
  ParamHelp.cpp    one sentence per control, for the tooltips
  AppOptions.h     per-machine settings (tooltips, device state)

Source/Standalone/
  DeviceSetup.cpp  compiled into the standalone only; installs the device selector

Resources/
  icon.png         the application icon
  make_icon.py     the script that cuts it

Tests/
  DivergenceTest.cpp   the measurement above
```

`PROGRESS.md` carries the full working log, the root-cause analysis and the research
the design is based on.

---

## Licence

See [LICENSE](LICENSE).
