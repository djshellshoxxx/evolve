#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Cells.h"
#include "Genome.h"
#include "Environment.h"
#include "SourceAnalyzer.h"
#include "OrganismState.h"
#include "Rng.h"

namespace mutagen
{
    /*  The colony: a fixed pool of cells plus the ecological rules that decide,
        every tick, who is born, who dies, who cooperates and who changes.

        Real-time contract: prepare() allocates the pool once. Everything after
        that reuses slots - no allocation, no locks on the audio thread. The
        one exception is source material, which is swapped in via an atomic
        pointer handed over from the message thread.                            */
    class Colony
    {
    public:
        Colony();

        void prepare (double sampleRate, int maxBlock, int maxCells);
        void reset();

        // ---- seeding -------------------------------------------------------
        /** Take ownership of new source material (called on audio thread with a
            pointer built on the message thread). Returns the retired one. */
        SourceMaterial* adoptSource (SourceMaterial* incoming);
        const SourceMaterial& source() const { return *src; }

        void germinateFromSource (float initialPop, float dGrain, float dSpec, float dRes);
        void reseedRandom (float initialPop);
        void clearAll();

        // ---- per-block audio --------------------------------------------
        void setEnvironment (const Environment& e) { env = e; }
        Environment& environment() { return env; }

        /** Per-species output emphasis from the performance macros (Body / Voice
            weight the resonator / spectral populations; 1.0 = neutral).        */
        void setSpeciesEmphasis (float grain, float spectral, float resonator)
        {
            speciesGain[0] = grain; speciesGain[1] = spectral; speciesGain[2] = resonator;
        }
        void setMasterGain (float g) { masterGain = g; }

        /** Soft population ceiling (CPU quality). Never reallocates. */
        void setActiveCap (int n) { maxCellCount = juce::jlimit (8, (int) cells.size(), n); }

        /** Adds the colony's output into `out` (already sized, may contain dry
            signal for effect mode). `liveIn` is the current input block or null. */
        void process (juce::AudioBuffer<float>& out,
                      const juce::AudioBuffer<float>* liveIn);

        // ---- MIDI-driven temporary organisms --------------------------
        void noteOn (int midiNote, float velocity);
        void noteOff (int midiNote);
        void allNotesOff();

        // ---- commands from the GUI -----------------------------------
        void mutateScope (ScopeLevel lvl, int id);
        void applySelectionBurst (ScopeLevel lvl, int id, float amount);
        void lockTraits (ScopeLevel lvl, int id, uint32_t mask, bool locked);
        void setGeneValue (ScopeLevel lvl, int id, int trait, float value);
        void setGeneDominance (ScopeLevel lvl, int id, int trait, int dominance);
        void transferGenes (int fromSpecies, int toSpecies, uint32_t traitMask);
        void infectScope (ScopeLevel lvl, int id, Infection type);
        void cureScope (ScopeLevel lvl, int id);
        void apoptosisScope (ScopeLevel lvl, int id, float fraction);
        void isolateScope (ScopeLevel lvl, int id);
        void clearIsolation();
        void muteScope (ScopeLevel lvl, int id, bool mute);
        void preserveScope (ScopeLevel lvl, int id, bool preserve);
        void eliminateScope (ScopeLevel lvl, int id);
        void injectGenome (const Genome& g, Species sp, int count);
        void injectOrganism (const OrganismState& o, bool asMemory);

        // ---- state ---------------------------------------------------
        void restoreOrganism (const OrganismState& o);
        OrganismState captureOrganism() const;

        void writeSnapshot (EngineSnapshot& snap) const;

        // ---- identity ----------------------------------------------
        uint64_t seed() const { return colonySeed; }
        void     setSeed (uint64_t s) { colonySeed = s; rng.seed (s); }
        int      generation() const { return generationCount; }
        int      population() const;
        bool     isExploring() const { return env.explore; }

    private:
        struct Isolation { bool active = false; ScopeLevel level = ScopeLevel::colony; int id = 0; };

        void ecologyTick (double dt);
        void evaluateFitness();
        int  findFreeSlot();
        Cell* spawnChild (const Cell& parent);
        void  killSlot (int slot, bool violent);
        float genomeVariance() const;
        bool  inScope (const Cell& c, ScopeLevel lvl, int id) const;
        Genome selectionTargetGenome() const;
        void  pushArc (float x1, float y1, float x2, float y2, int kind);

        std::vector<Cell> cells;
        int   maxCellCount = 56;
        double sr = 44100.0;
        int   maxBlockSize = 512;

        std::unique_ptr<SourceMaterial> src;

        Environment env;
        Rng   rng;
        uint64_t colonySeed = 0x1234ABCDULL;

        int      generationCount = 0;
        uint64_t tickCounter = 0;
        double   tickAccumSec = 0.0;
        int      nextFamilyId = 1;
        int      nextGroupId  = 1;

        Genome baselineGenome;   // the seed genome, for divergence measurement

        Isolation isolation;

        // scratch buffers (allocated in prepare)
        juce::AudioBuffer<float> excBuffer;
        juce::AudioBuffer<float> workBuffer;

        // visual event fields (decay over time; read into snapshot)
        float geneTransferFlash = 0.0f;
        float extinctionFlash   = 0.0f;
        float pressureFront     = 0.0f;
        float infectionField    = 0.0f;
        float outRmsSmoothed    = 0.0f;

        static constexpr int maxArcs = EngineSnapshot::maxArcs;
        GeneArc arcs[maxArcs] {};
        int     arcHead = 0;

        // MIDI note -> voice group id
        int noteGroup[128];
        int noteActiveCount = 0;

        float lastAvgFitness = 0.5f;
        float lastDiversity  = 0.5f;
        int   liveWritePos   = 0;
        float speciesGain[numSpecies] { 1.0f, 1.0f, 1.0f };
        float masterGain = 0.9f;
    };
}
