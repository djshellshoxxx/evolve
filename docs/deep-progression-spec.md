# MUTAGEN Deep Progression Specification

Status: implementation target for the deep-progression phase.

## Goal

MUTAGEN already has a deep generative engine and a strong run-level game loop. This phase adds a persistent meta-game without turning the instrument into a conventional RPG. Progression is earned by making and rescuing sounds, discovering timbral territory, changing worlds, breeding organisms, feeding the colony source material, and taking risks.

The progression layer must never run on the audio thread and must never make an old saved organism unplayable. It may reveal optional tools, lore, challenges and modifiers, but it must not silently alter the DSP of an existing run.

## Ten depth systems

1. Field Journal. Persistent lifetime research record: score earned, generations observed, timbre discoveries, worlds visited, rescues, samples digested, radiation exposures, breeding operations and steering interventions.
2. Researcher Characters. Six recurring characters unlock through different styles of play. Each has a distinct point of view and briefing text. Characters are narrative lenses, not player-stat bonuses.
3. Story Chapters. Eight story chapters unlock from research milestones. The story concerns the lab discovering that the colony is not merely converging on sound, but learning which forms of intervention the operator repeats.
4. Branching Directives. Three late-game philosophies become visible after enough evidence: Preserve, Accelerate and Release. The player's observed behavior determines which directive is recommended, but no choice deletes content.
5. Research Protocols. Ten optional protocols unlock over time. Protocols are named capabilities and advanced workflows that teach or expose existing deep systems instead of replacing basic controls.
6. Challenge Deck. Deterministic challenge cards are generated from the player's progression tier. Challenges reward trying underused systems: rescue a noise lock, hold high variety, visit new worlds, breed lineages, or use restrained steering.
7. Anomaly Catalogue. Rare combinations of descriptor/state conditions are recorded as anomalies. The catalogue makes unusual runs memorable and gives long-term collectors something other than score to pursue.
8. Relic Cabinet. Major milestones award non-consumable relics. Relics are trophies with descriptions tied to significant events, such as the first rescued saturation or a long-running lineage.
9. Lineage Codex. Breeding and generation milestones build lineage rank. The codex summarizes how far the user has taken inherited organisms and unlocks deeper breeding-related story material.
10. World Atlas and Endings. Visiting enough distinct worlds expands an abstract atlas. Once the late story is unlocked, three endings can be reached without permanently locking out the others. The endings summarize the user's dominant interaction style.

## Character roster

- Dr. Mara Voss, Ecologist. Unlock: first meaningful discovery. Believes diversity is a property to protect, not maximize blindly.
- Ivo Chen, Signal Engineer. Unlock: sustained generation progress. Focuses on measurable structure and repeatability.
- Nadi Okafor, Field Recordist. Unlock: digest source material. Treats outside sound as environmental DNA.
- MOTH, Lab automation. Unlock: repeated steering interventions. Starts as a diagnostic assistant and becomes an unreliable narrator.
- Jun Vale, Archivist. Unlock: enough world changes and lineage work. Cares about what survives between runs.
- The Visitor, unknown observer. Unlock: late-game anomaly and discovery thresholds. Its messages imply the colony may be responding to the operator's habits.

## Story chapters

0. Petri Dish
1. Variation
2. Selection Leaves a Trace
3. The Colony Remembers
4. Signal From Outside
5. MOTH's Hypothesis
6. The Visitor
7. Three Directives

Chapters unlock cumulatively. Story text is short enough to read inside a plugin panel.

## Progression rules

Progression is monotonic. A corrupt or older progression file must fail safely to an empty/default state. Unlock evaluation is deterministic from lifetime stats. No unlock depends on wall-clock dates or random chance.

ResearchState is derived from LifetimeStats; it is not the canonical persisted object. Persist the stats and unlocked anomaly/relic IDs. Recompute derived unlocks when loading so future versions can add thresholds without migration churn.

## Compatibility

- Existing runs and presets remain valid.
- The progression file is stored separately from high scores and organism saves.
- Audio-thread classes never depend on progression.
- UI may be closed at any time without affecting the engine.
- All progression evaluation logic must be covered by a small non-I/O unit test.
