# MUTAGEN — "EVOLVE" overhaul progress log

> Continuation file. If a session ends, read this top-to-bottom and resume at the first
> unchecked milestone. Every milestone ends with a commit + push to GitHub.

---

## 1. What the user asked for

Verbatim requirements, grouped. Each maps to a milestone in §5.

### Sound divergence
1. Every run must sound **different**. Today every colony converges on the same timbre.
2. Each individual sound must be **unique**, not a variation of one attractor.
3. More **variance** in the sound generally.
4. Sounds must **wobble** much more.
5. **LFOs at wildly different rates** — some fast, some as slow as *one cycle per minute or longer* —
   modulating frequencies and oscillators.
6. The sound must **always be varying and always evolving**; never settle into a static drone.
7. The sound must **never devolve into pure noise/static**. It should reach a *unique* sound instead.
8. Mutations should **gravitate toward appealing sounds**, not toward noise — *unless* the user's
   input is negative (right-click damage), in which case ugliness is a legitimate direction.

### Interaction
9. GUI gives the user far more control over the sound.
10. **Left click** changes both the visuals *and* the sound it mutates into.
11. **Drag with the mouse held down** creates *waves* in the visualisation that cause **mass mutations**.
12. **Right click** causes **damaging, subtractive** mutations — removing elements rather than adding.
13. **Drag & drop samples** into the window; the evolved sound **eats** them and takes some of their form.
    Unlimited drops, but the result must not collapse into noise.
14. **Microphone input** as an alternative to dropping samples — with **very strong feedback protection**.
15. **Other random input sources, e.g. radio noise**, harvested as entropy and **mixed with a
    traditional PRNG** to drive randomness.

### Game layer
16. A **score** per sound, displayed. It means nothing mechanically but should feel like a game.
17. **More variance ⇒ score counts up faster.**
18. If the sound becomes noise, the **score stops counting**.
19. The user fixes that by **clicking to remove elements**, which makes it count again.
20. **Save** sounds / score / game state; show a **high-score table**.
21. Other fun features welcome.

### Direct-control knobs
25. **Gesture knobs** - a few knobs that hit the sound directly: **PITCH**, **LFO**, **OSC**.
    They are not ordinary parameters. What they do depends on *when* and in *which direction* they
    are turned, and it is partly random, so the same gesture is never guaranteed the same result.
    Turning them can improve the sound or damage it. Direction gives intent (up = grow/add,
    down = strip/subtract), speed gives violence, and timing decides which part of the colony is
    caught by the gesture.

### Enzymes
26. An **"ADD ENZYME"** button. It puts **sparkles** into both the audio and the visuals, and
    randomly affects the sound positively or negatively - but **usually as a *positive subtraction***.
    Read biologically: an enzyme digests. So the common case is that it strips the roughest /
    noisiest / most crowded elements and the result is cleaner, with an audible shimmer as it works;
    the uncommon case is that it eats something the sound needed. A cheap, repeatable "tidy this up"
    action that is not guaranteed safe.

27. An **"ADD CATALYST"** button. It makes the sound **wobble quickly up and down in pitch**, and
    that wobble then **subtracts something from the sound, subtly**. The pitch excursion is the
    visible/audible part; the quiet removal it leaves behind is the actual effect. Louder gesture
    than the enzyme, gentler consequence.

28. **Both buttons re-roll every press.** Enzyme and catalyst are random in *amount* and in
    *effect* each time they are used. No fixed dose and no fixed target - pressing the same button
    twice in a row must not do the same thing twice. The bias stated in 26/27 describes the
    distribution the roll is drawn from, not a guaranteed outcome.

29. A **"RADIATE"** button - the high-stakes one.
    - **5%**: kills the sound and **resets the score to zero**.
    - **10%**: adds to the sound in some **random beneficial** way.
    - the remaining **85%**: a minor, mostly-neutral mutation.
    - **Visual:** a brief **red flash** that fades back to normal.
    - **Audio:** a subtle **click** (Geiger-counter) layer over the loop for several seconds,
      then fading out.
    Unlike the enzyme and catalyst this one can cost the player everything, so the odds are fixed
    and stated rather than randomised.

30. An **"ADD HEAT"** button. It **speeds up the frequency** of one thing chosen at random: an
    **oscillator**, an **LFO lane**, or the **loop/grain playback rate**. Which of the three it
    catches is the roll; heat does not say what it will accelerate.

31. An **"ADD WATER"** button - the opposite of heat. It **slows down** one randomly chosen
    oscillator, LFO lane or loop/grain rate. Together heat and water are the player's coarse
    tempo controls over the colony's movement, and neither says in advance what it will catch.

### Visualisation
22. Very **interactive and colourful**.
23. **More noise ⇒ greyer.** **More varied ⇒ more colourful.**
24. **Fractal ghost reward.** When the audio is *appealing* and the score is climbing, a ghostly
    fractal flashes over the mutations as a reward. Must be **brief**, **rare** ("once in a while"),
    and **shown once** per earning — never a constant overlay. It is the game's "you're doing well"
    signal, so it has to stay scarce enough to feel earned.

---

## 2. Why the current build converges (root-cause analysis)

Read from `Source/Engine/Colony.cpp`, `Genome.cpp`, `Cells.cpp`.

| # | Cause | Location |
|---|---|---|
| C1 | **One global attractor.** `selectionTargetGenome()` builds a single target genome; step 8 of `ecologyTick` calls `driftToward(target)` on *every* cell every tick. This is a direct homogenising force. | `Colony.cpp` `ecologyTick` step 8 |
| C2 | **Fitness = closeness to that one target.** `evaluateFitness()` scores cells purely by distance to `tgt`, so selection reinforces C1. No novelty term, no fitness sharing. | `Colony.cpp::evaluateFitness` |
| C3 | **All founders come from one baseline.** `germinateFromSource` seeds every cell as `baselineGenome` + mutate, and `baselineGenome` is derived deterministically from the source's spectral features — so two runs on the same source start from the same place. | `Colony.cpp::germinateFromSource` |
| C4 | **Immigrants are pulled home.** The diversity-rescue immigrant is `blend(baseline, random, 0.6)` — 40% baseline, so even the anti-convergence mechanism converges. | `Colony.cpp` `ecologyTick` step 9 |
| C5 | **No per-cell modulation.** `Cell::renderAdd` is a pure function of the genome. Two cells with similar genomes are *sample-identical*. Nothing wobbles; there are no per-cell LFOs at all. | `Cells.cpp::renderAdd` |
| C6 | **Only 20 traits, all timbre-static.** No genes for modulation rate/depth/shape/destination. | `Genome.h::Trait` |
| C7 | **No per-run world variation.** The rules of the ecology are identical every run; only the seed changes, and the seed only perturbs a shared attractor. | whole engine |
| C8 | **Colony-level PostChain LFOs are global**, so they move every cell together — that reads as one wobbling sound, not many. | `PostChain.cpp` |

