/*  ----------------------------------------------------------------------
    DivergenceTest

    The whole point of the overhaul was: "this program makes each sound sound
    very similar ... they all evolve to sound almost the same". That is a
    measurable claim, so this measures it rather than asserting it.

    It runs several colonies offline, each with its own world seed, and reports:

      1. BETWEEN RUNS - how far apart the runs end up, both in what they sound
         like (measured descriptors) and in what they are (mean genome). If the
         engine still converges, these numbers collapse toward zero.

      2. WITHIN A RUN - how much each colony moves over its own lifetime. A run
         that is merely *different* from its neighbours but static in itself
         still fails the brief.

      3. THE NOISE FLOOR - the worst spectral flatness each run reached, and
         how long it spent noise-locked. The requirement is that a colony left
         entirely alone never parks in noise.

    Build:  cmake --build build --config Release --target MutagenDivergenceTest
    Run:    build/MutagenDivergenceTest_artefacts/Release/MutagenDivergenceTest.exe
    ---------------------------------------------------------------------- */

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../Source/Engine/Colony.h"
#include "../Source/Engine/Descriptors.h"
#include "../Source/Engine/Entropy.h"
#include "../Source/Engine/ScoreSystem.h"

#include <cstdio>
#include <functional>
#include <vector>
#include <cmath>

using namespace mutagen;

namespace
{
    constexpr double sr        = 44100.0;
    constexpr int    blockSize = 512;
    constexpr int    numRuns   = 6;
    constexpr double runLength = 45.0;    // seconds of colony life per run

    struct RunResult
    {
        uint64_t seed = 0;
        juce::String worldName;

        // what it ended up sounding like (mean of the last third)
        float flatness = 0, centroid = 0, roughness = 0, tonalness = 0;
        float rawFlatness = 0;
        float variety = 0, appeal = 0;

        // how much it moved over its own lifetime
        float centroidSpread = 0;
        float flatnessSpread = 0;

        // the noise requirement
        float worstFlatness = 0;
        double noiseLockedSeconds = 0;

        // what it became, genetically
        float meanGenome[numTraits] {};

        int  finalPopulation = 0;
        float coverage = 0;

        /*  The mean magnitude spectrum of the final third.

            Summary statistics turned out to be a poor test of "do these sound
            the same": two colonies can share a centroid and a flatness while
            being obviously different instruments, and - more importantly for
            this project - can differ in their genomes while landing on the
            same spectrum. The spectrum itself is the thing the ear compares,
            so compare that. */
        static constexpr int specBins = DescriptorAnalyser::spectrumBins;
        float spectrum[specBins] {};
    };

    /** L1 distance between two loudness-normalised mean spectra, 0..1.
        This is the number that actually answers the brief. */
    float spectrumDistance (const RunResult& a, const RunResult& b)
    {
        float sa = 0.0f, sb = 0.0f;
        for (int i = 0; i < RunResult::specBins; ++i) { sa += a.spectrum[i]; sb += b.spectrum[i]; }
        if (sa < 1.0e-9f || sb < 1.0e-9f) return 0.0f;

        float acc = 0.0f;
        for (int i = 0; i < RunResult::specBins; ++i)
            acc += std::fabs (a.spectrum[i] / sa - b.spectrum[i] / sb);

        // L1 between two normalised distributions is in [0, 2]; halve it so
        // 0 = identical, 1 = no overlap at all.
        return acc * 0.5f;
    }

    float descriptorDistance (const RunResult& a, const RunResult& b)
    {
        // Normalised euclidean distance over the audible descriptors.
        const float d[] = { a.flatness - b.flatness,
                            a.centroid - b.centroid,
                            a.roughness - b.roughness,
                            a.variety - b.variety,
                            a.appeal - b.appeal };
        float acc = 0.0f;
        for (float v : d) acc += v * v;
        return std::sqrt (acc / 5.0f);
    }

