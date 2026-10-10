# MUTAGEN bug audit (read-only)

Scope: story layer (StoryPanel, StoryFx, Storyline, ChanceEvents, MusicFacts, storyio),
editor and lab-game integration, processor threading and state, score/radiate logic.
Method: source reading only. Nothing was built, run or edited, and git was not touched.

Basis: the audit started at commit 967afe7 with a clean tree. While it ran, HEAD moved to
394d515 (commits aa600e4, 39747b3, 394d515) and another working-tree edit landed
(StoryPanel.cpp, PluginProcessor.cpp/.h, PluginEditor.cpp, OrganismState.h, ParamHelp.cpp,
Parameters.*, CMakeLists.txt). Line numbers below are for the current working tree.
Only call sites of LiveMorph (WIP, "not yet built into the plugin") were reviewed.

## Counts

| Severity    | Count |
|-------------|-------|
| Crash       | 1     |
| Data loss   | 1     |
| Wrong behaviour | 11 |
| Cosmetic    | 3     |
| Total       | 16    |

## Findings

### C1  Crash: corrupt EVOLUTION node index or parent
- File: Source/Engine/EvolutionHistory.cpp:100-125 (fromValueTree), 60-78 (relayout, visit at 63).
  Reached from Source/PluginProcessor.cpp:1199-1206 (setStateInformation, host thread).
- Scenario: a state or project whose EVOLUTION NODE has an `id` that is negative or >= node count
  (fromValueTree keeps the stored id, line 109, and pushes it into childIds, line 120), or whose
  `parent` equals its own index (for example node 0 with parent 0). relayout then does
  `tree[(size_t) id]` unchecked (line 63), so an out-of-range id is an out-of-bounds write, and a
  self-parent recurses without bound until the stack overflows.
- Severity: crash. Confidence: verified-by-reading.
- Fix: in fromValueTree set `n.id = (int) tree.size()` and ignore the stored id; accept a parent
  only when `0 <= parent < tree.size()` at insert time (strictly earlier index); make relayout
  iterative or depth-capped.

### D1  Data loss: stale instance rolls back the fact deck and the seen set
- File: Source/Engine/SoundCollection.cpp:161-168 (merge), 191-192 (writes); Source/GUI/StoryPanel.cpp:88-91
  (destructor calls persist()); MusicFacts.cpp:1125 (replay).
