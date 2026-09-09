#pragma once

#include <array>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Genome.h"
#include "Rng.h"

namespace mutagen
{
    enum class Species : int { grain = 0, spectral, resonator, count };
    inline constexpr int numSpecies = (int) Species::count;

    enum class LifeStage : int
    {
        germinating = 0, growing, mature, reproducing, dormant, dying, dead
    };

    // Infections are mutations that spread between compatible cells.
    enum class Infection : int
    {
        none = 0,
        metallize,     // pushes resonance / inharmonicity up
        reverse,       // flips playback direction over time
        vocalise,      // drags formants toward a vowel
        destabilise,   // injects pitch / time jitter
        count
    };

    struct SourceMaterial;  // fwd (SourceAnalyzer.h)

    /*  A single cell. One flat struct for the whole taxonomy: cheap to pool,
        copy, snapshot and serialise. Only a handful exist at once, and the
        per-sample work lives inside renderAdd(), so the species switch there
        costs nothing measurable.                                              */
    struct Cell
    {
        // ---- identity & genetics ----
        Species   species   = Species::grain;
        Genome    genome;
        int       familyId  = 0;      // inherited lineage id
        int       speciesGroupId = 0; // sub-population ("a species") id
        int       generation = 0;
        int       parentSlot = -1;
        int       voiceGroup = -1;    // -1 = persistent colony; >=0 = a MIDI note's organism
        uint64_t  birthTick  = 0;

        // ---- lifecycle ----
        LifeStage stage   = LifeStage::germinating;
        bool      alive   = false;
        bool      preserved = false;  // shielded from apoptosis / culling
        bool      muted   = false;    // user isolate/mute
        double    ageSec  = 0.0;
        double    lifeSec = 8.0;      // programmed death age (from genome + env)
        float     energy  = 0.15f;    // 0..1 available drive
        float     health  = 1.0f;
        float     stageGain = 0.0f;   // target amplitude for the current life stage
        float     extGain = 1.0f;     // external per-species emphasis (macros)
        float     ampSmoothed = 0.0f; // per-sample-smoothed output gain
        float     panSmoothed = 0.0f; // per-sample-smoothed pan
        float     fitness = 0.5f;     // last evaluated selection fitness

        // ---- infection ----
        Infection infection = Infection::none;
        float     infectionLoad = 0.0f;

        // ---- space / visualisation ----
        float x = 0.5f, y = 0.5f;     // position in the culture chamber (0..1)
        float vx = 0.0f, vy = 0.0f;
        float visualPulse = 0.0f;     // decays; spikes on divide / gene transfer
        int   linkTo = -1;            // symbiotic partner slot (-1 = none)

        // ---- DSP scratch (per species) ----
        double sampleRate = 44100.0;

        // grain
        float grainPhase[2] { 0.0f, 0.0f };
        float grainInc[2]   { 0.0f, 0.0f };
        double grainReadPos[2] { 0.0, 0.0 };
        double grainRate = 1.0;
        int   grainActive = 0;
        double sourceCursor = 0.0;

        // spectral (partial bank)
        static constexpr int maxPartials = 12;
        float partialPhase[maxPartials] {};
        float partialInc[maxPartials]   {};
        float partialAmp[maxPartials]   {};
        float noiseLpZ = 0.0f, noiseBpZ1 = 0.0f, noiseBpZ2 = 0.0f;
        uint32_t noiseState = 0x2545F491u;

        // resonator bank
        static constexpr int maxModes = 6;
        float modeY1[maxModes] {}, modeY2[maxModes] {};
        float modeF[maxModes] {}, modeFb[maxModes] {};
        float toneZ = 0.0f;   // shared one-pole "depth" filter

        // ---- API ----
        void prepare (double sr);
        void germinate (Species sp, const Genome& g, int family, int group,
                        int gen, Rng& rng);
        void updateLifecycle (double dt, float envStress, float nutrients,
                              float metabolism, float lifespanScale, Rng& rng);

        /** grain + spectral cells write here (the excitation bus). resonator
            cells read `exc` and add their resonated output to `out`.          */
        void renderAdd (juce::AudioBuffer<float>& out,
                        juce::AudioBuffer<float>& exc,
                        const SourceMaterial& src,
                        float envStress);

        float visualRadius() const;

        void kill() { alive = false; stage = LifeStage::dead; }
    };
}
