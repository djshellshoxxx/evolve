#pragma once

#include <cstdint>
#include <cmath>

namespace mutagen
{
    /*  A tiny, fast, fully deterministic PRNG (xoshiro256** core seeded through
        splitmix64). MUTAGEN leans on determinism hard: Preserve mode must be
        able to replay an entire evolutionary history from a single 64-bit seed,
        so every stochastic decision in the engine draws from one of these.     */
    class Rng
    {
    public:
        Rng() { seed (0x9E3779B97F4A7C15ULL); }
        explicit Rng (uint64_t s) { seed (s); }

        void seed (uint64_t s)
        {
            auto sm = [&s]
            {
                uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                return z ^ (z >> 31);
            };
            for (auto& v : state) v = sm();
        }

        uint64_t nextU64()
        {
            const uint64_t result = rotl (state[1] * 5, 7) * 9;
            const uint64_t t = state[1] << 17;
            state[2] ^= state[0];
            state[3] ^= state[1];
            state[1] ^= state[2];
            state[0] ^= state[3];
            state[2] ^= t;
            state[3] = rotl (state[3], 45);
            return result;
        }

        /** Uniform float in [0, 1). */
        float nextFloat() { return (float) ((nextU64() >> 40) * (1.0 / 16777216.0)); }

        /** Uniform float in [lo, hi). */
        float range (float lo, float hi) { return lo + (hi - lo) * nextFloat(); }

        /** Symmetric bipolar noise in (-1, 1). */
        float bipolar() { return nextFloat() * 2.0f - 1.0f; }

        /** Approx. standard normal via sum of uniforms (cheap, good enough). */
        float gaussian()
        {
            float s = 0.0f;
            for (int i = 0; i < 4; ++i) s += nextFloat();
            return (s - 2.0f) * 1.4142135f;
        }

        /** True with probability p. */
        bool chance (float p) { return nextFloat() < p; }

        int intRange (int lo, int hiExclusive)
        {
            if (hiExclusive <= lo) return lo;
            return lo + (int) (nextU64() % (uint64_t) (hiExclusive - lo));
        }

        uint64_t peekState (int i) const { return state[i & 3]; }
        void     pokeState (int i, uint64_t v) { state[i & 3] = v; }

    private:
        static uint64_t rotl (uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
        uint64_t state[4] {};
    };
}