    float genomeDistance (const RunResult& a, const RunResult& b)
    {
        float acc = 0.0f;
        for (int i = 0; i < numTraits; ++i)
        {
            const float v = a.meanGenome[i] - b.meanGenome[i];
            acc += v * v;
        }
        return std::sqrt (acc / (float) numTraits);
    }

    RunResult runOne (uint64_t seed)
    {
        RunResult r;
        r.seed = seed;

        Colony colony;
        colony.setSeed (seed);
        colony.prepare (sr, blockSize, 56);
        colony.setWorld (WorldSeed::fromSeed (seed ^ 0xD1B54A32D192ED03ULL));
        r.worldName = juce::String (colony.getWorld().name);

        Environment env;              // stock settings, nobody touching anything
        colony.setEnvironment (env);
        colony.germinateFromSource (0.55f, 0.34f, 0.33f, 0.33f);

        juce::AudioBuffer<float> buf (2, blockSize);

        const int totalBlocks = (int) (runLength * sr / blockSize);
        const int tailStart   = totalBlocks * 2 / 3;

        // Trackers over the whole run, and over the final third separately.
        std::vector<float> centroidTrace, flatnessTrace;
        centroidTrace.reserve ((size_t) totalBlocks);
        flatnessTrace.reserve ((size_t) totalBlocks);

        double tailFlat = 0, tailRaw = 0, tailCent = 0, tailRough = 0, tailTonal = 0,
               tailVar = 0, tailAppeal = 0;
        int tailCount = 0;

        for (int b = 0; b < totalBlocks; ++b)
        {
            buf.clear();
            colony.process (buf, nullptr);

            const auto& d = colony.descriptors();

            centroidTrace.push_back (d.centroid);
            flatnessTrace.push_back (d.flatness);

            if (d.flatness > r.worstFlatness) r.worstFlatness = d.flatness;
            if (d.noiseLocked) r.noiseLockedSeconds += (double) blockSize / sr;

            if (b >= tailStart)
            {
                const float* spec = colony.spectrum();
                for (int k = 0; k < RunResult::specBins; ++k)
                    r.spectrum[k] += spec[k];

                tailFlat   += d.flatness;
                tailRaw    += d.rawFlatness;
                tailCent   += d.centroid;
                tailRough  += d.roughness;
                tailTonal  += d.tonalness;
                tailVar    += d.variety;
                tailAppeal += d.appeal;
                ++tailCount;
            }
        }

        if (tailCount > 0)
        {
            for (int k = 0; k < RunResult::specBins; ++k)
                r.spectrum[k] /= (float) tailCount;

            r.flatness  = (float) (tailFlat / tailCount);
            r.rawFlatness = (float) (tailRaw / tailCount);
            r.centroid  = (float) (tailCent / tailCount);
            r.roughness = (float) (tailRough / tailCount);
            r.tonalness = (float) (tailTonal / tailCount);
            r.variety   = (float) (tailVar / tailCount);
            r.appeal    = (float) (tailAppeal / tailCount);
        }

        // spread over the run's own lifetime
        auto spreadOf = [] (const std::vector<float>& v)
        {
            if (v.size() < 2) return 0.0f;
            double mean = 0.0;
            for (float x : v) mean += x;
            mean /= (double) v.size();
            double var = 0.0;
            for (float x : v) { const double d = x - mean; var += d * d; }
            return (float) std::sqrt (var / (double) v.size());
        };
        r.centroidSpread = spreadOf (centroidTrace);
        r.flatnessSpread = spreadOf (flatnessTrace);

        // what the colony became
        const auto organism = colony.captureOrganism();
        r.finalPopulation = organism.cellCount;
        r.coverage = colony.archiveCoverage();

        if (organism.cellCount > 0)
        {
            for (int i = 0; i < organism.cellCount; ++i)
                for (int t = 0; t < numTraits; ++t)
                    r.meanGenome[t] += organism.cells[i].genome.value[t];
            for (int t = 0; t < numTraits; ++t)
                r.meanGenome[t] /= (float) organism.cellCount;
        }

        return r;
    }
}

