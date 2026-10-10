# Audit status (2026-10-10)

Reports: `features-and-controls.md`, `bugs.md` (and the performance findings, delivered as a message, summarised below).

## Fixed
| ID | What |
|---|---|
| F-01 | RANDOM keeps Output, Dry/Wet, CPU, Role and MIDI routing |
| F-02 | The Source selector now drives GERMINATE (noise, impulse, tone, live input); loading a sample or organism switches the selector |
| F-03 | Open Run restores score and play time |
| F-07 | Output and Dry/Wet knobs, plus Morph and React for the effect mode |
| F-09 | Double-click resets a knob to its real default |
| F-10 | Shift (coarse) and Ctrl/Cmd (ultra-fine) knob drag |
| F-11 | No dead "Reset to Default" item on plain buttons |
| C1 | Corrupt history file can no longer crash the editor |
| D1 | A stale instance can no longer roll back fact progress |
| W1, W2 | Story timers hold while paused; an unanswered ear check gives up after 90 s |
| W3 | Story moments no longer roll the score-wipe radiation |
| W4 | Every radiation result is delivered (small ring, no lost results) |
| W5 | The colony seed is set on the audio thread (command), not from the host thread |
| W6 | State restore ignores states from newer versions and clears stale history |
| W7 | EXPORT copies every sound in the shared folder |
| W8 | A damaged fact counter is clamped (no startup hang) |
| W10 | Dropped note commands are retried (no stuck notes) |
| K1, K2, K3 | Sigil phase no longer freezes; two story lines play the right sound; no repeated pitch in the tape-stop demo |
| RT-1 | No string allocation per audio block in `buildPostParams` |

## Open
- F-04, F-05, F-06: export length choice, instrument-only gating, stereo output
- F-08: no control for Memory
- F-12: XY pad right-click menu
- F-18, F-19: live score in the DAW project state; duplicate high-score entries
- W9: payload slots reused without acknowledgement (needs a design decision)
- W11: lab-game clock counts overlay time (intent unclear)
- Performance (estimates from reading, not measured): per-sample `cos` and `fmod` in the resonator and grain paths (`Cells.cpp`), per-block EQ filter allocation (`PostChain.cpp`), large by-value state copies on the audio thread, 40 Hz full repaints in several GUI components, the O(N^2) fitness sharing in `Colony.cpp`. Profile before changing the DSP ones.
