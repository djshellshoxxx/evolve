# Architecture and Design Rationale

Source: the README "The problem this version exists to solve" section, `Source/Engine/` and `PROGRESS.md`.

The README records why the previous build sounded the same across seeds, and how each cause was addressed:

| Cause | Change |
|---|---|
| One global attractor genome that every cell was pulled toward | Islands: 2 to 8 semi-isolated niches per world, each with a wandering target |
| Fitness equal to closeness to the attractor, so average was optimal | Fitness sharing, novelty search and a MAP-Elites archive; crowding is penalised |
| Founders derived from one seed genome | Founders drawn from the islands; islands re-rolled per run |
| Rescue immigrant blended back toward the problem | Re-seeding from the most behaviourally distant stored elite |
| No per-cell modulation, so similar genomes sounded identical | Six LFO lanes per cell, from about 0.003 Hz to 26 Hz |

Other design points from the layout:

- **Entropy pool.** `Entropy.*` mixes `random_device`, timer jitter, audio LSBs and gestures.
- **Genome.** 34 traits, 14 of them modulation (`Genome.*`).
- **Real-time descriptors.** `Descriptors.*` computes flatness, centroid, flux, roughness and appeal in a real-time-safe way.
- **Ecology separated from GUI.** `Source/Engine/` holds the colony, scoring and ingest; `Source/GUI/` holds the views.
- **Measured constants.** The divergence test calibrates the analyser on every run, so the constants are measurements.
