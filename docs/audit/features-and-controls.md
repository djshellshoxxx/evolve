# MUTAGEN features and controls audit

Read-only audit of `/home/user/evolve` (version 0.3.0 per `CMakeLists.txt:3`).
Sources checked: `include.md`, `theme.md`, `README.md`, the requirement list in `PROGRESS.md` (items 1-31), `Source/Parameters.{h,cpp}`, `Source/ParamHelp.cpp`, `Source/GUI/*`, `Source/PluginEditor.*`, `Source/PluginProcessor.*`, `Source/Engine/PresetManager.*`, `Source/Engine/MidiLearn.h`, `Source/AppOptions.h`, `Source/Engine/ScoreSystem.*`, `Source/Engine/Colony.cpp`.
JUCE behaviour was checked against the vendored source at `/tmp/mbuild/_deps/juce-src`. Nothing was built or run.

Severity: high = contradicts a documented promise or a control does nothing; med = missing or partial feature a user will hit; low = polish, dead code, doc drift, or unverified.

## Summary

| Severity | Count |
|---|---|
| High | 4 |
| Medium | 12 |
| Low | 17 |
| **Total** | **33** |

Line numbers are as read on this checkout.

---

## 1. Findings

| ID | Area | Finding | Evidence file:line | Severity | Suggested fix | Effort |
|---|---|---|---|---|---|---|
| F-01 | Random | Every press after the first calls `resetToDefaults()`, which resets **all** parameters. That includes Output (`masterGain`), Dry/Wet, CPU Quality, Role, and the MIDI routing params. The code comment, the Help text and the README all say these are "left alone". Each second press silently overwrites the user's rig. | `Source/Engine/PresetManager.cpp:333-339` (randomise calls `resetToDefaults`), `:241-250` (no exclusion), `:320-331` (`isRandomisable` excludes them). Contradicted by `PresetManager.h:80-82`, `Source/GUI/HelpView.cpp:361-362`, `README.md:127-129` | High | On the reset step inside `randomise()`, reset only params for which `isRandomisable()` is true. Leave the excluded set untouched. | S |
| F-02 | Germination | The **Source** selector (`sourceMode`) is inert. `germinate()` and `captureLive()` never read it, and the processor only ever loads `primitiveTone` (at reset). Presets and RANDOM that set it have no audible effect. The tooltip claims it chooses what the colony feeds on. | `Source/GUI/GerminationPanel.cpp:21-26` (control), `:72-77` (`germinate`), `:120-128` (`captureLive`). Only engine use: `Source/PluginProcessor.cpp:373`. Claim at `Source/ParamHelp.cpp:77-79` | High | Dispatch on `sourceMode` in the germinate path (sample, live input, primitives, preserved organism). Or remove the control and param and keep the rest of the path. | M |
| F-03 | Run files | **Open Run** restores only the plugin-state blob. The score, seconds, peakVariety, discoveries and generation are written by Save Run but never read back. README and Help both promise that a run carries "the score and the run statistics". | Write: `Source/PluginEditor.cpp:706-711`. Read: `:752-762` (only `"state"` is used). Promise: `README.md:132-133`, `Source/GUI/HelpView.cpp:349-352` | High | In `loadRun`, read those properties and add a `ScoreSystem::restore(...)`. Reset `peakVar`, `runSeconds` and discoveries from the file. | M |
| F-04 | Export | Help says the user is asked for a length. The code always renders a fixed 12 s with no prompt, no cancel, and only a text status ("rendering 12s ..."). | `Source/GUI/HelpView.cpp:353-355` (promise). `Source/PluginEditor.cpp:475-501` (`job.seconds = 12.0f`, no length UI). `RenderEngine.cpp` clamps to 30 s | Med | Add a length choice (for example a small popup before the save dialog, using the 0.5-30 s range that `RenderEngine` already allows). Or correct the Help. | S |
| F-05 | Export | Export is not limited to instruments. include.md says "This function is only for instruments and not for effects". The default role is Hybrid (`Parameters.cpp:125`), so Export is offered out of the box. | `Source/PluginEditor.cpp:475-505` (no role check). Default: `Source/Parameters.cpp:125` (`pid (pluginRole), "Role", ..., 2`) | Med | Disable or hide Export when role is not Instrument, or add a visible "special function" label. Consider defaulting Role to Instrument. | S |
| F-06 | Export | The exported WAV is mono duplicated to L and R. The output ring is one channel and records only channel 0, so the stereo spread, osc pan and stereo FX are lost from exports. | `Source/PluginProcessor.cpp:97` (`outputRing.setSize (1, ...)`), `:937` (records channel 0 only), `:1098-1101` (`readRing(..., 1)`). `Source/Engine/RenderEngine.cpp:37-44` (copies mono to both channels) | Med | Make `outputRing` two channels, copy both in `copyRecentOutput`, and drop the L=R duplication in `RenderEngine`. | M |
| F-07 | Controls | **Output** (`masterGain`) and **Dry/Wet** (`dryWet`) are real parameters with tooltips and they act on the audio, but no on-screen control exists. Effect and Hybrid roles therefore cannot be blended from the UI. Nothing documents them as deliberately hidden. | `Source/Parameters.cpp:128-129`. No GUI reference (grep across `Source/GUI` and `PluginEditor.cpp`). Tooltips: `Source/ParamHelp.cpp:91-92`. Use: `Source/PluginProcessor.cpp:329`, `:916-921` | High | Add an Output fader and a Dry/Wet knob (header, or the FX rack). Or document them as hidden host-automation params. | S |
| F-08 | Controls | **Memory** (colony inheritance across notes) has no on-screen control and no Help entry. It is used by the engine, so it is a hidden feature. | `Source/Parameters.cpp:123`. Tooltip only: `Source/ParamHelp.cpp:84`. Used: `Source/PluginProcessor.cpp:167`, `Source/Engine/Colony.cpp:1552`, `:1761`. No GUI or Help reference | Med | Add a knob to the Environment panel, or add a Help entry and state it is host-only. | S |
| F-09 | Knob reset | Double-click on a knob sets the value to **0.0**, not to the parameter default. For knobs where 0 is outside the range (Capture Length 0.1-12 s, Gator Length, and similar) double-click does nothing. Help says double-click "Resets a knob to its default". | `Source/GUI/Widgets.cpp:19` (`setDoubleClickReturnValue (true, 0.0)`). JUCE behaviour: `juce_Slider.cpp:1110-1123` (only when 0 is in range, sets the raw return value). Claim: `Source/GUI/HelpView.cpp:378` | Med | Override `mouseDoubleClick` in `ParamSlider` to reset to `getDefaultValue()` through a begin/end gesture. | S |
| F-10 | Knob drag | Help promises "Hold Shift for coarse, Ctrl (Cmd on macOS) for ultra-fine". Nothing implements this. `ParamSlider::mouseDrag` is a pass-through with a comment saying "the ratios are ours". JUCE rotary drag has no such modifiers, and velocity mode is off. | Claim: `Source/GUI/HelpView.cpp:379-380`. Stub: `Source/GUI/ParamControl.h:63-66`. Velocity mode off: `Source/GUI/Widgets.cpp:20` | Med | Implement the scaling in `ParamSlider::mouseDrag` from `e.mods`, or correct the Help text. | S |
| F-11 | Right-click | Right-click on any **non-parameter** `ParamButton`/`ParamCombo` opens a menu with a "Reset to Default" item. Choosing it does nothing, because `param == nullptr` returns early. This affects RANDOM, RESET, FILE, A, B, Clone, Freeze, Export, Perform, FX, Inspector, the preset combo, the options gear and "Clear All MIDI". | `Source/GUI/ParamControl.cpp:85-87` (idReset always added), `:245` (early return). Affected: `Source/GUI/TopBar.h:101-127`, `:102` (`presetBox`), `Source/GUI/OptionsView.h:44-47` | Med | For untagged components, omit the menu, or show only items that can act. | S |
| F-12 | Right-click | **XY pad**: right-click moves the pad (no popup check), and the hidden sliders behind it are not tagged. `xyStability` and `xyRepro` therefore have no Set Value, Reset or MIDI map from the pad. | `Source/GUI/PerformanceView.cpp:50` (`mouseDown` always drags), `:92-102` (sliders hidden, untagged) | Med | Check `e.mods.isPopupMenu()` in `XYPad` and open `paramMenu` on the tagged hidden sliders. | S |
| F-13 | Tooltips | Missing hover tooltips on **NEW WORLD, SAVE RUN, LOAD RUN, SCORES, JOURNAL, CAPTURE 4s**, and on the **PITCH / LFO / OSC** gesture knobs. include.md requires tooltips on the controls. | `Source/GUI/GameBar.cpp:131-137` (`setup` sets no tooltip), `:150-156` (gesture knobs), `:179` (micCapture). Present for enzyme/catalyst/heat/water/radiate: `:139-147` | Med | Add `setTooltip` calls. | S |
| F-14 | Options | "Show values on hover" is read only when a knob is constructed. Toggling it does not change the open editor. | Toggle: `Source/GUI/OptionsView.cpp:24-28`. Read once: `Source/GUI/Widgets.cpp:23` | Low | Apply `setPopupDisplayEnabled` to existing knobs from an options callback, as tooltips already do. | S |
| F-15 | Keyboard | **Esc does not close** Help, Options, Scores, Journal, Breeding Lab, Performance, Lab Game, FX or the Story overlays. Only `IntroOverlay` handles keys. Closing these overlays is by mouse only. | Only handler: `Source/GUI/IntroOverlay.cpp:84-88`. Close buttons only: `Source/GUI/HelpView.h:59`, `Source/GUI/OptionsView.h:53`, `Source/GUI/PerformanceView.h:36`, `Source/GUI/ProgressionView.h:25` | Med | Add a shared overlay base that maps Esc to close. | S |
| F-16 | Keyboard | **No keyboard shortcuts** at all (RANDOM, RESET, RADIATE, FILE, FX and others). The build asks for keyboard focus (`EDITOR_WANTS_KEYBOARD_FOCUS TRUE`) but the editor has no key handling. | `CMakeLists.txt:63`. KeyPress use in source: `Source/GUI/ParamControl.cpp:48-49` (alert only), `Source/GUI/IntroOverlay.cpp:84-88` | Low | Add editor-level `keyPressed` for a few verbs and name the keys in the tooltips. | M |
| F-17 | MIDI learn | Once armed, MIDI learn can only be cancelled by reopening the menu and choosing the item again. Esc does nothing. The status line keeps prompting "move a MIDI controller" until then. | Cancel paths: `Source/GUI/ParamControl.cpp:199`, `:258` (menu only). Prompt: `Source/GUI/TopBar.cpp` (timer, "move a MIDI controller to map it..."). `Source/Engine/MidiLearn.h:43` | Low | Cancel on Esc, or on any click outside the menu. | S |
| F-18 | Persistence | The live **score, run statistics and progression** are not in the DAW project state. `ScoreSystem` and `ProgressionSystem` are editor members and reload from machine-wide files. Reopening a project therefore starts the live score at 0, and the run's score is only kept if the user runs Save Run. include.md asks for save/restore of game state. | State contents: `Source/PluginProcessor.cpp:1115-1135` (apvts, organism, seed, name, history, MIDI only). Members: `Source/PluginEditor.h:114`, `:118`. Global files: `Source/Engine/ScoreSystem.cpp:318-323`, `Source/Engine/ProgressionSystem.cpp:50` | Med | Store the live score block in `getStateInformation` and restore it in `setStateInformation`. Or document it as per-session. | M |
| F-19 | High scores | Every Save Run files a new board entry, with no de-duplication for the same run. The entry's name field receives the **world name**, not a player name. | `Source/PluginEditor.cpp:718`. `Source/Engine/ScoreSystem.cpp:377-395` (no dedupe), `:380` (`h.name = playerName`) | Low | Dedupe by a run id, and take the name from a user label. | S |
| F-20 | Reset | Reset keeps the running score and the progression record. Help says "Everything back to factory". | Claim: `Source/GUI/HelpView.cpp:363-366`. Reset path: `Source/PluginEditor.cpp:207-214`, `Source/PluginProcessor.cpp:357-378` (no score reset) | Low | Document that the score survives, or reset the run score (keep the journal). | S |
| F-21 | Host | The host program list is a single program named "Colony". The 20-entry factory bank and user presets are not visible to DAW preset browsers. | `Source/PluginProcessor.h:57-60` | Low | Expose `PresetManager` through `getNumPrograms`, `setCurrentProgram` and `getProgramName`. | M |
| F-22 | Host | No bypass handling. There is no bypass parameter and `processBlockBypassed` is not overridden. On Effect and Hybrid roles, host bypass still runs the full colony. | `Source/PluginProcessor.h:43-60` (no override, no bypass param) | Low | Override `processBlockBypassed` to pass through, or fade the colony out. | S |
| F-23 | Host | Latency is not reported (`getLatencySamples` is not overridden). A grep found no lookahead buffer, so zero is probably right, but this is unconfirmed. | `Source/PluginProcessor.h` (absence) | Low | Set latency explicitly, and measure with a null test. | S |
| F-24 | Options | MIDI and audio-device choice exists only in the standalone. Plugin builds show a text note. README documents this, but include.md asks the options page to let the user choose the MIDI and audio card. | Note path: `Source/GUI/OptionsView.cpp` (`deviceNote` when `deviceSelector == nullptr`). Standalone only: `Source/Standalone/DeviceSetup.cpp:15-29`. Doc: `README.md:141-143` | Low | Accept as a deliberate deviation and record it in include.md, or add a host-side MIDI-in filter. | S |
| F-25 | Options | `AppOptions::audioDeviceState()` and `setAudioDeviceState()` are never called. This is dead code. Standalone device persistence therefore relies on JUCE's own store, which was not verified. | `Source/AppOptions.h:33-37`. No callers (grep across `Source`) | Low | Delete the functions, or wire them to the standalone device state. | S |
| F-26 | Help | Help has no entries for several visible controls: the Germination buttons (GERMINATE, Load Sample, Load Organism, Capture Live, Random Seed), the timeline Restore and To Lab buttons, the Story panel COLLECT and EXPORT WAV, and the performance keyboard. include.md requires the help to explain every control. | Missing from `Source/GUI/HelpView.cpp` ("Germination" `:230-248`, "The Action Bar" `:249-284`). Controls: `Source/GUI/GerminationPanel.h:31-35`, `Source/GUI/EvolutionTimeline.h:40-43`, `Source/GUI/StoryPanel.h:143`, `Source/GUI/PerformanceView.cpp:71` | Med | Add Help entries for each. | S |
| F-27 | Docs | Path drift. README says Collected Sounds on macOS is `~/Library/MUTAGEN/Collected Sounds`. The code uses `userApplicationDataDirectory`, which on macOS is `~/Library/Application Support/MUTAGEN/`. Presets are documented only as `%APPDATA%`. | `README.md:122`, `:266`. Code: `Source/Engine/SoundCollection.cpp:13-16`, `Source/Engine/PresetManager.cpp:191-194` | Low | Correct the README paths for macOS. | S |
| F-28 | Import | The drop filter accepts `.m4a`, but `digestFile` uses `registerBasicFormats()`, which has no AAC reader. The user then gets "COULD NOT READ THAT FILE". Help lists WAV, AIFF, FLAC and MP3 only. Not run here. | Accept: `Source/GUI/CultureChamber.cpp:736`. Decode: `Source/PluginProcessor.cpp:1047-1048`, `:1180-1181` | Low | Remove `.m4a` from the accept list. | S |
| F-29 | Tooltips | Dead tooltip entries. "Pulse width" (`osc*_pw`) and oscillator `phase` describe parameters that do not exist in the layout. | `Source/ParamHelp.cpp:189-190` (no matching param in `Source/Parameters.cpp`) | Low | Delete the entries. | S |
| F-30 | A/B | A/B slots live only in memory in `TopBar`. They are not in project state or presets, so they are lost on reopen. README describes them as "two whole parameter states". | `Source/GUI/TopBar.cpp` (`toggleAB`, `copyAcross`), `Source/GUI/TopBar.h` (slot members) | Low | Persist the two slots in plugin state, or state that they are session-only. | S |
| F-31 | Accessibility | No accessibility handlers or accessible names were found (grep for `accessib` is empty). The XY pad, gesture knobs, wave field and chamber are invisible to screen readers and to keyboard users. | Grep: no `AccessibilityHandler`. Examples: `Source/GUI/GameBar.h:19-46`, `Source/GUI/PerformanceView.cpp:12` | Low | Add accessibility handlers and names for knobs and pads. | M |
| F-32 | Window | Resize limits are 1060x700 to 2600x1700. No explicit HiDPI or scale handling was found (grep for `getScaleFactor` and `setScaleFactor` is empty). Not tested at runtime. | `Source/PluginEditor.cpp:233-235` | Low | Test at 150% and 200% scaling and at the 1060x700 minimum. | S |
| F-33 | Presets | The default preset is named "Init", which is not descriptive. The other 19 names are descriptive. | `Source/Engine/PresetManager.cpp:22` | Low | Rename to something like "Factory Default" (keep "Init" as an alias if needed). | S |

