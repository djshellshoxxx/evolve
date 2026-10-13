# Evolve / MUTAGEN remaining feature specifications
Status: implementation baseline, 2026-10-12. Parent: PR #16 at a00154ee. Authority: docs/audit/features-and-controls.md; docs/audit/STATUS.md; docs/audit/integration-followup-20261012.md. An item marked "pending" is not a confirmed regression: inspect its current implementation before editing.

## Architecture and change policy
Each feature is independent and must have a focused commit, unit or JUCE smoke regression where feasible, and a checked acceptance criterion. Preserve stable APVTS parameter IDs, preset formats and backward-compatible project-state loading. No disk I/O, allocations, locks, GUI calls, file dialogs, or blocking waits on the audio callback. GUI async callbacks capture Component::SafePointer and re-resolve processor lifetime. Audio-thread state transfers use bounded lock-free staging or a safe nonblocking protocol. Keep changes on feature/complete-audit-gaps-20261012 until review and successful Windows/Linux/macOS CI. PR #16 is the integration dependency; never overwrite other agents' branches.

## A. Host integration and persistent project state

### F-20 New World/Reset semantics
Contract: Distinguish Reset Sound/Environment from New Run and Factory Reset. The visible RESET behavior must be explicit. If RESET means full game reset, clear transient score and run ID, stop current timers, and create a new run; do not silently erase machine-wide achievements/journal without separate confirmation. Preserve other plugin instances. Specify in Help and add tests for after-reset state and persistence. Implement in PluginEditor.cpp / ScoreSystem with message-thread ownership. Acceptance: reset UI, score HUD, save file, and reopened host agree.

### F-21 DAW preset-browser support
Contract: expose deterministic factory preset count and names using AudioProcessor getNumPrograms/getCurrentProgram/setCurrentProgram/getProgramName; preserve index 0 as stable default. Resolve preset operations on host-safe thread; do not mutate APVTS from audio callback; program changes must not unexpectedly replace living colony if existing preset semantics modify only environment. User presets remain accessible from existing FILE menu rather than inventing unstable indices. Test bounds (-1, >=count), repeated selection, host project restore, all factory names, and selection under playback. Depends on PresetManager program accessors and processor methods.

### F-22 Host bypass
Contract: host bypass must be predictably silent for instrument-only output and pass through dry input for effect/hybrid roles, with matching channel mapping for mono/stereo. Support JUCE processBlockBypassed and where feasible host-exposed bypass parameter, ensuring state and automation compatibility. Prevent allocations in the callback and preserve queued note-off safety. If manual wet/dry blending is used, implement smoothing to prevent clicks. Test bypass while transport runs, automation, mixed layouts and host reopen. Implementation: PluginProcessor.h/.cpp and a regression test.

### F-23 Latency
Contract: explicitly document and report the true processing latency. Only report zero if validated by impulse/null testing at supported buffer and sample-rate combinations. Use setLatencySamples when the graph changes, never in processBlock. Test standalone and plugin-host alignment including effect/hybrid dry path.

### F-25 Standalone device persistence
Contract: identify whether JUCE AudioDeviceManager already persists device configuration. If not, serialize selected device, sample rate, buffer size and enabled MIDI inputs to existing AppOptions, with schema versioning and graceful fallback for disconnected devices. Plugin builds must not attempt to own host devices. Verify cold restart and missing-device fallback on Windows/macOS/Linux. Remove dead AppOptions accessors only after confirming no dependent callers.

### F-30 A/B persistence
Contract: store both A/B parameter snapshots, active slot and initialized flags in versioned host state. Restore only known IDs and clamp malformed normalized values. Loading old state must preserve default empty-slot semantics. Switching slots should generate correct host parameter gestures and avoid modifying the organism beyond established A/B contract. Smoke tests: save/reopen/reload, old state, malformed slot, repeated switching, multiple instances. Design: editor TopBar state must be mirrored in processor-owned serializable state; processor cannot dereference destroyed editor.

## B. Interaction and workflow

### F-13 Missing tooltips
Contract: NEW WORLD, SAVE RUN, LOAD RUN, SCORES, JOURNAL, CAPTURE 4s, PITCH, LFO, OSC must each have concise accurate hover explanations. Preserve existing enabled/disabled and tooltip toggle behavior. Simple patch in Source/GUI/GameBar.cpp. Acceptance: all nine return nonempty tooltip, original six tooltips remain intact; no control behavior changes.

### F-14 Live 'show values on hover'
Contract: options toggle affects every existing ParamSlider without reopening editor. Centralize editor-level traversal/observer and update constructed controls on message thread; support subsequent lazy-created views, plugin reopens and per-instance setting. Test toggle on/off, including FX rack and hidden overlays; no per-frame scanning.

### F-15 Esc overlay dismissal
Contract: topmost dismissible overlay closes on Esc in order: alert/menu owned by JUCE, modal encounter/help/options/scores/journal/lab/performance/FX/story where appropriate. Intro and destructive confirmation flows must retain their own semantics. Closing encounter must not grant reward or leak notes. Focus returns to originating control. Add common editor key handler / key listener with a single overlay resolver; prevent invoking gameplay shortcuts when modal visible. GUI smoke test each overlay and repeated Esc.

