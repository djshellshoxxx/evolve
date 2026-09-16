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
- [ ] **M1 — Entropy & per-run identity.** `EntropyPool` (random_device + timer jitter + audio-input
      LSBs w/ von Neumann debiasing + mouse jitter), `Rng` stirring, `WorldSeed` randomising the
      per-run character. Fixes C3, C7.
- [ ] **M2 — Wobble.** Genome gains modulation traits; `ModBank` gives every cell octave-spaced
      LFOs from ~24 Hz to ~0.004 Hz routed to pitch/amp/formant/brightness/pan/density/resonance.
      Fixes C5, C6, satisfies req. 4/5.
- [ ] **M3 — Anti-convergence.** Niche attractors (islands), fitness sharing, novelty term,
      MAP-Elites archive, stagnation→hypermutation. Fixes C1, C2, C4.
- [ ] **M4 — Descriptors & homeostasis.** RT-safe spectral flatness / flux / centroid / roughness.
      Anti-noise guard + anti-static "boredom" drive + appeal-seeking fitness. Req. 6/7/8.
- [ ] **M5 — Score & persistence.** ScoreSystem, combo, events, freeze-on-noise, high-score table,
      save/load runs. Req. 16-21.
- [ ] **M6 — Interactive visuals + gesture knobs.** WaveField, left-click mutate, drag waves ⇒ mass mutation,
      right-click subtractive damage, colour⇄grey mapping, score HUD, rare fractal-ghost reward
      flash gated on appeal + rising score, and the PITCH / LFO / OSC gesture knobs.
      Req. 9-12, 22-28.
- [ ] **M7 — Ingestion.** Drag & drop samples eaten into the colony, multi-sample source pool,
      mic capture with feedback protection, radio-noise entropy tap. Req. 13-15.
- [ ] **M8 — Polish & extras.** Additional fun features, README rewrite, final tuning pass.

---

## 6. Session log

### 2026-09-15 - session 1
- Read the whole engine + GUI. Wrote the root-cause table in §2.
- Ran 8 web searches; wrote §3.
- Created this file. Next: create the GitHub repo, push M0, then start M1.