---

## 2. Checklist against the eight audit areas

### (1) Parameter coverage (93 const-named params plus generated osc/lfo/gator leaves)

| Check | Result |
|---|---|
| Parameter has a visible control | **All except** `memory` (F-08), `masterGain` (F-07), `dryWet` (F-07). Osc, LFO, gator and glitch leaves are all reached from `FxRackView.cpp`. |
| Tooltip in `ParamHelp.cpp` | **All** parameters have one (table or leaf fallback). Dead leaves `pw` and osc `phase` (F-29). |
| Right-click (MIDI map, reset, set value) | Covered by `ParamSlider`, `ParamCombo`, `ParamToggle` and tagged `ParamButton`. **Missing** on the XY pad sliders (`xyStability`, `xyRepro`, F-12), and `memory`, `masterGain` and `dryWet` have no control at all. |
| Included in Reset | **Yes**, all parameters (`PresetManager.cpp:241-250`, `PluginProcessor.cpp:357-363`). |
| Included in Random | Yes, except the 13 excluded in `PresetManager.cpp:324-327`. The excluded set is **also reset** on press 2+ (F-01). |
| Double-click reset | Wrong value (0.0, F-09). |

### (2) Help and version

- Version: `project(MUTAGEN VERSION 0.3.0)` at `CMakeLists.txt:3`. Footer uses `JucePlugin_VersionString` (`Source/GUI/HelpView.cpp:583`, `Source/GUI/OptionsView.cpp:141`). **Matches.** The beta suffix from PROGRESS ("0.3.0 beta 1") is not shown (low, not logged).
- Help coverage: good for the environment, selection targets, FX rack, presets, and the game. **Gaps:** F-26 (buttons), F-08 and F-07 (no control), plus wrong claims in F-04, F-09, F-10 and F-03. Help also says Random leaves output and role alone (F-01).

