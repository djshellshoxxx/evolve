#pragma once

#include <atomic>
#include <cstdint>
#include "Rng.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        EntropyPool

        MUTAGEN's randomness is deliberately *not* purely algorithmic. A
        xoshiro PRNG seeded from the clock gives every instance the same
        statistical character; what makes two runs feel genuinely unrelated
        is real-world entropy folded into the seed.

        Four harvesters feed one 256-bit pool:

          1. std::random_device            - the platform's own source
          2. high-resolution timer jitter  - scheduling noise between reads
          3. audio-input LSBs              - the "radio noise" tap. Point an
                                             untuned radio, an SDR, a hissing
                                             preamp or a mic at the input and
                                             the bottom bit of the converter is
                                             a physical noise source.
          4. GUI gesture micro-timing      - where and *when* the user moves

        Raw hardware bits are biased, so the audio tap is von Neumann
        debiased before it reaches the pool: bit pairs 01 -> 0, 10 -> 1,
        00 / 11 -> discarded. Silence therefore contributes nothing rather
        than contributing a stream of zeros, which is exactly what we want -
        an unplugged input must not be able to *lower* the pool's quality.

        Threading: feedAudio() is called from the audio thread and is
        wait-free (it only ever xors into one atomic staging word). Everything
        else runs on the message thread. The pool is an entropy accumulator,
        not a cryptographic CSPRNG - interleaving is harmless.
        ------------------------------------------------------------------ */
    class EntropyPool
    {
    public:
        EntropyPool();

        // ---- audio thread ------------------------------------------------
        /** Harvest the converter's noise floor. Wait-free; safe to call every
            block. `gain` scales how many bits we trust (an obviously silent
            block yields nothing regardless). */
        void feedAudio (const float* data, int numSamples) noexcept;

        // ---- message thread ----------------------------------------------
        /** Mix in scheduling jitter from the high-resolution timer. */
        void addTimingJitter() noexcept;

        /** Mix in an arbitrary observation (mouse position, event time, a
            file's size, a note number - anything the user caused). */
        void addEvent (uint64_t observation) noexcept;

        /** Drain the audio staging word into the pool. Call periodically. */
        void collect() noexcept;

        /** A fresh 64-bit seed. Always mixes the pool forward, so two calls
            never return the same value. */
        uint64_t nextSeed() noexcept;

        /** Reseed an Rng from the pool, then stir the pool. */
        void reseed (Rng& rng) noexcept;

        /** Stir an *already seeded* Rng without fully replacing its state:
            keeps the run reproducible in character while nudging it. */
        void stir (Rng& rng) noexcept;

        // ---- telemetry for the GUI ---------------------------------------
        /** Total debiased bits ever harvested from the audio tap. */
        uint64_t audioBitsHarvested() const noexcept { return audioBits.load (std::memory_order_relaxed); }

        /** 0..1 - how lively the audio tap has been recently. 0 means the
            input is silent or absent and we are running on software entropy
            alone. */
        float audioTapLevel() const noexcept { return tapLevel.load (std::memory_order_relaxed); }

        /** True once the audio tap has contributed a meaningful amount. */
        bool hasLiveTap() const noexcept { return audioTapLevel() > 0.02f; }

    private:
        void mix (uint64_t v) noexcept;

        std::atomic<uint64_t> pool[4];
        std::atomic<uint64_t> counter { 0 };

        // audio-thread staging
        std::atomic<uint64_t> audioStage { 0 };
        std::atomic<uint64_t> audioBits  { 0 };
        std::atomic<float>    tapLevel   { 0.0f };

        // per-thread von Neumann accumulator (audio thread only)
        uint64_t vnWord = 0;
        int      vnCount = 0;
        int      vnPending = -1;   // -1 = no half-pair held
    };

    /** The process-wide pool. One per plugin binary is plenty - more
        instances just means more harvesters feeding the same reservoir. */
    EntropyPool& globalEntropy();
}
