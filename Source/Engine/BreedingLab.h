#pragma once

#include <vector>
#include <juce_core/juce_core.h>
#include "OrganismState.h"
#include "Genome.h"
#include "Rng.h"

namespace mutagen
{
    /*  The Breeding Lab. Given one or two preserved organisms it produces a
        family of descendants. Each descendant is a full OrganismState with a
        freshly generated population grown from a recombined "average genome".  */

    // Which parent contributes each functional block of the sound.
    // 0 = parent A, 1 = parent B, 2 = blend / auto-recombine.
    struct BreedingRecipe
    {
        int body      = 2;   // resonator traits: resonance, decay, formant body
        int voice     = 2;   // spectral traits: brightness, formant, noise colour
        int texture   = 2;   // grain traits: duration, direction, density
        int movement  = 2;   // spatial position & motion
        int lifecycle = 2;   // lifespan, repro rate, metabolism
        int envBehav  = 2;   // mutability, aggression, symbiosis, infection resist
        bool autoRecombine = true;   // ignore the above, recombine per-gene at random
        float mutationAmount = 0.25f;
        float variationSpread = 0.5f; // how different siblings are from each other
    };

    struct Specimen
    {
        OrganismState organism;
        juce::String  name;
        float similarityA = 0.0f;
        float similarityB = 0.0f;
        int   popBySpecies[numSpecies] { 0, 0, 0 };
        int   dominantTrait = 0;      // Trait index most divergent from the mean
        bool  rejected  = false;
        bool  preserved = false;
    };

    class BreedingLab
    {
    public:
        void setParents (const OrganismState& a, const OrganismState& b, bool hasSecondParent);
        bool hasParents() const { return hasA; }
        bool hasTwoParents() const { return hasA && hasB; }
        const OrganismState& parentA() const { return pa; }
        const OrganismState& parentB() const { return pb; }

        void breed (int count, const BreedingRecipe& recipe, uint64_t seed);
        void clear() { specimens.clear(); }

        std::vector<Specimen>&       results()       { return specimens; }
        const std::vector<Specimen>& results() const { return specimens; }

        /** Average genome across a parent's living cells. */
        static Genome averageGenome (const OrganismState& o);

    private:
        Genome recombineByRecipe (const Genome& ga, const Genome& gb,
                                  const BreedingRecipe& r, Rng& rng) const;
        static OrganismState growFrom (const Genome& g, const OrganismState& tmpl,
                                       uint64_t seed, float spread);

        OrganismState pa, pb;
        bool hasA = false, hasB = false;
        std::vector<Specimen> specimens;
    };
}
