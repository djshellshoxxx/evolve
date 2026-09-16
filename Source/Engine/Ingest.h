#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include "SourceAnalyzer.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        SourcePool - how the colony eats a sample.

        The brief: dropped samples get "eaten by the evolved sound and take
        some form of the sound that was dropped", the player can drop as many
        as they like, and the result must never collapse into noise.

        That last constraint rules out the obvious implementation. Mixing N
        uncorrelated recordings together *is* the definition of noise - sum
        enough of them and you get a hiss with a spectrum shaped like the
        average of your sample library. So nothing here ever sums.

        Instead the digest buffer is stitched. Each new sample replaces a
        fraction of the buffer with crossfaded segments of itself, leaving the
        rest intact. The material stays structured no matter how many drops
        happen, and the colony ends up granulating a chimera assembled from
        pieces of everything it has been fed rather than an average of it.

        The replaced fraction also shrinks with each drop (60% for the first,
        asymptotically less after), so the newest sample can never completely
        displace what the colony already is - it takes *some* of the dropped
        sound's form, which is what was asked for.
        ------------------------------------------------------------------ */
    class SourcePool
    {
    public:
        static constexpr int   maxSamples   = 16;
        static constexpr float digestLength = 5.0f;    // seconds of working material

        void prepare (double sampleRate);

        /** Fold a new sample into the digest. Returns the fraction of the
            buffer that was replaced, for the score feed. */
        float digest (const juce::AudioBuffer<float>& incoming, double incomingRate,
                      const juce::String& name);

        /** Reset to a primitive seed. */
        void clear();

        const juce::AudioBuffer<float>& buffer() const { return digestBuffer; }
        double rate() const { return sr; }

        int  digestCount() const { return numDigested; }
        juce::StringArray eatenNames() const { return names; }
        bool hasMaterial() const { return numDigested > 0; }

    private:
        juce::AudioBuffer<float> digestBuffer;   // mono
        juce::AudioBuffer<float> scratch;
        double sr = 44100.0;
        int    numDigested = 0;
        juce::StringArray names;
        juce::Random rnd;
    };

    // =====================================================================

    /*  ------------------------------------------------------------------
        MicInput - live capture that refuses to howl.

        Feedback is the obvious hazard of "let the instrument listen to the
        room while it is playing into the room", so this is built defensively
        in three layers rather than one:

          1. By default the output is *muted while capturing*. No loop can
             form if the loop is not closed. This is the mode the button uses.

          2. A howl detector runs regardless. A feedback tone is narrow-band,
             persistent and growing - all three at once, which ordinary
             material almost never is. When it fires, capture aborts, the
             offending band is notched, and the output ducks.

          3. A hard output ceiling underneath both, so even a detector miss
             cannot become painful.

        Live monitoring is possible but opt-in, and the guard stays armed.
        ------------------------------------------------------------------ */
    class MicInput
    {
    public:
        enum class State { idle, armed, capturing, aborted };

        void prepare (double sampleRate, int maxBlock);
        void reset();

        void arm (bool shouldArm);
        bool isArmed() const { return state.load() != State::idle; }

        /** Begin capturing `seconds` of input. While capturing,
            shouldMuteOutput() is true unless live monitoring was enabled. */
        void startCapture (float seconds);
        void cancel();

        /** Audio thread. Returns true while a capture is running. */
        bool process (const float* input, int numSamples) noexcept;

        /** Audio thread: apply the guard to the outgoing block (ducking and
            notching if a howl was detected). */
        void protectOutput (juce::AudioBuffer<float>& out) noexcept;

        bool  shouldMuteOutput() const noexcept;
        State currentState() const noexcept { return state.load(); }
        float level() const noexcept { return inputLevel.load(); }
        float captureProgress() const noexcept;
        bool  consumeAbortFlag() noexcept { return aborted.exchange (false); }

        /** True once a capture finished and material is waiting. */
        bool  consumeReady() noexcept { return ready.exchange (false); }

        /** Message thread: the captured audio. Only valid after consumeReady(). */
        const juce::AudioBuffer<float>& captured() const { return captureBuffer; }
        int capturedLength() const { return capturedSamples; }

        void setLiveMonitoring (bool shouldMonitor) { liveMonitor = shouldMonitor; }
        bool liveMonitoring() const { return liveMonitor; }

        /** Hz of the detected howl, 0 if none. Shown in the UI so the player
            knows what happened rather than just losing the capture. */
        float howlFrequency() const noexcept { return howlHz.load(); }

    private:
        bool detectHowl (const float* input, int numSamples) noexcept;

        static constexpr int numBands = 24;
        float bandLo = 180.0f, bandHi = 9000.0f;

        struct Band
        {
            float z1 = 0.0f, z2 = 0.0f;
            float coeff = 0.1f, q = 0.02f;
            float energy = 0.0f;
        };
        std::array<Band, numBands> bands {};
        std::array<float, numBands> bandHz {};

        int   suspectBand = -1;
        float suspectSeconds = 0.0f;
        float rmsSlow = 0.0f, rmsFast = 0.0f;

        // the notch applied after a detection
        float notchHz = 0.0f, notchGain = 1.0f;
        float notchZ1[2] { 0.0f, 0.0f }, notchZ2[2] { 0.0f, 0.0f };
        float duck = 1.0f;

        juce::AudioBuffer<float> captureBuffer;
        int   capturedSamples = 0;
        int   wantSamples = 0;
        int   writePos = 0;

        std::atomic<State> state { State::idle };
        std::atomic<float> inputLevel { 0.0f };
        std::atomic<float> howlHz { 0.0f };
        std::atomic<bool>  aborted { false };
        std::atomic<bool>  ready { false };
        bool liveMonitor = false;

        double sr = 44100.0;
    };
}
