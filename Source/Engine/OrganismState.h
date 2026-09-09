#pragma once

#include <cstdint>
#include <cstring>
#include "Genome.h"
#include "Cells.h"
#include "Environment.h"

namespace mutagen
{
    // =====================================================================
    //  Plain serialisable snapshots
    // =====================================================================

    struct GenomeData
    {
        float   value[numTraits];
        float   mutationRange[numTraits];
        uint8_t dominance[numTraits];
        uint8_t locked[numTraits];

        static GenomeData from (const Genome& g)
        {
            GenomeData d {};
            for (int i = 0; i < numTraits; ++i)
            {
                d.value[i]         = g.raw()[i].value;
                d.mutationRange[i] = g.raw()[i].mutationRange;
                d.dominance[i]     = (uint8_t) g.raw()[i].dominance;
                d.locked[i]        = g.raw()[i].locked ? 1 : 0;
            }
            return d;
        }
        Genome to() const
        {
            Genome g;
            for (int i = 0; i < numTraits; ++i)
            {
                g.raw()[i].value         = value[i];
                g.raw()[i].mutationRange = mutationRange[i];
                g.raw()[i].dominance     = (Dominance) dominance[i];
                g.raw()[i].locked        = locked[i] != 0;
            }
            return g;
        }
    };

    struct CellData
    {
        uint8_t species = 0, stage = 0, alive = 0, preserved = 0, muted = 0, infection = 0;
        int32_t familyId = 0, speciesGroupId = 0, generation = 0, voiceGroup = -1;
        float   ageSec = 0, lifeSec = 8, energy = 0, health = 1, x = 0.5f, y = 0.5f;
        float   infectionLoad = 0;
        GenomeData genome {};
    };

    // A complete, replayable organism: enough to reconstruct the colony bit
    // for bit (given the same seed) or to store as a preset / history node.
    struct OrganismState
    {
        static constexpr int maxCells = 128;

        uint64_t   seed = 0;
        int32_t    generation = 0;
        int32_t    cellCount = 0;
        CellData   cells[maxCells] {};
        Environment env {};
        GenomeData baseline {};
        int32_t    popBySpecies[numSpecies] { 0, 0, 0 };
        float      avgFitness = 0.5f;
        float      diversity  = 0.5f;
        char       name[64] { "MUTAGEN" };

        void setName (const char* n)
        {
            std::memset (name, 0, sizeof (name));
            std::strncpy (name, n, sizeof (name) - 1);
        }
    };

    // =====================================================================
    //  Lightweight visual snapshot (audio thread -> GUI), lock-free swapped
    // =====================================================================

    struct CellView
    {
        int8_t species = 0, stage = 0, infectionType = 0;
        bool   preserved = false, muted = false, selected = false;
        float  x = 0.5f, y = 0.5f, radius = 0.01f;
        float  energy = 0, infection = 0, pulse = 0, fitness = 0.5f;
        int32_t familyId = 0, speciesGroupId = 0, generation = 0, linkTo = -1;
    };

    struct GeneArc { float x1 = 0, y1 = 0, x2 = 0, y2 = 0, life = 0; int kind = 0; };

    struct EngineSnapshot
    {
        static constexpr int maxCells = 128;
        static constexpr int maxArcs  = 32;

        int      count = 0;
        CellView cells[maxCells] {};

        int      population = 0;
        int      popBySpecies[numSpecies] { 0, 0, 0 };
        int      generation = 0;
        uint64_t seed = 0;

        float avgFitness = 0.5f;
        float diversity  = 0.5f;
        float outputRms  = 0.0f;
        float nutrientField = 0.5f;
        float mutationField = 0.3f;
        float pressureFront = 0.0f;   // selection intensity, decays
        float geneTransferFlash = 0.0f;
        float extinctionFlash   = 0.0f;
        float infectionField    = 0.0f;
        float temperatureField  = 0.25f;
        bool  preservedMode = false;

        int     arcCount = 0;
        GeneArc arcs[maxArcs] {};
    };

    // =====================================================================
    //  Commands (GUI -> audio), fixed-size, lock-free queued
    // =====================================================================

    enum class CommandType : int
    {
        none = 0,
        germinate,          // (re)seed the colony from the current source
        reseedRandom,       // random-genome colony
        clearColony,
        mutateNow,          // force a mutation pass on scope
        applySelection,     // apply a selection-pressure burst to scope
        lockTraits,         // traitMask -> lock
        unlockTraits,       // traitMask -> unlock
        setGeneValue,       // ia = trait, fa = value, on scope
        setGeneDominance,   // ia = trait, ib = dominance
        transferGenes,      // ia = fromSpecies, ib = toSpecies, traitMask = which
        infect,             // ia = Infection, on scope
        cure,               // clear infection on scope
        apoptosis,          // fa = fraction, on scope
        isolate,            // solo scope
        unisolate,
        muteScope,          // ib = 0/1
        preserveScope,      // ib = 0/1
        eliminateScope,     // kill scope now
        injectGenome,       // payload genome, ia = species, ib = count
        restoreOrganism,    // payload organism
        breedInject,        // payload organism (a bred specimen back into colony)
        captureGeneration,  // push an OrganismState to the history FIFO
        noteBurst,          // ia = midiNote, fa = velocity  (also handled via MIDI)
        hardReset           // u64 = new seed; wipes colony & regrows from source
    };

    enum class ScopeLevel : int { colony = 0, species, family, cell };

    struct EngineCommand
    {
        CommandType type  = CommandType::none;
        ScopeLevel  scope = ScopeLevel::colony;
        int      scopeId  = 0;      // species id / family id / cell slot
        int      ia = 0, ib = 0;
        float    fa = 0.0f, fb = 0.0f;
        uint32_t traitMask = 0;
        uint64_t u64 = 0;
        int      payloadIndex = -1; // index into the processor's payload ring
    };
}
