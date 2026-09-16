#include "Entropy.h"

#include <chrono>
#include <cmath>
#include <random>

namespace mutagen
{
    static uint64_t splitmix64 (uint64_t& s) noexcept
    {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    static uint64_t rotl64 (uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }

    static uint64_t hiResTicks() noexcept
    {
        return (uint64_t) std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    // -----------------------------------------------------------------------

    EntropyPool::EntropyPool()
    {
        // Seed the pool from everything available at construction: the
        // platform entropy source, the clock, and ASLR (the address of a
        // stack object differs per process on any modern OS).
        std::random_device rd;
        uint64_t s = hiResTicks();

        for (int i = 0; i < 4; ++i)
        {
            uint64_t v = splitmix64 (s);
            v ^= (uint64_t) rd() | ((uint64_t) rd() << 32);
            pool[i].store (v, std::memory_order_relaxed);
        }

        const auto stackAddr = (uint64_t) (uintptr_t) &s;
        mix (stackAddr);
        mix ((uint64_t) (uintptr_t) this);
        addTimingJitter();
    }

    void EntropyPool::mix (uint64_t v) noexcept
    {
        // Fold the observation across all four words with different rotations
        // so a low-entropy input still spreads rather than sitting in one lane.
        const uint64_t c = counter.fetch_add (1, std::memory_order_relaxed);
        uint64_t s = v ^ (c * 0x9E3779B97F4A7C15ULL);

        for (int i = 0; i < 4; ++i)
        {
            const uint64_t contribution = splitmix64 (s);
            uint64_t prev = pool[i].load (std::memory_order_relaxed);
            pool[i].store (rotl64 (prev, 17 + 7 * i) ^ contribution, std::memory_order_relaxed);
        }
    }

    // -----------------------------------------------------------------------

    void EntropyPool::addTimingJitter() noexcept
    {
        // The *difference* between consecutive clock reads is the entropy:
        // it varies with cache state, scheduling and interrupts. The absolute
        // time is predictable and contributes almost nothing.
        uint64_t acc = 0;
        uint64_t prev = hiResTicks();
        for (int i = 0; i < 8; ++i)
        {
            const uint64_t now = hiResTicks();
            acc = rotl64 (acc, 7) ^ (now - prev);
            prev = now;
        }
        mix (acc);
    }

    void EntropyPool::addEvent (uint64_t observation) noexcept
    {
        mix (observation ^ rotl64 (hiResTicks(), 32));
    }

    // -----------------------------------------------------------------------

    void EntropyPool::feedAudio (const float* data, int numSamples) noexcept
    {
        if (data == nullptr || numSamples <= 0) return;

        // Block level decides whether there is anything to harvest at all.
        float peak = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float a = std::fabs (data[n]);
            if (a > peak) peak = a;
        }

        // A digitally silent input has no noise floor to sample. Bail before
        // touching the pool so an absent device cannot dilute it.
        if (! (peak > 1.0e-6f))
        {
            const float lvl = tapLevel.load (std::memory_order_relaxed);
            tapLevel.store (lvl * 0.95f, std::memory_order_relaxed);
            return;
        }

        int produced = 0;

        for (int n = 0; n < numSamples; ++n)
        {
            // Bottom bit of a 24-bit quantisation of the sample. For anything
            // that is not a synthetic constant this bit is converter noise.
            const int32_t q = (int32_t) (data[n] * 8388607.0f);
            const int bit = (int) (q & 1);

            // von Neumann debias: pairs 01 -> 0, 10 -> 1, 00/11 -> discard.
            if (vnPending < 0)
            {
                vnPending = bit;
                continue;
            }

            const int first = vnPending;
            vnPending = -1;
            if (first == bit) continue;          // biased pair, thrown away

            vnWord = (vnWord << 1) | (uint64_t) (first == 0 ? 0 : 1);
            if (++vnCount >= 64)
            {
                audioStage.fetch_xor (vnWord, std::memory_order_relaxed);
                audioBits.fetch_add (64, std::memory_order_relaxed);
                produced += 64;
                vnWord = 0;
                vnCount = 0;
            }
        }

        if (produced > 0)
        {
            // Smoothed "the tap is alive" indicator for the GUI.
            const float lvl = tapLevel.load (std::memory_order_relaxed);
            const float target = produced >= 64 ? 1.0f : (float) produced / 64.0f;
            tapLevel.store (lvl + (target - lvl) * 0.2f, std::memory_order_relaxed);
        }
        else
        {
            const float lvl = tapLevel.load (std::memory_order_relaxed);
            tapLevel.store (lvl * 0.98f, std::memory_order_relaxed);
        }
    }

    void EntropyPool::collect() noexcept
    {
        const uint64_t staged = audioStage.exchange (0, std::memory_order_relaxed);
        if (staged != 0) mix (staged);
        addTimingJitter();
    }

    // -----------------------------------------------------------------------

    uint64_t EntropyPool::nextSeed() noexcept
    {
        collect();

        uint64_t s = 0;
        for (int i = 0; i < 4; ++i)
            s = rotl64 (s, 13) ^ pool[i].load (std::memory_order_relaxed);
        s ^= counter.fetch_add (1, std::memory_order_relaxed) * 0xD1B54A32D192ED03ULL;

        const uint64_t out = splitmix64 (s);
        mix (out ^ hiResTicks());      // never hand out the same seed twice
        return out;
    }

    void EntropyPool::reseed (Rng& rng) noexcept
    {
        rng.seed (nextSeed());
    }

    void EntropyPool::stir (Rng& rng) noexcept
    {
        // Perturb rather than replace: the run keeps its identity but stops
        // being a pure function of its original seed.
        const uint64_t e = nextSeed();
        for (int i = 0; i < 4; ++i)
            rng.pokeState (i, rng.peekState (i) ^ rotl64 (e, 11 * i + 5));

        // A xoshiro state of all zeros is a fixed point; make that impossible.
        bool allZero = true;
        for (int i = 0; i < 4; ++i) if (rng.peekState (i) != 0) { allZero = false; break; }
        if (allZero) rng.seed (e | 1ULL);
    }

    // -----------------------------------------------------------------------

    EntropyPool& globalEntropy()
    {
        static EntropyPool pool;
        return pool;
    }
}