### (3) Presets

- 20 factory entries (`PresetManager.cpp:22-176`), in six categories, which matches README ("twenty-odd ... six categories").
- Names are descriptive except "Init" (F-33).
- Save, Save As, Open and "Show Preset Folder" are in the FILE menu (`TopBar.cpp`, `showFileMenu`). A dirty flag is tracked (`PresetManager.h:63`). The preset selector is in the header.
- Preset files are `.mutagenpreset` XML (`PresetManager.cpp:268-300`).

### (4) Options

- Tooltips toggle: works and persists (`AppOptions.h:26-27`, `PluginEditor.cpp:523`, `:541`). **Live**.
- Show values on hover: persists but is not live (F-14).
- MIDI: "Clear all" works (`OptionsView.cpp:37-41`). MIDI-map list persists in state (`PluginProcessor.cpp:1132`).
- Audio and MIDI device choice: standalone only (F-24). Stored settings unused (F-25).
- Options page reached via the gear and via FILE > Options (`TopBar.cpp`, `showFileMenu`).

### (5) Export audio

- Present (FILE > Export Audio, and the header "Export" button). Writes 24-bit WAV at host rate (`RenderEngine.cpp:50-53`).
- Problems: fixed 12 s (F-04), not role-gated (F-05), mono (F-06).

