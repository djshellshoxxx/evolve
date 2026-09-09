#include "BreedingLab.h"
#include <cmath>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // trait groups ------------------------------------------------------------
    static const int bodyTraits[]      = { (int) Trait::resonance, (int) Trait::decayShape, (int) Trait::envRelease };
    static const int voiceTraits[]     = { (int) Trait::brightness, (int) Trait::formant, (int) Trait::noiseColour };
    static const int textureTraits[]   = { (int) Trait::duration, (int) Trait::direction, (int) Trait::density, (int) Trait::envAttack };
    static const int movementTraits[]  = { (int) Trait::spatialPos, (int) Trait::spatialMotion };
    static const int lifecycleTraits[] = { (int) Trait::lifespan, (int) Trait::reproRate, (int) Trait::metabolism, (int) Trait::pitch };
    static const int envTraits[]       = { (int) Trait::mutability, (int) Trait::aggression, (int) Trait::symbiosisAffinity, (int) Trait::infectionResist };

    void BreedingLab::setParents (const OrganismState& a, const OrganismState& b, bool hasSecondParent)
    {
        pa = a; hasA = true;
        pb = b; hasB = hasSecondParent;
        if (! hasB) pb = a;
        specimens.clear();
    }

    Genome BreedingLab::averageGenome (const OrganismState& o)
    {
        Genome g;
        double acc[numTraits] = {};
        double accRange[numTraits] = {};
        int n = 0;
        for (int i = 0; i < o.cellCount && i < OrganismState::maxCells; ++i)
        {
            if (! o.cells[i].alive) continue;
            for (int t = 0; t < numTraits; ++t)
            {
                acc[t]      += o.cells[i].genome.value[t];
                accRange[t] += o.cells[i].genome.mutationRange[t];
            }
            ++n;
        }
        if (n == 0)
            return o.baseline.to();

        for (int t = 0; t < numTraits; ++t)
        {
            g.raw()[t].value         = clamp01 ((float) (acc[t] / n));
            g.raw()[t].mutationRange = clamp01 ((float) (accRange[t] / n));
        }
        return g;
    }

    static int groupChoice (int traitIdx, const BreedingRecipe& r)
    {
        auto in = [] (const int* arr, int count, int v)
        {
            for (int i = 0; i < count; ++i) if (arr[i] == v) return true;
            return false;
        };
        if (in (bodyTraits,      3, traitIdx)) return r.body;
        if (in (voiceTraits,     3, traitIdx)) return r.voice;
        if (in (textureTraits,   4, traitIdx)) return r.texture;
        if (in (movementTraits,  2, traitIdx)) return r.movement;
        if (in (lifecycleTraits, 4, traitIdx)) return r.lifecycle;
        if (in (envTraits,       4, traitIdx)) return r.envBehav;
        return 2;
    }

    Genome BreedingLab::recombineByRecipe (const Genome& ga, const Genome& gb,
                                           const BreedingRecipe& r, Rng& rng) const
    {
        Genome child;
        for (int t = 0; t < numTraits; ++t)
        {
            int choice = r.autoRecombine ? rng.intRange (0, 3) : groupChoice (t, r);

            float v;
            if      (choice == 0) v = ga.raw()[t].value;
            else if (choice == 1) v = gb.raw()[t].value;
            else                  v = 0.5f * (ga.raw()[t].value + gb.raw()[t].value)
                                    + rng.bipolar() * 0.05f;

            child.raw()[t].value         = clamp01 (v);
            child.raw()[t].mutationRange = 0.5f * (ga.raw()[t].mutationRange + gb.raw()[t].mutationRange);
            child.raw()[t].dominance     = (rng.chance (0.5f) ? ga : gb).raw()[t].dominance;
            child.raw()[t].locked        = ga.raw()[t].locked || gb.raw()[t].locked;
        }
        return child;
    }

    OrganismState BreedingLab::growFrom (const Genome& g, const OrganismState& tmpl,
                                         uint64_t seed, float spread)
    {
        Rng rng (seed);
        OrganismState o;
        o.seed = seed;
        o.generation = tmpl.generation + 1;
        o.env = tmpl.env;
        o.baseline = GenomeData::from (g);

        // keep the parent template's species mix and rough population size
        int mix[numSpecies] { 0, 0, 0 };
        int alive = 0;
        for (int i = 0; i < tmpl.cellCount && i < OrganismState::maxCells; ++i)
            if (tmpl.cells[i].alive)
            {
                mix[juce::jlimit (0, numSpecies - 1, (int) tmpl.cells[i].species)]++;
                ++alive;
            }
        if (alive == 0) { mix[0] = mix[1] = mix[2] = 6; alive = 18; }

        const int target = juce::jlimit (6, OrganismState::maxCells - 1, alive);
        int made = 0, fam = 1;
        for (int s = 0; s < numSpecies && made < target; ++s)
        {
            const int want = juce::jmax (1, (int) std::round ((float) mix[s] / (float) juce::jmax (1, alive) * target));
            for (int k = 0; k < want && made < target; ++k)
            {
                Genome cg = g;
                cg.mutate (rng, 0.7f, spread * 0.6f, 0.02f);
                if (s == (int) Species::grain)     cg.set (Trait::duration,  rng.range (0.05f, 0.45f));
                if (s == (int) Species::resonator) cg.set (Trait::resonance, juce::jlimit (0.3f, 0.95f, cg.get (Trait::resonance)));

                CellData cd;
                cd.species        = (uint8_t) s;
                cd.stage          = (uint8_t) LifeStage::growing;
                cd.alive          = 1;
                cd.familyId       = fam++;
                cd.speciesGroupId = s + 1;
                cd.generation     = o.generation;
                cd.energy         = 0.5f;
                cd.health         = 1.0f;
                cd.lifeSec        = 1.5f + 28.0f * std::pow (cg.get (Trait::lifespan), 1.5f);
                cd.x              = clamp01 (cg.get (Trait::spatialPos));
                cd.y              = 0.5f + rng.bipolar() * 0.25f;
                cd.genome         = GenomeData::from (cg);
                o.cells[made++]   = cd;
                o.popBySpecies[s]++;
            }
        }
        o.cellCount = made;
        return o;
    }

    void BreedingLab::breed (int count, const BreedingRecipe& recipe, uint64_t seed)
    {
        specimens.clear();
        if (! hasA) return;

        const Genome ga = averageGenome (pa);
        const Genome gb = averageGenome (hasB ? pb : pa);

        Rng master (seed);

        for (int i = 0; i < count; ++i)
        {
            const uint64_t childSeed = master.nextU64() ^ (0xA5A5A5A5ULL * (uint64_t) (i + 1));
            Rng rng (childSeed);

            Genome child = recombineByRecipe (ga, gb, recipe, rng);
            child.mutate (rng, 0.6f, recipe.mutationAmount, 0.03f + recipe.variationSpread * 0.05f);

            Specimen sp;
            sp.organism = growFrom (child, pa, childSeed, recipe.variationSpread);
            sp.organism.setName (juce::String ("Specimen " + juce::String (i + 1)).toRawUTF8());
            sp.name = "Specimen " + juce::String (i + 1);
            sp.similarityA = child.similarityTo (ga);
            sp.similarityB = child.similarityTo (gb);
            for (int s = 0; s < numSpecies; ++s) sp.popBySpecies[s] = sp.organism.popBySpecies[s];

            // dominant trait = furthest from the average of the two parents
            float best = -1.0f; int bestT = 0;
            for (int t = 0; t < numTraits; ++t)
            {
                const float mean = 0.5f * (ga.raw()[t].value + gb.raw()[t].value);
                const float d = std::abs (child.raw()[t].value - mean);
                if (d > best) { best = d; bestT = t; }
            }
            sp.dominantTrait = bestT;

            specimens.push_back (std::move (sp));
        }
    }
}