---

## 3. Research — findings and what we take from each

Searches performed 2026-09-15. Sources listed in §3.6.

### 3.1 Premature convergence / diversity maintenance
- **Fitness sharing / niching**: penalise an individual's fitness by how crowded its neighbourhood is
  in genome space. Directly cancels C2.
- **Island (deme) model**: split the population into semi-isolated sub-populations, each evolving its
  own way, with occasional migration. Prevents one attractor dominating. Maps perfectly onto the
  existing `speciesGroupId`.
- **Novelty search**: score individuals on *behavioural* novelty — distance to the k nearest
  neighbours *and* to an archive of previously seen behaviours — rather than on an objective.
  Known to avoid the deception that causes convergence.
- **Quality-Diversity / MAP-Elites**: discretise a *behaviour space* into bins, keep the best
  individual per bin. Yields an archive of diverse, good solutions rather than one winner.
  We get a free "library of timbres this colony discovered".
- **Adaptive operator control / hypermutation**: detect stagnation and temporarily crank mutation
  rate & depth, then relax. Classic cure for a flat-lined population.
- **Multi-island QD with periodic migration** is the current state of the art for "never stagnates".

**Design decision:** implement *all five* — niche attractors (islands), fitness sharing, a novelty
term, a MAP-Elites archive keyed on audible descriptors, and a stagnation→hypermutation trigger.

### 3.2 Keeping audio *interesting* rather than noisy
- **Spectral flatness (Wiener entropy)** = geometric mean / arithmetic mean of the power spectrum.
  ≈0 ⇒ tonal, ≈1 ⇒ noise. This is the exact measurement needed for requirements 7, 18, 23.
  MPEG-7 standardises it as `AudioSpectralFlatness`. Cheap enough to run per-block.
- **Spectral flux** (frame-to-frame spectral change) measures *movement*; near-zero flux over a long
  window = the "static drone" failure in requirement 6.
- So: **flatness ⇒ grey + score freeze**, **flux/variance ⇒ colour + score rate**. Two numbers drive
  almost the whole game layer.

### 3.3 What makes a sound *appealing* (requirement 8)
- **Sensory dissonance / roughness** (Plomp–Levelt, parameterised by Sethares) is the dominant
  predictor of perceived consonance. Pairwise over partials:

  ```
  d(f1,f2,a1,a2) = amin * [ e^(-b1*s*Δf) - e^(-b2*s*Δf) ]
  b1 = 3.5, b2 = 5.75, s = 0.24 / (0.0207*fmin + 18.96), Δf = fmax - fmin
  ```
  Summed over all partial pairs → total roughness. Low roughness ⇒ consonant ⇒ appealing.
- **Sharpness** (spectral weighting toward HF) also reduces consonance at the extremes; a mid
  centroid is preferred. **Tonalness** (inverse flatness) raises it.
- Therefore the "appeal" fitness term = `w1*(1-roughness) + w2*tonalness + w3*centroid_comfort
  + w4*(harmonicity)`, and crucially it is **only one term** — blended against novelty so we don't
  just re-create C2 with a prettier target.
- **Negative user input (right-click) flips the sign** of the appeal term locally, satisfying
  "unless the input is negative".

### 3.4 Randomness that stays interesting — 1/f
- **Voss–McCartney / 1/f pink noise**: sum several independent random sequences each updated at a
  *different rate* (octave-spaced). White noise (1/f⁰) is too random, Brownian (1/f²) too
  correlated; **1/f is the perceptual sweet spot** — listeners consistently rate 1/f-driven music as
  more musical than either extreme.
- This is a **direct answer to requirement 5**: a per-cell bank of octave-spaced LFOs summed
  together *is* a Voss–McCartney pink generator. Rates spread logarithmically from ~24 Hz down to
  ~0.004 Hz (≈250 s per cycle) give both the fast wobble and the >1-minute oscillation the user
  asked for, and their sum is perceptually pleasing rather than arbitrary.

### 3.5 Prior art on GitHub / in the literature
- **soundGene** — evolutionary VST host; user acts as the fitness function, with generation /
  crossover / mutation over patch parameters. Confirms the interaction model but it is *offline*
  (breed, audition, pick) rather than a continuously-living colony.
- **spiegelib** — synthesizer-programming research library (feature extraction + search).
  Useful reference for audio feature sets.
- **Evolutionary music** in general is dominated by **interactive** evolutionary computation,
  because aesthetic fitness is hard to compute. MUTAGEN's differentiator: a *real-time* ecology
  where the user's clicks are the interactive fitness signal and psychoacoustic metrics supply an
  automatic fitness floor.
- **Generative variational timbre spaces (IRCAM ACIDS)** — perceptually regularised timbre
  latent spaces; conceptually what our MAP-Elites descriptor grid approximates cheaply.

### 3.6 Sources
Diversity / convergence
- https://www.sciencedirect.com/science/article/abs/pii/S002002551500729X
- https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=4811bc9bcd24c9a363afe454e6737b48c24f8837
- https://arxiv.org/pdf/1407.0576
- https://www.emergentmind.com/topics/map-elites-algorithm
- https://www.emergentmind.com/topics/population-and-island-model
- https://arxiv.org/pdf/2105.10317
- https://www.ncbi.nlm.nih.gov/pmc/articles/PMC12129357/

Audio descriptors
- https://en.wikipedia.org/wiki/Spectral_flatness
- https://www.johndcook.com/blog/2016/05/03/spectral-flatness/
- https://www.mathworks.com/help/audio/ug/spectral-descriptors.html
- https://ismir2000.ismir.net/posters/izmirli.pdf

Psychoacoustic appeal
- https://pmc.ncbi.nlm.nih.gov/articles/PMC9166839/
- https://journals.sagepub.com/doi/10.1177/20592043211030471
- https://gist.github.com/endolith/3066664
- https://minds.wisconsin.edu/bitstream/handle/1793/66894/GarriganSpr2013.pdf?sequence=1

