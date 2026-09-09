# MUTAGEN

**A hybrid VST3 / AU instrument & effect that treats sound as a living population
rather than a signal passing through oscillators, filters and envelopes.**

You do not program a sound in MUTAGEN. You *plant* it, cultivate its environment,
select what survives, and breed what comes next.

---

## The idea

Every sound is a **colony** made of three interacting species of cell:

| Species | Colour | Carries |
|---|---|---|
| **Grain cells** | amber | fragments, attacks, textures, temporal detail |
| **Spectral cells** | cyan / violet | harmonics, formants, noise bands, tonal identity |
| **Resonator cells** | pale green | cavities, membranes, bodies, feedback structures |

Each cell carries a compact **genome** of 20 traits governing its sound, lifespan,
reproduction, movement, environmental response and relationships with other cells.
Genes can be **dominant, recessive, dormant, or activated** by changing conditions,
so one organism can express very different characteristics without losing its
underlying identity.

When audio enters MUTAGEN it is analysed for transients, tonal regions, noise,
formants, amplitude contour and resonance, then divided among the three species.
Cells germinate, grow, mature, reproduce, age, go dormant or undergo programmed
death. Descendants inherit recognisable traits but may mutate. Cells **compete**
for energy and frequency territory, **cooperate** through symbiosis, **consume**
weaker populations, **spread infections**, and **exchange genes** across species.

Evolution happens on two levels: individual notes grow and die as temporary
organisms, while a persistent colony remembers successful traits across the
performance — so repeated notes sound like *related descendants* rather than
identical retriggers.

## Guiding evolution

The musician changes the **environment**, never a synthesis graph:

- **Five large controls** — Nutrients, Mutation, Selection, Metabolism, Stability
- **Deeper ecology** — Fertility, Mutation Depth, Radiation, Temperature,
  Competition, Symbiosis, Lifespan, Apoptosis, Diversity, Migration
- **Selection pressure** — favour dark / bright, sparse / dense, harmonic / noisy,
  calm / aggressive, familiar / divergent
- **Trait locks** — protect a desirable quality while everything else keeps evolving
- **Explore vs Preserve** — discover new descendants on every playback, or freeze
  the genome, population, seed and history for exact recall and rendering

## The interface

A dark biological laboratory viewed through an imaging system.

- **Culture Chamber** (centre) — a *functional* read-out of the live engine. Grain
  cells appear as amber fragment clusters, spectral cells as glowing orbs,
  resonator cells as concentric membranes. Divisions, gene transfers, infections,
  extinctions and selection-pressure fronts are all drawn as they happen.
  Click a cell / family / species to select it; right-click to isolate, mute,
  preserve, eliminate, inspect or send to the Breeding Lab; double-click empty
  space to drop a seed.
- **Germination** (left) — load a sample, capture live audio, choose a primitive
  seed, or load a preserved organism; set capture length, transient sensitivity,
  initial population and species distribution.
- **Environment** (right) — the ecology controls above.
- **Genome Inspector** — examine, edit, mutate, re-express or lock the inherited
  traits of the whole colony, one species, or one family.
- **Evolution Timeline** (bottom) — a branching record of generations. Audition an
  ancestor, restore it, preserve it, fork a new branch, or send it to the lab.
- **Breeding Lab** — drop one or two preserved organisms into the parent slots,
  decide which parent contributes Body / Voice / Texture / Movement / Lifecycle /
  Env Behaviour, then breed a family of specimen cards to audition, reject, save
  or send back into the live colony.
- **Performance view** — the whole system reduced to eight assignable macros
  (Growth, Mutation, Stress, Density, Body, Voice, Movement, Decay), an XY field
  for stability vs reproductive aggression, and a playable keyboard.
- **Top bar** — organism name, generation, population, seed, CPU quality, role and
  Explore / Preserve state, with immediate Clone, Freeze, Reanimate, Render and a
  full **RESET** (everything back to defaults).