- Scenario: instance A and instance B are open. B loaded factCounter=4. A shows facts up to counter 10
  and saves. B closes, and its destructor persists. The merge takes factCounter and factSeen from
  `mine` (line 162 copies B's whole Progress), and the seed branch at line 168 only runs when
  factSeed==0, so disk now holds counter 4 and B's seen set. On the next launch the deck replays from 4,
  so facts 4..9 repeat and A's seen marks are gone. Knowledge is merged with max (line 167), so
  one instance's new points are dropped whenever the other has a higher total.
- Severity: data loss. Confidence: verified-by-reading.
- Fix: in save(), when disk.factCounter > p.factCounter take disk's factCounter, and union the two seen
  sets through SeenSet (fromBase64, mark, toBase64). For knowledge, track per-instance deltas, or accept
  the max and note the loss.

### W1  Wrong behaviour: the story clock runs while paused
- File: Source/GUI/StoryPanel.cpp:112 (`clock += dt;` before any pause check), 136 (`if (paused || quizIndex >= 0) return;`),
  200, 212, 218, 224 (schedules compared with `clock`). Editor pause predicate: PluginEditor.cpp:783-786
  (help, breeding lab, performance, lab game, intro, name overlay).
- Scenario: Help is open for 4 minutes, or the intro and name flow takes 4 minutes at first launch.
  Every schedule (twistAtSec, nextQuizSec, chanceDir.nextAtSec, nextBanterSec, nextEventSec, keyIntroAt)
  is overdue on return. Tick fires them back to back: the twist at line 200, then the ear check at 212, then
  a chance event at 218, all within two or three ticks (about 70 ms). The lab-collect cooldown and
  secondsSinceTwist() are also on `clock`.
- Severity: wrong behaviour. Confidence: verified-by-reading.
- Fix: `if (! paused) clock += dt;`. Move the note-queue and capture processing onto a separate wall
  clock that always advances, so pending notes and captures still drain.

### W2  Wrong behaviour: an unanswered ear check freezes the director
- File: Source/GUI/StoryPanel.cpp:136 (early return while quizIndex >= 0), 561-575 (askQuiz; no timeout),
  577 (answer is the only exit).
- Scenario: the ear check appears and the player ignores it. Nothing else runs: no lines, no facts, no chance
  events, no plot advance (playClock at 140 is after the return), no finale. It waits until someone answers.
- Severity: wrong behaviour. Confidence: verified-by-reading.
- Fix: track quiz time separately and auto-answer as wrong after 60-90 s of unpaused time, or keep the fact and
  plot timers running while the quiz is up.

### W3  Wrong behaviour: story effects can wipe the score
- File: Source/GUI/StoryPanel.cpp:621 (`Effect::radiate` pushes CommandType::radiate, ungated);
  Source/Engine/Storyline.h:89, 116, 120, 136 (events), 347, 365 (twists), 394 (banter);
  Source/Engine/Colony.cpp:2040 (5% fatal branch); PluginEditor.cpp:804-809 -> ScoreSystem.cpp:263-268
  (fatal sets total = 0 and clears the combo).
- Scenario: lexicon event 116 ("White noise...", minimum act 2) is drawn. The colony radiates, and with
  probability 5% the result is FATAL DOSE: the score goes to 0 with no player gesture. The brief says RADIATE is
  the only gesture with that cost.
- Severity: wrong behaviour (game-impacting). Confidence: verified-by-reading.
- Fix: add a CommandType (for example radiateScatter) that runs only the gift and scatter branches for story
  effects. Keep the 5% path for the GameBar button only.

### W4  Wrong behaviour: a radiation outcome can be lost
- File: Source/PluginEditor.cpp:804-809 (reads the counter, then the outcome in a separate call);
  Source/PluginProcessor.cpp:583-584 (store outcome, then fetch_add counter).
- Scenario: two radiate commands run in one audio block, or land between two editor ticks (a story radiate and a
  button press). The counter jumps by 2, but the editor reads only the last outcome once. The earlier outcome,
  including a FATAL reset, is never applied.
- Severity: wrong behaviour. Confidence: verified-by-reading (likelihood low).
- Fix: publish each outcome to a small AbstractFifo (or pack outcome and counter into one 64-bit atomic) and
  drain it in the editor.

### W5  Wrong behaviour (race): the colony seed is written from the host thread
- File: Source/PluginProcessor.cpp:1214 (setStateInformation calls colony.setSeed directly);
  Source/Engine/Colony.h:154 (`rng.seed(s)`), with rng used on the audio thread (Colony.cpp radiate and process).
- Scenario: the host calls setStateInformation while processBlock runs, which JUCE allows. The PRNG state is
  written from one thread while the audio thread advances it. That is a data race (UB), and the restored seed
  can be partly overwritten.
- Severity: wrong behaviour (UB). Confidence: plausible (depends on host call timing; the call site itself is verified).
- Fix: stage the seed as a command and apply it on the audio thread (hardReset already does this at
  PluginProcessor.cpp:552), instead of calling colony.setSeed from setStateInformation.

### W6  Wrong behaviour: state restore keeps stale history and skips version checks
- File: Source/PluginProcessor.cpp:1173 (writes version=1), 1193-1199 (no version read, hasType check only),
  1203-1206 (history restored only when an EVOLUTION child exists).
- Scenario: a state with no EVOLUTION child (older or hand-made) merges into the history tree of the session
  that is already open. A state from a newer version is accepted and partly applied, with no warning.
- Severity: wrong behaviour. Confidence: verified-by-reading.
- Fix: clear the history in the else branch, read `version`, and reject versions you do not know.

### W7  Wrong behaviour: EXPORT misses sounds collected by another open instance
- File: Source/Engine/SoundCollection.cpp:27 and 34-45 (rescan only in the constructor), 112 (own items only),
  116-124 (exportTo iterates `items`). The HUD JAR count is jar.size() (StoryPanel.cpp:762).
- Scenario: instance A opens. Instance B collects three sounds into the same folder. A runs EXPORT and copies
  only its own items. A's JAR count stays stale until restart.
- Severity: wrong behaviour. Confidence: verified-by-reading.
- Fix: have exportTo enumerate `collectionFolder().findChildFiles(findFiles, false, "*.wav")` instead of trusting
  `items`, and refresh the count from the same listing.

### W8  Wrong behaviour (UI hang): a huge factCounter replays on startup
- File: Source/Engine/MusicFacts.cpp:1125 (Deck constructor calls next() `counter` times), SoundCollection.cpp:148
  (factCounter read unbounded from JSON), StoryPanel.cpp (factDeck is built in the constructor, on the message thread).
- Scenario: a corrupt or hand-edited resonance-acts.json with factCounter in the millions makes the editor
  constructor loop for seconds to minutes. Normal play raises the counter by one per fact (about 12 per hour),
  so only a damaged file reaches this.
- Severity: wrong behaviour (hang). Confidence: verified-by-reading; needs corrupt input.
- Fix: clamp factCounter on load to a sane bound, or store the deck cursor so the replay is O(1).

### W9  Wrong behaviour (race): payload slots reused without acknowledgement
- File: Source/PluginProcessor.cpp:432-445 (stage functions write `genomePayloads` and `organismPayloads` at
  `payloadWrite.fetch_add(1) % kPayloads`, with no ack from the audio thread); applyCommand reads them by index
  (around 510-535).
- Scenario: more than 24 payloads are staged before the audio thread drains an earlier command. The slot
  is also consumed when pushCommand then fails. Or the message thread writes a slot while the audio thread
  copies it. An earlier restore, inject or breed then applies the wrong or a torn genome. No crash, since the
  index is bounded.
- Severity: wrong behaviour. Confidence: plausible (needs a burst of stagings).
- Fix: stage only when the ring has room (check cmdHead and cmdTail first), or size the pool to at least the ring
  length and free slots when the audio thread acknowledges them.

### W10  Wrong behaviour: dropped note-offs leave stuck notes
- File: 26 call sites discard the bool from pushCommand (PluginProcessor.cpp:409-418 returns false on a full ring).
  Notable: StoryPanel.cpp:124 (note releases in tick) and 661 (releaseAll).
- Scenario: a noteRelease is dropped on a full ring (512 slots), so the colony keeps the note sounding until a reset.
  Rare, but silent.
- Severity: wrong behaviour. Confidence: plausible (low likelihood).
- Fix: check the return for noteRelease and retry on the next tick, or send an all-notes-off command when the ring is full.

### W11  Wrong behaviour (inconsistent clocks): lab-game schedule counts overlay time
- File: Source/PluginEditor.cpp:787 (`playSeconds += dt`, unconditional) and LabGameFlow.cpp:242-250 (minute buckets),
  versus StoryPanel's paused-aware playClock.
- Scenario: the intro and name overlays (about 3 minutes at first launch) count toward lab-game minute buckets, so the
  first game can be offered right after naming. If that is not intended, the story and the games run on different clocks.
- Severity: wrong behaviour (low). Confidence: verified-by-reading; intent unclear.
- Fix: decide which clock is meant. If it is play time, advance playSeconds only when not paused.

### K1  Cosmetic: the sigil animation freezes after about 9.7 h
- File: Source/GUI/StoryPanel.cpp:720 (`sigilPhase += 1.0f / 30.0f`, float, never wrapped).
- Scenario: at 2^20 the float ulp reaches 0.125, so the 0.0333 step rounds to zero and the sigil stops moving.
  At 30 Hz that is about 9.7 h of editor uptime.
- Severity: cosmetic. Confidence: verified (arithmetic).
- Fix: make sigilPhase a double.

### K2  Cosmetic: two story lines play the wrong scale
- File: Source/Engine/Storyline.h:108 (the "Equal temperament" line uses Effect::scale with param 2, which selects
  modes()[9], Whole tone) and :126 (the Shepard tone line uses param 3, which selects modes()[0], Ionian).
- Scenario: the line says twelve equal steps and the colony sings a whole-tone scale. The Shepard line plays a plain
  major scale.
- Severity: cosmetic. Confidence: verified-by-reading.
- Fix: change the params to match the line, or reword the line.

### K3  Cosmetic: same-pitch notes in the tape-stop demo truncate each other
- File: Source/GUI/StoryPanel.cpp:370-375 (the timed-note helper pushes straight into noteQueue and skips the
  busy check in playNotes), 407-411 (kFxTapeStop: pitch `m` repeats for two steps).
- Scenario: the second note on the same pitch starts at 0.11 s, and the first note's note-off at 0.134 s releases the
  colony voice early, so the second note is cut from about 0.22 s to 0.13 s.
- Severity: cosmetic. Confidence: verified-by-reading.
- Fix: run the helper through the same busy check (or skip a pitch already queued), or give repeats a separate voice.

## Checked and not reported

- Radiate odds: Colony.cpp:2040 uses roll < 0.05 (fatal, 88% of cells), roll < 0.15 (gift) and the rest
  (scatter). That matches 5/10/85. The boons index `rng.intRange(0, 7)` is exclusive (Rng.h:63-67), so the array
  is never overrun.
- Lab rewards: no double payouts found. Instrument counters (gameSoundCount, and the per-type counters) are
  separate totals (ProgressionSystem.cpp:377, 414-430). A game win that also grants a skill calls grantFactReward
  twice (LabGameOverlay onWin plus awardGameSkill). Pending facts are capped at two (StoryPanel.h:60). That is
  designed, so it is not reported.
- Callbacks and lifetimes: StoryPanel's std::function members capture the editor, but the destructor never calls
  them. The editor's deferred calls use SafePointer (PluginEditor.cpp:100, 980). releaseAll() runs in ~StoryPanel
  and the processor outlives the editor. No use-after-free found.
- Music Facts index bounds: all `local / N` table indexes are bounded by family sizes (MusicFacts.cpp family
  table). The new curated and effects branches are bounds-checked: curatedText() returns null past kEmbedCount
  (MusicFactsCurated.inc:6, = 89) before kEmbedDemo[index] is read, and the effects range (29500-29999) is checked
  first. Deck permutation is a bijection (gcd check). SeenSet base64 round trip is sized correctly (3750 bytes).
- StoryFx: all 12 Anim values are handled (lifeFor and drawFx). Gradient stops stay in [0, 1] including the breathing
  corona. The timer stops when nothing is playing.
- Story note helper and playDemo: semis is checked non-empty before indexing, and playNotes and the helper clamp
  pitches to 12..108.
- Ring, state and buffers: Colony restore and serialisation clamp cellCount (Colony.cpp:1782 region,
  OrganismSerialization.cpp:34). MidiLearn.map bounds-checks the CC (MidiLearn.h:56). EngineCommand is default
  initialised (OrganismState.h:212-222). Colony processes in maxBlockSize chunks (Colony.cpp:1318). The dry scratch
  is resized and cleared every block, so the new morph never reads stale or undersized input. Mono output and
  mono/stereo input are handled in processBlock.
- Names: NameOverlay defaults an empty name to "Operator" (IntroOverlay.cpp:165-166).
- Knowledge stored as a JSON int64: values stay far below 2^53, and lexicon, secrets and factSeed are stored as
  strings, so there is no precision loss.
- Mic and history capture: pumpHistory and the captureOrganism publishing are single-producer and single-consumer,
  and the 2-buffer fullState and 3-buffer snapshot schemes are correct for their reader timings.

## Not reviewed

- LiveMorph.cpp/.h and Tests/LiveMorphTest.cpp (WIP, not in the listed scope; only the call sites in
  PluginProcessor.cpp were read).
- tools/*.py and the docs/data curated JSON (generation tooling, outside the runtime).