### (6) Keyboard, accessibility, resize

- Esc: only the intro overlay (F-15). No shortcuts (F-16). MIDI learn cannot be cancelled by key (F-17).
- Resize: enabled with limits (`PluginEditor.cpp:233-235`). HiDPI: not verified (F-32).
- Focus order and screen-reader support: not verified; no accessibility code (F-31).

### (7) Host integration

- Bus layouts: mono or stereo in and out (`PluginProcessor.cpp:51-60`). OK.
- Sample-rate change: `prepareToPlay` re-prepares the colony, post chain and ring buffers (`PluginProcessor.cpp:63-100`). OK.
- Automation: all APVTS parameters are automatable with stable IDs (`Parameters.cpp`). Gesture buttons (enzyme and similar) are not parameters, which is by design (req 25).
- State: colony, history, MIDI map and parameters are saved (`PluginProcessor.cpp:1115-1135`). Score, run stats and progression are not (F-18). A/B is not (F-30).
- Program list, bypass, latency: F-21, F-22, F-23.

### (8) Documented but not found

- Help: "Choose the length when asked" (F-04). "Hold Shift / Ctrl for coarse and fine" (F-10). "Double-click resets to default" (F-09). "Output level, dry/wet ... left alone" (F-01).
- README: "Save / Open Run ... the score and the run statistics" (F-03). Macros, XY pad, EQ, gator, glitch, A/B: all present.
- README: Collected Sounds macOS path (F-27).