1/f randomness
- https://people.smp.uq.edu.au/MichaelBulmer/research/papers/fractalmusic.pdf
- https://www.firstpr.com.au/dsp/pink-noise/
- https://dsprelated.com/showarticle/908.php

Prior art
- https://github.com/P1ger/soundGene
- https://acids-ircam.github.io/variational-timbre/
- https://en.wikipedia.org/wiki/Evolutionary_music

Entropy harvesting
- https://link.springer.com/chapter/10.1007/978-3-032-16342-4_1
- http://koclab.cs.ucsb.edu/teaching/cren/project/2005past/morrison.pdf
- https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4634515/

---

## 4. Architecture of the overhaul

```
                    EntropyPool  (radio/mic LSBs + timer jitter + random_device + mouse)
                         |  stirs
                         v
   WorldSeed ----> Colony ecology  <---- Environment (params + slow "weather" LFOs)
      |               |      ^
      |               |      | fitness = w_novelty*N + w_appeal*A + w_user*U - crowding
      |               v      |
      |          Cell bank --+-- ModBank: 6 octave-spaced LFOs/cell (24 Hz .. 0.004 Hz) = 1/f drift
      |               |
      |               v
      |          audio out --> Analyzer (flatness / flux / centroid / roughness / variance)
      |                            |
      |                            +--> Homeostasis (anti-noise, anti-static guard)
      |                            +--> ScoreSystem (rate ∝ variance, freeze on noise)
      |                            +--> Visuals (colour ∝ variance, grey ∝ flatness)
      v
   per-run character: scale, life tempo, modulation topology, species mix, descriptor weights
```

Key new files (planned):
- `Source/Engine/Entropy.h/.cpp` — entropy pool + PRNG stirring
- `Source/Engine/ModBank.h` — per-cell multi-rate LFO bank (Voss–McCartney shaped)
- `Source/Engine/Descriptors.h/.cpp` — flatness / flux / centroid / roughness, RT-safe
- `Source/Engine/Novelty.h/.cpp` — behaviour archive + MAP-Elites grid + novelty scoring
- `Source/Engine/WorldSeed.h` — per-run randomised ecology character
- `Source/Engine/ScoreSystem.h/.cpp` — score, combo, events, high-score table I/O
- `Source/Engine/MicInput.h/.cpp` — mic capture + feedback protection
- `Source/GUI/WaveField.h/.cpp` — 2D wave simulation for click/drag ripples
- `Source/GUI/ScoreHud.h/.cpp` — score / combo / event toasts / high-score table

---

## 5. Milestones

Each milestone: implement → build → commit → push. Tick when pushed.

- [x] **M0 — Recon + research + this file.** Repo created, baseline pushed.
- [x] **M1 — Entropy & per-run identity.** `EntropyPool` (random_device + timer jitter + audio-input
      LSBs w/ von Neumann debiasing + mouse jitter), `Rng` stirring, `WorldSeed` randomising the
      per-run character. Fixes C3, C7.
- [x] **M2 — Wobble.** Genome gains modulation traits; `ModBank` gives every cell octave-spaced
      LFOs from ~24 Hz to ~0.004 Hz routed to pitch/amp/formant/brightness/pan/density/resonance.
      Fixes C5, C6, satisfies req. 4/5.
- [x] **M3 — Anti-convergence.** Niche attractors (islands), fitness sharing, novelty term,
      MAP-Elites archive, stagnation→hypermutation. Fixes C1, C2, C4.
- [x] **M4 — Descriptors & homeostasis.** RT-safe spectral flatness / flux / centroid / roughness.
      Anti-noise guard + anti-static "boredom" drive + appeal-seeking fitness. Req. 6/7/8.
- [x] **M5 — Score & persistence.** ScoreSystem, combo, events, freeze-on-noise, high-score table,
      save/load runs. Req. 16-21.
- [x] **M6 — Interactive visuals + gesture knobs.** WaveField, left-click mutate, drag waves ⇒ mass mutation,
      right-click subtractive damage, colour⇄grey mapping, score HUD, rare fractal-ghost reward
      flash gated on appeal + rising score, and the PITCH / LFO / OSC gesture knobs.
      Req. 9-12, 22-31.
- [x] **M7 — Ingestion.** Drag & drop samples eaten into the colony, multi-sample source pool,
      mic capture with feedback protection, radio-noise entropy tap. Req. 13-15.
- [x] **M8 — Polish & extras.** Additional fun features, README rewrite, final tuning pass.
- [x] **M10 — The measurement was wrong.** The noisiness metric rebuilt around
      periodicity so a modulated note stops reading as noise; the noise verdict,
      the grey ramp and the fractal-reward gate re-derived from it. Closes the
      open item M8 left behind.
- [x] **M9 — The house kit.** `theme.md` (the visual identity shared by every plugin in
      the range) and `include.md` (the feature set every plugin must ship) applied in
      full: look and feel, header, presets, help, options, right-click contract,
      tooltips, random, reset, export, MIDI learn, and a project-specific icon.

---

## 6. Session log

### 2026-09-15 - session 1
- Read the whole engine + GUI. Wrote the root-cause table in §2.
- Ran 8 web searches; wrote §3.
- Created this file. Next: create the GitHub repo, push M0, then start M1.
- Repo: https://github.com/djshellshoxxx/evolve (private).
- **M1 done.** `Entropy.{h,cpp}` (pool + four harvesters, von Neumann debiased audio tap),
  `WorldSeed.h` (per-run rules: tuning, partial palette, life tempo, lane routing, niche count,
  fitness weights, per-world noise ceiling). Colony now owns a `WorldSeed` and re-points every
  cell at it via `setWorld`.
- **M2 done.** `ModBank.h` - 6 LFO lanes per cell, log-spread ~0.0028 Hz..26 Hz (about six minutes
  per cycle at the slow end), Voss-McCartney summed to 1/f, routed by the world, phase-scattered
  per cell. Genome grew 14 modulation traits (numTraits 20 -> 34) with a `enforceMovementFloor()`
  that runs after every mutation so a lineage can become subtle but never still.
  `Cells.cpp` renderAdd rewritten: modulation applied to pitch/amp/formant/brightness/pan/density/
  resonance/grain-rate/detune, world tuning quantisation, world partial palette for both the
  spectral and resonator species, per-world noise ceiling, and the enzyme/catalyst/Geiger overlays.
  Builds clean (Release standalone, exit 0).
