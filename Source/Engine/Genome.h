#pragma once

#include <array>
#include <cstdint>
#include "Rng.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        The genome.

        Every cell carries one Genome: a fixed vector of Genes. A Gene is a
        normalised value in [0,1] plus a dominance mode, an independent lock,
        and a per-gene mutation range. The *meaning* of each normalised value
        is trait-specific and is decoded by the cells (see Cells.cpp); the
        genome itself only knows how to copy, mutate, recombine and compare.
        ------------------------------------------------------------------ */

    enum class Trait : int
    {
        pitch = 0,        // transposition of the cell's material
        duration,         // grain / event length
        brightness,       // spectral tilt
        formant,          // formant / vowel centre
        direction,        // playback direction & time-warp bias
        spatialPos,       // stereo / depth position
        spatialMotion,    // how far & fast the cell wanders
        resonance,        // Q / feedback of resonant structure
        decayShape,       // how energy leaves the cell
        envAttack,        // onset softness
        envRelease,       // tail length
        lifespan,         // base age at programmed death
        reproRate,        // eagerness to divide
        mutability,       // how strongly this lineage mutates
        metabolism,       // energy burn rate
        aggression,       // predation / territory pressure exerted
        symbiosisAffinity,// tendency to bond & exchange genes
        infectionResist,  // resistance to spreading mutations
        density,          // internal grain/partial density
        noiseColour,      // tonal <-> noisy balance

        // ---- modulation genes (see ModBank.h) ----------------------------
        //  These are what make two genetically similar cells sound like two
        //  organisms rather than two copies. Everything above describes a
        //  static timbre; everything below describes how that timbre moves.
        lfoRateCentre,    // where this cell's six LFO lanes sit in the rate range
        lfoRateSpread,    // how far apart the lanes are (0 = all one speed)
        lfoRateSkew,      // bunches the lanes toward the slow or fast end
        lfoDepth,         // master modulation depth
        lfoDepthPitch,    // depth bias for pitch-ish destinations
        lfoDepthTimbre,   // depth bias for timbral destinations
        lfoShape,         // waveform family for the lanes
        lfoPhaseScatter,  // how desynchronised the lanes start
        wobbleSync,       // 1 = lanes lock together, 0 = fully independent
        drift,            // slow bounded random walk on pitch/brightness/formant
        jitter,           // per-sample white noise on grain position & detune
        vibrato,          // dedicated pitch wobble
        tremolo,          // dedicated amplitude wobble
        curiosity,        // how strongly this lineage is rewarded for novelty

        count
    };

    inline constexpr int numTraits = (int) Trait::count;

    enum class Dominance : uint8_t
    {
        dominant = 0,   // always expressed
        recessive,      // expressed only if not masked by a dominant partner
        dormant,        // silent until conditions activate it
        activated       // a dormant gene switched on by the environment
    };

    struct Gene
    {
        float     value        = 0.5f;
        float     mutationRange = 0.15f;   // max +/- excursion of a normal mutation
        Dominance dominance     = Dominance::dominant;
        bool      locked        = false;   // protected from mutation & recombination

        void clampValue() { value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }
    };

    class Genome
    {
    public:
        Genome() = default;

        Gene&       gene (Trait t)       { return genes[(int) t]; }
        const Gene& gene (Trait t) const { return genes[(int) t]; }

        float  get (Trait t) const { return genes[(int) t].value; }
        void   set (Trait t, float v) { genes[(int) t].value = v; genes[(int) t].clampValue(); }
        bool   isLocked (Trait t) const { return genes[(int) t].locked; }
        void   setLocked (Trait t, bool b) { genes[(int) t].locked = b; }

        /** Effective value once dominance & environment activation are resolved. */
        float expressed (Trait t, float environmentStress) const;

        /** Randomise every gene into a plausible starting spread. */
        void randomise (Rng& rng);

        /** True for the modulation block - those genes are treated differently
            by mutation (they are allowed to move much further, because a
            colony whose movement stops being varied is the failure mode). */
        static bool isModulationTrait (int i)
        {
            return i >= (int) Trait::lfoRateCentre && i <= (int) Trait::tremolo;
        }

        /** In-place mutation. rate 0..1 = probability a given gene moves;
            depth 0..1 scales the excursion; radiation 0..1 adds rare large jumps. */
        void mutate (Rng& rng, float rate, float depth, float radiation);

        /** Sexual recombination. For each trait, parentAMask bit set => take from a.
            When mask == nullptr, each trait is chosen at random 50/50.            */
        static Genome recombine (const Genome& a, const Genome& b, Rng& rng,
                                 const uint32_t* parentAMask = nullptr);

        /** Blend two genomes continuously (0 = all a, 1 = all b). Locked genes on
            'a' are preserved. Used for colony "memory" inheritance. */
        static Genome blend (const Genome& a, const Genome& b, float t);

        /** 0 (identical) .. 1 (maximally different) across unlocked traits. */
        float distanceTo (const Genome& other) const;

        /** 1 - distance. */
        float similarityTo (const Genome& other) const { return 1.0f - distanceTo (other); }

        /** Copy the dominance flags of a dormant gene into "activated" when the
            environment crosses its activation threshold; revert when it falls. */
        void updateGeneExpression (float environmentStress);

        /** Nudge every unlocked gene a little toward a target genome (selection). */
        void driftToward (const Genome& target, float amount);

        /** Guarantee the cell still moves. Called at the end of every mutation;
            call it after any external edit that could zero the modulation block. */
        void enforceMovementFloor();

        std::array<Gene, numTraits>&       raw()       { return genes; }
        const std::array<Gene, numTraits>& raw() const { return genes; }

    private:
        std::array<Gene, numTraits> genes {};
    };
}