---

## 3. Requirement status (PROGRESS.md list, items 1-31)

| Req | Summary | Status | Evidence |
|---|---|---|---|
| 1 | Every run sounds different | Implemented (structure) | `Source/Engine/WorldSeed.h`, `Source/PluginProcessor.cpp:30-34` (per-instance entropy seed). Sound outcome not measured. |
| 2 | Each individual unique | Implemented (structure) | Per-island genomes `Source/Engine/Colony.cpp:284-287` |
| 3 | More variance | Implemented (structure) | `Source/Engine/WorldSeed.h`, `Source/Engine/ModBank.h` |
| 4 | Sounds wobble | Implemented | `Source/Engine/ModBank.h` (six octave-spaced LFO lanes per cell) |
| 5 | LFOs from fast to one cycle per minute or slower | Implemented | `Source/Engine/ModBank.h` lane 0 about 0.003 Hz (one cycle per ~5 min) |
| 6 | Always evolving, never a static drone | Implemented | Boredom drive `Source/Engine/Colony.cpp:723`, `:1237` |
| 7 | Never pure noise | Implemented | Anti-noise guard `Source/Engine/Colony.cpp:652` (`noiseGuard`), measured by `Source/Engine/Descriptors.h` |
| 8 | Gravitate to appealing sounds | Implemented (structure) | Appeal term in fitness, `Source/Engine/Colony.cpp` |
| 9 | More GUI control | Implemented | Gesture knobs, macros, XY pad, FX rack (`Source/GUI/GameBar.cpp`, `PerformanceView.cpp`, `FxRackView.cpp`) |
| 10 | Left click changes visuals and sound | Implemented | `Source/GUI/CultureChamber.cpp` (mouse handling near `:618`) |
| 11 | Drag creates waves that mass-mutate | Implemented | `Source/GUI/WaveField.*`, `CultureChamber.cpp` |
| 12 | Right click is subtractive damage | Implemented | `CultureChamber.cpp:618`, `Source/Engine/Colony.cpp` |
| 13 | Drag and drop samples are eaten | Implemented | `Source/GUI/CultureChamber.cpp:730-770`, `Source/PluginEditor.cpp:660-677` (m4a issue F-28) |
| 14 | Mic input with feedback protection | Implemented | `Source/Engine/Ingest.h:68-77` (howl detector), `PluginProcessor.cpp:692`, `:933` |
| 15 | Radio or other entropy mixed with PRNG | Implemented, with caveat | Audio-input LSBs (`Source/Engine/Entropy.h:21`). No radio-specific source. |
| 16 | Score per sound, shown | Implemented | `Source/GUI/ScoreHud.cpp`, `Source/Engine/ScoreSystem.*` |
| 17 | More variance means faster score | Implemented | `Source/Engine/ScoreSystem.*` |
| 18 | Noise stops the score | Implemented | `ScoreSystem` freeze, `ScoreHud` reason text |
| 19 | Clicking removes elements restarts score | Implemented | Right-click strip, combo doubling (Help `HelpView.cpp:99-116`, `Source/Engine/ScoreSystem.*`) |
| 20 | Save sounds, score and game state; high-score table | **Partial** | Run save works, Open Run drops score (F-03). High-score table is machine-wide, and state is not in the DAW project (F-18). Duplicate entries (F-19). |
| 21 | Other fun features | Implemented | Story, chance events, lab games, journal (`Source/Engine/Storyline.h`, `ChanceEvents.h`, `ProgressionSystem.*`) |
| 22 | Very interactive and colourful | Implemented | `CultureChamber.cpp` |
| 23 | Noise is greyer, variety is colourful | Implemented | Flatness-driven colour, `Source/Engine/Descriptors.h` |
| 24 | Fractal ghost reward, brief and rare | Implemented | `Source/Engine/ScoreSystem.cpp:163-168` (7 s good, 30-70 s cooldown), `CultureChamber.cpp:871-872` |
| 25 | PITCH / LFO / OSC gesture knobs | Implemented | `Source/GUI/GameBar.cpp:94`, `:154-156`. No tooltips (F-13). |
| 26 | ADD ENZYME | Implemented | `Source/Engine/Colony.cpp:1873-1946`, `GameBar.cpp:139-146` |
| 27 | ADD CATALYST | Implemented | `Source/Engine/Colony.cpp:1948-1984` |
| 28 | Enzyme and catalyst re-roll every press | Implemented | `Source/Engine/Colony.cpp:1859-1870` (comment on the roll) |
| 29 | RADIATE 5 / 10 / 85 with red flash and Geiger click | Implemented | Odds `Source/Engine/Colony.cpp:2051-2068`, Geiger `:2043-2047`, red flash `Source/GUI/CultureChamber.cpp:61-64` |
| 30 | ADD HEAT | Implemented | `Source/Engine/Colony.cpp:1986+`, wiring `Source/PluginEditor.cpp:584-592` |
| 31 | ADD WATER | Implemented | Same routine with `c.fa = -1`, `Source/PluginEditor.cpp:593-600` |