namespace
{
    /*  Run a known signal through the real analyser and report what it says.
        This is how the flatness calibration in Descriptors.cpp was chosen -
        the constants there are measurements, not guesses, and this pass
        re-checks them every time the test runs. */
    struct CalResult { float rawFlat = 0, rawPeri = 0, noisiness = 0, greyness = 0; bool locked = false; };

    CalResult calibrate (const char* label, const std::function<float (int, juce::Random&)>& gen)
    {
        DescriptorAnalyser an;
        an.prepare (sr);

        juce::AudioBuffer<float> buf (2, blockSize);
        juce::Random rnd (12345);

        int idx = 0;
        for (int b = 0; b < (int) (4.0 * sr / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = gen (idx++, rnd);
                buf.setSample (0, n, v);
                buf.setSample (1, n, v);
            }
            an.process (buf);
        }

        const auto& d = an.current();
        juce::ignoreUnused (idx);
        std::printf ("  %-18s rawFlat %.3f  rawPeri %.3f  -> noisiness %.3f  grey %.2f  %s\n",
                     label, d.rawFlatness, d.periodicity, d.flatness, d.greyness,
                     d.noiseLocked ? "NOISE-LOCKED" : "-");

        return { d.rawFlatness, d.periodicity, d.flatness, d.greyness, d.noiseLocked };
    }

