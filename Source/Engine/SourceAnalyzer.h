#pragma once

#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"

namespace mutagen
{
    /*  The distilled seed. Everything a colony needs to germinate from a chunk
        of audio: the raw material to granulate, plus a cheap analysis of where
        the transients, tone and noise live so the three species can be given
        the parts of the sound they each care about.                           */
    struct SourceMaterial
    {
        juce::AudioBuffer<float> mono;          // the raw seed (mono, normalised)
        double sampleRate = 44100.0;

        std::vector<int>   transients;          // sample indices of detected onsets
        std::vector<float> avgSpectrum;         // long-term magnitude spectrum (log bins)
        float fundamentalHz = 110.0f;
        float noisiness     = 0.3f;             // 0 tonal .. 1 noisy
        float brightness    = 0.5f;             // spectral centroid, normalised
        float formantHz     = 700.0f;           // dominant resonance
        float decayRate     = 0.5f;             // how fast energy leaves (0 slow .. 1 fast)
        bool  valid         = false;

        int   numSamples() const { return mono.getNumSamples(); }

        // A safe read with linear interpolation and wrap-around.
        float readInterp (double pos) const
        {
            const int n = mono.getNumSamples();
            if (n <= 1) return 0.0f;
            pos = std::fmod (pos, (double) n);
            if (pos < 0.0) pos += (double) n;
            const int i0 = (int) pos;
            const int i1 = (i0 + 1) % n;
            const float f = (float) (pos - (double) i0);
            const float* d = mono.getReadPointer (0);
            return d[i0] + (d[i1] - d[i0]) * f;
        }
    };

    class SourceAnalyzer
    {
    public:
        SourceAnalyzer();

        /** Analyse an arbitrary input buffer into SourceMaterial. Mono-sums,
            trims silence, peak-normalises, then runs onset / spectral analysis. */
        SourceMaterial analyse (const juce::AudioBuffer<float>& input,
                                double sampleRate,
                                float transientSensitivity) const;

        /** Build a primitive seed (noise or impulse train) of the given length. */
        SourceMaterial makePrimitive (params::SourceMode mode,
                                      double sampleRate,
                                      float lengthSeconds) const;

    private:
        static constexpr int fftOrder = 11;               // 2048
        static constexpr int fftSize  = 1 << fftOrder;

        mutable juce::dsp::FFT fft { fftOrder };
        mutable juce::dsp::WindowingFunction<float> window
            { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    };
}