- Next: M3 - replace the single global attractor in `Colony::ecologyTick` with niche attractors,
  novelty + fitness sharing, MAP-Elites re-seeding, stagnation storms. `Novelty.h` is written.
- **M3 done.** `Novelty.h` wired in and `Colony` rebuilt around it:
  - **Islands.** `Niche` structs (2-8 per world) each own a target genome that random-walks
    independently and occasionally jumps. Founders are mutated copies of an island's target, not of
    one source-derived baseline. Selection drift goes to a cell's *own* island and its coefficient
    dropped from 0.25 to 0.11 - it nudges, it no longer herds.
  - **Fitness sharing.** Cells within 0.18 of each other in behaviour space divide their fitness
    through a triangular kernel, so a crowd is always worth less per head than an empty niche.
  - **Novelty search.** k-NN (k=8) distance to a 192-entry rolling behaviour archive, amortised a
    few cells per tick. Only genuinely new behaviours enter the archive, otherwise a static colony
    fills it with copies of itself and everything reads as novelty zero.
  - **MAP-Elites.** 6x5x4 grid over brightness/density/noise. Population rescues re-seed from the
    *most distant* stored elite, replacing the old immigrant that was blended 60/40 back toward the
    baseline - i.e. the old anti-convergence mechanism was itself converging.
  - **Dispersal.** An explicit repulsive force: cells that sit within 0.12 of the colony mean get one
    trait shoved. Selection is attractive; without this the population still slowly balls up.
  - **Stagnation storms.** Two exponential averages at 3 s and 45 s; when they agree and both sit
    low for 12 s, hypermutation fires, a third of the colony migrates between islands, and the
    islands themselves are mutated.
- **M4 done.** `Descriptors.{h,cpp}` - 1024-point FFT at a 512 hop computing spectral flatness
  (Wiener entropy), centroid, positive-only flux, Plomp-Levelt/Sethares roughness over the 12
  strongest peaks, tonalness, crest, long-window variety and stasis, and a combined `appeal`.
  `noiseLocked` is hysteretic (engages at 2.2 s of sustained flatness, releases at 0.6 s) so a
  musical burst of noise does not end a run. Colony consumes it:
  - **noiseGuard** does nothing for the first 8 seconds of a lock - the player is meant to see the
    colour drain, notice the score stopped and fix it. Only if they ignore it does the guard start
    pulling survivors tonal and culling the noisiest cell. A backstop, not an autopilot.
  - **boredomDrive** engages in 3 s, because a drone is boring immediately. It widens modulation
    rather than changing timbre, so the sound the player built survives - it just starts moving.
  - **appeal weight slides**: when the measured output is pleasant, novelty dominates and the colony
    explores; as it drifts toward noise the appeal weight rises and pulls it back. Negative user
    intent (right-click) inverts it, so damage is a legitimate direction when asked for.
- **Gestures done** (engine half of M6): `addEnzyme`, `addCatalyst`, `addHeat(+/-1)` and `radiate`,
  all re-rolling amount *and* effect per press except RADIATE, whose 5/10/85 odds are fixed because
  it is the only one that can cost the whole run. Cells grew a shimmer band, a decaying pitch
  excursion and a Poisson Geiger click train.
- **Processor** now seeds both the colony and its WorldSeed from harvested entropy, taps the audio
  input into the entropy pool every block, and routes the new commands.
- Note: `cmake --build build --config Release --target MUTAGEN_Standalone` occasionally dies with
  `CL.exe exited with code -1` under full parallelism on this machine; `-j 1` always completes.
- Next: M5 (score + high-score table) and M6 (visuals, wave field, gesture knobs, fractal reward).
- **M5 done.** `ScoreSystem.{h,cpp}` on the message thread, driven entirely by the published
  snapshot - nothing on the audio thread knows the score exists.
  - Rate is `variety^1.25` scaled by novelty, archive coverage and a small appeal term, times the
    combo multiplier. Appeal is deliberately a *small* term: weight it heavily and the optimal
    strategy becomes one consonant drone, which is the opposite of the point.
  - Approaching a noise lock sags the rate before it bites (`greyness` costs up to 70%), so the
    number starts falling while there is still time to act.
  - Freezes on `noiseLocked` or an empty colony, with the reason shown. Clearing a lock pays 1200
    and acting *while* frozen builds combo at double rate - the move the game wants you to learn.
  - Events: NEW TIMBRE (archive coverage grew), BLOOM, GENERATION, COLONY RESCUED, MOVING AGAIN,
    DIGESTED, FATAL DOSE, BENEFICIAL MUTATION.
  - High-score table in `%APPDATA%/MUTAGEN/scores.json`, top 20, with live projected rank.
  - Runs save to `.mutagen` (plugin state + score + stats); saving also files the run on the board.
- **M6 done.**
  - `WaveField.{h,cpp}` - 112x72 damped wave equation at fixed 120 Hz substeps, absorbing edges,
    a separate damage channel, rendered through an Image so cost is independent of window size.
    Saturation = variety, desaturation = greyness, exactly as specified.
  - `CultureChamber` rewritten as the play surface. Left click mutates, left drag leaves a wake
    whose strength comes from cursor speed, right click/drag strips, ctrl+right opens the old menu.
    Cells physically swell where the wave passes, so the ripple and the mutation are one event.
  - `ScoreHud.{h,cpp}` - score, multiplier, combo, VARIETY and NOISE meters (the whole scoring rule,
    legible from two bars), event toasts, frozen banner, high-score overlay.
  - `FractalGhost` - recursive branching structure, re-rolled per appearance, gated on
    appealing + varied + scoring well held for 7 s, then a 30-70 s random cooldown.
  - `GameBar.{h,cpp}` - ENZYME, CATALYST, HEAT, WATER, RADIATE, NEW WORLD, SAVE/LOAD RUN, SCORES,
    mic controls, and the three `GestureKnob`s. The knobs spring back to centre because there is no
    setting to return to - they send a gesture, not a value.
- **M7 done.** `Ingest.{h,cpp}`:
  - `SourcePool` stitches rather than mixes. Summing N uncorrelated recordings *is* noise, so each
    drop replaces crossfaded segments of the digest buffer instead of being added to it, and the
    replaced share shrinks with each drop (0.60 / (1 + 0.55n)) so the newest sample can never
    completely displace what the colony already is.
  - `MicInput` guards feedback in three layers: output muted while capturing (an open loop cannot
    howl - this is the default), a howl detector requiring narrow-band *and* same-band-persistent
    *and* growing simultaneously for 0.45 s (music is regularly one or two of those, almost never
    all three) which aborts and notches, and a hard output ceiling underneath both.
