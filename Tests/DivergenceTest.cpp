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
    void calibrate (const char* label, const std::function<float (int, juce::Random&)>& gen)
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
        std::printf ("  %-18s raw %.3f -> flatness %.3f   centroid %.3f   roughness %.3f\n",
                     label, d.rawFlatness, d.flatness, d.centroid, d.roughness);
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
        std::printf ("  %-14s %8s %10s %10s %9s\n", "case", "raw", "flatness", "roughness", "appeal");

        // Per species as well as per size: if one species is generating all
        // the broadband energy, capping the population will never fix it.
        struct Case { const char* label; int cap; float g, sp, rs; };
        const Case cases[] = {
            { "grain x8",      8,  1.0f, 0.0f, 0.0f },
            { "spectral x8",   8,  0.0f, 1.0f, 0.0f },
            { "resonator x8",  8,  0.0f, 0.0f, 1.0f },
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

            double aRaw = 0, aFlat = 0, aRough = 0, aAppeal = 0;
            int count = 0;

            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                colony.process (buf, nullptr);
                colony.setActiveCap (cap);          // hold it down against growth

                if (b > blocks / 2)
                {
                    const auto& d = colony.descriptors();
                    aRaw += d.rawFlatness; aFlat += d.flatness;
                    aRough += d.roughness; aAppeal += d.appeal;
                    ++count;
                }
            }

            if (count > 0)
                std::printf ("  %-14s %8.3f %10.3f %10.3f %9.3f\n", kase.label,
                             aRaw / count, aFlat / count, aRough / count, aAppeal / count);
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

        calibrate ("harmonic tone", [twoPi] (int i, juce::Random&)
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

        calibrate ("white noise", [] (int, juce::Random& r)
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

    std::printf ("\n%s\n", ok ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
