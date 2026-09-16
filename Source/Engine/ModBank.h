#pragma once

#include <cmath>
#include <cstdint>
#include "Genome.h"
#include "WorldSeed.h"
#include "Rng.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        ModBank - the wobble.

        Every cell carries six low-frequency oscillators whose rates are
        spread *logarithmically* across more than four decades:

            lane 0   ~0.003 Hz   one cycle per ~5 minutes
            lane 1   ~0.017 Hz   one cycle per ~60 seconds
            lane 2   ~0.09  Hz   one cycle per ~11 seconds
            lane 3   ~0.5   Hz
            lane 4   ~2.7   Hz
            lane 5   ~15    Hz   audible-rate flutter

        (The actual spacing is genetic - the numbers above are the default
        centre/spread. A cell can compress all six lanes into a narrow band
        or stretch them wider still.)

        Two things fall out of that choice.

        First, it answers the brief directly: some things oscillate quickly
        and some take a minute or more per cycle, so a colony is never
        "wobbling" at one identifiable speed.

        Second, summing independent random-ish sources updated at
        octave-spaced rates *is* the Voss-McCartney construction for 1/f
        noise. Pink modulation is the documented perceptual sweet spot
        between white (too twitchy to follow) and Brownian (too smooth to
        notice), which is why `pink()` - the normalised sum of all lanes -
        is used as the general-purpose drift signal on top of the per-lane
        routing.

        Lanes are routed to destinations by the WorldSeed, not by the cell,
        so a world has a consistent *kind* of movement while every cell in
        it moves differently. Updated once per audio block at control rate;
        the per-sample cost is one multiply-add where the value is used.
        ------------------------------------------------------------------ */

    enum class ModDest : int
    {
        pitch = 0,      // transposition, in semitones * depth
        amp,            // tremolo
        formant,        // vowel centre
        brightness,     // spectral tilt
        pan,            // stereo position
        density,        // grain / partial count
        resonance,      // Q & feedback
        grainRate,      // playback speed of grain cells
        detune,         // partial-by-partial spread
        count
    };
    inline constexpr int numModDests = (int) ModDest::count;

    class ModBank
    {
    public:
        static constexpr int numLanes = WorldSeed::numLanes;

        /*  Rate limits. The slow end is deliberately absurd: a lane at
            0.0028 Hz takes about six minutes to complete one cycle, so a
            colony left running keeps unfolding long after every note has
            been reborn many times over. */
        static constexpr float minRateHz = 0.0028f;   // ~6 minutes per cycle
        static constexpr float maxRateHz = 26.0f;

        void configure (const Genome& g, const WorldSeed& world, Rng& rng)
        {
            const float geneCentre = g.get (Trait::lfoRateCentre);
            const float geneSpread = g.get (Trait::lfoRateSpread);
            const float geneSkew   = g.get (Trait::lfoRateSkew);
            const float depthMain  = g.get (Trait::lfoDepth);
            const float depthPitch = g.get (Trait::lfoDepthPitch);
            const float depthTimbre = g.get (Trait::lfoDepthTimbre);
            const float shapeGene  = g.get (Trait::lfoShape);
            const float scatter    = g.get (Trait::lfoPhaseScatter);
            const float sync       = g.get (Trait::wobbleSync);

            // Blend the cell's own idea of rate with the world's.
            const float centre = 0.5f * (geneCentre + world.rateCentre);
            const float spread = 0.5f * (geneSpread + world.rateSpread);

            const float logMin = std::log (minRateHz);
            const float logMax = std::log (maxRateHz);

            for (int i = 0; i < numLanes; ++i)
            {
                auto& L = lane[i];

                // Position of this lane in the rate range, 0 (slowest) .. 1.
                float t = numLanes > 1 ? (float) i / (float) (numLanes - 1) : 0.5f;

                // skew bunches the lanes toward one end without collapsing them
                t = std::pow (t, 0.35f + 1.8f * geneSkew);

                // centre shifts the whole bank, spread scales it around centre
                t = centre + (t - 0.5f) * spread * 2.0f;
                t += rng.bipolar() * 0.05f;               // per-cell rate error
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

                L.rateHz = std::exp (logMin + (logMax - logMin) * t);

                // Fast lanes must be shallower or everything turns to buzz;
                // slow lanes can move a long way because you hear them arrive.
                const float speedNorm = t;
                const float depthCurve = 1.0f - 0.72f * speedNorm * speedNorm;

                L.dest = (ModDest) juceLimitDest (world.laneDest[i]);

                float d = depthMain * depthCurve * world.modDepth;
                if (L.dest == ModDest::pitch || L.dest == ModDest::detune
                    || L.dest == ModDest::grainRate)
                    d *= 0.35f + 1.3f * depthPitch;
                else
                    d *= 0.35f + 1.3f * depthTimbre;

                L.depth = d * rng.range (0.55f, 1.0f);

                // Shape: the gene picks a family, the lane varies within it.
                const float s = shapeGene + rng.bipolar() * 0.18f;
                L.shape = s < 0.22f ? Shape::sine
                        : s < 0.42f ? Shape::triangle
                        : s < 0.60f ? Shape::smoothRandom
                        : s < 0.76f ? Shape::sampleHold
                        : s < 0.90f ? Shape::ramp
                                    : Shape::exponential;

                // Phase scatter is what stops a family of related cells from
                // breathing in lockstep, which is most of why the old build
                // sounded like one organism instead of many.
                const float lockedPhase = (float) i / (float) numLanes;
                L.phase = lockedPhase * (1.0f - scatter * (1.0f - sync))
                        + rng.nextFloat() * scatter * (1.0f - sync);

                L.rndState = rng.nextU64() | 1ULL;
                L.rndCurrent = rng.bipolar();
                L.rndTarget  = rng.bipolar();
            }

            driftAmount = g.get (Trait::drift);
            jitterAmount = g.get (Trait::jitter);
            vibrato = g.get (Trait::vibrato);
            tremolo = g.get (Trait::tremolo);
            driftState = rng.bipolar() * 0.3f;
            jitterState = 0x2545F491u ^ (uint32_t) rng.nextU64();

            for (auto& v : out) v = 0.0f;
            pinkSum = 0.0f;
        }

        /** Advance every lane. Call once per audio block. */
        void advance (float dtSeconds) noexcept
        {
            for (auto& v : out) v = 0.0f;

            float sum = 0.0f;
            for (int i = 0; i < numLanes; ++i)
            {
                auto& L = lane[i];

                const float prevPhase = L.phase;
                L.phase += L.rateHz * dtSeconds;
                const bool wrapped = L.phase >= 1.0f;
                if (wrapped) L.phase -= std::floor (L.phase);

                const float v = shapeValue (L, wrapped);
                (void) prevPhase;
                L.value = v;

                sum += v;
                out[(int) L.dest] += v * L.depth;
            }

            // Voss-McCartney: the sum of octave-spaced sources is 1/f.
            pinkSum = sum / (float) numLanes;

            // A slow unbounded-ish random walk on top, gently pulled home so
            // it cannot run away. This is the "it never settles" term.
            driftState += (nextWhite() * 0.5f - driftState * 0.06f) * dtSeconds * 0.8f;
            driftState = driftState < -1.5f ? -1.5f : (driftState > 1.5f ? 1.5f : driftState);

            // Dedicated vibrato / tremolo on top of the routed lanes, so a
            // cell always has *some* movement even if the world happens to
            // route every lane somewhere inaudible.
            vibPhase += (0.7f + 8.0f * vibrato) * dtSeconds;
            if (vibPhase > 1.0f) vibPhase -= std::floor (vibPhase);
            tremPhase += (0.3f + 11.0f * tremolo) * dtSeconds;
            if (tremPhase > 1.0f) tremPhase -= std::floor (tremPhase);

            constexpr float twoPi = 6.28318530718f;
            out[(int) ModDest::pitch] += std::sin (vibPhase * twoPi) * vibrato * 0.35f;
            out[(int) ModDest::amp]   += std::sin (tremPhase * twoPi) * tremolo * 0.5f;

            const float drift = driftState * driftAmount;
            out[(int) ModDest::pitch]      += drift * 0.5f;
            out[(int) ModDest::brightness] += drift * 0.6f;
            out[(int) ModDest::formant]    += drift * 0.5f;
        }

        /** Bipolar modulation for a destination, roughly -1 .. 1. */
        float get (ModDest d) const noexcept { return out[(int) d]; }

        /** Normalised 1/f sum of every lane, -1 .. 1. */
        float pink() const noexcept { return pinkSum; }

        /** Per-sample white jitter, for grain position and partial detune. */
        float jitter() noexcept
        {
            if (jitterAmount < 1.0e-4f) return 0.0f;
            jitterState = jitterState * 1664525u + 1013904223u;
            const float w = (float) (jitterState >> 8) * (1.0f / 8388608.0f) - 1.0f;
            return w * jitterAmount;
        }

        float rateOfLane (int i) const noexcept
        {
            return lane[i < 0 ? 0 : (i % numLanes)].rateHz;
        }

        /** Slowest lane period in seconds - shown in the GUI so the player can
            see that something really is moving on a one-minute timescale. */
        float slowestPeriodSeconds() const noexcept
        {
            float slowest = maxRateHz;
            for (int i = 0; i < numLanes; ++i)
                if (lane[i].rateHz < slowest) slowest = lane[i].rateHz;
            return slowest > 0.0f ? 1.0f / slowest : 0.0f;
        }

    private:
        enum class Shape : uint8_t { sine, triangle, smoothRandom, sampleHold, ramp, exponential };

        struct Lane
        {
            float    rateHz = 1.0f;
            float    phase  = 0.0f;
            float    depth  = 0.0f;
            float    value  = 0.0f;
            ModDest  dest   = ModDest::pitch;
            Shape    shape  = Shape::sine;
            uint64_t rndState = 1;
            float    rndCurrent = 0.0f, rndTarget = 0.0f;
        };

        static int juceLimitDest (uint8_t d)
        {
            return d < numModDests ? (int) d : (int) d % numModDests;
        }

        static float laneRandom (Lane& L) noexcept
        {
            L.rndState ^= L.rndState << 13;
            L.rndState ^= L.rndState >> 7;
            L.rndState ^= L.rndState << 17;
            return (float) ((L.rndState >> 40) * (1.0 / 16777216.0)) * 2.0f - 1.0f;
        }

        float nextWhite() noexcept
        {
            jitterState = jitterState * 1664525u + 1013904223u;
            return (float) (jitterState >> 8) * (1.0f / 8388608.0f) - 1.0f;
        }

        static float shapeValue (Lane& L, bool wrapped) noexcept
        {
            constexpr float twoPi = 6.28318530718f;
            switch (L.shape)
            {
                case Shape::sine:
                    return std::sin (L.phase * twoPi);

                case Shape::triangle:
                    return 4.0f * std::fabs (L.phase - 0.5f) - 1.0f;

                case Shape::ramp:
                    return L.phase * 2.0f - 1.0f;

                case Shape::exponential:
                {
                    // fast rise, slow fall - good for "breathing"
                    const float e = std::exp (-4.0f * L.phase);
                    return e * 2.0f - 1.0f;
                }

                case Shape::sampleHold:
                    if (wrapped) L.rndCurrent = laneRandom (L);
                    return L.rndCurrent;

                case Shape::smoothRandom:
                default:
                {
                    if (wrapped)
                    {
                        L.rndCurrent = L.rndTarget;
                        L.rndTarget  = laneRandom (L);
                    }
                    // cosine interpolation between successive random points
                    const float t = 0.5f - 0.5f * std::cos (L.phase * 3.14159265f);
                    return L.rndCurrent + (L.rndTarget - L.rndCurrent) * t;
                }
            }
        }

        Lane  lane[numLanes] {};
        float out[numModDests] {};
        float pinkSum = 0.0f;

        float driftAmount = 0.0f, driftState = 0.0f;
        float jitterAmount = 0.0f;
        uint32_t jitterState = 0x2545F491u;
        float vibrato = 0.0f, tremolo = 0.0f;
        float vibPhase = 0.0f, tremPhase = 0.0f;
    };
}