- **Performance.** Measured on the standalone: 0.85 -> 0.57 cores total.
  - `evaluateFitness` (O(n^2) crowding + k-NN) moved off the audio-block rate onto its own 22 Hz
    accumulator; nothing it measures changes faster than that.
  - `WaveField::render` uses an inline HSV->RGB and resolves nearest-island hue on a quarter-
    resolution grid (64k distance tests per frame -> 4k). GUI cost 0.21 -> 0.07 cores.
  - `fastSin` (Bhaskara + correction, ~0.1% error) for the spectral partial bank, which was the
    hottest loop in the engine at up to 12 sines per sample per cell. Audio 0.71 -> 0.50 cores.
- Smoke test: standalone launches, creates its window at 1322x888, stays responsive, memory flat.
  Screen capture is not available in this session, so the visuals have not been verified by eye -
  that needs a human look.
- Next: M8 - README, a pass over the remaining fun features, and tuning by ear.

### 2026-09-15 - session 1, M8

Built `Tests/DivergenceTest.cpp` (target `MutagenDivergenceTest`) because "they all
evolve to sound almost the same" is a measurable claim and should not be answered with
an assertion. Six colonies, 45 s each, nobody touching anything.

**It immediately falsified the first fix.** Genome distance was healthy (0.22) but the
*spectrum* distance was 0.05 - the runs had diverged genetically and still sounded
alike. Centroids clustered at 0.646-0.719 across every world. Cause: `initNiches` drew
each island uniformly across 0..1 on every axis, in *every* world, so islands within a
colony were nicely spread and every colony averaged to the same spectrum. Diversity
inside the colony was cancelling diversity between colonies.

Fixes, in the order the test forced them:

1. **Worlds got narrow character.** `brightSpread`, `densitySpread`, `pitchSpreadN`,
   `noiseBias` - a world is now a *place* in timbre space, not a sampling of all of it.
   Islands and founders are placed relative to it.
2. **A colony-wide voice.** `applyWorldVoice` - tilt plus one broad formant applied
   *after* the sum, which is the only part of a world's identity that averaging cannot
   erase. Spectrum distance 0.05 -> 0.35.
3. **The metric was wrong too.** A bright world's tilt inflated global spectral flatness
   and froze its score while it was still perfectly tonal. Tried band-wise flatness
   (worse - musical material saturated at 1.0), then settled on **envelope whitening**:
   divide the spectrum by its own smoothed envelope and measure flatness on the residual.
   A tilt scales both identically so it cancels exactly. Also raised the FFT 1024 -> 2048,
   because at 43 Hz per bin the harmonics of anything low-pitched were under three bins
   apart and the whitening window could not separate them.
4. **Calibration became a measurement.** The test now runs sine / harmonic tone /
   detuned saws / pink / white noise through the real analyser on every run and prints
   the numbers, so the constants in `Descriptors.cpp` are read off data.
5. **The default source was pink noise.** `makePrimitive` defaulted to noise, and the
   grain species granulates the seed directly - so a noise seed gives noise no matter
   how the colony evolves, and the spectral species scaled its own noise mix by the
   seed's measured noisiness on top. Added `primitiveTone` (plucked harmonic decays at
   related pitches) and made it the default. Appeal 0.39 -> 0.62, flatness stopped
   pinning and started varying per world.
6. **Density.** Noise ceiling 0.30-0.70 -> 0.10-0.42, voice limit 5-16 -> 4-10,
   quantisation probability 0.72 -> 0.88, spectral noise band no longer boosted 1.4x,
   partial tilt floor 0.2 -> 0.65. Added `updateVoicing` - a dominance hierarchy so the
   top few cells are audible and the rest are texture.
7. **Two real bugs the test surfaced:** colonies going fully extinct on their own (now
   floored at 4 cells while exploring - the player may still wipe it out, the ecology may
   not), and the noise guard culling a colony of forty down to two, which technically
   ends the noise the way unplugging something ends a hum (now floored at 10 cells).
8. **A per-sample bug I introduced in M2:** `mod.jitter()` was multiplying the grain read
   rate *per sample*, which is frequency modulation by white noise - it turns any source
   into hiss. Moved to once per grain.

Final state: all six checks pass. Spectrum distance 0.34 mean, genome distance 0.24,
every run keeps moving, no run parks in noise.

**Honest limitation, documented in the README.** The `flatness` column still reads 1.000
for every colony. That is the metric saturating, not the engine failing: the density
sweep shows a *single* cell already reads 0.70 against white noise's 0.84, because
granular synthesis genuinely fills the space between partials - grain windows smear,
modulation adds sidebands, detuned voices overlap. Capping the population was never
going to fix it. So the "has this collapsed" verdict is a composite of flatness, appeal
and roughness rather than flatness alone. Getting the raw number down is a synthesis
change (fewer partials, longer grains, tighter quantisation), not a threshold change,
and it is the obvious next piece of work.

Also not yet verified: **the visuals have never been seen.** Screen capture is
unavailable in this session, so the wave field, the colour/grey mapping, the score HUD
and the fractal ghost have been compiled and exercised but not looked at. That needs a
human.

Standalone at the end of M8: builds clean, launches, window created, responsive,
~0.81 cores, memory flat.


### 2026-09-16 - session 2, M9: theme.md and include.md

Two specification files arrived in the repo: `theme.md`, the visual identity shared by
every plugin in the range, and `include.md`, the feature set every one of them has to
ship. Part of the work was already on disk from the end of session 1 but was not in the
build - `AppOptions.h`, `MidiLearn.h`, `PresetManager.*`, `HelpView.*`, `OptionsView.*`
and `ParamControl.*` existed as files that nothing compiled or called. This session
finished them and wired them in.

**theme.md - the visual identity.** `MutagenLookAndFeel` now carries the house palette,
the Inter / JetBrains Mono type scale with documented fallbacks, the 270-degree value
arc drawn outside the knob body, the 4px-radius buttons with the 15%-accent on state,
the teal-to-red meter gradient, the dark tooltip pill, and the two house marks: the 2px
diagonal accent notch in the top-left and the version number in a 9px mono footer.
The one licensed deviation is MUTAGEN's species hues, which are *data* colours - they
encode which organism you are looking at, the way a chart's categorical palette encodes
a series - so they sit outside the "re-tint one accent" rule rather than breaking it.