    /*  How much of the flatness is the cells and how much is the crowd?

        A colony is a sum of voices, and summing many independent voices tends
        toward noise no matter how tonal each one is. Before blaming the per-
        cell DSP it is worth knowing which end the density is coming from, so
        this runs the same engine with a hard cap on the population and reports
        what each size measures. If one cell is tonal and thirty are not, the
        problem is aggregation and the fix is voicing, not synthesis. */
    void runDensitySweep()
    {
        std::printf ("--- 0b. DENSITY SWEEP: one engine, different population caps ---\n");
        std::printf ("  %-14s %8s %8s %10s %10s %9s\n",
                     "case", "rawFlat", "rawPeri", "noisiness", "roughness", "appeal");

        // Per species as well as per size: if one species is generating all
        // the broadband energy, capping the population will never fix it.
        struct Case { const char* label; int cap; float g, sp, rs; };
        const Case cases[] = {
            { "grain x1",      1,  1.0f, 0.0f, 0.0f },
            { "grain x4",      4,  1.0f, 0.0f, 0.0f },
            { "grain x8",      8,  1.0f, 0.0f, 0.0f },
            { "spectral x1",   1,  0.0f, 1.0f, 0.0f },
            { "spectral x4",   4,  0.0f, 1.0f, 0.0f },
            { "spectral x8",   8,  0.0f, 1.0f, 0.0f },
            // The resonator is excited by the other species, so it is silent
            // on its own - measuring it alone measures nothing. Pair it with a
            // little grain so there is something for it to ring on.
            { "reson+grain x8",8,  0.25f, 0.0f, 0.75f },
            { "mixed x2",      2,  0.34f, 0.33f, 0.33f },
            { "mixed x4",      4,  0.34f, 0.33f, 0.33f },
            { "mixed x8",      8,  0.34f, 0.33f, 0.33f },
            { "mixed x16",    16,  0.34f, 0.33f, 0.33f },
            { "mixed x32",    32,  0.34f, 0.33f, 0.33f },
        };

        for (const auto& kase : cases)
        {
            const int cap = kase.cap;
            Colony colony;
            colony.setSeed (0xC0FFEEULL);
            colony.prepare (sr, blockSize, 56);
            colony.setWorld (WorldSeed::fromSeed (0xBEEF1234ULL));

            Environment env;
            colony.setEnvironment (env);
            colony.germinateFromSource (1.0f, kase.g, kase.sp, kase.rs);
            colony.setActiveCap (cap);

            juce::AudioBuffer<float> buf (2, blockSize);
            const int blocks = (int) (12.0 * sr / blockSize);

            double aRaw = 0, aPeri = 0, aFlat = 0, aRough = 0, aAppeal = 0;
            int count = 0;

            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                colony.process (buf, nullptr);
                colony.setActiveCap (cap);          // hold it down against growth

                if (b > blocks / 2)
                {
                    const auto& d = colony.descriptors();
                    aRaw += d.rawFlatness; aPeri += d.periodicity; aFlat += d.flatness;
                    aRough += d.roughness; aAppeal += d.appeal;
                    ++count;
                }
            }

            if (count > 0)
                std::printf ("  %-14s %8.3f %8.3f %10.3f %10.3f %9.3f\n", kase.label,
                             aRaw / count, aPeri / count, aFlat / count,
                             aRough / count, aAppeal / count);
        }
        std::printf ("\n");
    }

    /*  The two ends of the scale, kept so the verdicts can be asserted on
        rather than eyeballed: a sound a listener would call musical must not
        be reported as noise, and one that genuinely is noise must be. */
    CalResult calVibrato, calWhite, calHarmonic;

    /*  ---- the fractal reward -------------------------------------------

        Requirement 24 asks for a reward that is brief, rare and earned. Two of
        those three are easy to get wrong in the same direction, and this one
        was: it gates on measured appeal, and appeal carries a tonalness term
        that was pinned near zero for every colony while the noisiness
        measurement saturated. The gate asked for 0.62 from an instrument that
        was reporting 0.52-0.59, so in practice it could never fire, and across
        two sessions nobody had ever seen it.

        "Rare" and "impossible" look the same from the outside, which is why
        this is measured rather than watched for. Four colonies run with a real
        ScoreSystem consuming their real snapshots, and the rewards are counted.
        Zero means the gate is unreachable again; one every few seconds means it
        has stopped being a reward.

        The simulated player clicks about every two seconds, because the reward
        is deliberately gated on the score *rate* as well as on the sound, and
        the rate carries the combo multiplier. A colony nobody is touching sits
        at roughly 0.44 of the base rate against a gate of 0.55, so it cannot
        earn the reward however good it sounds - which is the intended shape of
        the thing ("you are doing well" is about playing, not about watching),
        but it does mean the passive runs above will always report zero and an
        idle window will never show one. That is worth stating rather than
        leaving as a surprise.                                                 */
    void runRewardCheck (int& outCount, double& outMinutes)
    {
        std::printf ("--- 0c. THE FRACTAL REWARD: reachable, and still rare? ---\n");
        std::printf ("  %-16s %8s %8s %8s %9s %8s\n",
                     "world", "rewards", "appeal", "variety", "rate/base", "minutes");

        outCount = 0;
        outMinutes = 0.0;

        const double rewardRun = 150.0;         // long enough for the 30-70 s cooldown

        for (int i = 0; i < 4; ++i)
        {
            const uint64_t seed = 0xA24BAED4963EE407ULL * (uint64_t) (i + 1) + 0x5150ULL;

            Colony colony;
            colony.setSeed (seed);
            colony.prepare (sr, blockSize, 56);
            colony.setWorld (WorldSeed::fromSeed (seed ^ 0x9E3779B97F4A7C15ULL));
            Environment env;
            colony.setEnvironment (env);
            colony.germinateFromSource (0.55f, 0.34f, 0.33f, 0.33f);

            ScoreSystem score;
            EngineSnapshot snap;
            juce::AudioBuffer<float> buf (2, blockSize);

            const int blocks = (int) (rewardRun * sr / blockSize);
            const double dt = (double) blockSize / sr;

            int rewards = 0;
            double appealSum = 0, varietySum = 0, rateSum = 0;
            int n = 0;
            double sinceClick = 0.0;

            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                colony.process (buf, nullptr);
                colony.writeSnapshot (snap);
                score.update (snap, dt);
                if (score.consumeRewardFlash()) ++rewards;

                // A player mutating the colony every couple of seconds: enough
                // to hold a combo, not enough to be thrashing it.
                sinceClick += dt;
                if (sinceClick >= 2.0)
                {
                    sinceClick = 0.0;
                    score.registerInteraction (0.6f);
                }

                appealSum += snap.appeal;
                varietySum += snap.variety;
                rateSum += score.rate();
                ++n;
            }

            outCount += rewards;
            outMinutes += rewardRun / 60.0;

            std::printf ("  %-16s %8d %8.3f %8.3f %9.3f %8.1f\n",
                         colony.getWorld().name, rewards,
                         n > 0 ? appealSum / n : 0.0,
                         n > 0 ? varietySum / n : 0.0,
                         n > 0 ? rateSum / n / 140.0 : 0.0,
                         rewardRun / 60.0);
        }
        std::printf ("\n");
    }

    void runCalibration()
    {
        std::printf ("--- 0. CALIBRATION: known signals through the real analyser ---\n");

        const double twoPi = 6.283185307179586;

        calibrate ("sine 220", [twoPi] (int i, juce::Random&)
        {
            return 0.5f * (float) std::sin (twoPi * 220.0 * (double) i / sr);
        });

        calHarmonic = calibrate ("harmonic tone", [twoPi] (int i, juce::Random&)
        {
            double v = 0.0;
            for (int h = 1; h <= 8; ++h)
                v += std::sin (twoPi * 220.0 * h * (double) i / sr) / (double) h;
            return 0.35f * (float) v;
        });

        calibrate ("detuned saws", [twoPi] (int i, juce::Random&)
        {
            double v = 0.0;
            for (int h = 1; h <= 14; ++h)
            {
                v += std::sin (twoPi * 147.0 * h * (double) i / sr) / (double) h;
                v += std::sin (twoPi * 151.5 * h * (double) i / sr) / (double) h;
            }
            return 0.18f * (float) v;
        });

        /*  The case that broke the old measurement, and the reason the
            periodicity term exists: the same harmonic tone with a 5 Hz,
            +/-300 cent vibrato on it. Musically this is an ordinary sound - a
            singer, a theremin, any string player's left hand - and it is
            roughly what every cell in this synth is doing all the time.
            Whitened flatness alone scores it as indistinguishable from noise.
            It has to land near the harmonic tone, not near the pink. */
        calVibrato = calibrate ("vibrato tone", [twoPi] (int i, juce::Random&)
        {
            static double phase = 0.0;
            if (i == 0) phase = 0.0;
            const double t  = (double) i / sr;
            const double f0 = 220.0 * std::pow (2.0, 0.25 * std::sin (twoPi * 5.0 * t));
            phase += twoPi * f0 / sr;           // integrate, so the phase stays continuous
            double v = 0.0;
            for (int h = 1; h <= 8; ++h)
                v += std::sin (phase * (double) h) / (double) h;
            return 0.35f * (float) v;
        });

        calibrate ("pink noise", [] (int, juce::Random& r)
        {
            // Voss-McCartney style: octave-spaced sources summed
            static float rows[8] = {};
            static int counter = 0;
            ++counter;
            for (int b = 0; b < 8; ++b)
                if ((counter & ((1 << b) - 1)) == 0)
                    rows[b] = r.nextFloat() * 2.0f - 1.0f;
            float v = 0.0f;
            for (float x : rows) v += x;
            return v * 0.12f;
        });

        calWhite = calibrate ("white noise", [] (int, juce::Random& r)
        {
            return (r.nextFloat() * 2.0f - 1.0f) * 0.5f;
        });

        std::printf ("\n");
    }
}

