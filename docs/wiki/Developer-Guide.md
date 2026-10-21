# Developer Guide

## Build

```bash
cmake -B build                      # fetches JUCE 8.0.6 on first run
cmake --build build --config Release --target MUTAGEN_Standalone
```

The VST3 is copied to the system plugin folder. A CLAP build is produced unless `-DMUTAGEN_BUILD_CLAP=OFF`.
The README notes that a fully parallel build can fail on Windows with `CL.exe exited with code -1`; `-j 1` completes.

## Tests (from CMakeLists.txt and the README)

- `MutagenGuiSmokeTest` (ctest): offscreen GUI smoke test for the score HUD.
- `MutagenDivergenceTest`: six colonies run offline for 45 s; the test measures divergence and calibrates the analyser.
- Other test sources in `Tests/`: DreamRooms, HostFeatures, LiveMorph, MusicFacts, Phantom, ProcessorSmoke, Progression,
  SoundCollection, TunerDuel.

These were not run in this documentation pass (the JUCE fetch and build were not performed).

## Notes

- `03.wav` (about 3 MB) sits in the repository root. Consider moving it under `Tests/` or `Resources/`, or
  removing it if it is not needed.
- `AGENTS.md` and `PROGRESS.md` hold the working rules and log. Read them before changing code.