**The header.** `TopBar` was rebuilt as two strips. The upper one is the 32px house
header the spec defines - name on the left; FILE menu, preset selector with step
arrows, A/B compare, options gear and manual on the right. The lower one is MUTAGEN's
own: role, CPU budget, colony statistics, EXPLORING/PRESERVED and the run verbs. The
split is deliberate: the header is a contract shared with the other plugins, and mixing
this instrument's verbs into it would give each plugin a differently-shaped header.

**include.md, item by item.**
- *Help* - `HelpView`, fourteen sections, navigation on the left, definition lists on
  the right, version in the footer. Its descriptions of ENZYME, CATALYST, HEAT, WATER
  and RADIATE were wrong - inherited from an earlier design - and have been rewritten
  against what `Colony.cpp` actually does, including RADIATE's 5/10/85.
- *Icon* - `Resources/icon.png`, a petri dish and colony with the house notch, kept as
  the script that cut it (`make_icon.py`) so the next plugin can start from the same
  geometry with its own accent.
- *Presets* - `PresetManager`, twenty-two factory presets in six categories. A preset
  carries the parameter surface only: loading one changes the world the colony lives in
  and lets it react, rather than replacing the thing that has been evolving. Saving the
  organism as well is what a run file is for. Every preset resets to defaults first, so
  a preset cannot inherit stray values from whatever was loaded before it.
- *Reset* - already existed; it now also clears the MIDI map and the randomiser's
  history.
- *Save / Save As / Open / Options* - on the FILE menu, alongside the run files and the
  audio export.
- *Export audio* - the existing `RenderEngine`, reachable from both the verb strip and
  the menu.
- *Right-click* - `paramMenu` gives Set Value, Reset to Default and Map to MIDI on every
  control. Implemented by subclassing (`ParamSlider`/`ParamButton`/`ParamCombo`/
  `ParamToggle`) rather than by a mouse listener, because a listener cannot stop
  `juce::Slider` arming a right-drag first - a listener-based menu leaves every knob
  jumping around underneath the menu it just opened.
- *Tooltips* - `ParamHelp.cpp` holds one sentence per control, kept apart from
  `createLayout()` because the layout is the contract with the host and this is prose
  that will be reworded far more often. A `TooltipWindow` is created and destroyed
  rather than hidden when the option is toggled, so "off" costs nothing.
- *Random* - `PresetManager::randomise()`, first press from where you are, every press
  after that from defaults. Output level, dry/wet, CPU budget and role are excluded:
  they are the user's setup, not the sound.
- *Progress file* - this document.

**A/B compare** holds two whole parameter states and switches them under the same
living colony. The first switch copies the slot being left into the empty one, so there
is always something on the other side to compare against.

**One real bug, found by looking at it.** The options page told standalone users they
were running inside a host. Everything in `target_sources(MUTAGEN ...)` is compiled
once into the shared code all three formats link, with `JucePlugin_Build_Standalone`
= 0, so `#if`-ing on it in `OptionsView.cpp` could never be true - the device selector
was unreachable in every build. Inverted the direction: `GUI/DeviceSetupHook.h` holds
one factory slot, and `Standalone/DeviceSetup.cpp` - compiled into the standalone
target only - fills it at static-initialisation time. The plugin builds never compile
that file, the slot stays empty, and the page says the host owns the soundcard, which
there is true.

**The visuals have now been looked at.** Screen capture worked this session
(`PrintWindow` with `PW_RENDERFULLCONTENT`; `SetForegroundWindow` is refused to a
background process, so synthetic input has to go through `PostMessage` rather than
`mouse_event`). Three layout faults were visible and are fixed:
- The action bar was clipping RADIATE and the mic controls off its right-hand end at
  anything near the minimum window size. It now runs the full width above the timeline
  instead of sitting in the chamber's column, and its rows scale to the width they
  have rather than using fixed button widths.
- The entropy / mic read-out was drawn at a fixed offset from the bar's bottom edge,
  which the second row of buttons grew over. `resized()` now reserves it a strip.
- "Push Selection" did not fit its button and read as "USH SELECTIO". Shortened to
  "Select", with the full meaning in the tooltip.

Standalone and VST3 both build clean. Help, Options (with the device selector), the
preset selector, A/B, RANDOM, RESET and the tooltips were each exercised in the running
app and screenshotted.

**Still not verified by eye:** the fractal-ghost reward (it is gated on sustained good
play and a 30-70 s cooldown, so it did not appear during these sessions) and the
high-score overlay.


### 2026-09-16 - session 3, M10: the flatness saturation, and what it actually was

M8 left one open item and named it wrongly. The `flatness` column read 1.000 for
every colony, and the conclusion recorded at the time was that the metric was
saturating against genuinely dense granular material, so "getting the raw number
down is a synthesis change (fewer partials, longer grains, tighter quantisation),
not a threshold change". That was wrong, and the way to find out was to measure it
rather than to act on it.

**First, the sweep was lying.** `setActiveCap` clamped its argument to a minimum of
8, so the "x1", "x2" and "x4" rows of the density sweep had all silently been
measuring eight cells - the tool built specifically to tell us whether the density
came from the cells or from the crowd could not distinguish the two, and printed
three identical numbers to say so. Lower bound relaxed to 1. Nothing in the live
plugin asks for a cap below the CPU-budget minimum.

With that fixed the sweep immediately contradicted the crowd theory: a single
*spectral* cell read 0.735 against white noise's 0.843, while a single grain cell
read 0.559. So it was one species, not the population.

Two hypotheses, both tested by muting one thing at a time:

1. **The noise oscillator.** The band is white noise through a resonant SVF whose
   gain at centre is about q, and q reaches 8.5, so `noiseColour` arguably did not
   mean the fraction it says. Level-matched both sides and re-measured: it got
   *worse*, 0.735 to 0.775. The band had been quieter than the harmonics all along,
   and the "fix" was amplifying it. Reverted - the gene's effect was already
   monotonic and the honest reason to change it had evaporated.

