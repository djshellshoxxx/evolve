#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace mutagen
{
    /*  ------------------------------------------------------------------
        What the colony currently sounds like, measured rather than assumed.

        Three of the brief's hardest requirements are really one measurement
        problem:

          "it must never devolve into noise"      -> spectral flatness
          "it must never go static"               -> spectral flux
          "mutations should favour appealing sounds" -> roughness + tonalness

        Spectral flatness (Wiener entropy) is the geometric mean of the power
        spectrum over its arithmetic mean. A pure tone concentrates its energy
        in a few bins and scores near 0; white noise spreads energy evenly and
        scores near 1. It is the standard measure for exactly the tonal/noisy
        judgement the game needs, and it is what turns the visuals grey and
        freezes the score.

        Roughness uses the Plomp-Levelt curve as parameterised by Sethares,
        summed pairwise over the strongest spectral peaks. Roughness is the
        dominant predictor of perceived dissonance, so "low roughness plus
        high tonalness plus a comfortable centroid" is a serviceable
        computational stand-in for "appealing", which is what the mutation
        bias needs in order to gravitate somewhere other than noise.

        Everything is fixed-size and computed in place; safe on the audio
        thread after prepare().
        ------------------------------------------------------------------ */

    struct AudioDescriptors
    {
        // ---- instantaneous ------------------------------------------------
        float flatness   = 0.25f;   // 0 tonal .. 1 noise (Wiener entropy)
        float centroid   = 0.35f;   // normalised spectral centre of gravity
        float flux       = 0.0f;    // frame-to-frame spectral change
        float roughness  = 0.15f;   // sensory dissonance, 0 smooth .. 1 harsh
        float tonalness  = 0.75f;   // 1 - flatness, smoothed
        float rms        = 0.0f;
        float crest      = 0.0f;    // peak / rms, catches "it is just a buzz"

        // ---- long-window --------------------------------------------------
        float variety    = 0.5f;    // how much the descriptors have been moving
        float stasis     = 0.0f;    // 1 = nothing has changed for a long time
        float appeal     = 0.5f;    // psychoacoustic pleasantness, 0..1

        // ---- verdicts -----------------------------------------------------
        /** Sustained high flatness. The score freezes while this is true and
            the visuals go grey; the player clears it by removing elements. */
        bool  noiseLocked = false;

        /** Sustained near-zero flux. The ecology forces movement when true. */
        bool  stuck = false;

        /** 0..1 how deep into the noise state we are, for the grey ramp. */
        float greyness = 0.0f;

        /** Uncalibrated whitened flatness, exposed so the test can re-derive
            the calibration constants rather than trusting them. */
        float rawFlatness = 0.0f;

        /** How periodic the waveform is, from the autocorrelation: 1 = a
            perfectly repeating waveform, 0 = no repetition at any musical lag.
            This is the measurement that survives vibrato, which whitened
            flatness does not - see the long note in analyseFrame(). */
        float periodicity = 0.0f;
    };

    class DescriptorAnalyser
    {
    public:
        DescriptorAnalyser();

        void prepare (double sampleRate);
        void reset();

        /** Feed the colony's output. Analyses on a fixed hop; cheap enough to
            call every block from the audio thread. */
        void process (const juce::AudioBuffer<float>& buffer) noexcept;

        const AudioDescriptors& current() const noexcept { return desc; }

        /** The magnitude spectrum of the most recent frame, normalised 0..1.
            Copied into the snapshot so the GUI can draw a live spectrum. */
        const float* spectrum() const noexcept { return magNorm.data(); }
        static constexpr int spectrumBins = 256;

    private:
        void analyseFrame() noexcept;
        float computeRoughness() noexcept;

        /*  Where each raw measurement sits for material at either extreme.
            Measured, not assumed - see the calibration pass in the test, which
            re-prints all of these on every run so drift is visible.

            The flatness anchors had drifted badly: `flatCalHigh` said noise
            began at 0.62 while the test was measuring white noise at 0.843, so
            every colony above 0.62 - which was all of them - mapped to exactly
            1.000 and the whole top half of the scale was unreachable. */
        static constexpr float flatCalLow  = 0.13f;   // clearly tonal
        static constexpr float flatCalHigh = 0.85f;   // clearly noise

        /*  ... and for periodicity, which runs the other way round. */
        static constexpr float periCalLow  = 0.06f;   // clearly noise
        static constexpr float periCalHigh = 0.55f;   // clearly periodic

        /*  2048, not 1024.

            At 1024 and 44.1 kHz a bin is 43 Hz, so the harmonics of anything
            with a low fundamental are less than three bins apart and the
            whitening window cannot separate a partial from its neighbours -
            every dense sound whitened to flat and read as noise. 2048 halves
            the bin width and makes the measurement work on real material; at
            a 1024 hop it still only runs about 43 times a second. */
        static constexpr int fftOrder = 11;          // 2048
        static constexpr int fftSize  = 1 << fftOrder;
        static constexpr int hopSize  = fftSize / 2;
        static constexpr int numBins  = fftSize / 2;

        juce::dsp::FFT fft { fftOrder };
        std::array<float, fftSize> window {};
        std::array<float, fftSize * 2> fftBuffer {};
        std::array<float, fftSize> ring {};
        std::array<float, numBins> mag {};
        std::array<float, numBins> prevMag {};
        std::array<float, numBins + 1> prefix {};   // running sum, for the whitening window

        /*  Autocorrelation, computed as the inverse transform of the power
            spectrum. `acfBuffer` needs the full complex layout JUCE's
            real-only transforms use; `winAcf` is the analysis window's own
            autocorrelation, which every lag has to be divided by or long lags
            are penalised simply for having fewer overlapping samples. */
        std::array<float, fftSize * 2> acfBuffer {};
        std::array<float, fftSize> winAcf {};

        /*  The musical lag range the periodicity search covers: about 1.4 kHz
            down to 40 Hz. Below the bottom of this the window does not hold a
            whole cycle; above the top, a lag that short is still inside the
            correlation any filtered noise has with itself. */
        static constexpr int minLag = 32;
        static constexpr int maxLag = fftSize / 2;
        std::array<float, spectrumBins> magNorm {};

        // peak list for the roughness calculation
        static constexpr int maxPeaks = 12;
        float peakHz[maxPeaks] {};
        float peakAmp[maxPeaks] {};
        int   numPeaks = 0;

        int   ringPos = 0;
        int   sinceHop = 0;
        bool  primed = false;
        double sr = 44100.0;

        AudioDescriptors desc;

        // long-window trackers
        float centroidMean = 0.35f, centroidVar = 0.0f;
        float flatMean = 0.25f;
        float fluxMean = 0.0f;
        float noiseSeconds = 0.0f;
        float quietSeconds = 0.0f;
        float frameSeconds = 0.01f;
    };
}