int main()
{
    std::printf ("MUTAGEN divergence test\n");
    std::printf ("%d colonies, %.0f seconds each, stock settings, no user input.\n\n",
                 numRuns, runLength);

    runCalibration();
    runDensitySweep();

    int rewardCount = 0;
    double rewardMinutes = 0.0;
    runRewardCheck (rewardCount, rewardMinutes);

    std::vector<RunResult> results;
    results.reserve (numRuns);

    for (int i = 0; i < numRuns; ++i)
    {
        const uint64_t seed = 0x9E3779B97F4A7C15ULL * (uint64_t) (i + 1) + 0x1234ABCDULL;
        std::printf ("  running %d/%d ...", i + 1, numRuns);
        std::fflush (stdout);
        results.push_back (runOne (seed));
        std::printf (" %s\n", results.back().worldName.toRawUTF8());
    }

    // ------------------------------------------------------------------
    std::printf ("\n--- 1. WHAT EACH RUN BECAME (mean of final third) ---\n");
    std::printf ("%-16s %6s %7s %8s %9s %8s %7s %5s %5s\n",
                 "world", "raw", "flat", "centroid", "roughness", "variety", "appeal",
                 "pop", "map%");
    for (const auto& r : results)
        std::printf ("%-16s %6.3f %7.3f %8.3f %9.3f %8.3f %7.3f %5d %5.0f\n",
                     r.worldName.toRawUTF8(), r.rawFlatness, r.flatness, r.centroid,
                     r.roughness, r.variety, r.appeal, r.finalPopulation, r.coverage * 100.0f);

    // ------------------------------------------------------------------
    std::printf ("\n--- 2. BETWEEN RUNS: are they different from each other? ---\n");
    float minD = 1.0e9f, maxD = 0.0f, sumD = 0.0f;
    float minG = 1.0e9f, maxG = 0.0f, sumG = 0.0f;
    float minS = 1.0e9f, maxS = 0.0f, sumS = 0.0f;
    int pairs = 0;

    for (size_t i = 0; i < results.size(); ++i)
        for (size_t j = i + 1; j < results.size(); ++j)
        {
            const float d = descriptorDistance (results[i], results[j]);
            const float gd = genomeDistance (results[i], results[j]);
            const float sd = spectrumDistance (results[i], results[j]);
            minD = juce::jmin (minD, d); maxD = juce::jmax (maxD, d); sumD += d;
            minG = juce::jmin (minG, gd); maxG = juce::jmax (maxG, gd); sumG += gd;
            minS = juce::jmin (minS, sd); maxS = juce::jmax (maxS, sd); sumS += sd;
            ++pairs;
        }

    if (pairs > 0)
    {
        std::printf ("  SPECTRUM distance min %.4f   mean %.4f   max %.4f   <- the ear's test\n",
                     minS, sumS / (float) pairs, maxS);
        std::printf ("  descriptor dist.  min %.4f   mean %.4f   max %.4f\n",
                     minD, sumD / (float) pairs, maxD);
        std::printf ("  genome distance   min %.4f   mean %.4f   max %.4f\n",
                     minG, sumG / (float) pairs, maxG);
        std::printf ("  (near zero would mean the runs converged on one sound)\n");
    }

    // ------------------------------------------------------------------
    std::printf ("\n--- 3. WITHIN A RUN: does it keep moving? ---\n");
    std::printf ("%-16s %14s %14s\n", "world", "centroid sd", "flatness sd");
    float worstSpread = 1.0e9f;
    for (const auto& r : results)
    {
        std::printf ("%-16s %14.4f %14.4f\n",
                     r.worldName.toRawUTF8(), r.centroidSpread, r.flatnessSpread);
        worstSpread = juce::jmin (worstSpread, r.centroidSpread);
    }
    std::printf ("  (a static drone would sit near 0.000)\n");

    // ------------------------------------------------------------------
    std::printf ("\n--- 4. THE NOISE FLOOR: left alone, does it park in noise? ---\n");
    std::printf ("%-16s %14s %20s\n", "world", "worst flatness", "seconds noise-locked");
    double worstLocked = 0.0;
    float worstFlat = 0.0f;
    for (const auto& r : results)
    {
        std::printf ("%-16s %14.3f %20.1f\n",
                     r.worldName.toRawUTF8(), r.worstFlatness, r.noiseLockedSeconds);
        worstLocked = juce::jmax (worstLocked, r.noiseLockedSeconds);
        worstFlat = juce::jmax (worstFlat, r.worstFlatness);
    }

    // ------------------------------------------------------------------
    std::printf ("\n--- VERDICT ---\n");
    const float meanD = pairs > 0 ? sumD / (float) pairs : 0.0f;
    const float meanG = pairs > 0 ? sumG / (float) pairs : 0.0f;

    bool ok = true;
    auto check = [&ok] (bool pass, const char* label, const char* detail)
    {
        std::printf ("  [%s] %-34s %s\n", pass ? "PASS" : "FAIL", label, detail);
        if (! pass) ok = false;
    };

    char buf[128];

    const float meanS = pairs > 0 ? sumS / (float) pairs : 0.0f;
    std::snprintf (buf, sizeof (buf), "mean spectrum distance %.4f (want > 0.18)", meanS);
    check (meanS > 0.18f, "runs SOUND different", buf);

    std::snprintf (buf, sizeof (buf), "worst pair %.4f (want > 0.08)", minS);
    check (minS > 0.08f, "even the closest pair differs", buf);

    std::snprintf (buf, sizeof (buf), "mean descriptor distance %.4f (want > 0.04)", meanD);
    check (meanD > 0.04f, "summary descriptors differ", buf);

    std::snprintf (buf, sizeof (buf), "mean genome distance %.4f (want > 0.08)", meanG);
    check (meanG > 0.08f, "runs are genetically different", buf);

    std::snprintf (buf, sizeof (buf), "quietest run sd %.4f (want > 0.01)", worstSpread);
    check (worstSpread > 0.01f, "every run keeps moving", buf);

    std::snprintf (buf, sizeof (buf), "worst run locked %.1fs of %.0fs", worstLocked, runLength);
    check (worstLocked < runLength * 0.25, "no run parks in noise", buf);

    /*  Positive and negative controls on the noise detector itself.

        "No run parks in noise" is only worth anything if the detector can
        still recognise noise when it hears it. A metric that returns zero for
        everything passes that check perfectly and is useless, which is very
        nearly what the previous one did in reverse - it returned 1.000 for
        everything, and the noise verdict had to be propped up with appeal and
        roughness terms to stop it locking every run permanently.

        So both ends are asserted. White noise must lock. A vibrato'd harmonic
        tone - an ordinary musical sound, and close to what every cell in this
        synth does continuously - must not, and must not go grey either. */
    std::snprintf (buf, sizeof (buf), "white noise noisiness %.3f, %s",
                   calWhite.noisiness, calWhite.locked ? "locked" : "NOT locked");
    check (calWhite.locked && calWhite.noisiness > 0.7f,
           "noise is still detected as noise", buf);

    std::snprintf (buf, sizeof (buf), "vibrato tone noisiness %.3f, grey %.2f",
                   calVibrato.noisiness, calVibrato.greyness);
    check (! calVibrato.locked && calVibrato.noisiness < 0.35f && calVibrato.greyness < 0.2f,
           "a wobbling note is not called noise", buf);

    std::snprintf (buf, sizeof (buf), "harmonic %.3f vs white %.3f (want a gap > 0.5)",
                   calHarmonic.noisiness, calWhite.noisiness);
    check (calWhite.noisiness - calHarmonic.noisiness > 0.5f,
           "the scale has usable range", buf);

    const double perMin = rewardMinutes > 0.0 ? rewardCount / rewardMinutes : 0.0;
    std::snprintf (buf, sizeof (buf), "%d in %.0f min = %.2f/min (want 0.1 .. 2.0)",
                   rewardCount, rewardMinutes, perMin);
    check (perMin >= 0.1 && perMin <= 2.0, "fractal reward is earnable but rare", buf);

    std::printf ("\n%s\n", ok ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
