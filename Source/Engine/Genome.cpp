#include "Genome.h"
#include <cmath>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    void Genome::randomise (Rng& rng)
    {
        for (int i = 0; i < numTraits; ++i)
        {
            auto& g = genes[i];
            g.value        = rng.nextFloat();
            g.mutationRange = rng.range (0.06f, 0.28f);
            g.locked       = false;

            const float d = rng.nextFloat();
            g.dominance = d < 0.55f ? Dominance::dominant
                        : d < 0.85f ? Dominance::recessive
                                    : Dominance::dormant;
        }

        // Bias a few structural traits toward musical middle ground so a fresh
        // colony is playable rather than pure chaos.
        genes[(int) Trait::pitch].value      = rng.range (0.4f, 0.6f);
        genes[(int) Trait::metabolism].value = rng.range (0.35f, 0.65f);
        genes[(int) Trait::lifespan].value   = rng.range (0.4f, 0.8f);
    }

    void Genome::mutate (Rng& rng, float rate, float depth, float radiation)
    {
        rate      = clamp01 (rate);
        depth     = clamp01 (depth);
        radiation = clamp01 (radiation);

        for (auto& g : genes)
        {
            if (g.locked) continue;

            if (rng.chance (rate))
            {
                const float excursion = g.mutationRange * (0.25f + 1.75f * depth);
                g.value += rng.gaussian() * excursion;
            }

            // Radiation: rare, violent, and it can also flip dominance / retune
            // how mutable the lineage is from here on.
            if (rng.chance (radiation * 0.15f))
            {
                g.value += rng.bipolar() * (0.4f + 0.6f * depth);
                g.mutationRange = clamp01 (g.mutationRange + rng.bipolar() * 0.1f);

                if (rng.chance (0.4f))
                {
                    const int step = rng.chance (0.5f) ? 1 : 2;
                    g.dominance = (Dominance) (((int) g.dominance + step) % 3); // stay within first 3
                }
            }

            g.clampValue();
        }
    }

    Genome Genome::recombine (const Genome& a, const Genome& b, Rng& rng,
                              const uint32_t* parentAMask)
    {
        Genome child;
        for (int i = 0; i < numTraits; ++i)
        {
            const bool takeA = parentAMask != nullptr
                             ? ((*parentAMask >> i) & 1u) != 0u
                             : rng.chance (0.5f);

            const Gene& src   = takeA ? a.genes[i] : b.genes[i];
            const Gene& other = takeA ? b.genes[i] : a.genes[i];

            child.genes[i] = src;

            // Crossover jitter: occasionally average the two parents for a trait,
            // which is where "family resemblance" comes from.
            if (rng.chance (0.25f))
                child.genes[i].value = 0.5f * (src.value + other.value);

            // A locked trait on either parent stays locked in the child.
            child.genes[i].locked = src.locked || other.locked;
            child.genes[i].clampValue();
        }
        return child;
    }

    Genome Genome::blend (const Genome& a, const Genome& b, float t)
    {
        t = clamp01 (t);
        Genome out = a;
        for (int i = 0; i < numTraits; ++i)
        {
            if (a.genes[i].locked) continue;
            out.genes[i].value        = clamp01 (a.genes[i].value * (1.0f - t) + b.genes[i].value * t);
            out.genes[i].mutationRange = a.genes[i].mutationRange * (1.0f - t) + b.genes[i].mutationRange * t;
            if (t > 0.5f) out.genes[i].dominance = b.genes[i].dominance;
        }
        return out;
    }

    float Genome::distanceTo (const Genome& other) const
    {
        float acc = 0.0f;
        int   n   = 0;
        for (int i = 0; i < numTraits; ++i)
        {
            if (genes[i].locked) continue;
            const float d = genes[i].value - other.genes[i].value;
            acc += d * d;
            ++n;
        }
        if (n == 0) return 0.0f;
        return clamp01 (std::sqrt (acc / (float) n));
    }

    void Genome::updateGeneExpression (float environmentStress)
    {
        environmentStress = clamp01 (environmentStress);
        for (auto& g : genes)
        {
            // A dormant gene "wakes" under stress proportional to its own value
            // (the gene encodes its own activation threshold).
            if (g.dominance == Dominance::dormant && environmentStress > (0.35f + 0.5f * g.value))
                g.dominance = Dominance::activated;
            else if (g.dominance == Dominance::activated && environmentStress < (0.25f + 0.5f * g.value))
                g.dominance = Dominance::dormant;
        }
    }

    float Genome::expressed (Trait t, float environmentStress) const
    {
        (void) environmentStress;
        const Gene& g = genes[(int) t];
        switch (g.dominance)
        {
            case Dominance::dominant:  return g.value;
            case Dominance::activated: return g.value;
            case Dominance::recessive: return 0.5f * g.value + 0.25f; // pulled toward neutral
            case Dominance::dormant:   return 0.5f;                    // not expressed
        }
        return g.value;
    }

    void Genome::driftToward (const Genome& target, float amount)
    {
        amount = clamp01 (amount);
        for (int i = 0; i < numTraits; ++i)
        {
            if (genes[i].locked) continue;
            genes[i].value = clamp01 (genes[i].value + (target.genes[i].value - genes[i].value) * amount);
        }
    }
}
