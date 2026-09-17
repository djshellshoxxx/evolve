#include "Descriptors.h"
#include <cmath>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    DescriptorAnalyser::DescriptorAnalyser()
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                         * (float) i / (float) (fftSize - 1));

        /*  The analysis window's own autocorrelation.

            Every lag in the autocorrelation of a windowed frame is scaled by
            how much the window overlaps itself at that lag, which falls away
            steadily with lag. Without dividing it out, a low note is reported
            as less periodic than a high one purely because its period is
            longer, and the periodicity floor would be a pitch detector.

            O(n^2) once, at construction. */
        double w0 = 0.0;
        for (int i = 0; i < fftSize; ++i) w0 += (double) window[(size_t) i] * window[(size_t) i];

        for (int lag = 0; lag < fftSize; ++lag)
        {
            double acc = 0.0;
            for (int i = 0; i + lag < fftSize; ++i)
                acc += (double) window[(size_t) i] * window[(size_t) (i + lag)];
            winAcf[(size_t) lag] = w0 > 0.0 ? (float) (acc / w0) : 1.0f;
        }
    }

    void DescriptorAnalyser::prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        frameSeconds = (float) ((double) hopSize / sr);
        reset();
    }

    void DescriptorAnalyser::reset()
    {
        ring.fill (0.0f);
        mag.fill (0.0f);
        prevMag.fill (0.0f);
        acfBuffer.fill (0.0f);
        magNorm.fill (0.0f);
        ringPos = 0;
        sinceHop = 0;
        primed = false;
        numPeaks = 0;
        desc = AudioDescriptors {};
        centroidMean = 0.35f; centroidVar = 0.0f;
        flatMean = 0.25f; fluxMean = 0.0f;
        noiseSeconds = quietSeconds = 0.0f;
    }

    // -----------------------------------------------------------------------

    void DescriptorAnalyser::process (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples();
        if (n <= 0 || buffer.getNumChannels() <= 0) return;

        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;

        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = 0.5f * (l[i] + r[i]);
            ringPos = (ringPos + 1) % fftSize;

            if (++sinceHop >= hopSize)
            {
                sinceHop = 0;
                primed = true;
                analyseFrame();
            }
        }
    }

    // -----------------------------------------------------------------------

    void DescriptorAnalyser::analyseFrame() noexcept
    {
        // Copy the ring out in order, windowed.
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (ringPos + i) % fftSize;
            fftBuffer[(size_t) i] = ring[(size_t) idx] * window[(size_t) i];
        }
        for (int i = fftSize; i < fftSize * 2; ++i) fftBuffer[(size_t) i] = 0.0f;

        fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

        // --- magnitudes, skipping DC and the lowest couple of bins ----------
        constexpr int firstBin = 3;
        float sumMag = 0.0f;
        float weighted = 0.0f;
        float peak = 0.0f;
        int   counted = 0;

        for (int k = firstBin; k < numBins; ++k)
        {
            const float m = fftBuffer[(size_t) k];
            mag[(size_t) k] = m;
            sumMag += m;
            weighted += m * (float) k;
            if (m > peak) peak = m;
            ++counted;
        }

        if (counted <= 0 || sumMag <= 1.0e-9f)
        {
            // Silence: decay everything toward neutral rather than reporting
            // "perfectly tonal", which would be a lie the score could farm.
            desc.rms *= 0.9f;
            desc.flux *= 0.8f;
            quietSeconds += frameSeconds;
            desc.stuck = quietSeconds > 3.0f;
            return;
        }

        /*  ---- spectral flatness on a whitened spectrum --------------------

            Plain Wiener entropy over the raw spectrum is as much a measure of
            spectral *slope* as of noisiness: tilt a perfectly tonal signal
            bright and its geometric-to-arithmetic ratio rises, because the
            energy is now spread more evenly across the range. That matters
            here because every world applies its own tilt, so a bright world
            was being reported as noisy and having its score frozen while it
            was still perfectly tonal.

            Splitting into bands and averaging was worse - within a wide
            high-frequency band a bright sawtooth has many partials and looks
            flat, so ordinary musical material saturated at 1.0.

            What actually works is whitening. Divide the spectrum by its own
            smoothed envelope, computed over a window that widens with
            frequency, and measure flatness on what is left. A tilt scales the
            spectrum and its envelope identically, so it cancels exactly; what
            survives is how far the spectrum departs from its own local mean,
            which is precisely "is the energy in partials or smeared". Peaky
            residual means tonal, flat residual means noise.

            The prefix sum makes the variable-width moving average O(n).      */
        {
            prefix[0] = 0.0f;
            for (int k = 0; k < numBins; ++k)
                prefix[(size_t) (k + 1)] = prefix[(size_t) k] + mag[(size_t) k];
        }

        float logSum = 0.0f, linSum = 0.0f;
        int   whitenedCount = 0;

        for (int k = firstBin; k < numBins; ++k)
        {
            /*  The window has to be comfortably wider than a partial, or the
                envelope simply tracks the partial and the residual flattens -
                which is what made every dense colony read as pure noise. A
                Hann-windowed partial is about three bins across, so the floor
                here is ten, and it widens with frequency to stay roughly
                constant-Q. */
            const int w = juce::jlimit (10, 96, k / 4 + 10);
            const int k0 = juce::jmax (0, k - w);
            const int k1 = juce::jmin (numBins, k + w + 1);
            const float envelope = (prefix[(size_t) k1] - prefix[(size_t) k0])
                                 / (float) (k1 - k0);

            const float ratio = mag[(size_t) k] / (envelope + 1.0e-9f);
            logSum += std::log (ratio + 1.0e-6f);
            linSum += ratio;
            ++whitenedCount;
        }

        float rawFlat = 0.0f;
        if (whitenedCount > 0)
        {
            const float geo = std::exp (logSum / (float) whitenedCount);
            const float arith = linSum / (float) whitenedCount;
            rawFlat = clamp01 (geo / (arith + 1.0e-12f));
        }

        /*  ---- periodicity, the measurement that survives vibrato ----------

            Whitened flatness answers "is the energy in partials or smeared
            between them", and on this instrument that turned out to be the
            wrong question. Every cell is deliberately frequency-modulated -
            six LFO lanes per cell, from about 0.003 Hz to 26 Hz, which is the
            point of the whole thing - and a partial carrying a few hundred
            cents of vibrato sweeps across a dozen bins inside one 46 ms frame.
            Integrated over the frame it deposits its energy evenly across that
            span, which to any single-frame spectral statistic is exactly what
            broadband noise looks like.

            That is not a small effect and it is not the crowd. The offline
            density sweep was extended down to a single cell to find out; with
            the modulation bank muted and nothing else changed, one spectral
            cell moved from 0.613 to 0.432 and a two-cell colony from 0.591 to
            0.305. Six untouched runs were all pinning the flatness at 1.000,
            which left the visuals permanently 63% grey and the score one bad
            frame away from freezing, on colonies that were perfectly musical.

            A longer window resolves partials better but smears modulation
            worse, and a shorter one does the reverse, so no choice of window
            fixes it. What does fix it is asking a question modulation does not
            disturb: is the waveform *repeating*. A vibrato'd note is still
            locally periodic; noise is not periodic at any lag. The
            autocorrelation is the inverse transform of the power spectrum, so
            it costs one more transform of a buffer we already have.          */
        float rawPeri = 0.0f;
        {
            // Power spectrum, mirrored into the full complex layout the
            // real-only inverse transform expects. mag[0..2] are never written
            // (see firstBin), so DC and the lowest bins contribute nothing -
            // which is what we want: a DC offset is not a period.
            for (int k = 0; k < fftSize; ++k)
            {
                const int kk = k < numBins ? k : fftSize - k;
                const float m = kk < numBins ? mag[(size_t) kk] : 0.0f;
                acfBuffer[(size_t) (2 * k)]     = m * m;
                acfBuffer[(size_t) (2 * k + 1)] = 0.0f;
            }

            fft.performRealOnlyInverseTransform (acfBuffer.data());

            const float zero = acfBuffer[0];
            if (zero > 1.0e-12f)
            {
                float best = 0.0f;
                for (int lag = minLag; lag < maxLag; ++lag)
                {
                    // Divide out the window's self-overlap, with a floor so
                    // the longest lags - where the overlap is small and the
                    // estimate is noisy - cannot be amplified without limit.
                    const float taper = juce::jmax (0.08f, winAcf[(size_t) lag]);
                    const float v = acfBuffer[(size_t) lag] / (zero * taper);
                    if (v > best) best = v;
                }
                rawPeri = clamp01 (best);
            }
        }

        /*  Calibration.

            Measured by the calibration pass in Tests/DivergenceTest, which
            runs known signals through this exact code path. The constants
            below are read off those measurements rather than guessed, and the
            test re-prints them on every run so drift is visible.

            Both views are kept. Flatness is the sensitive one and catches a
            sound that has genuinely smeared; periodicity is the robust one and
            is the only one of the two that can tell a wobbling note from hiss.
            They are combined by weighting periodicity the more heavily,
            because flatness's failure here is a false *positive* - it calls
            healthy material noise - and that is the error that breaks the
            game.                                                             */
        const float flatNoise = clamp01 ((rawFlat - flatCalLow) / (flatCalHigh - flatCalLow));
        const float periNoise = clamp01 (1.0f - (rawPeri - periCalLow) / (periCalHigh - periCalLow));
        const float flat = clamp01 (0.32f * flatNoise + 0.68f * periNoise);

        desc.rawFlatness  = desc.rawFlatness * 0.8f + rawFlat * 0.2f;
        desc.periodicity  = desc.periodicity * 0.8f + rawPeri * 0.2f;

        // ---- centroid ------------------------------------------------------
        const float centroidBin = weighted / sumMag;
        // log-normalised, because pitch perception is logarithmic
        const float centHz = centroidBin * (float) sr / (float) fftSize;
        const float cent = clamp01 (std::log2 (juce::jmax (20.0f, centHz) / 20.0f) / 10.0f);

        // ---- flux ----------------------------------------------------------
        float flux = 0.0f;
        const float norm = 1.0f / (sumMag + 1.0e-9f);
        for (int k = firstBin; k < numBins; ++k)
        {
            const float d = mag[(size_t) k] * norm - prevMag[(size_t) k];
            if (d > 0.0f) flux += d;              // positive-only: onsets, not decays
            prevMag[(size_t) k] = mag[(size_t) k] * norm;
        }
        flux = clamp01 (flux * 6.0f);

        // ---- level ---------------------------------------------------------
        float energy = 0.0f;
        for (int i = 0; i < fftSize; ++i) energy += ring[(size_t) i] * ring[(size_t) i];
        const float rms = std::sqrt (energy / (float) fftSize);
        const float crest = rms > 1.0e-6f ? clamp01 (peak / (sumMag + 1.0e-9f) * 8.0f) : 0.0f;

        // ---- peaks + roughness ---------------------------------------------
        const float rough = computeRoughness();

        // ---- smoothing -------------------------------------------------------
        auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };
        const float fast = 0.25f, slow = 0.02f;

        desc.flatness  = lerp (desc.flatness, flat, fast);
        desc.centroid  = lerp (desc.centroid, cent, fast);
        desc.flux      = lerp (desc.flux, flux, 0.35f);
        desc.roughness = lerp (desc.roughness, rough, 0.12f);
        desc.tonalness = 1.0f - desc.flatness;
        desc.rms       = lerp (desc.rms, rms, 0.2f);
        desc.crest     = lerp (desc.crest, crest, 0.1f);

        // ---- long-window variety --------------------------------------------
        // Variance of the centroid over a long window is a good proxy for "is
        // this sound going anywhere". A drone has a steady centroid; something
        // evolving moves it around.
        centroidMean += (desc.centroid - centroidMean) * slow;
        const float dev = desc.centroid - centroidMean;
        centroidVar += (dev * dev - centroidVar) * slow;
        flatMean += (desc.flatness - flatMean) * slow;
        fluxMean += (desc.flux - fluxMean) * slow;

        const float spread = clamp01 (std::sqrt (centroidVar) * 7.0f);
        desc.variety = clamp01 (0.55f * spread + 0.45f * clamp01 (fluxMean * 3.0f));
        desc.stasis  = clamp01 (1.0f - desc.variety * 1.4f);

        // ---- appeal ----------------------------------------------------------
        // Roughness dominates perceived dissonance, tonalness separates a note
        // from a hiss, and a mid centroid avoids both mud and shrillness.
        const float comfort = std::exp (-std::pow ((desc.centroid - 0.38f) / 0.26f, 2.0f));
        desc.appeal = clamp01 (0.45f * (1.0f - desc.roughness)
                             + 0.32f * desc.tonalness
                             + 0.23f * comfort);

        /*  ---- verdicts ---------------------------------------------------

            This used to be a composite in which flatness was deliberately only
            half the weight, propped up by appeal and roughness. That was not a
            design choice, it was a workaround: flatness alone read 0.80-1.00
            for healthy colonies *and* for dead ones, so thresholding it either
            locked every run permanently or never locked at all, and the other
            two terms were carrying the decision.

            With the periodicity term in the noisiness measurement that is no
            longer true. Measured over the same six untouched runs, healthy
            colonies now sit at 0.29-0.32 and their worst frame reaches 0.43,
            while pink noise reads 0.75 and white noise 0.92. The measurement
            can answer the question on its own, so it does, and appeal and
            roughness are demoted to the small corrective role they should
            always have had - a sound can be tonal and still unbearable.

            The threshold sits at 0.66, which locks both pink and white noise
            and leaves better than a factor of two of headroom above where a
            working colony lives.                                             */
        const float noiseScore = 0.80f * desc.flatness
                               + 0.14f * (1.0f - desc.appeal)
                               + 0.06f * desc.roughness;

        if (noiseScore > 0.66f && desc.rms > 1.0e-4f) noiseSeconds += frameSeconds;
        else                                          noiseSeconds -= frameSeconds * 1.6f;
        noiseSeconds = juce::jlimit (0.0f, 6.0f, noiseSeconds);

        // Hysteretic on purpose: a momentary burst of noise is musical. It is
        // sustained noise that stops the score, and the colour starts draining
        // well before that so there is warning.
        if (desc.noiseLocked) desc.noiseLocked = noiseScore > 0.60f && noiseSeconds > 0.3f;
        else                  desc.noiseLocked = noiseSeconds > 2.2f;

        // Colour drains across the range the colony actually occupies. The
        // ramp starts at 0.42 - above where a working colony sits, below the
        // lock - so the drain is a warning rather than a permanent state. It
        // used to start at 0.52 on a scale that never went below 0.67, which
        // meant the visuals were about two thirds grey at all times.
        desc.greyness = clamp01 (juce::jmax (noiseSeconds / 2.2f,
                                             (noiseScore - 0.42f) / 0.24f));

        if (desc.flux < 0.02f && desc.rms > 1.0e-4f) quietSeconds += frameSeconds;
        else                                          quietSeconds = 0.0f;
        desc.stuck = quietSeconds > 4.0f;

        // ---- spectrum for the GUI -----------------------------------------
        const float invPeak = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
        const int step = numBins / spectrumBins;
        for (int i = 0; i < spectrumBins; ++i)
        {
            float m = 0.0f;
            for (int k = 0; k < step; ++k)
            {
                const int idx = i * step + k;
                if (idx < numBins && mag[(size_t) idx] > m) m = mag[(size_t) idx];
            }
            const float v = clamp01 (m * invPeak);
            magNorm[(size_t) i] = magNorm[(size_t) i] * 0.6f + v * 0.4f;
        }
    }

    // -----------------------------------------------------------------------

    float DescriptorAnalyser::computeRoughness() noexcept
    {
        // --- pick the strongest spectral peaks ---------------------------
        numPeaks = 0;
        for (int k = 4; k < numBins - 1; ++k)
        {
            const float m = mag[(size_t) k];
            if (m <= mag[(size_t) (k - 1)] || m < mag[(size_t) (k + 1)]) continue;
            if (m < 1.0e-6f) continue;

            if (numPeaks < maxPeaks)
            {
                peakAmp[numPeaks] = m;
                peakHz[numPeaks]  = (float) k * (float) sr / (float) fftSize;
                ++numPeaks;
            }
            else
            {
                // replace the weakest stored peak if this one beats it
                int weakest = 0;
                for (int i = 1; i < maxPeaks; ++i)
                    if (peakAmp[i] < peakAmp[weakest]) weakest = i;
                if (m > peakAmp[weakest])
                {
                    peakAmp[weakest] = m;
                    peakHz[weakest]  = (float) k * (float) sr / (float) fftSize;
                }
            }
        }

        if (numPeaks < 2) return 0.0f;

        // --- Plomp-Levelt / Sethares pairwise dissonance ------------------
        //   d = amin * ( e^(-b1*s*df) - e^(-b2*s*df) )
        //   s  = 0.24 / (0.0207*fmin + 18.96),  b1 = 3.5, b2 = 5.75
        constexpr float b1 = 3.5f, b2 = 5.75f;

        float total = 0.0f;
        float ampSum = 0.0f;
        for (int i = 0; i < numPeaks; ++i) ampSum += peakAmp[i];
        if (ampSum <= 1.0e-9f) return 0.0f;

        for (int i = 0; i < numPeaks; ++i)
        {
            for (int j = i + 1; j < numPeaks; ++j)
            {
                const float f1 = peakHz[i], f2 = peakHz[j];
                const float fmin = f1 < f2 ? f1 : f2;
                const float fmax = f1 < f2 ? f2 : f1;
                const float a1 = peakAmp[i] / ampSum, a2 = peakAmp[j] / ampSum;
                const float amin = a1 < a2 ? a1 : a2;

                const float s  = 0.24f / (0.0207f * fmin + 18.96f);
                const float df = fmax - fmin;
                const float x  = s * df;
                if (x > 6.0f) continue;                  // far apart: no beating

                total += amin * (std::exp (-b1 * x) - std::exp (-b2 * x));
            }
        }

        // The raw sum is small; scale it into a usable 0..1 and clamp.
        return clamp01 (total * 5.5f);
    }
}