## Ten core operations

1. **Germinate** — turn a sample / input / impulse / noise / preserved organism
   into an initial population, splitting transients → grain, tone → spectral,
   decay → resonator.
2. **Mutate** — create altered descendants (pitch, timing, spectrum, formants,
   direction, movement, resonance, lifespan, behaviour) with rate & depth.
3. **Apply Selection Pressure** — let cells with the desired character survive and
   reproduce.
4. **Lock Trait** — protect a quality while the rest keeps evolving.
5. **Transfer Genes** — move sonic traits between species (a grain's transient into
   a resonator, a spectral cell's formants into a grain family…).
6. **Breed Organisms** — combine preserved organisms into a family of descendants.
7. **Infect Colony** — introduce a mutation that spreads between compatible cells
   (metallize, reverse, vocalise, destabilise).
8. **Trigger Apoptosis** — programmed death of selected cells for thinning,
   rhythmic gaps, drone collapse or synchronised extinction.
9. **Remember Evolution** — carry successful traits across MIDI notes; the Memory
   control sets how strongly new notes inherit the colony's history.
10. **Preserve & Render** — freeze genome / population / seed / environment /
    history; clone, reanimate, save as a preset, or render to audio.

---

## Building

Requirements: **CMake ≥ 3.22** and a C++20 compiler (MSVC 2022, Xcode 15+, or
GCC/Clang 12+). JUCE 8 is fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To build against a local JUCE checkout instead of downloading it:

```sh
cmake -B build -DJUCE_SOURCE_DIR=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
```

Artifacts (under `build/`):

- `MUTAGEN_artefacts/Release/VST3/MUTAGEN.vst3`
- `MUTAGEN_artefacts/Release/AU/MUTAGEN.component` *(macOS)*
- `MUTAGEN_artefacts/Release/Standalone/MUTAGEN`

`COPY_PLUGIN_AFTER_BUILD` is on, so the VST3 is also installed into your user
plugin folder.

### Quick start

1. Open MUTAGEN as an instrument on a MIDI track (or as an effect on an audio
   track and set **Role → Effect**).
2. In **Germination**, keep the default noise seed or **Load Sample**, then press
   **GERMINATE**.
3. Play notes. Turn **Nutrients** up for a denser colony, **Mutation** up for
   faster drift, and drag the **Selection** targets toward the character you want.
4. When you like where it is going, toggle **PRESERVED** in the top bar to freeze
   it for recall, and **Render** to bounce it.

---

## Layout

```
Source/
  Parameters.*            parameter surface (the environment, not a synth graph)
  PluginProcessor.*       audio <-> GUI marshalling, MIDI, state, offline render
  PluginEditor.*          top-level editor & view switching
  Engine/
    Rng.h                 deterministic PRNG (Preserve mode replays from a seed)
    Genome.*              20 traits, dominance, locks, mutation, recombination
    Cells.*               grain / spectral / resonator DSP + lifecycle
    SourceAnalyzer.*      onset / spectral / formant / decay analysis -> seed
    Colony.*              the ecological simulation and audio rendering
    EvolutionHistory.*    branching generation tree
    BreedingLab.*         descendant families from one or two parents
    OrganismState.h       POD snapshots, GUI<->audio commands
    OrganismSerialization.* versioned blob <-> preset / state
    RenderEngine.*        record the live colony to WAV
  GUI/
    MutagenLookAndFeel.*  the laboratory theme
    CultureChamber.*      the animated, interactive centrepiece
    GerminationPanel.*  EnvironmentPanel.*  GenomeInspector.*
    EvolutionTimeline.*  BreedingLabView.*  PerformanceView.*  TopBar.*
    Widgets.*             shared knob / panel / bar components
```

## Licence

MUTAGEN is released under the **GNU AGPL v3** (see `LICENSE`). It builds against
the JUCE framework, used here under the AGPLv3 option; a closed-source build
requires a commercial JUCE licence.