2. **The modulation bank.** Muted it, changed nothing else: one spectral cell 0.613
   to 0.432, a two-cell colony 0.591 to 0.305, spectral x8 0.689 to 0.302. That is
   the answer, and it is not a synthesis problem, because the modulation is the
   entire point of the instrument. Six LFO lanes per cell from 0.003 Hz to 26 Hz,
   and a partial carrying a few hundred cents of vibrato sweeps across a dozen bins
   inside one 46 ms analysis frame. Spread over the frame it deposits its energy
   evenly across that span, which to any single-frame spectral statistic is exactly
   what broadband noise looks like. It is not what a *listener* hears: a wobbling
   note is obviously a note.

So the measurement was wrong, not the engine.

**A peak hold was tried first and rejected.** Holding a decaying per-bin maximum
does restore the ridge a wandering partial leaves, but it flattens noise too -
white noise went 0.843 to 0.957 - because the max over several frames converges to
a stable value either way. It moved both ends of the scale and separated nothing.
The tension is fundamental: resolving close partials needs a long window, not
smearing vibrato needs a short one, and no window length is good at both.

**What works is asking a different question.** Periodicity. A vibrato'd note is
still locally periodic and noise repeats at no lag, and frequency modulation barely
disturbs that. The autocorrelation is the inverse transform of the power spectrum,
which the analyser already computes, so it costs one extra transform of a buffer we
already have - measured at no CPU regression (0.759 cores against the 0.81 recorded
at the end of M8). Each lag is divided by the analysis window's own self-overlap,
precomputed once, or a low note scores as less periodic than a high one purely for
having a longer period and the metric becomes a pitch detector.

Noisiness is now `0.32 * whitened-flatness + 0.68 * (1 - periodicity)`. Periodicity
carries the weight because flatness's failure here is a false *positive* - it calls
healthy material noise - and that is the error that breaks the game.

**Three consequences, all of which were live bugs:**

- *The flatness calibration constants had drifted and nobody had looked.* The test
  prints them on every run precisely so drift is visible; `flatCalHigh` claimed
  noise began at 0.62 while the test was measuring white noise at 0.843, so
  everything above 0.62 mapped to exactly 1.000 and the top half of the scale was
  unreachable. Re-read off the measurements: 0.13 and 0.85.
- *The noise verdict was a composite propping up a broken number.* Flatness had
  been deliberately held to half the weight, with appeal and roughness carrying the
  decision, because flatness alone read 0.80-1.00 for healthy colonies and for dead
  ones alike. It can answer the question now, so it does: 0.80 flatness, 0.14
  appeal, 0.06 roughness, threshold 0.66. Healthy colonies measure 0.29-0.32 and
  pink and white noise both lock.
- *The visuals were permanently two-thirds grey.* `greyness` ramped from 0.52 on a
  scale that never went below 0.67. Requirement 23 asks for grey to mean something.
  Ramp moved to 0.42-0.66, which is above where a working colony lives and below
  the lock, so the drain is a warning again. Confirmed by eye in the running app:
  the chamber renders in colour.

**And the fractal ghost, which M8 recorded as "never verified by eye".** It had
never been seen because it could not happen. It gates on `appeal > 0.62`, appeal
carries a tonalness term, tonalness is `1 - flatness`, and flatness was pinned at
1.000 - so measured appeal across six healthy runs was 0.52-0.59 and the gate was
above the maximum the instrument could produce. "Rare" and "impossible" look
identical from outside, which is why it is now measured instead of watched for:
`runRewardCheck` drives four colonies through a real `ScoreSystem` and counts.

The gate moved to 0.76, just under the median of the six measured worlds. 0.80 was
tried first and is too high - above the mean of four of the six, and a two-minute
capture of the running app caught nothing. Measured result: **12 rewards in 10
minutes of active play, 0 with nobody touching it.** The reward is also gated on
the score *rate*, which carries the combo multiplier, so an untouched colony sits
at 0.44 of base against a 0.55 gate and can never earn it however good it sounds.
That is the intended shape - it is a "you are playing well" signal, not a "this
sounds nice" signal - but it does mean an idle window will never show one, and that
is worth stating rather than leaving as a surprise for whoever looks next.

**Still not seen by eye:** the ghost itself. It lasts 1.2 s on a 30-70 s cooldown;
three capture runs totalling about seven minutes, one of them driving synthetic
clicks into the window, did not land on one. The trigger path is short and was read
end to end (`consumeRewardFlash` -> `triggerReward` -> `ghost.trigger/update/render`)
and the flag is now confirmed to be raised at a measured rate, but the drawing has
not been photographed. So has the high-score overlay not been.

Test grew from six checks to ten. The three new ones are controls on the detector
itself, because a metric that returns zero for everything passes "no run parks in
noise" perfectly and is useless: white noise must still lock, a wobbling note must
not, and the gap between musical and noise must be worth having.


### 2026-10-05 - session 4, M11: HUD input-routing regression

The remaining M10 visual gap led to an input audit of the floating score HUD. The HUD
is sized to the entire CultureChamber and is brought to the front. PluginEditor then
re-enables mouse interception so the score/table controls can be clicked. Without a
custom hit-test that makes the *entire* HUD rectangle interactive, so ordinary chamber
clicks can be swallowed before CultureChamber gets a chance to turn them into mutation
or damage gestures.

Fix:
- `ScoreHud::hitTest()` now returns true only for the score card, SCORES button, or
  the open high-score sheet. The rest of the overlay is transparent to mouse input.
- Added `MutagenGuiSmokeTest`, an offscreen regression test that checks all three
  states: chamber background passes through, score card captures, open table captures.
- Registered the GUI smoke test with CTest and added it to the three-platform GitHub
  Actions build.

The fix is on main. The newest Actions run was queued when this continuation entry was
written, so the CI result still needs to be read before calling M11 fully verified.


## 2026-10-06 — Evolution steering and play pass

- Added **PUSH THE SOUND** controls to the Environment panel: DARK, BRIGHT, SPARSE, DENSE, NOISE, TONE, CALM and FIERCE. Each button writes a clean selection profile and applies an immediate selection burst.
- Added **COUNTER-EVOLVE**, which reads the live centroid, tonalness and roughness descriptors and intentionally selects away from the current character before forcing a mutation pass.
- Added **MUTATION DICE**, a bounded random four-axis steering throw with divergence plus a forced mutation. The random input is transformed deterministically so its mapping can be regression-tested.
- Added `Engine/EvolutionSteering.h` to keep direction, counter and dice profile logic independent of the GUI.
- Extended `MutagenGuiSmokeTest` with steering-profile, counter-evolution and dice-range assertions.
- Fixed one-shot selection bursts so parameter changes made immediately before the command are loaded into the Colony before selection is applied.
- Added tooltips and Help content for all three new interactions.


