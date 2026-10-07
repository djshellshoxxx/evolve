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
