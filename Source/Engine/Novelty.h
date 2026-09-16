#pragma once

#include <cmath>
#include <cstdint>
#include "Genome.h"
#include "Rng.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        Novelty search, fitness sharing and a MAP-Elites archive.

        The old engine measured fitness as "distance to one target genome",
        which is the textbook recipe for premature convergence: the fittest
        thing to be is the same as everything else, so the population
        collapses onto a single point and the colony stops surprising you.

        Three mechanisms replace it, all of them standard in the
        quality-diversity literature:

          * Novelty     - an individual is scored on how *unlike* its
                          neighbours and its lineage's history it is, rather
                          than on how close it is to a goal. Novelty search
                          is specifically known for not converging.

          * Fitness sharing - individuals crowded together in behaviour space
                          divide their fitness between them, so a crowd is
                          always worth less per head than an empty niche.

          * MAP-Elites  - the behaviour space is discretised into bins and
                          the best individual in each bin is kept. Instead of
                          one winner we end up with an archive of many
                          different good sounds, which is also exactly what
                          you want to re-seed from when a population crashes.

        Everything here is fixed-size and allocation-free so the colony can
        use it on the audio thread.
        ------------------------------------------------------------------ */

    /** What a cell *sounds like*, as opposed to what its genes say.
        Novelty is measured here rather than in genome space, because two
        different genomes can produce the same sound and we only care about
        the sound. */
    struct Behaviour
    {
        static constexpr int dims = 6;

        float brightness = 0.5f;   // spectral tilt
        float density    = 0.5f;   // how many things are happening at once
        float noise      = 0.3f;   // tonal .. noisy
        float pitch      = 0.5f;   // register
        float movement   = 0.5f;   // how much it modulates
        float attack     = 0.5f;   // envelope character

        static Behaviour fromGenome (const Genome& g)
        {
            Behaviour b;
            b.brightness = g.get (Trait::brightness);
            b.density    = g.get (Trait::density);
            b.noise      = g.get (Trait::noiseColour);
            b.pitch      = g.get (Trait::pitch);
            b.movement   = 0.45f * g.get (Trait::lfoDepth)
                         + 0.30f * g.get (Trait::lfoRateSpread)
                         + 0.25f * g.get (Trait::drift);
            b.attack     = 0.5f * (g.get (Trait::envAttack) + g.get (Trait::decayShape));
            return b;
        }

        float at (int i) const
        {
            switch (i)
            {
                case 0: return brightness;
                case 1: return density;
                case 2: return noise;
                case 3: return pitch;
                case 4: return movement;
                default: return attack;
            }
        }

        /** Euclidean distance normalised to roughly 0..1. */
        float distanceTo (const Behaviour& o) const
        {
            float acc = 0.0f;
            for (int i = 0; i < dims; ++i)
            {
                const float d = at (i) - o.at (i);
                acc += d * d;
            }
            return std::sqrt (acc / (float) dims);
        }
    };

    // =====================================================================

    /** A rolling archive of behaviours the colony has already produced.
        Novelty is the mean distance to the k nearest entries: an individual
        that resembles the colony's own past scores badly even if it is
        unlike its living neighbours. */
    class NoveltyArchive
    {
    public:
        static constexpr int capacity = 192;
        static constexpr int k        = 8;

        void clear() noexcept { count = 0; head = 0; }

        void add (const Behaviour& b) noexcept
        {
            entries[head] = b;
            head = (head + 1) % capacity;
            if (count < capacity) ++count;
        }

        /** Add only if it is actually new - otherwise a static colony would
            fill the archive with copies of itself and novelty would read as
            zero for everything, including genuinely new arrivals. */
        bool addIfNovel (const Behaviour& b, float threshold = 0.06f) noexcept
        {
            if (count > 0 && noveltyOf (b) < threshold) return false;
            add (b);
            return true;
        }

        float noveltyOf (const Behaviour& b) const noexcept
        {
            if (count == 0) return 1.0f;

            // Partial selection of the k smallest distances - cheaper and
            // simpler than sorting the whole archive.
            float best[k];
            for (int i = 0; i < k; ++i) best[i] = 1.0e9f;

            for (int i = 0; i < count; ++i)
            {
                const float d = entries[i].distanceTo (b);
                // insertion into the running top-k
                for (int j = 0; j < k; ++j)
                {
                    if (d < best[j])
                    {
                        for (int m = k - 1; m > j; --m) best[m] = best[m - 1];
                        best[j] = d;
                        break;
                    }
                }
            }

            const int n = count < k ? count : k;
            float sum = 0.0f;
            for (int i = 0; i < n; ++i) sum += best[i];
            const float mean = sum / (float) n;
            return mean > 1.0f ? 1.0f : mean;
        }

        int size() const noexcept { return count; }

    private:
        Behaviour entries[capacity] {};
        int count = 0, head = 0;
    };

    // =====================================================================

    /** MAP-Elites over three audible axes. The grid is deliberately coarse:
        we want a handful of clearly different sounds to re-seed from, not a
        precise map. */
    class EliteGrid
    {
    public:
        static constexpr int nB = 6;   // brightness bins
        static constexpr int nD = 5;   // density bins
        static constexpr int nN = 4;   // noise bins
        static constexpr int total = nB * nD * nN;

        struct Elite
        {
            Genome    genome;
            Behaviour behaviour;
            float     quality = 0.0f;
            bool      filled  = false;
            int       species = 0;
        };

        void clear() noexcept
        {
            for (auto& e : bins) e = Elite {};
            filledCount = 0;
        }

        static int binIndex (const Behaviour& b) noexcept
        {
            auto q = [] (float v, int n)
            {
                int i = (int) (v * (float) n);
                return i < 0 ? 0 : (i >= n ? n - 1 : i);
            };
            return (q (b.brightness, nB) * nD + q (b.density, nD)) * nN + q (b.noise, nN);
        }

        /** Offer an individual to the archive. Keeps it if its bin is empty
            or if it beats the incumbent. */
        bool consider (const Genome& g, const Behaviour& b, float quality, int species) noexcept
        {
            const int i = binIndex (b);
            auto& e = bins[i];
            if (e.filled && quality <= e.quality) return false;
            if (! e.filled) ++filledCount;
            e.genome = g;
            e.behaviour = b;
            e.quality = quality;
            e.species = species;
            e.filled = true;
            return true;
        }

        /** A random stored elite, or null if the archive is empty. */
        const Elite* randomElite (Rng& rng) const noexcept
        {
            if (filledCount == 0) return nullptr;
            for (int attempt = 0; attempt < 32; ++attempt)
            {
                const int i = rng.intRange (0, total);
                if (bins[i].filled) return &bins[i];
            }
            for (int i = 0; i < total; ++i) if (bins[i].filled) return &bins[i];
            return nullptr;
        }

        /** An elite chosen to push *away* from where the colony currently
            lives: prefers the filled bin furthest from the given behaviour.
            This is what makes a rescue diversify instead of re-converging. */
        const Elite* mostDistantElite (const Behaviour& from) const noexcept
        {
            const Elite* best = nullptr;
            float bestD = -1.0f;
            for (int i = 0; i < total; ++i)
            {
                if (! bins[i].filled) continue;
                const float d = bins[i].behaviour.distanceTo (from);
                if (d > bestD) { bestD = d; best = &bins[i]; }
            }
            return best;
        }

        /** 0..1 - how much of the behaviour space the colony has explored.
            Shown in the GUI and used by the score as a discovery bonus. */
        float coverage() const noexcept { return (float) filledCount / (float) total; }
        int   filled() const noexcept { return filledCount; }

        const Elite& binAt (int i) const noexcept { return bins[i < 0 ? 0 : (i % total)]; }

    private:
        Elite bins[total] {};
        int   filledCount = 0;
    };

    // =====================================================================

    /** Watches whether the colony is still going anywhere. When novelty and
        diversity both flat-line, the ecology has found a local optimum and
        will sit in it forever unless something shoves it - so we shove it. */
    class StagnationDetector
    {
    public:
        void reset() noexcept
        {
            slowAvg = fastAvg = 0.5f;
            stagnantSeconds = 0.0f;
            stormSeconds = 0.0f;
        }

        void update (float diversity, float noveltyRate, float dt) noexcept
        {
            const float signal = 0.5f * diversity + 0.5f * noveltyRate;

            // Two exponential averages at very different time constants; when
            // they agree, nothing has changed for a long time.
            fastAvg += (signal - fastAvg) * (1.0f - std::exp (-dt / 3.0f));
            slowAvg += (signal - slowAvg) * (1.0f - std::exp (-dt / 45.0f));

            const bool flat = std::fabs (fastAvg - slowAvg) < 0.02f && fastAvg < 0.30f;
            if (flat) stagnantSeconds += dt;
            else      stagnantSeconds = std::fmax (0.0f, stagnantSeconds - dt * 2.0f);

            if (stormSeconds > 0.0f) stormSeconds -= dt;
        }

        /** True once the colony has been flat long enough to intervene. */
        bool shouldStorm() const noexcept
        {
            return stagnantSeconds > 12.0f && stormSeconds <= 0.0f;
        }

        void beginStorm (float seconds = 4.0f) noexcept
        {
            stormSeconds = seconds;
            stagnantSeconds = 0.0f;
        }

        bool storming() const noexcept { return stormSeconds > 0.0f; }
        float stagnation() const noexcept
        {
            const float s = stagnantSeconds / 12.0f;
            return s > 1.0f ? 1.0f : s;
        }

    private:
        float slowAvg = 0.5f, fastAvg = 0.5f;
        float stagnantSeconds = 0.0f;
        float stormSeconds = 0.0f;
    };
}