### F-16 Global shortcuts
Contract: editor-managed discoverable shortcuts with modifier safety, no collision with standard DAW shortcuts or text entry. Initial map: Ctrl/Cmd+Shift+R -> Random, Ctrl/Cmd+Shift+F -> FX, Ctrl/Cmd+Shift+J -> Journal, Ctrl/Cmd+Shift+H -> Help; RESET and RADIATE remain click-only due destructive/irreversible outcomes unless explicitly confirmed. Commands route through existing UI callbacks, never duplicate mutation logic. Disable during text input/any modal. Document keys in Help and tooltips; test focus, window reopen, and multiple plugin instances.

### F-17 MIDI learn cancellation
Contract: Escape cancels pending CC Learn before attempting overlay closure; no mapping changes or stuck status text. Existing right-click toggle behavior remains. Wire through editor dispatcher calling processor.midiLearn.cancelLearn() on message thread; test armed/unarmed, Esc and map persistence.

### F-26 Help coverage
Contract: document every visible action and its exact side effects: GERMINATE, Load Sample/Organism, Capture Live, Random Seed, timeline Restore/To Lab, Story COLLECT/EXPORT WAV, performance keyboard, A/B, game overlays, save/export options and new keyboard commands. Content must agree with implemented roles, timings and persistence. Snapshot or static test validates headings/names; manually inspect legibility at minimum editor size.

### F-31 Accessibility
Contract: provide accessible names, roles, state/value and keyboard operations for custom gesture knobs, XY pad, chamber and actionable buttons. Gesture knobs must offer equivalent keyboard trigger without claiming they are sustained parameters. Chamber should provide a separate keyboard-accessible inspect/mutate action with clear selection semantics. Hide decorative visuals from screen readers. Test tab order, focus indication, voice narration and standard OS accessibility at least on Windows.

### F-32 High-DPI and resize
Contract: support minimum 1060x700 and 125%/150%/200% desktop scales, with no clipped controls, overlapped overlays, invisible hit targets or blurry type. JUCE component bounds remain logical coordinates; avoid manual double scaling. Add Windows screenshot and automated component-bounds smoke checks, including dialog/menu alignment and multi-monitor movement.

## C. Polish, import and documentation

### F-24 Host device ownership
Contract: plugin audio/MIDI hardware routing is host-controlled. Keep standalone device selection; in plugin mode show explanatory text and optional MIDI channel filter only if spec requires it. Amend include.md and Help so promises accurately distinguish standalone from DAW host. Acceptance: no misleading device picker in VST3/AU/CLAP.

### F-27 Platform paths
Contract: use platform-accurate application-data locations for collected sounds and presets (macOS ~/Library/MUTAGEN for the pinned JUCE 8.0.6 runtime, Windows roaming app data, Linux platform-specific JUCE result); prefer userApplicationDataDirectory as canonical runtime source. Documentation must not promise hardcoded paths contrary to implementation. Validate README examples.

### F-28 M4A import filtering
Contract: only advertise/allow formats supported by registered JUCE audio readers in each build. Remove unsupported .m4a from drag filter unless a decoder exists. Unsupported file must show actionable failure rather than silently modifying colony; supported WAV/AIFF/FLAC/MP3 handling checked platform by platform. Tests: file filter, bad extension, corrupt supported file, stereo data.

### F-29 Dead tooltip catalog
Contract: remove misleading ParamHelp entries for nonexistent osc pulse-width and phase leaves, or add actual parameters only if intentionally specified elsewhere. Acceptance: every named tooltip ID maps to stable APVTS param or intentionally generated control; test through param enumeration.

### F-33 Init naming
Contract: display 'Factory Default' in the factory preset selector while preserving legacy stored 'Init' references as alias. Current projects must load unchanged; regression covers old preset selection and fresh init.

## D. Validation, performance and release
Profile Cells.cpp resonator/grain trig/modulo, PostChain.cpp filter allocations, audio-thread by-value copies, Colony.cpp quadratic fitness sharing, and GUI repaint traffic before changing implementations. Metrics: p50/p95/p99 block duration on 44.1/48/96kHz, buffers 64/128/512, low/high cell counts, CPU allocation count per block (target zero), glitch/xrun rate; GUI paint rates and latency. Guard audio quality with deterministic seed renders and numerical tolerance plus listening QA; optimize only after baseline profiling. Test corrupt states, note-off queue pressure, rapid reopen, two plugin instances and device unplug.

Definition of done per feature: implementation and test merged through review; all supported CI platforms build and ctest pass, no ASAN/TSAN findings in feasible isolated tests, documentation updated and old project states load. Windows standalone/VST3/CLAP should undergo manual DAW and listening QA before beta release. Never claim a feature complete solely because its source exists.

## Work sequence
Phase 0 compare audit F-13..F-33 against PR #16 head and label confirmed/pending; do not redo F-01..F-12/F-18/F-19 already resolved. Phase 1 F-13, F-27, F-28, F-29, F-33. Phase 2 F-14/15/16/17/20/26. Phase 3 F-21/22/23/25/30 with dedicated regression harness. Phase 4 F-31/32 and profiling. Phase 5 cross-platform CI, instrument/effect DAW QA and integration PR merge. Record deviations and remaining failures in docs/audit/STATUS.md rather than marking them resolved prematurely.