### 2026-10-06 — Steering intensity refinement
- Added an automatable **Push Strength** parameter for the new sound-direction controls.
- Direction, Counter-Evolve and Mutation Dice targets now scale continuously from subtle guidance to forceful steering.
- Selection-burst strength and the minimum selection pressure scale with the same control, so a low setting does not secretly apply a full-strength ecological shove.
- Added regression coverage for target scaling and clamping.


### 2026-10-06 — Deep progression / Field Journal
- Added a persistent, message-thread-only Field Journal with deterministic research levels.
- Implemented ten depth systems: journal, researchers, story chapters, branching directives, protocols, challenge deck, anomaly catalogue, relic cabinet, lineage codex and world atlas.
- Added six unlockable researcher characters and eight story chapters.
- Added ten unlockable research protocols tied to increasingly deep workflows.
- Added eight descriptor-driven anomaly records and eight milestone relics.
- Added late-game PRESERVE / ACCELERATE / RELEASE analysis; no ending permanently locks the others.
- Wired real gameplay events into progression: score growth, generations, discoveries, world changes, rescues, samples/mic capture, radiation, breeding, steering, saves and combo peaks.
- Added JOURNAL to the action strip plus a scrollable in-app progression view.
- Added a pure C++20 progression regression test; standalone compile passed with -Wall -Wextra -Werror.
- Full product rationale and thresholds are in docs/deep-progression-spec.md.


### 2026-10-06 — Haunted Laboratory / hidden discovery phase
- Added broad-zone sequential-click discovery with a deliberately easy five-click window.
- Added 100 persistent hidden creatures with stable generated names, lore, sound recipes and animation identities.
- Added five additional accidental patterns: Mirror Event, Corner Choir, Panic Bloom, MOTH Looks Back and Visitor Footprint.
- Added random later reappearances of already-discovered creatures.
- Added transparent desktop phantom animations that travel beyond the plugin bounds where the host/OS permits.
- Added a real-time-safe procedural hidden-event voice in the plugin output.
- Added whole-million score artifacts with unique numbered sound recipes, animation recipes and replayable skills.
- Added Journal collection display and a milestone-skill replay selector.
- Rewrote the story so surreal inconsistencies begin at chapter zero and escalate through the existing progression.
- Added a randomized-per-start session RNG with random_device plus high-resolution fallback.
- Added the requested 100,385-point one-shot 1-in-10 roll: roll 4 starts a guarded five-second mic capture, bypasses normal ingestion, reverses and granularly stretches it, then waits one silent second before playback.
- Added the requested 1,000,035-point one-shot 1-in-20 roll: roll 17 pulses an available Windows optical-drive tray three times.
- Added best-effort Windows system beeps at randomized hidden-event moments. Modern Windows may route these through the system audio device rather than a physical motherboard speaker.
- Replaced detached hardware threads with editor-owned timer/state logic so no worker can outlive a plugin DLL.
- Added regression coverage for exact score crossings, winning rolls, deterministic discovery, all 100 unique creature names/lore entries, hidden patterns, randomized roll bounds and unique consecutive million-point artifacts.
- Full design notes live in docs/haunted-lab-spec.md.


### 2026-10-07 — Build and lifetime audit

- Fixed JUCE `var` integer conversions that failed on Linux with the pinned JUCE build.
- Scoped desktop phantom windows to the editor lifetime; timed-out windows remove themselves through the owning registry.
- Replaced assertion-only progression checks so the test remains active in Release builds.
- Limited CI build parallelism and run the full CTest suite on each platform.

---

## Integration session (claude/focused-dirac-wb88c7)

Step 1 — DONE (builds on Linux, all tests pass): merged open PR #5 (feature/lab-games-orbs) and PR #6
(fix/pinned-build-lifetime-audit) into this branch; resolved the
Tests/ProgressionTest.cpp conflict (all checks use CHECK so they run in Release);
added the missing Source/GUI/LabGameOverlay.cpp to CMakeLists.
Also fixed compile errors: stale launchDesktopPhantom call sites, jlimit on
std::atomic, Display::userBounds -> userArea, incomplete PhantomRegistry type.

Remaining milestones (resume at the first unchecked):
- [x] S2 Story chapters: new levels/chapters, characters, twists, random events
      (seeded per run so no two games match). Every beat teaches an audio /
      music-theory fact woven into dialogue (intervals, harmonic series, ADSR,
      filters, Nyquist, dB, phase, reverb, scales, rhythm).
- [x] S3 Sound collection + WAV export: capture collected sounds (orb/game
      rewards, colony snapshots) and export them as 24-bit WAV files.
- [x] S4 Audit + bug fix pass; S5 optimisation + tests (ctest all green).
- [ ] S6 CLAP via clap-juce-extensions; CI release job builds Windows
      standalone .exe + VST3 + CLAP, Linux standalone + VST3 + CLAP, and
      publishes a GitHub release.
- [ ] S7 PR to main, green CI, merge.

Local build: cmake -B build -G Ninja -DJUCE_SOURCE_DIR=<JUCE 8.0.6 checkout>

S2/S3 notes: Source/Engine/Storyline.h (8 Resonance Acts, 7 new characters,
60 lexicon events, 16 ear checks, 10 modes x 12 keys, 8 twists, seeded per run),
Source/GUI/StoryPanel.* (narrator strip + director + COLLECT/EXPORT WAV),
Source/Engine/SoundCollection.* (DC-remove, -1 dBFS normalise, 10 ms fades,
24-bit WAV in <appdata>/MUTAGEN/Collected Sounds). Lab games from PR #5 were
declared but never wired: now wired in Source/LabGameFlow.cpp, each instrument
reward is collected as a WAV and comes with a fact. New CommandType::noteRelease.
Tests: MutagenSoundCollectionTest + story checks in MutagenProgressionTest.

S4 audit fixes: capture ring use-after-free/race (lock-guarded rings, atomic
heads) and a separate output ring so COLLECT/render record what is heard;
closing the editor no longer disarms the user's mic; trip-delay 1.5 kHz buzz
and stale-tail replay; orbs now spawn for lab-game prizes; soft aces in 21;
piano keys persisted; no WAV overwrite / progress loss across instances;
duplicate story notes; overlay button layout; -50 dBFS collect gate.
