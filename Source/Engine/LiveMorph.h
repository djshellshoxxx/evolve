// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// LiveMorph: a live effect that re-shapes the INCOMING audio from what the input
// sounds like and what is happening in the colony.
//
// The input is written into a ~2 s stereo ring. A granular recomposer reads it
// back strictly behind the write head, so the effect has zero latency. Scale
// tuned feedback combs ring the input in the run's key, a tilt tone stage and an
// onset-triggered stutter shape the result, and colony events add short colourings
// that fade out over about six seconds.
//
// Everything runs per sample: input analysis, control smoothing and random draws,
// so the output does not depend on the block size. process() does no heap
// allocation, locking or exception throwing. setState() and process() are expected
// to be called from the same (audio) thread, between blocks.
//
// Pure standard C++ so the test target can exercise it without JUCE.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mutagen
{
    struct MorphState
    {
        float variety = 0.5f;
        float appeal = 0.5f;
        float greyness = 0.0f;   // noisiness; above 0.85 the ring freezes
        float roughness = 0.15f;
        float centroid = 0.35f;
        float tonalness = 0.75f;
        float heat = 0.25f;
        float pressure = 0.0f;
        float infection = 0.0f;
        float score01 = 0.0f;
        float eventEnergy = 0.0f;
        int eventKind = 0;       // 0 none, 1 enzyme, 2 catalyst, 3 radiate, 4 heat, 5 chanceSparkle, 6 factDemo, 7 twist, 8 frozenNoise
        float eventAge = 99.0f;  // seconds since the event
        std::array<float, 3> species { 0.34f, 0.33f, 0.33f };
        std::uint16_t scaleMask = 0x0ab5; // 12-bit pitch-class mask of the run's scale, bit0 = root
        int rootPc = 0;
        float bpm = 0.0f;
        float beatPhase = 0.0f;
        std::uint32_t seed = 1;
    };

    class LiveMorph
    {
    public:
        LiveMorph();

        // Allocates the ring and comb buffers. Not for the audio thread.
        void prepare (double sampleRate, int maxBlock);

        // Clears the ring, grains, filters and clock. The state is kept.
        void reset();

        // Cheap. Values are clamped to their legal ranges and changes are smoothed.
        void setState (const MorphState&);

        // WET signal only. outL/outR must not alias inL/inR.
        void process (const float* inL, const float* inR, float* outL, float* outR, int n);

        float inputLevel() const;      // linear RMS of the input at the end of the last block
        float inputBrightness() const; // 0..1 first-difference energy ratio at the end of the last block
        bool onsetThisBlock() const;   // an onset fired during the last process() call

    private:
        static constexpr int kCtl = 12;       // smoothed controls
        static constexpr int kGrains = 12;    // main grain voices (4..12 active)
        static constexpr int kSparkles = 4;
        static constexpr int kTwists = 2;
        static constexpr int kCombs = 12;     // one per pitch class of the scale mask
        static constexpr int kEvents = 9;     // indexed by eventKind
        static constexpr int kHannSize = 1024;

        struct Grain
        {
            bool active = false;
            double pos = 0.0;     // absolute read position in ring samples
            double inc = 1.0;     // signed step per sample (negative = reversed)
            float phase = 0.0f;   // samples elapsed
            float len = 1.0f;     // samples
            float amp = 0.0f;
            float gl = 0.0f;
            float gr = 0.0f;
        };

        struct Comb
        {
            double d = 500.0;
            double dTarget = 500.0;
            float amp = 0.0f;
            float ampTarget = 0.0f;
            float lp = 0.0f;
        };

        struct Shifter
        {
            double d1 = 0.0;
            double d2 = 0.0;
            float w1 = 0.0f;
            float w2 = 0.0f;
        };

        void applyState (const MorphState&);
        float uniform();
        float hannAt (float x) const;
        float window (float x) const;
        float readRing (const std::vector<float>& buf, double pos) const;
        void stepShifter (Shifter& s, double ratio) const;
        float readShifter (const std::vector<float>& buf, const Shifter& s) const;
        int mainTarget() const;
        void spawnMainGrain (Grain& g);
        void spawnSparkle (Grain& g, float strength);
        void spawnTwist (Grain& g, float strength);
        void renderGrain (Grain& g, float& l, float& r);

        MorphState st_;
        double sr_ = 48000.0;
        int maxBlock_ = 512;
        bool prepared_ = false;

        std::array<float, kCtl> tgt_ {};
        std::array<float, kCtl> cur_ {};
        std::array<float, kEvents> ev_ {};
        int kind_ = 0;
        float evStrength_ = 0.35f;
        double ageBase_ = 99.0;
        double elapsed_ = 0.0;
        float bpm_ = 0.0f;
        float vibHz_ = 6.0f;

        bool seedInit_ = false;
        std::uint32_t seedApplied_ = 0;
        std::uint64_t rng_ = 1;

        std::vector<float> ringL_, ringR_;
        std::uint64_t ringMask_ = 0;
        std::uint64_t wAbs_ = 0;         // samples written into the ring
        std::uint64_t warmSamples_ = 0;  // freeze only after this many written samples
        std::array<float, kHannSize + 1> hann_ {};

        std::vector<float> combBuf_;
        std::size_t combSize_ = 1;
        std::size_t combMask_ = 0;
        std::uint64_t combW_ = 0;
        std::array<Comb, kCombs> combs_ {};

        std::array<Grain, kGrains> grains_ {};
        std::array<Grain, kSparkles> sparkles_ {};
        std::array<Grain, kTwists> twists_ {};
        double spawnTimer_ = 0.0;
        double twistTimer_ = 0.0;

        std::array<Shifter, 2> shifters_ {};   // 0 = enzyme shimmer, 1 = catalyst vibrato
        double shMin_ = 240.0;
        double shWin_ = 2400.0;
        double vibPhase_ = 0.0;

        float powFast_ = 0.0f, powSlow_ = 0.0f, powRms_ = 0.0f, powLong_ = 0.0f, powWet_ = 0.0f;
        float mPrev_ = 0.0f, dEma_ = 0.0f, eEma_ = 0.0f;
        bool onsetArmed_ = false;
        int refractory_ = 0;
        float level_ = 0.0f;
        float bright_ = 0.0f;
        bool onsetBlock_ = false;

        float tlp1L_ = 0.0f, tlp1R_ = 0.0f, tlp2L_ = 0.0f, tlp2R_ = 0.0f;
        int holdCount_ = 0;
        float holdL_ = 0.0f, holdR_ = 0.0f, clickVal_ = 0.0f;
        float stEnv_ = 0.0f;
        int stutLen_ = 3528;
        float compGain_ = 1.0f;
        float fLpL_ = 0.0f, fLpR_ = 0.0f, closeCur_ = 0.0f;

        float aCtl_ = 0.0f, aFast_ = 0.0f, aSlow_ = 0.0f, aRms_ = 0.0f, aLong_ = 0.0f;
        float aBright_ = 0.0f, aComp_ = 0.0f, aClose_ = 0.0f, aFz_ = 0.0f;
        float aT1_ = 0.0f, aT2_ = 0.0f, clickDecay_ = 0.0f, stDecay_ = 0.0f;
    };
}
