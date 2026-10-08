# Spec 7: Idea Scheduler (presentation, progression and endless mutation)

Status: planned. Pure C++ core (`Source/Engine/IdeaScheduler.h`), driven from `StoryPanel`. Governs when each of the five ideas appears, how they change every time, and what they give back.

## The five ideas
`P` Phantom Accompanist, `T` Tuner Duels, `D` Dream Rooms, `A` The Room Remembers (mic), `B` The Choir of One (mic). A mic idea without a mic runs its fallback; it is never skipped.

## Two ways an idea arrives
Each cycle, a seeded shuffle assigns every idea a mode: three are **timed** (arrive by chance) and two are **milestone** (arrive when the player has earned them). The assignment is reshuffled every cycle and prefers to flip an idea's previous mode.

### Timed (chance over minutes)
- Earliest time: `lastPresentedAt + gap(c)` where `gap(c) = 120 s * 1.15^c + jitter(0..30 s)`. Never under 120 s, grows every cycle.
- After the earliest time, a check runs every `checkEvery(c) = 40 s * 1.08^c`. Each check succeeds with `p(c) = max(0.25, 0.75 * 0.93^c) + 0.06 * missStreak` (capped at 0.97). So the first idea is very likely, the second may or may not follow 2 minutes after, and a miss simply means a bit more time and another roll. `missStreak` resets on a success.
- Which timed idea arrives is a weighted draw from the timed ideas not yet presented this cycle (weights favour ideas whose prerequisites were met earliest).

### Milestone (progressive unlock)
- Each milestone idea has a requirement vector: unpaused play seconds, lexicon count, sounds collected, highest act, run score. Requirements are staggered by tier so unlocks come one after another over a long time, never together.
- Cycle scaling: every requirement is multiplied by `1.25^c`.
- Spacing: a milestone unlock also needs `>= 6 min * 1.2^c` since the previous milestone unlock and `>= gap(c)` since any presentation.
- Base tiers (cycle 0): tier 1 needs 10 min play + 6 lexicon, tier 2 needs 25 min + 14 lexicon + 4 collected + Act 3.

## Global rules
- One encounter at a time (idea, duel, room, Phantom call, quiz, banter): the director defers while any is active.
- Presenting an idea launches its encounter once at the current mutation level. If ignored for 3 minutes it expires; it still counts as presented (scratched off), but gives only a reduced reward.
- Cooldown after presentation: the gap timers above apply to both modes.

## Scratch-off list
The journal shows the five ideas; a presented idea is struck through for the rest of the cycle. When all five are struck, the cycle completes: a pause of `2 * gap(c)`, then `c += 1`, every idea's mutation level increases, names change (Roman numerals plus a generated epithet), the list resets, `p` falls, gaps and requirements grow.

## Mutation: the endless permutation algorithm
Each idea has a **genome**: `tempo`, `hue`, `timbre` (waveform/FM ratio family), `mode`, `rootOffset`, `rhythm` pattern id, `swing`, `style` (presentation id 0..9), `voicePatch`, `fx` id, `difficulty`, `motifTransform` flags (inversion, retrograde, augmentation, transposition).

For level `n` of idea `i` in run `s`:
1. Seed `r = hash(s, i, n)`. Start from the genome of level `n-1` (level 0 is the base genome).
2. Apply 2 to 4 operators chosen by `r` from: transpose by a circle-of-fifths step; modal shift; tempo multiply by a ratio from {3/4, 4/3, 2/3, 3/2, golden ratio} folded into 48..176 BPM; hue rotate by the golden angle (137.5 degrees); swap timbre family; change rhythm pattern; flip swing; change presentation style by `floor(n * 0.618 * styles) mod styles`; swap voice patch; swap fx; invert or retrograde the motif; difficulty plus one step (bounded, slow).
3. Reject and retry (up to 32 sub-seeds) unless the result is **noticeably different** from level `n-1`: at least two of the four sensory axes changed beyond thresholds (speed: tempo ratio >= 8%; colour: hue >= 40 degrees; sound: timbre or mode or voice patch changed; presentation: style changed), and weighted distance >= 2.0. Fallback after 32 failures: force hue +137.5 degrees and a style change.
4. **Make sense**: notes always from the chosen mode; pitch range C2..C6; tempo 48..176; consonance floor (reject interval sets whose roughness exceeds the level's allowance, which rises slowly with difficulty); never noise.
5. Avoid the last 12 fingerprints of this idea (hash of genome) so nothing repeats within a long session. Hue by golden angle and style by a low-discrepancy sequence guarantee the sequence never cycles.
6. Every 7th level performs a **family hop**: timbre, voice patch and fx are re-drawn from a new family, so very long sessions keep changing character.
The sequence is endless: it continues until the player quits.

## Rewards (positive, unique each presentation)
A reward table of 12 categories: score burst (`5000 * 1.2^n`), knowledge points, guaranteed demo fact, relic detune, new colony voice patch, dish colour theme, collected sound (rendered from the encounter), journal page (generated lore), luck boost (raises rare chance weights for 10 minutes), radiate shield (one use), mutation charges, title/epithet. The chosen `(category, flavour)` is derived from `hash(s, i, n)` and never repeats for the same idea within the last 8 levels. Reduced reward (about 40%) for an expired encounter. Magnitudes grow slowly (logarithmic), and nothing ever removes progress.

## Persistence
`IdeaProgress` in `resonance-acts.json`: `cycle`, `presentedMask`, `level[5]`, `lastPresentedPlaySec`, `missStreak`, `milestoneUnlockedMask`, `lastMilestoneAtPlaySec`, `rewardHistory[5][8]`, `fingerprints[5][12]`. All merged on save (max/OR), like existing progress.

## Tests (pure C++)
- Timed mode: gap never under 120 s; first-presentation probability high (>= 0.7 per check in simulation); eventually certain; p and gap monotone in cycle.
- Milestone mode: unlocks staggered, never simultaneous, spaced by the spacing rule, requirement scaling correct.
- Cycle completion: all five presented before reset; mode reshuffle flips where possible.
- Mutation: for 500 levels every adjacent pair is noticeably different; all genomes within bounds and musical; no fingerprint repeats within 12; family hop at multiples of 7.
- Rewards: no (category, flavour) repeat within 8 levels; magnitudes increase; deterministic per seed.
- Simulation: a 10-hour virtual session never presents two ideas within 120 s and never stalls.
