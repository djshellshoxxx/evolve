#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../Parameters.h"

namespace mutagen
{
    /*  Everything the colony flows into once it has made a sound:

          colony  ->  (+/- subtractive-synth voice)  ->  filter (LFO)  ->  EQ
                  ->  gator  ->  tremolo (LFO)  ->  auto-pan (LFO)  ->  output

        A fourth LFO does not touch the audio at all - it modulates the colony's
        ecology (mutation rate by default). Oscillators, ADSR, LFOs and the
        gator can all lock to host tempo and react to MIDI.                     */

    struct TransportInfo
    {
        double bpm = 120.0;
        double ppqPosition = 0.0;
        bool   isPlaying = false;
    };

    struct PostParams
    {
        struct Osc
        {
            bool  on = false;
            int   wave = 0;         // params::OscWave
            float tune = 0.0f;      // semitones
            float fine = 0.0f;      // cents
            float level = 0.0f;
            float pan = 0.0f;       // -1..1
        };
        std::array<Osc, params::numOscillators> osc {};
        float oscLevel = 0.6f;
        bool  oscKeytrack = true;
        float oscFreeHz = 55.0f;
        float oscSpread = 0.4f;
        float oscBlend = 0.5f;      // -1 subtract .. +1 add

        float synthA = 0.05f, synthD = 0.2f, synthS = 0.8f, synthR = 0.3f;
        bool  synthDrone = false;

        bool  filterOn = false;
        int   filterType = 0;       // params::FilterType
        float cutoff = 1200.0f;
        float res = 0.25f;
        float drive = 0.0f;

        struct Lfo
        {
            bool  sync = false;
            float rateHz = 1.0f;
            int   div = 4;          // syncDivChoices index
            float depth = 0.0f;
            int   shape = 0;        // params::LfoShape
            float phase = 0.0f;     // start-phase offset 0..1
        };
        std::array<Lfo, params::numLfos> lfo {};
        int   envLfoDest = 0;       // params::EnvLfoDest

        bool  eqOn = false;
        float eqLowF = 110.0f,  eqLowG = 0.0f;
        float eqMidF = 900.0f,  eqMidG = 0.0f, eqMidQ = 0.9f;
        float eqHighF = 6000.0f, eqHighG = 0.0f;

        bool  gatorOn = false;
        bool  gatorSync = true;
        float gatorRateHz = 8.0f;
        int   gatorDiv = 6;
        int   gatorLength = 16;
        float gatorAttack = 0.15f, gatorRelease = 0.25f, gatorDepth = 1.0f;
        std::array<bool, params::gatorSteps> gatorPattern { {} };

        bool  glitchOn = false;
        float glitchAmount = 0.35f;
        bool  glitchSync = true;
        float glitchRateHz = 6.0f;
        int   glitchDiv = 4;
        float glitchRepeat = 0.5f;
        float glitchReverse = 0.3f;
        float glitchCrush = 0.0f;
        float glitchTape = 0.15f;
        float glitchMix = 1.0f;

        // MIDI reactivity
        bool  midiReactive = true;
        bool  lfoKeyRetrigger = false;
        bool  gatorRetrigger = true;
        float bendRange = 2.0f;
        float velToSynth = 1.0f;
        float velToFilter = 0.0f;

        float masterGain = 0.9f;

        // live MIDI state (not parameters)
        float pitchBend = 0.0f;    // -1..1
        float modWheel = 0.0f;     // 0..1  (already routed by the processor)
        float lastVelocity = 0.8f;
        int   heldNotes = 0;
        int   lastNote = -1;
    };

    class PostChain
    {
    public:
        PostChain();

        void prepare (double sampleRate, int maxBlock, int numChannels);
        void reset();

        void setParams (const PostParams& p) { pp = p; }

        void noteOn (int note, float velocity);
        void noteOff (int note);
        void allNotesOff();
        void setPitchBend (float norm) { pp.pitchBend = norm; }
        void setModWheel (float v)     { pp.modWheel = v; }

        void process (juce::AudioBuffer<float>& buffer, const TransportInfo&);

        /** LFO 3, advanced by the audio thread, for the processor to fold into
            the ecology on the following block. Value is -1..1. */
        float environmentLfoValue() const { return envLfoValue; }
        float environmentLfoDepth() const { return pp.lfo[params::envLfoIndex].depth; }
        int   environmentLfoDest()  const { return pp.envLfoDest; }

    private:
        struct Lfo
        {
            double phase = 0.0;
            float  shHold = 0.0f, shPrev = 0.0f, shNext = 0.0f;
            juce::Random rng { 0x1234 };

            void retrigger (float startPhase) { phase = startPhase; }
            float shapeValue (int shape, double ph);
            float advance (int shape, double incr);   // returns -1..1, steps phase by incr
        };

        float lfoFreq (int i, const TransportInfo&) const;
        float oscSample (int wave, double& phase, double inc, juce::Random& rng);

        PostParams pp;
        double sr = 44100.0;
        int    numCh = 2;

        std::array<Lfo, params::numLfos> lfos;
        std::array<double, params::numOscillators> oscPhase { {} };
        juce::Random oscNoiseRng { 0x9E37 };

        juce::ADSR adsr;
        juce::ADSR::Parameters adsrParams;

        juce::dsp::StateVariableTPTFilter<float> svf;

        // 3 EQ bands x 2 channels
        std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, 3> eq;

        double gatorStepPos = 0.0;   // in steps
        float  gatorEnv = 1.0f;

        // ---- glitch ----
        void processGlitch (juce::AudioBuffer<float>& buffer, const TransportInfo&);
        juce::AudioBuffer<float> glitchCapture;   // rolling stereo capture
        int    glitchWrite = 0;
        double glitchSlicePos = 0.0;              // 0..1 through the current slice
        int    glitchSliceSamples = 22050;
        bool   glitchActive = false;             // is the current slice glitched?
        int    glitchReadStart = 0;              // capture index the slice froze at
        double glitchReadPos = 0.0;
        float  glitchSpeed = 1.0f;
        bool   glitchRev = false;
        int    glitchSubSamples = 4096;          // stutter sub-slice length
        int    glitchCrushHold = 1;              // sample-rate reduction factor
        float  glitchCrushL = 0.0f, glitchCrushR = 0.0f;
        int    glitchCrushCount = 0;
        float  glitchTapeSpeed = 1.0f;           // 1 -> slows to 0 on tape stop
        bool   glitchTapeStop = false;
        juce::Random glitchRng { 0xB16B00B5 };

        float  envLfoValue = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PostChain)
    };
}
