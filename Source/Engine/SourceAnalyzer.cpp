#include "SourceAnalyzer.h"
#include <cmath>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    SourceAnalyzer::SourceAnalyzer() = default;

    SourceMaterial SourceAnalyzer::analyse (const juce::AudioBuffer<float>& input,
                                            double sampleRate,
                                            float transientSensitivity) const
    {
        SourceMaterial m;
        m.sampleRate = sampleRate;

        const int chans = juce::jmax (1, input.getNumChannels());
        const int n     = input.getNumSamples();
        if (n < 64)
        {
            m.mono.setSize (1, 64);
            m.mono.clear();
            m.valid = false;
            return m;
        }

        // ---- mono sum ----
        juce::AudioBuffer<float> mono (1, n);
        mono.clear();
        for (int c = 0; c < chans; ++c)
            mono.addFrom (0, 0, input, c, 0, n, 1.0f / (float) chans);

        // ---- peak normalise ----
        const float peak = mono.getMagnitude (0, 0, n);
        if (peak > 1.0e-6f)
            mono.applyGain (1.0f / peak * 0.98f);

        m.mono = std::move (mono);
        const float* d = m.mono.getReadPointer (0);

        // ---- onset detection (spectral-flux-lite: rectified high-passed energy) ----
        const int hop = 256;
        float prevEnergy = 0.0f;
        float fluxMean   = 0.0f;
        std::vector<float> flux;
        flux.reserve ((size_t) (n / hop) + 1);
        for (int i = 0; i + hop < n; i += hop)
        {
            float e = 0.0f;
            for (int k = 0; k < hop; ++k)
            {
                const float hp = d[i + k] - (k > 0 ? d[i + k - 1] : 0.0f);
                e += hp * hp;
            }
            e = std::sqrt (e / (float) hop);
            const float f = juce::jmax (0.0f, e - prevEnergy);
            flux.push_back (f);
            fluxMean += f;
            prevEnergy = e;
        }
        if (! flux.empty()) fluxMean /= (float) flux.size();

        const float thresh = fluxMean * (2.4f - 1.8f * clamp01 (transientSensitivity)) + 1.0e-5f;
        int lastOnset = -hop * 8;
        for (int fi = 1; fi + 1 < (int) flux.size(); ++fi)
        {
            const int sampleIdx = fi * hop;
            if (flux[(size_t) fi] > thresh
                && flux[(size_t) fi] >= flux[(size_t) fi - 1]
                && flux[(size_t) fi] >= flux[(size_t) fi + 1]
                && sampleIdx - lastOnset > hop * 3)
            {
                m.transients.push_back (sampleIdx);
                lastOnset = sampleIdx;
            }
        }
        if (m.transients.empty())
            m.transients.push_back (0);

        // ---- averaged spectrum over a few frames ----
        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        std::vector<float> accum   ((size_t) fftSize / 2, 0.0f);
        int frames = 0;
        for (int start = 0; start + fftSize < n; start += fftSize / 2)
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int k = 0; k < fftSize; ++k) fftData[(size_t) k] = d[start + k];
            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            for (int k = 0; k < fftSize / 2; ++k) accum[(size_t) k] += fftData[(size_t) k];
            ++frames;
            if (frames >= 24) break;
        }
        if (frames > 0)
            for (auto& v : accum) v /= (float) frames;

        m.avgSpectrum.assign (accum.begin(), accum.end());

        // ---- derive summary features ----
        double centroidNum = 0.0, centroidDen = 0.0, totalMag = 0.0;
        float  maxMag = 0.0f; int maxBin = 1;
        const double binHz = sampleRate / (double) fftSize;
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const float mag = accum[(size_t) k];
            centroidNum += mag * (k * binHz);
            centroidDen += mag;
            totalMag    += mag;
            if (mag > maxMag && k * binHz < 4000.0) { maxMag = mag; maxBin = k; }
        }
        const double centroid = centroidDen > 0.0 ? centroidNum / centroidDen : 1000.0;
        m.brightness  = clamp01 ((float) (centroid / 6000.0));
        m.formantHz   = (float) juce::jlimit (120.0, 4000.0, maxBin * binHz);

        // crude fundamental: first strong low bin
        for (int k = 1; k < fftSize / 4; ++k)
        {
            if (accum[(size_t) k] > maxMag * 0.35f)
            {
                m.fundamentalHz = (float) juce::jlimit (30.0, 1500.0, k * binHz);
                break;
            }
        }

        // noisiness: spectral flatness (geo mean / arith mean)
        double logSum = 0.0; int cnt = 0;
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const float mag = accum[(size_t) k] + 1.0e-9f;
            logSum += std::log (mag);
            ++cnt;
        }
        const double geo  = std::exp (logSum / juce::jmax (1, cnt));
        const double arith = totalMag / juce::jmax (1, cnt);
        m.noisiness = clamp01 ((float) (geo / (arith + 1.0e-9)) * 3.0f);

        // decay rate from the amplitude envelope after the last transient
        {
            const int t = m.transients.back();
            float startRms = 0.0f, endRms = 0.0f; int w = juce::jmin (4096, (n - t) / 2);
            if (w > 64)
            {
                for (int k = 0; k < w; ++k)            startRms += d[t + k] * d[t + k];
                for (int k = n - w; k < n; ++k)        endRms   += d[k] * d[k];
                startRms = std::sqrt (startRms / (float) w);
                endRms   = std::sqrt (endRms   / (float) w);
                m.decayRate = clamp01 (1.0f - endRms / (startRms + 1.0e-6f));
            }
        }

        m.valid = true;
        return m;
    }

    SourceMaterial SourceAnalyzer::makePrimitive (params::SourceMode mode,
                                                  double sampleRate,
                                                  float lengthSeconds) const
    {
        SourceMaterial m;
        m.sampleRate = sampleRate;
        const int n = juce::jlimit (2048, (int) (sampleRate * 12.0),
                                    (int) (sampleRate * juce::jmax (0.25f, lengthSeconds)));
        m.mono.setSize (1, n);
        auto* d = m.mono.getWritePointer (0);
        juce::Random r (0x51ED);

        if (mode == params::SourceMode::primitiveImpulse)
        {
            m.mono.clear();
            const int period = juce::jmax (128, (int) (sampleRate / 60.0));
            for (int i = 0; i < n; i += period)
            {
                // short decaying click
                for (int k = 0; k < 256 && i + k < n; ++k)
                    d[i + k] = std::exp (-k * 0.05f) * (1.0f - 2.0f * ((k & 1) != 0));
            }
            m.noisiness = 0.15f; m.brightness = 0.7f; m.fundamentalHz = 60.0f;
            m.formantHz = 1200.0f; m.decayRate = 0.85f;
            m.transients.clear();
            for (int i = 0; i < n; i += period) m.transients.push_back (i);
        }
        else if (mode == params::SourceMode::primitiveTone)
        {
            /*  The default seed.

                It used to be pink noise, and that single choice undid most of
                the engine: grain cells granulate the seed directly, so a noise
                seed gives noise no matter how the colony evolves, and the
                spectral species scales its own noise mix by the seed's
                measured noisiness, so a noise seed also pushed every spectral
                cell toward hiss. Every run converged on the same grey cloud
                because every run was chewing the same grey material.

                This is a short sequence of plucked, harmonically rich decays
                at related pitches - something with partials to inherit, clear
                transients for the onset detector, and a low noise floor. It is
                deliberately not a single sustained tone: the colony needs
                internal variety in its material, just not *random* variety. */
            m.mono.clear();

            const double baseHz = 110.0;
            const double ratios[6] = { 1.0, 1.5, 2.0, 1.25, 3.0, 1.6667 };
            const int    events = 6;
            const int    span = n / events;

            for (int e = 0; e < events; ++e)
            {
                const int start = e * span;
                const double f0 = baseHz * ratios[e % 6];
                const double decay = 2.2 + 2.6 * (double) ((e * 7) % 5) / 4.0;

                for (int i = 0; i < span && start + i < n; ++i)
                {
                    const double t = (double) i / sampleRate;
                    const double env = std::exp (-t * decay);
                    if (env < 1.0e-4) break;

                    double v = 0.0;
                    // odd-weighted harmonic series: bright at the attack,
                    // mellowing as the partials decay at different rates
                    for (int h = 1; h <= 12; ++h)
                    {
                        const double hf = f0 * (double) h;
                        if (hf > sampleRate * 0.45) break;
                        const double hEnv = std::exp (-t * decay * (1.0 + 0.22 * (double) h));
                        v += std::sin (juce::MathConstants<double>::twoPi * hf * t)
                             * hEnv / (double) h;
                    }
                    d[start + i] += (float) (v * env * 0.5);
                }
            }

            m.mono.applyGain (0.9f / juce::jmax (1.0e-4f, m.mono.getMagnitude (0, 0, n)));
            m.noisiness = 0.10f; m.brightness = 0.45f; m.fundamentalHz = (float) baseHz;
            m.formantHz = 700.0f; m.decayRate = 0.55f;
            m.transients.clear();
            for (int e = 0; e < events; ++e) m.transients.push_back (e * span);
        }
        else // noise
        {
            float z = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float wnoise = r.nextFloat() * 2.0f - 1.0f;
                z = 0.15f * wnoise + 0.85f * z;         // pink-ish
                d[i] = 0.6f * z + 0.4f * wnoise;
            }
            m.mono.applyGain (0.9f / juce::jmax (1.0e-4f, m.mono.getMagnitude (0, 0, n)));
            m.noisiness = 0.9f; m.brightness = 0.55f; m.fundamentalHz = 90.0f;
            m.formantHz = 900.0f; m.decayRate = 0.2f;
            m.transients = { 0, n / 3, (2 * n) / 3 };
        }

        m.avgSpectrum.assign ((size_t) (fftSize / 2), 0.0f);
        m.valid = true;
        return m;
    }
}
