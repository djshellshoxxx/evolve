#include "PostChain.h"
#include <cmath>

namespace mutagen
{
    static constexpr float kPi  = juce::MathConstants<float>::pi;
    static constexpr float k2Pi = juce::MathConstants<float>::twoPi;

    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float flushd (float v) { return std::isfinite (v) ? v : 0.0f; }

    static float softclip (float x)
    {
        if (x < -3.0f) return -1.0f;
        if (x >  3.0f) return  1.0f;
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    static float polyBlep (double t, double dt)
    {
        if (t < dt)            { const double x = t / dt;        return (float) (x + x - x * x - 1.0); }
        if (t > 1.0 - dt)      { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }

    static float midiToHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

    // ------------------------------------------------------------------
    //  LFO
    // ------------------------------------------------------------------

    float PostChain::Lfo::shapeValue (int shape, double ph)
    {
        const float t = (float) ph;
        switch ((params::LfoShape) shape)
        {
            case params::LfoShape::sine:      return std::sin (t * k2Pi);
            case params::LfoShape::triangle:  return 2.0f * std::abs (2.0f * t - 1.0f) - 1.0f;
            case params::LfoShape::sawUp:     return 2.0f * t - 1.0f;
            case params::LfoShape::sawDown:   return 1.0f - 2.0f * t;
            case params::LfoShape::square:    return t < 0.5f ? 1.0f : -1.0f;
            case params::LfoShape::sampleHold: return shHold;
            case params::LfoShape::randomSmooth:
                return shPrev + (shNext - shPrev) * t;
        }
        return 0.0f;
    }

    float PostChain::Lfo::advance (int shape, double incr)
    {
        phase += incr;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            shPrev = shNext;
            shHold = rng.nextFloat() * 2.0f - 1.0f;
            shNext = rng.nextFloat() * 2.0f - 1.0f;
        }
        while (phase < 0.0) phase += 1.0;
        return shapeValue (shape, phase);
    }

    // ------------------------------------------------------------------

    PostChain::PostChain() = default;

    void PostChain::prepare (double sampleRate, int maxBlock, int numChannels)
    {
        sr    = sampleRate;
        numCh = juce::jmax (1, numChannels);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) numCh };
        svf.prepare (spec);
        svf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        juce::dsp::ProcessSpec mono { sr, (juce::uint32) juce::jmax (1, maxBlock), 1 };
        const auto flat = juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0f);
        for (auto& band : eq)
            for (auto& f : band)
            {
                f.prepare (mono);
                f.coefficients = flat;   // never leave coefficients null
                f.reset();
            }

        adsr.setSampleRate (sr);

        glitchCapture.setSize (2, juce::jmax (2048, (int) (sr * 2.0)));
        glitchCapture.clear();
        glitchWrite = 0;

        reset();
    }

    void PostChain::reset()
    {
        svf.reset();
        for (auto& b : eq) for (auto& f : b) f.reset();
        adsr.reset();
        for (auto& l : lfos) l.phase = 0.0;
        for (auto& ph : oscPhase) ph = 0.0;
        gatorStepPos = 0.0;
        gatorEnv = 1.0f;
        glitchActive = false;
        glitchTapeStop = false;
        glitchTapeSpeed = 1.0f;
        envLfoValue = 0.0f;
    }

    // ------------------------------------------------------------------

    void PostChain::noteOn (int note, float velocity)
    {
        pp.lastNote = note;
        pp.lastVelocity = velocity;
        pp.heldNotes = juce::jmax (1, pp.heldNotes + 1);

        if (! pp.synthDrone)
            adsr.noteOn();

        if (pp.midiReactive && pp.lfoKeyRetrigger)
            for (int i = 0; i < params::numLfos; ++i)
                lfos[(size_t) i].retrigger (pp.lfo[(size_t) i].phase);

        if (pp.midiReactive && pp.gatorRetrigger)
            gatorStepPos = 0.0;
    }

    void PostChain::noteOff (int note)
    {
        juce::ignoreUnused (note);
        pp.heldNotes = juce::jmax (0, pp.heldNotes - 1);
        if (pp.heldNotes == 0 && ! pp.synthDrone)
            adsr.noteOff();
    }

    void PostChain::allNotesOff()
    {
        pp.heldNotes = 0;
        adsr.noteOff();
    }

    // ------------------------------------------------------------------

    float PostChain::lfoFreq (int i, const TransportInfo& tr) const
    {
        const auto& L = pp.lfo[(size_t) i];
        if (L.sync)
        {
            const double bpm = tr.bpm > 1.0 ? tr.bpm : 120.0;
            const double beats = params::syncDivBeats (L.div);
            return (float) ((bpm / 60.0) / juce::jmax (1.0e-4, beats));
        }
        return juce::jlimit (0.001f, 60.0f, L.rateHz);
    }

    float PostChain::oscSample (int wave, double& phase, double inc, juce::Random& rng)
    {
        const double dt = juce::jlimit (1.0e-6, 0.5, inc);
        phase += inc;
        while (phase >= 1.0) phase -= 1.0;
        while (phase < 0.0)  phase += 1.0;
        const double t = phase;

        switch ((params::OscWave) wave)
        {
            case params::OscWave::sine:     return std::sin ((float) t * k2Pi);
            case params::OscWave::triangle: return 2.0f * std::abs (2.0f * (float) t - 1.0f) - 1.0f;
            case params::OscWave::saw:      return (float) (2.0 * t - 1.0) - polyBlep (t, dt);
            case params::OscWave::square:
            {
                float s = t < 0.5 ? 1.0f : -1.0f;
                s += polyBlep (t, dt);
                s -= polyBlep (std::fmod (t + 0.5, 1.0), dt);
                return s;
            }
            case params::OscWave::pulse:
            {
                float s = t < 0.25 ? 1.0f : -1.0f;
                s += polyBlep (t, dt);
                s -= polyBlep (std::fmod (t + 0.75, 1.0), dt);
                return s * 0.9f;
            }
            case params::OscWave::noise:    return rng.nextFloat() * 2.0f - 1.0f;
        }
        return 0.0f;
    }

    // ------------------------------------------------------------------

    void PostChain::process (juce::AudioBuffer<float>& buffer, const TransportInfo& tr)
    {
        const int n  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        if (n <= 0) return;

        float* L = buffer.getWritePointer (0);
        float* R = ch > 1 ? buffer.getWritePointer (1) : L;

        // ---- LFO 3 (ecology) : block-rate, recorded for the processor ----
        {
            const float f = lfoFreq (params::envLfoIndex, tr);
            envLfoValue = lfos[(size_t) params::envLfoIndex].advance (
                              pp.lfo[(size_t) params::envLfoIndex].shape,
                              (double) f * (double) n / sr);
        }

        // ---- ADSR parameters ----
        auto shape = [] (float v, float maxSec) { return juce::jmax (0.001f, v * v * maxSec); };
        adsrParams.attack  = shape (pp.synthA, 4.0f);
        adsrParams.decay   = shape (pp.synthD, 4.0f);
        adsrParams.sustain = clamp01 (pp.synthS);
        adsrParams.release = shape (pp.synthR, 8.0f);
        adsr.setParameters (adsrParams);
        if (pp.synthDrone && ! adsr.isActive()) adsr.noteOn();

        // ---- oscillator base frequency ----
        const bool  useKey  = pp.midiReactive && pp.oscKeytrack && pp.lastNote >= 0;
        float baseHz = useKey ? midiToHz ((float) pp.lastNote) : pp.oscFreeHz;
        if (pp.midiReactive)
            baseHz *= std::pow (2.0f, pp.pitchBend * pp.bendRange / 12.0f);
        baseHz = juce::jlimit (8.0f, 12000.0f, baseHz);

        const float velSynth = pp.midiReactive
            ? juce::jmap (pp.velToSynth, 1.0f, pp.lastVelocity) : 1.0f;
        const float blendGain = pp.oscBlend;   // signed

        // ---- filter setup (LFO 0 modulates cutoff every K samples) ----
        const float filtLfoF = lfoFreq (0, tr);
        svf.setResonance (juce::jlimit (0.05f, 12.0f, 0.2f + pp.res * 6.0f));
        switch ((params::FilterType) pp.filterType)
        {
            case params::FilterType::lowpass:  svf.setType (juce::dsp::StateVariableTPTFilterType::lowpass); break;
            case params::FilterType::highpass: svf.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
            case params::FilterType::bandpass: svf.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
            case params::FilterType::notch:    svf.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
        }
        const float velFilt = pp.midiReactive
            ? juce::jmap (pp.velToFilter, 1.0f, 0.25f + 1.75f * pp.lastVelocity) : 1.0f;

        // ---- EQ coefficients (block-rate) ----
        if (pp.eqOn)
        {
            auto g = [] (float dB) { return std::pow (10.0f, dB / 40.0f); };
            const auto lo = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, juce::jlimit (20.0f, 500.0f,  pp.eqLowF),  0.7f, g (pp.eqLowG));
            const auto md = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, juce::jlimit (100.0f, 12000.0f, pp.eqMidF), juce::jlimit (0.1f, 12.0f, pp.eqMidQ), g (pp.eqMidG));
            const auto hi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, juce::jlimit (1000.0f, 20000.0f, pp.eqHighF), 0.7f, g (pp.eqHighG));
            for (int c = 0; c < 2; ++c)
            {
                *eq[0][(size_t) c].coefficients = *lo;
                *eq[1][(size_t) c].coefficients = *md;
                *eq[2][(size_t) c].coefficients = *hi;
            }
        }

        // ---- gator timing ----
        float gStepInc = 0.0f;
        if (pp.gatorOn)
        {
            if (pp.gatorSync && tr.bpm > 1.0)
            {
                const double beatsPerStep = params::syncDivBeats (pp.gatorDiv);
                const double stepDurSec = beatsPerStep * 60.0 / tr.bpm;
                gStepInc = (float) (1.0 / (stepDurSec * sr));
                if (tr.isPlaying)
                {
                    const double stepsElapsed = tr.ppqPosition / juce::jmax (1.0e-4, beatsPerStep);
                    gatorStepPos = std::fmod (stepsElapsed, (double) juce::jmax (2, pp.gatorLength));
                    if (gatorStepPos < 0.0) gatorStepPos += pp.gatorLength;
                }
            }
            else
            {
                gStepInc = (float) (juce::jlimit (0.1f, 30.0f, pp.gatorRateHz) / sr);
            }
        }
        const float gAtkCoef = 1.0f - std::exp (-1.0f / juce::jmax (1.0e-4f, (0.0005f + 0.05f * pp.gatorAttack) * (float) sr));
        const float gRelCoef = 1.0f - std::exp (-1.0f / juce::jmax (1.0e-4f, (0.0010f + 0.15f * pp.gatorRelease) * (float) sr));

        const float oscSpread = clamp01 (pp.oscSpread);
        const int   filtK = 32;
        float lfo0Val = lfos[0].shapeValue (pp.lfo[0].shape, lfos[0].phase);

        for (int i = 0; i < n; ++i)
        {
            // ---- subtractive-synth oscillator bank ----
            float oscL = 0.0f, oscR = 0.0f;
            if (pp.oscLevel > 1.0e-4f && std::abs (blendGain) > 1.0e-4f)
            {
                for (int o = 0; o < params::numOscillators; ++o)
                {
                    const auto& O = pp.osc[(size_t) o];
                    if (! O.on || O.level <= 1.0e-4f) continue;
                    const float semis = O.tune + O.fine * 0.01f;
                    const float f = juce::jlimit (4.0f, (float) sr * 0.45f, baseHz * std::pow (2.0f, semis / 12.0f));
                    const double inc = (double) f / sr;
                    const float v = oscSample (O.wave, oscPhase[(size_t) o], inc, oscNoiseRng) * O.level;
                    const float pan = juce::jlimit (-1.0f, 1.0f, O.pan * (0.4f + 0.6f * oscSpread));
                    oscL += v * std::cos ((pan * 0.5f + 0.5f) * kPi * 0.5f);
                    oscR += v * std::sin ((pan * 0.5f + 0.5f) * kPi * 0.5f);
                }
                const float env = pp.synthDrone ? 1.0f : adsr.getNextSample();
                const float gmul = pp.oscLevel * velSynth * env * blendGain;
                oscL *= gmul;
                oscR *= gmul;
            }
            else if (! pp.synthDrone)
            {
                adsr.getNextSample();   // keep the envelope advancing
            }

            float sL = L[i] + oscL;
            float sR = (ch > 1 ? R[i] : L[i]) + oscR;

            // ---- filter ----
            if (pp.filterOn)
            {
                if ((i % filtK) == 0)
                {
                    const float cutMod = std::pow (2.0f, lfo0Val * pp.lfo[0].depth * 4.0f);
                    const float cut = juce::jlimit (20.0f, (float) sr * 0.45f, pp.cutoff * cutMod * velFilt);
                    svf.setCutoffFrequency (cut);
                }
                lfo0Val = lfos[0].advance (pp.lfo[0].shape, (double) filtLfoF / sr);

                float fL = svf.processSample (0, sL);
                float fR = svf.processSample (juce::jmin (1, numCh - 1), sR);
                if ((params::FilterType) pp.filterType == params::FilterType::notch)
                { fL = sL - fL; fR = sR - fR; }
                if (pp.drive > 1.0e-3f)
                {
                    const float d = 1.0f + 5.0f * pp.drive;
                    fL = std::tanh (fL * d) / std::tanh (d) * 0.9f;
                    fR = std::tanh (fR * d) / std::tanh (d) * 0.9f;
                }
                sL = fL; sR = fR;
            }
            else
            {
                lfos[0].advance (pp.lfo[0].shape, (double) filtLfoF / sr);
            }

            // ---- EQ ----
            if (pp.eqOn)
            {
                sL = eq[0][0].processSample (sL); sL = eq[1][0].processSample (sL); sL = eq[2][0].processSample (sL);
                sR = eq[0][1].processSample (sR); sR = eq[1][1].processSample (sR); sR = eq[2][1].processSample (sR);
            }

            // ---- gator ----
            if (pp.gatorOn)
            {
                const int len = juce::jlimit (2, params::gatorSteps, pp.gatorLength);
                const int step = ((int) gatorStepPos) % len;
                const bool open = pp.gatorPattern[(size_t) step];
                const float target = open ? 1.0f : (1.0f - clamp01 (pp.gatorDepth));
                const float coef = (target > gatorEnv) ? gAtkCoef : gRelCoef;
                gatorEnv += (target - gatorEnv) * coef;
                sL *= gatorEnv; sR *= gatorEnv;

                gatorStepPos += gStepInc;
                if (gatorStepPos >= len) gatorStepPos -= len;
            }

            // ---- tremolo (LFO 1) ----
            {
                const float lv = lfos[1].advance (pp.lfo[1].shape, (double) lfoFreq (1, tr) / sr);
                if (pp.lfo[1].depth > 1.0e-4f)
                {
                    const float tgain = 1.0f - pp.lfo[1].depth * (0.5f - 0.5f * lv);
                    sL *= tgain; sR *= tgain;
                }
            }

            // ---- auto-pan (LFO 2) ----
            {
                const float pv = lfos[2].advance (pp.lfo[2].shape, (double) lfoFreq (2, tr) / sr);
                if (pp.lfo[2].depth > 1.0e-4f)
                {
                    const float pan = pv * pp.lfo[2].depth;               // -1..1
                    const float a = (pan * 0.5f + 0.5f) * kPi * 0.5f;
                    sL *= std::cos (a) * 1.41421f;
                    sR *= std::sin (a) * 1.41421f;
                }
            }

            // ---- output ----
            sL = softclip (flushd (sL) * pp.masterGain);
            sR = softclip (flushd (sR) * pp.masterGain);
            L[i] = sL;
            if (ch > 1) R[i] = sR;
        }

        // ---- glitch stage (operates on the finished block) ----
        if (pp.glitchOn)
            processGlitch (buffer, tr);
    }

    // ------------------------------------------------------------------
    //  Glitch: rolling capture + per-slice beat-repeat / stutter /
    //  reverse / crush / tape-stop.
    // ------------------------------------------------------------------

    void PostChain::processGlitch (juce::AudioBuffer<float>& buffer, const TransportInfo& tr)
    {
        const int n  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        const int cap = glitchCapture.getNumSamples();
        if (cap < 64) return;

        float* L = buffer.getWritePointer (0);
        float* R = ch > 1 ? buffer.getWritePointer (1) : L;
        float* capL = glitchCapture.getWritePointer (0);
        float* capR = glitchCapture.getWritePointer (1);

        // slice length in samples
        double sliceSamples;
        if (pp.glitchSync && tr.bpm > 1.0)
            sliceSamples = params::syncDivBeats (pp.glitchDiv) * 60.0 / tr.bpm * sr;
        else
            sliceSamples = sr / juce::jlimit (0.5f, 30.0f, pp.glitchRateHz);
        sliceSamples = juce::jlimit (64.0, (double) cap * 0.9, sliceSamples);
        glitchSliceSamples = (int) sliceSamples;

        const float mix = clamp01 (pp.glitchMix);

        for (int i = 0; i < n; ++i)
        {
            const float dryL = L[i];
            const float dryR = (ch > 1 ? R[i] : L[i]);

            // always keep capturing the dry signal
            capL[glitchWrite] = dryL;
            capR[glitchWrite] = dryR;
            const int writeNow = glitchWrite;
            glitchWrite = (glitchWrite + 1) % cap;

            // slice boundary?
            if (glitchSlicePos >= 1.0 || glitchSliceSamples <= 0)
            {
                glitchSlicePos -= std::floor (glitchSlicePos);
                glitchActive = glitchRng.nextFloat() < clamp01 (pp.glitchAmount);

                if (glitchActive)
                {
                    glitchReadStart = writeNow;
                    glitchReadPos   = 0.0;

                    glitchRev = glitchRng.nextFloat() < clamp01 (pp.glitchReverse);

                    // stutter sub-slice: divide the slice into 1..8 repeats
                    const int maxDiv = 1 + (int) std::round (clamp01 (pp.glitchRepeat) * 7.0f);
                    const int div = 1 + glitchRng.nextInt (juce::jmax (1, maxDiv));
                    glitchSubSamples = juce::jmax (32, glitchSliceSamples / div);

                    // crush: hold every N samples
                    glitchCrushHold = 1 + (int) std::round (clamp01 (pp.glitchCrush) * 24.0f
                                        * glitchRng.nextFloat());
                    glitchCrushCount = 0;
                    glitchCrushL = glitchCrushR = 0.0f;

                    // pitch of the repeat
                    const float sp[] = { 1.0f, 1.0f, 0.5f, 2.0f, 0.75f, 1.5f };
                    glitchSpeed = sp[glitchRng.nextInt (6)];

                    // tape stop
                    glitchTapeStop = glitchRng.nextFloat() < clamp01 (pp.glitchTape);
                    glitchTapeSpeed = 1.0f;
                }
            }

            float wetL = dryL, wetR = dryR;

            if (glitchActive)
            {
                // position within the (possibly sub-) slice
                double rp = glitchReadPos;
                const int sub = juce::jmax (1, glitchSubSamples);
                double local = std::fmod (rp, (double) sub);
                if (glitchRev) local = (double) sub - 1.0 - local;

                double idx = (double) glitchReadStart + local;
                idx = std::fmod (idx + cap, (double) cap);
                const int i0 = (int) idx;
                const int i1 = (i0 + 1) % cap;
                const float fr = (float) (idx - (double) i0);
                wetL = capL[i0] + (capL[i1] - capL[i0]) * fr;
                wetR = capR[i0] + (capR[i1] - capR[i0]) * fr;

                // crush
                if (glitchCrushHold > 1)
                {
                    if (glitchCrushCount == 0) { glitchCrushL = wetL; glitchCrushR = wetR; }
                    glitchCrushCount = (glitchCrushCount + 1) % glitchCrushHold;
                    const float bits = 1.0f + 6.0f * (1.0f - clamp01 (pp.glitchCrush));
                    const float q = std::pow (2.0f, bits);
                    wetL = std::round (glitchCrushL * q) / q;
                    wetR = std::round (glitchCrushR * q) / q;
                }

                float adv = glitchSpeed;
                if (glitchTapeStop)
                {
                    glitchTapeSpeed = juce::jmax (0.0f, glitchTapeSpeed - 1.0f / (float) juce::jmax (1, glitchSliceSamples));
                    adv *= glitchTapeSpeed * glitchTapeSpeed;
                }
                glitchReadPos += adv;
            }

            L[i] = dryL + (wetL - dryL) * mix;
            if (ch > 1) R[i] = dryR + (wetR - dryR) * mix;

            glitchSlicePos += 1.0 / juce::jmax (1.0, (double) glitchSliceSamples);
        }
    }
}
