#include "Cells.h"
#include "SourceAnalyzer.h"
#include <cmath>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float flush (float v) { return std::isfinite (v) ? v : 0.0f; }

    static float fastTanh (float x)
    {
        if (x < -3.0f) return -1.0f;
        if (x >  3.0f) return  1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    static float semisToRatio (float semis) { return std::pow (2.0f, semis / 12.0f); }
    static float midiToHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

    // normalised bandpass centre coefficient
    static float bpCoeff (float hz, float sr) { return juce::jlimit (0.001f, 0.45f, 2.0f * hz / sr); }

    // per-cell white noise (deterministic LCG, bounded [-1, 1))
    static float cellNoise (uint32_t& s)
    {
        s = s * 1664525u + 1013904223u;
        return (float) (s >> 8) * (1.0f / 8388608.0f) - 1.0f;
    }

    // ----------------------------------------------------------------------

    void Cell::prepare (double sr)
    {
        sampleRate = sr;
        for (auto& z : partialPhase) z = 0.0f;
        for (auto& z : modeY1) z = 0.0f;
        for (auto& z : modeY2) z = 0.0f;
        noiseLpZ = noiseBpZ1 = noiseBpZ2 = toneZ = 0.0f;
    }

    void Cell::germinate (Species sp, const Genome& g, int family, int group,
                          int gen, Rng& rng)
    {
        species        = sp;
        genome         = g;
        familyId       = family;
        speciesGroupId = group;
        generation     = gen;
        alive          = true;
        preserved      = false;
        muted          = false;
        stage          = LifeStage::germinating;
        ageSec         = 0.0;
        energy         = 0.12f + 0.1f * rng.nextFloat();
        health         = 1.0f;
        stageGain      = 0.0f;
        ampSmoothed    = 0.0f;
        infection      = Infection::none;
        infectionLoad  = 0.0f;
        linkTo         = -1;
        visualPulse    = 1.0f;
        noiseState     = 0x2545F491u ^ (uint32_t) (family * 2654435761u) ^ (uint32_t) gen;

        const float base = g.get (Trait::spatialPos);
        x = clamp01 (base + rng.bipolar() * 0.1f);
        y = clamp01 (0.5f + rng.bipolar() * 0.3f);
        panSmoothed = (x - 0.5f) * 1.6f;

        lifeSec = 1.5 + 28.0 * std::pow (g.get (Trait::lifespan), 1.5f);

        sourceCursor    = rng.nextFloat();
        grainReadPos[0] = grainReadPos[1] = 0.0;
        grainPhase[0]   = 0.0f; grainPhase[1] = 0.5f;

        for (int i = 0; i < maxModes; ++i)   { modeY1[i] = modeY2[i] = 0.0f; }
        for (int i = 0; i < maxPartials; ++i) { partialPhase[i] = rng.nextFloat() * juce::MathConstants<float>::twoPi; }
    }

    void Cell::updateLifecycle (double dt, float envStress, float nutrients,
                                float metabolism, float lifespanScale, Rng& rng)
    {
        if (! alive) return;

        ageSec += dt;
        genome.updateGeneExpression (envStress);

        const double effectiveLife = lifeSec * (0.4 + 1.6 * lifespanScale);

        const float gather = nutrients * (0.35f + 0.65f * genome.get (Trait::metabolism));
        const float burn   = 0.05f + 0.35f * metabolism * genome.get (Trait::metabolism);
        energy += (float) dt * (gather - burn - 0.25f * infectionLoad);
        energy  = clamp01 (energy);
        health  = clamp01 (health - (float) dt * 0.15f * infectionLoad + (float) dt * 0.02f);

        const double a = ageSec;
        const double L = effectiveLife;
        LifeStage next = stage;
        if      (a < L * 0.08) next = LifeStage::germinating;
        else if (a < L * 0.28) next = LifeStage::growing;
        else if (a < L * 0.75) next = (energy > 0.55f ? LifeStage::reproducing : LifeStage::mature);
        else if (a < L * 0.95) next = LifeStage::mature;
        else                   next = LifeStage::dying;

        if (energy < 0.03f && a > L * 0.2) next = LifeStage::dying;
        if (health < 0.05f)                next = LifeStage::dying;

        if (next == LifeStage::mature && envStress > 0.7f && rng.chance (0.002f))
            next = LifeStage::dormant;
        if (stage == LifeStage::dormant && (envStress < 0.4f || rng.chance (0.01f)))
            next = LifeStage::mature;

        stage = next;

        float target = 0.0f;
        switch (stage)
        {
            case LifeStage::germinating: target = 0.15f; break;
            case LifeStage::growing:     target = 0.55f; break;
            case LifeStage::mature:      target = 1.00f; break;
            case LifeStage::reproducing: target = 0.90f; break;
            case LifeStage::dormant:     target = 0.08f; break;
            case LifeStage::dying:       target = 0.00f; break;
            case LifeStage::dead:        target = 0.00f; break;
        }
        const float coeff = 1.0f - std::exp (-(float) dt / 0.25f);
        stageGain += (target * energy - stageGain) * coeff;

        if (stage == LifeStage::dying && stageGain < 0.002f && a > L)
            kill();

        if (infection != Infection::none)
            infectionLoad = clamp01 (infectionLoad + (float) dt * 0.05f
                                     * (1.0f - genome.get (Trait::infectionResist)));

        visualPulse *= std::exp (-(float) dt * 3.0f);

        const float motion = genome.get (Trait::spatialMotion);
        const float speed  = 0.15f + motion * 2.2f;
        const float cx     = genome.get (Trait::spatialPos);
        x = clamp01 (cx + 0.34f * motion * std::sin ((float) ageSec * speed + familyId * 1.7f));
        y = clamp01 (0.5f + 0.30f * motion * std::sin ((float) ageSec * speed * 0.63f
                                                       + speciesGroupId * 2.1f));
    }

    // ----------------------------------------------------------------------

    float Cell::visualRadius() const
    {
        const float e = 0.35f + 0.65f * energy;
        const float s = 0.5f + 0.5f * genome.get (Trait::density);
        return juce::jlimit (0.006f, 0.05f,
                             0.012f * e * s * (stage == LifeStage::dying ? 0.6f : 1.0f));
    }

    // ----------------------------------------------------------------------

    void Cell::renderAdd (juce::AudioBuffer<float>& out,
                          juce::AudioBuffer<float>& exc,
                          const SourceMaterial& src,
                          float envStress)
    {
        if (! alive || muted || stageGain < 1.0e-4f) return;

        const int   nS = out.getNumSamples();
        const float sr = (float) sampleRate;
        const float g  = genome.expressed (Trait::pitch, envStress);

        float* outL = out.getWritePointer (0);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : outL;
        float* excL = exc.getWritePointer (0);
        float* excR = exc.getNumChannels() > 1 ? exc.getWritePointer (1) : excL;

        const float targetPan = juce::jlimit (-0.95f, 0.95f, (x - 0.5f) * 1.7f);
        const float panCoeff  = 1.0f - std::exp (-1.0f / (0.010f * sr));
        const float ampCoeff  = 1.0f - std::exp (-1.0f / (0.005f * sr));
        const bool  destab    = infection == Infection::destabilise;
        const float targetAmp = stageGain * extGain * (0.85f - (destab ? 0.4f * infectionLoad : 0.0f));

        const float depthCut = 300.0f + (1.0f - y) * 9000.0f;
        const float depthA   = std::exp (-2.0f * juce::MathConstants<float>::pi * depthCut / sr);

        if (species == Species::grain)
        {
            const float dirGene = genome.get (Trait::direction);
            float dirSign = dirGene < 0.5f ? -1.0f : 1.0f;
            if (infection == Infection::reverse && infectionLoad > 0.5f) dirSign = -dirSign;

            const float semis    = (g - 0.5f) * 48.0f;
            const float rate     = semisToRatio (semis);
            const float grainMs  = 5.0f + 245.0f * std::pow (genome.get (Trait::duration), 2.0f);
            const int   grainLen = juce::jmax (32, (int) (grainMs * 0.001f * sr));
            const float hop      = juce::jmax (0.05f, 1.0f - genome.get (Trait::density)) * (float) grainLen;
            const int   nSrc     = juce::jmax (2, src.numSamples());
            const float jitter   = destab ? infectionLoad * 0.06f : 0.0f;
            const float phaseInc = 1.0f / (float) grainLen;

            for (int n = 0; n < nS; ++n)
            {
                float s = 0.0f;
                for (int gr = 0; gr < 2; ++gr)
                {
                    if (grainPhase[gr] >= 1.0f)
                    {
                        grainPhase[gr] -= 1.0f;
                        sourceCursor += hop;
                        if (sourceCursor >= nSrc) sourceCursor -= nSrc;
                        grainReadPos[gr] = sourceCursor;
                    }
                    const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * grainPhase[gr]);
                    s += win * src.readInterp (grainReadPos[gr]);
                    const float rr = rate * (1.0f + jitter * std::sin ((float) grainReadPos[gr] * 0.001f));
                    grainReadPos[gr] += (double) (rr * dirSign);
                    grainPhase[gr]   += phaseInc;
                }
                s *= 0.5f;

                toneZ = s + depthA * (toneZ - s);
                s = toneZ;

                panSmoothed += (targetPan - panSmoothed) * panCoeff;
                ampSmoothed += (targetAmp - ampSmoothed) * ampCoeff;
                const float l = s * ampSmoothed * (0.5f - 0.5f * panSmoothed);
                const float r = s * ampSmoothed * (0.5f + 0.5f * panSmoothed);
                outL[n] += l; outR[n] += r;
                excL[n] += l * 0.7f; excR[n] += r * 0.7f;
            }
        }
        else if (species == Species::spectral)
        {
            const float baseHz   = juce::jlimit (25.0f, 6000.0f, midiToHz (24.0f + g * 90.0f));
            const int   nPart    = juce::jlimit (2, maxPartials,
                                     2 + (int) (genome.get (Trait::density) * (maxPartials - 2)));
            const float bright   = genome.expressed (Trait::brightness, envStress);
            const float tilt     = 1.7f - bright * 1.5f;
            const float noiseMix = clamp01 (genome.get (Trait::noiseColour) * (0.35f + 0.65f * src.noisiness));
            float formantHz      = juce::jlimit (150.0f, 5000.0f,
                                     150.0f + genome.expressed (Trait::formant, envStress) * 4000.0f);
            if (infection == Infection::vocalise)
                formantHz = formantHz * (1.0f - infectionLoad) + 850.0f * infectionLoad;
            const float inharm   = (infection == Infection::metallize) ? infectionLoad * 0.10f : 0.0f;

            for (int k = 0; k < nPart; ++k)
            {
                const float ratio = (float) (k + 1) * std::pow (1.0f + inharm, (float) k);
                const float f     = baseHz * ratio;
                partialInc[k] = juce::MathConstants<float>::twoPi * f / sr;
                float amp = std::pow ((float) (k + 1), -tilt);
                const float df = (f - formantHz) / (formantHz * 0.35f + 1.0f);
                amp *= 1.0f + 1.8f * std::exp (-df * df);
                partialAmp[k] = amp;
            }
            float ampNorm = 0.0f;
            for (int k = 0; k < nPart; ++k) ampNorm += partialAmp[k];
            ampNorm = ampNorm > 0.0f ? 1.0f / ampNorm : 1.0f;

            const float wc = bpCoeff (formantHz, sr);
            const float q  = 0.5f + 8.0f * genome.get (Trait::resonance);

            for (int n = 0; n < nS; ++n)
            {
                float harm = 0.0f;
                for (int k = 0; k < nPart; ++k)
                {
                    partialPhase[k] += partialInc[k];
                    if (partialPhase[k] > juce::MathConstants<float>::twoPi)
                        partialPhase[k] -= juce::MathConstants<float>::twoPi;
                    harm += std::sin (partialPhase[k]) * partialAmp[k];
                }
                harm *= ampNorm;

                const float wn = cellNoise (noiseState);
                noiseBpZ1 += wc * (wn - noiseBpZ1 - noiseBpZ2 / q);
                noiseBpZ2 += wc * noiseBpZ1;
                const float band = noiseBpZ1;

                float s = harm * (1.0f - noiseMix) + band * noiseMix * 1.4f;

                toneZ = s + depthA * (toneZ - s);
                s = toneZ;

                panSmoothed += (targetPan - panSmoothed) * panCoeff;
                ampSmoothed += (targetAmp - ampSmoothed) * ampCoeff;
                const float l = s * ampSmoothed * (0.5f - 0.5f * panSmoothed) * 0.6f;
                const float r = s * ampSmoothed * (0.5f + 0.5f * panSmoothed) * 0.6f;
                outL[n] += l; outR[n] += r;
                excL[n] += l * 0.8f; excR[n] += r * 0.8f;
            }
        }
        else // resonator
        {
            const float baseHz = juce::jlimit (20.0f, 2500.0f, midiToHz (12.0f + g * 84.0f));
            const float res    = genome.expressed (Trait::resonance, envStress);
            const float decayG = genome.get (Trait::decayShape);
            const float rGain  = 0.90f + 0.0995f * res;
            static const float ratios[maxModes] = { 1.0f, 2.01f, 2.99f, 4.21f, 5.44f, 7.13f };
            const float metal  = (infection == Infection::metallize) ? infectionLoad : 0.0f;

            for (int i = 0; i < maxModes; ++i)
            {
                float f = baseHz * (ratios[i] + metal * (float) i * 0.15f) * (0.5f + res);
                f = juce::jlimit (20.0f, sr * 0.45f, f);
                modeF[i]  = juce::MathConstants<float>::twoPi * f / sr;
                modeFb[i] = juce::jlimit (0.80f, 0.9995f,
                              rGain * (1.0f - (float) i / (float) maxModes * (1.0f - decayG) * 0.4f));
            }

            const float selfNoise = res > 0.85f ? (res - 0.85f) * 0.05f : 0.0f;

            for (int n = 0; n < nS; ++n)
            {
                float in = 0.5f * (excL[n] + excR[n]) + selfNoise * cellNoise (noiseState);

                float s = 0.0f;
                for (int i = 0; i < maxModes; ++i)
                {
                    const float cosw = std::cos (modeF[i]);
                    const float y = in * (1.0f - modeFb[i] * modeFb[i]) * 0.5f
                                    + 2.0f * modeFb[i] * cosw * modeY1[i]
                                    - modeFb[i] * modeFb[i] * modeY2[i];
                    modeY2[i] = modeY1[i];
                    modeY1[i] = flush (y);
                    s += y;
                }
                s = fastTanh (s * (0.6f + 0.8f * res));

                toneZ = s + depthA * (toneZ - s);
                s = toneZ;

                panSmoothed += (targetPan - panSmoothed) * panCoeff;
                ampSmoothed += (targetAmp - ampSmoothed) * ampCoeff;
                outL[n] += s * ampSmoothed * (0.5f - 0.5f * panSmoothed) * 0.9f;
                outR[n] += s * ampSmoothed * (0.5f + 0.5f * panSmoothed) * 0.9f;
            }
        }
    }
}