Status totals: 30 implemented (some structure-only, outcome not auditioned), 1 partial (req 20), 0 missing.

## 4. include.md checklist

| Item | Status | Notes |
|---|---|---|
| Help file with version | Implemented, partial coverage | F-26, F-07, F-08, plus wrong claims F-01, F-03, F-04, F-09, F-10 |
| Custom icon | Implemented | `CMakeLists.txt` `ICON_BIG` (`Resources/icon.png`) |
| Preset bank with descriptive names | Implemented | 20 entries, "Init" weak (F-33) |
| Reset button | Implemented | Resets all params, history, colony, MIDI (`PluginProcessor.cpp:357-378`). Keeps score (F-20). |
| Save, Save As, Open, Options in dropdown | Implemented | FILE menu, `TopBar.cpp` |
| Options page: tooltips, MIDI, audio card | Partial | F-14, F-24, F-25 |
| Export audio (instruments only) | Partial | F-04, F-05, F-06 |
| Right-click: MIDI map, reset, set value | Partial | F-11, F-12, F-08 |
| Hover tooltips | Mostly implemented | F-13 |
| Random (first press randomises, later press resets first) | Implemented, but wrong side effects | F-01, F-02 |

---

## 5. Eight most valuable fixes

1. **F-01** Keep the user's rig through RANDOM (only reset randomisable params). Small change, removes a silent data loss.
2. **F-03** Make Open Run restore score and statistics. Fulfils req 20 and the README promise.
3. **F-02** Wire the Source selector into germination, or remove it. A visible control that does nothing undermines trust in the rest.
4. **F-07** Add Output and Dry/Wet controls (Effect and Hybrid roles currently cannot be blended in the UI).
5. **F-09 + F-10** Knob interaction: double-click resets to the real default, and the documented Shift and Ctrl drag modifiers are implemented.
6. **F-04 + F-05 + F-06** Export: length choice, instrument-only gating, and stereo output.
7. **F-11 + F-12** Right-click contract: remove the dead "Reset to Default" entries on untagged buttons, and give the XY pad the same menu as the knobs.
8. **F-18** Store the live score and run statistics in the DAW project state, so a reopened project keeps its score.

Lower-effort follow-ups that fit alongside these: F-15 (Esc closes overlays), F-13 (missing tooltips), F-26 (Help entries), F-08 (Memory control).

## 6. Method notes and limits

- Every finding above was checked by reading the code. JUCE behaviour (F-09, F-10) was confirmed in the vendored JUCE source.
- Not verified at runtime: HiDPI (F-32), `.m4a` decode failure (F-28), host bypass and latency behaviour (F-22, F-23), standalone device persistence (F-25), and the sound quality claims for requirements 1-8.
- Nothing was built, run or committed. Only this file was written, under `docs/audit/`.
