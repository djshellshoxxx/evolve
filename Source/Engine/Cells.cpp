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

    /*  A fast sine for the per-sample partial bank.

        The spectral species evaluates up to twelve sines per sample per cell,
        which is far and away the hottest loop in the engine. This is the
        Bhaskara-style parabolic approximation with one correction term: about
        0.1% peak error, which is roughly -60 dB of harmonic junk on a partial
        that is already being summed with eleven others and then filtered. It
        is not good enough for an oscillator you would tune by ear; it is more
        than good enough for a partial in a bank.

        Input must be in [-pi, pi].                                          */
    static inline float fastSin (float x) noexcept
    {
        constexpr float B = 4.0f / juce::MathConstants<float>::pi;
        constexpr float C = -4.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi);

        const float y = B * x + C * x * std::fabs (x);
        return 0.225f * (y * std::fabs (y) - y) + y;
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

        dominance = 1.0f;
        catalystAmount = sparkle = geiger = 0.0f;
        catalystPhase = clickEnv = clickPhase = 0.0f;
        sparkleZ1 = sparkleZ2 = 0.0f;
        geigerState = 0x9E3779B9u ^ (uint32_t) (family * 2246822519u) ^ (uint32_t) group;

        mod.configure (genome, worldOrDefault(), rng);
    }

    // ----------------------------------------------------------------------

    const WorldSeed& Cell::worldOrDefault() const
    {
        // A cell should always have the colony's world, but a default keeps
        // the DSP safe if one is ever rendered before prepare() runs.
        static const WorldSeed fallback = WorldSeed::fromSeed (0xA5A5A5A5A5A5A5A5ULL);
        return world != nullptr ? *world : fallback;
    }

    void Cell::advanceModulation (float dt) noexcept
    {
        if (! alive) return;

        mod.advance (dt);

        // The catalyst is a fast pitch wobble that decays over a few seconds.
        // It is loud while it lasts; what it leaves behind is handled by the
        // colony, which strips an element as the wobble fades.
        if (catalystAmount > 1.0e-4f)
        {
            catalystPhase += catalystRate * dt;
            if (catalystPhase > 1.0f) catalystPhase -= std::floor (catalystPhase);
            catalystAmount *= std::exp (-dt / 2.2f);
        }
        else catalystAmount = 0.0f;

        if (sparkle > 1.0e-4f) sparkle *= std::exp (-dt / 1.6f); else sparkle = 0.0f;

        // Radiation clicks linger noticeably longer than the other gestures -
        // the point is that you keep hearing the Geiger counter after the
        // decision has already been made for you.
        if (geiger > 1.0e-4f) geiger *= std::exp (-dt / 4.5f); else geiger = 0.0f;
    }

    float Cell::movementAmount() const noexcept
    {
        if (! alive) return 0.0f;
        const float depth = genome.get (Trait::lfoDepth);
        const float spread = genome.get (Trait::lfoRateSpread);
        const float extra = 0.5f * (genome.get (Trait::vibrato) + genome.get (Trait::tremolo))
                          + genome.get (Trait::drift);
        return clamp01 (0.45f * depth + 0.30f * spread + 0.25f * clamp01 (extra));
    }

    float Cell::overlaySample (float sr) noexcept
    {
        float out = 0.0f;

        // ---- enzyme shimmer -------------------------------------------
        // A bright, thin band of noise that rings just above the cell's own
        // register, so it reads as "something is being dissolved" rather than
        // as an extra voice.
        if (sparkle > 1.0e-4f)
        {
            const float wn = cellNoise (noiseState);
            const float wc = juce::jlimit (0.02f, 0.45f, 9000.0f * 2.0f / sr);
            sparkleZ1 += wc * (wn - sparkleZ1 - sparkleZ2 * 0.35f);
            sparkleZ2 += wc * sparkleZ1;
            out += sparkleZ1 * sparkle * 0.22f;
        }

        // ---- Geiger clicks ---------------------------------------------
        if (geiger > 1.0e-4f)
        {
            // Poisson-ish: a small per-sample chance of starting a click,
            // scaled by how much radiation is still in the cell.
            geigerState = geigerState * 1664525u + 1013904223u;
            const float u = (float) (geigerState >> 8) * (1.0f / 16777216.0f);
            if (u < geiger * 26.0f / sr)
            {
                clickEnv = 1.0f;
                clickPhase = 0.0f;
                geigerState = geigerState * 1664525u + 1013904223u;
                clickRate = 1800.0f + (float) (geigerState >> 20) * 0.35f;
            }

            if (clickEnv > 1.0e-4f)
            {
                clickPhase += clickRate / sr;
                if (clickPhase > 1.0f) clickPhase -= std::floor (clickPhase);
                const float tick = std::sin (clickPhase * juce::MathConstants<float>::twoPi);
                out += tick * clickEnv * clickEnv * 0.12f;
                clickEnv *= 0.9985f - 0.0035f;      // ~2 ms tick
            }
            else clickEnv = 0.0f;
        }

        return out;
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
        const WorldSeed& W = worldOrDefault();

        float* outL = out.getWritePointer (0);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : outL;
        float* excL = exc.getWritePointer (0);
        float* excR = exc.getNumChannels() > 1 ? exc.getWritePointer (1) : excL;

        // ---- modulation for this block -----------------------------------
        auto md = [this] (ModDest d) { return juce::jlimit (-1.5f, 1.5f, mod.get (d)); };

        const float mPitch  = md (ModDest::pitch);
        const float mAmp    = md (ModDest::amp);
        const float mForm   = md (ModDest::formant);
        const float mBright = md (ModDest::brightness);
        const float mPan    = md (ModDest::pan);
        const float mDens   = md (ModDest::density);
        const float mRes    = md (ModDest::resonance);
        const float mRate   = md (ModDest::grainRate);
        const float mDet    = md (ModDest::detune);

        // The catalyst rides on top of everything: a fast, obvious pitch
        // excursion that decays away over a couple of seconds.
        const float catSemis = catalystAmount > 0.0f
            ? std::sin (catalystPhase * juce::MathConstants<float>::twoPi) * catalystAmount * 9.0f
            : 0.0f;

        const float pitchSemis = mPitch * 5.0f + catSemis;
        const float ampScale   = juce::jlimit (0.25f, 1.6f, 1.0f + 0.45f * mAmp);

        const float targetPan = juce::jlimit (-0.95f, 0.95f,
                                              (x - 0.5f) * 1.7f + mPan * 0.5f);
        const float panCoeff  = 1.0f - std::exp (-1.0f / (0.010f * sr));
        const float ampCoeff  = 1.0f - std::exp (-1.0f / (0.005f * sr));
        const bool  destab    = infection == Infection::destabilise;
        const float targetAmp = stageGain * extGain * dominance * ampScale
                              * (0.85f - (destab ? 0.4f * infectionLoad : 0.0f));

        // Brightness modulation moves the one-pole "depth" filter, which is
        // the cheapest place to hear a slow lane doing its work.
        const float brightShift = std::pow (2.0f, mBright * 1.6f);
        const float depthCut = juce::jlimit (120.0f, sr * 0.45f,
                                             (300.0f + (1.0f - y) * 9000.0f) * brightShift);
        const float depthA   = std::exp (-2.0f * juce::MathConstants<float>::pi * depthCut / sr);

        const bool wantOverlay = (sparkle > 1.0e-4f) || (geiger > 1.0e-4f);

        if (species == Species::grain)
        {
            const float dirGene = genome.get (Trait::direction);
            float dirSign = dirGene < 0.5f ? -1.0f : 1.0f;
            if (infection == Infection::reverse && infectionLoad > 0.5f) dirSign = -dirSign;

            // The world's register, not four octaves of whatever.
            const float semis    = (W.geneToMidi (g) - 60.0f) + pitchSemis;
            // grainRate modulation is separate from pitch: it slides the
            // playback speed of the loop rather than transposing the grain.
            const float rate     = semisToRatio (semis) * std::pow (2.0f, mRate * 0.8f);
            const float grainMs  = 5.0f + 245.0f * std::pow (genome.get (Trait::duration), 2.0f);
            const int   grainLen = juce::jmax (32, (int) (grainMs * 0.001f * sr));
            const float densMod  = juce::jlimit (0.0f, 1.0f,
                                                 genome.get (Trait::density) + mDens * 0.35f);
            const float hop      = juce::jmax (0.05f, 1.0f - densMod) * (float) grainLen;
            const int   nSrc     = juce::jmax (2, src.numSamples());
            const float jitterAmt = (destab ? infectionLoad * 0.06f : 0.0f);
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
                        // one new random detune per grain: audible as
                        // shimmer between grains, not as noise inside them
                        grainRateJitter[gr] = 1.0f + mod.jitter() * 0.03f;
                    }
                    const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * grainPhase[gr]);
                    s += win * src.readInterp (grainReadPos[gr]);
                    // Jitter is applied once per grain, at grain start, not per
                    // sample. Per-sample random deviation of the read rate is
                    // frequency modulation by white noise, which turns any
                    // source - however tonal - into broadband hiss. That one
                    // multiply was most of why a single cell measured almost
                    // as flat as white noise.
                    const float rr = rate * grainRateJitter[gr]
                                     * (1.0f + jitterAmt * std::sin ((float) grainReadPos[gr] * 0.001f));
                    grainReadPos[gr] += (double) (rr * dirSign);
                    grainPhase[gr]   += phaseInc;
                }
                s *= 0.5f;

                toneZ = s + depthA * (toneZ - s);
                s = toneZ;

                if (wantOverlay) s += overlaySample (sr);

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
            // The world decides the tuning. Quantise the *unmodulated* pitch so
            // the cell sits in the world's scale, then apply modulation on top
            // as a continuous ratio - otherwise vibrato would stair-step.
            float baseHz = juce::jlimit (25.0f, 6000.0f, midiToHz (W.geneToMidi (g)));
            baseHz = juce::jlimit (25.0f, 6000.0f, W.quantise (baseHz));
            baseHz *= semisToRatio (pitchSemis);
            baseHz = juce::jlimit (20.0f, 7000.0f, baseHz);

            const float densMod = juce::jlimit (0.0f, 1.0f,
                                                genome.get (Trait::density) + mDens * 0.3f);
            const int   nPart   = juce::jlimit (2, maxPartials,
                                     2 + (int) (densMod * (maxPartials - 2)));
            const float bright  = juce::jlimit (0.0f, 1.0f,
                                    genome.expressed (Trait::brightness, envStress) + mBright * 0.25f);
            /*  Partial roll-off.

                This used to reach 0.2 at full brightness, which is almost no
                roll-off at all: twelve partials of nearly equal amplitude, and
                eight such cells sounding together put ninety-odd equal-weight
                sinusoids across the spectrum. That is a noise generator with
                extra steps, and it measured like one. A floor of 0.65 keeps a
                bright cell bright while still giving it a recognisable
                spectral slope, which is what makes it read as a note rather
                than as a band of energy.                                     */
            const float tilt    = 1.85f - bright * 1.2f;   // 0.65 .. 1.85

            // The world's noise ceiling is a hard cap. A cell may be grainy or
            // breathy; it may not become a noise generator, because a colony of
            // noise generators is the one outcome that is never interesting.
            // Scaling by the seed's own noisiness means a noisy seed pushed
            // every spectral cell toward hiss; the square root softens that so
            // the material colours the cell without deciding it.
            const float noiseMix = juce::jlimit (0.0f, W.noiseCeiling,
                                     genome.get (Trait::noiseColour)
                                     * (0.35f + 0.65f * std::sqrt (src.noisiness)));

            float formantHz = juce::jlimit (150.0f, 5000.0f,
                                150.0f + genome.expressed (Trait::formant, envStress) * 4000.0f);
            formantHz *= std::pow (2.0f, mForm * 1.1f);
            formantHz = juce::jlimit (120.0f, 6000.0f, formantHz);

            if (infection == Infection::vocalise)
                formantHz = formantHz * (1.0f - infectionLoad) + 850.0f * infectionLoad;
            const float inharm = (infection == Infection::metallize) ? infectionLoad * 0.10f : 0.0f;

            // Detune modulation spreads the partials apart and back together,
            // which is the chorusing "breathing" of the spectral species.
            const float detune = mDet * 0.012f + genome.get (Trait::jitter) * 0.004f;

            for (int k = 0; k < nPart; ++k)
            {
                const float ratio = W.partialRatio (k)
                                  * std::pow (1.0f + inharm, (float) k)
                                  * (1.0f + detune * (float) k);
                const float f     = juce::jlimit (10.0f, sr * 0.47f, baseHz * ratio);
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
            const float q  = 0.5f + 8.0f * juce::jlimit (0.0f, 1.0f,
                                    genome.get (Trait::resonance) + mRes * 0.25f);

            for (int n = 0; n < nS; ++n)
            {
                float harm = 0.0f;
                for (int k = 0; k < nPart; ++k)
                {
                    partialPhase[k] += partialInc[k];
                    if (partialPhase[k] > juce::MathConstants<float>::twoPi)
                        partialPhase[k] -= juce::MathConstants<float>::twoPi;

                    // shift [0, 2pi) into [-pi, pi) for fastSin; sin(x+pi) = -sin(x)
                    const float xx = partialPhase[k] - juce::MathConstants<float>::pi;
                    harm -= fastSin (xx) * partialAmp[k];
                }
                harm *= ampNorm;

                const float wn = cellNoise (noiseState);
                noiseBpZ1 += wc * (wn - noiseBpZ1 - noiseBpZ2 / q);
                noiseBpZ2 += wc * noiseBpZ1;
                const float band = noiseBpZ1;

                // The noise band used to be boosted 1.4x against the harmonic
                // part, so even a modest noiseColour gene dominated the cell.
                float s = harm * (1.0f - noiseMix * 0.75f) + band * noiseMix * 0.8f;

                toneZ = s + depthA * (toneZ - s);
                s = toneZ;

                if (wantOverlay) s += overlaySample (sr);

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
            float baseHz = juce::jlimit (20.0f, 2500.0f, midiToHz (W.geneToMidi (g) - 12.0f));
            baseHz = juce::jlimit (20.0f, 2500.0f, W.quantise (baseHz));
            baseHz *= semisToRatio (pitchSemis);
            baseHz = juce::jlimit (18.0f, 3000.0f, baseHz);

            const float res    = juce::jlimit (0.0f, 1.0f,
                                   genome.expressed (Trait::resonance, envStress) + mRes * 0.2f);
            const float decayG = genome.get (Trait::decayShape);
            const float rGain  = 0.90f + 0.0995f * res;
            const float metal  = (infection == Infection::metallize) ? infectionLoad : 0.0f;
            const float detune = mDet * 0.01f;

            for (int i = 0; i < maxModes; ++i)
            {
                // The world's partial palette also sets the resonator's modes,
                // so a "golden" world rings like a bell and a "harmonic" world
                // rings like a string, using the same code.
                float f = baseHz * W.partialRatio (i) * (1.0f + detune * (float) i)
                          * (1.0f + metal * (float) i * 0.05f) * (0.5f + res);
                f = juce::jlimit (20.0f, sr * 0.45f, f);
                modeF[i]  = juce::MathConstants<float>::twoPi * f / sr;
                modeFb[i] = juce::jlimit (0.80f, 0.9995f,
                              rGain * (1.0f - (float) i / (float) maxModes * (1.0f - decayG) * 0.4f));
                modeC[i]  = 2.0f * modeFb[i] * std::cos (modeF[i]);   // constant for the block
            }

            const float selfNoise = res > 0.85f ? (res - 0.85f) * 0.05f : 0.0f;

            for (int n = 0; n < nS; ++n)
            {
                float in = 0.5f * (excL[n] + excR[n]) + selfNoise * cellNoise (noiseState);
                if (wantOverlay) in += overlaySample (sr) * 0.6f;

                float s = 0.0f;
                for (int i = 0; i < maxModes; ++i)
                {
                    const float y = in * (1.0f - modeFb[i] * modeFb[i]) * 0.5f
                                    + modeC[i] * modeY1[i]
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
