## MUTAGEN 0.3.0 beta 2: Music Facts

This is a **beta**. The facts are verified by automated tests and an independent checker, but the demonstrations have not yet been heard on a wide range of hosts. Please report anything odd.

**New**
- **Music Facts**: 30,000 educational facts about pitch, intervals, chords, scales, rhythm, tuning, acoustics and hearing. One appears every 5 minutes of play as a card over the dish, and a won lab game or an unlocked skill brings a bonus fact.
- **Demonstrations**: 1 in 5 facts is demonstrable. The colony plays the note, interval, chord, scale or circle-of-fifths steps as the text appears, and a demonstrable fact adds **+9,344** to your score. Other facts get a short chime in the run's key.
- **KNOWLEDGE score**: a second score under the main score. Every fact is worth 1 to 4 points.
- Facts never repeat before every fact of their kind has been shown, and your progress is saved.
- **Verification**: the computed facts were re-derived by an independent script (zero mismatches across about 30,000 facts). The 89 hand-written facts each cite two sources and were reviewed before being included (they are not in this build yet).

**Fixed**
- Windows build error in the facts code (a compiler-specific annotation).
- CI builds no longer run out of memory on Linux and macOS (build parallelism is capped).

**Downloads**
- Windows x64: standalone `.exe`, VST3, CLAP
- Linux x64: standalone, VST3, CLAP (`.tar.gz`)
- macOS universal: app, AU, VST3, CLAP (unsigned; right-click, then Open the first time)

---

## MUTAGEN 0.3.0 beta 1: Chance, Conversation and Coda

This is a **beta**. The new story, animations and events are covered by automated tests and render checks, but they have not yet been heard on a wide range of hosts. Please report anything odd.

**New**
- **Chance events**: 24 random happenings (common, uncommon, rare, mythic) such as a meteor shower, a golden spore, a total eclipse and the colony singing back. Each one changes the colony, plays a sound, scores a little and has its own animation. A pity counter makes rare events likelier over a long session.
- **Animations**: twelve effects over the culture chamber (comets, spore rain, aurora, eclipse, gold dust, lightning, static, bubbles, fireflies, falling sine waves, heartbeat, ripples), title banners for acts, twists, rare events and endings, a glitch flash for twists, and an animated sigil for each of the nine speakers.
- **Story**: Act IX, *Coda: The Listener*; four more lexicon facts (64 in all); four more twists (12 in all); fourteen two-voice conversations that end in something the colony plays; four endings chosen by how you played.
- Field Journal status line now tracks chance events and endings found.

**Carried from earlier**
- All open feature branches (deep progression, haunted lab, steering, steering intensity) were checked and are already part of this build.

**Downloads**
- Windows x64: standalone `.exe`, VST3, CLAP
- Linux x64: standalone, VST3, CLAP (`.tar.gz`)
- macOS universal: app, AU, VST3, CLAP (unsigned; right-click, then Open the first time)

---

## MUTAGEN 0.2.0: The Resonance Acts

**New**
- **The Resonance Acts**: eight new story acts and seven new characters, 60 lexicon events and 16 ear checks. They teach acoustics, sound design and music theory through the story; the colony performs each fact while it is spoken.
- **No two runs alike**: each run gets its own key and mode (12 × 10), twist (8), event order and pacing.
- **Sound collection + WAV export**: COLLECT records the colony, and EXPORT WAV saves the whole jar as 24-bit WAV files (DC-free, -1 dBFS peak, click-free fades).
- **Lab games are now playable**: roulette, three-card monte, slots, dice, twenty-one and scratch cards. Instrument rewards are recorded as real sounds.
- **CLAP** plugin format alongside VST3 (and AU on macOS).

**Fixed**
- Integrated the open lab-games and pinned-build branches. Fixed the five errors that stopped the merged code from building (a missing source file, stale phantom-window calls, an atomic passed to `jlimit`, a JUCE 8 display API change, an incomplete type).
- COLLECT, export and render now record the final output (previously the dry input whenever an input bus existed); the capture rings can no longer be freed mid-read when the host re-prepares
- Closing the editor no longer disarms your microphone or cancels your capture
- Trip delay: removed a ~1.5 kHz buzz and stale audio replay from earlier activations
- Lab-game orb prizes now actually appear in the chamber; aces are soft in twenty-one; piano-key prizes are saved
- Two open instances can no longer overwrite each other's collected WAVs or story progress
- Story demonstrations release their notes (new `noteRelease` command), so nothing is left droning.

**Downloads**
- Windows x64: standalone `.exe` (static runtime), VST3, CLAP
- Linux x64: standalone, VST3, CLAP
- macOS universal: app, AU, VST3, CLAP (unsigned; right-click → Open the first time)
