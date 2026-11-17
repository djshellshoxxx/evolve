# Codebase Map: where to find what

| I want to... | Look in |
|---|---|
| Colony, cells, genome, ecology | `Source/Engine/Colony.*`, `Cells.*`, `Genome.*` |
| Modulation | `Source/Engine/ModBank.h` |
| Entropy and randomness | `Source/Engine/Entropy.*`, `WorldSeed.h` |
| Novelty, MAP-Elites, stagnation | `Source/Engine/Novelty.h` |
| Descriptors (flatness, centroid, flux, roughness, appeal) | `Source/Engine/Descriptors.*` |
| Score, combo, events, high scores | `Source/Engine/ScoreSystem.*` |
| Sample ingest and mic guard | `Source/Engine/Ingest.*`, `AudioImport.h` |
| Post-colony rack | `Source/Engine/PostChain.*` |
| Looks and views | `Source/GUI/` (`MutagenLookAndFeel`, `CultureChamber`, `WaveField`, `ScoreHud`, `HelpView`, `OptionsView`) |
| Parameters and tooltips | `Source/Parameters.*`, `ParamHelp.cpp` |
| Standalone device setup | `Source/Standalone/DeviceSetup.cpp` |
| Tests | `Tests/` |
| Build and CI | `CMakeLists.txt`, `.github/workflows/build.yml` |
| Working log and plans | `PROGRESS.md`, `AGENTS.md`, `docs/` |
