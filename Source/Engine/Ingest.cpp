#include "Ingest.h"
#include <cmath>

namespace mutagen
{
    // =====================================================================
    //  SourcePool
    // =====================================================================

    void SourcePool::prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int n = (int) (digestLength * sr);
        digestBuffer.setSize (1, n, false, true, false);
        scratch.setSize (1, n, false, true, false);
        clear();
    }

    void SourcePool::clear()
    {
        digestBuffer.clear();
        numDigested = 0;
        names.clear();

        // Seed with something structured rather than silence, so the very
        // first colony has material to granulate before anything is dropped.
        const int n = digestBuffer.getNumSamples();
        if (n <= 0) return;
        auto* d = digestBuffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            d[i] = 0.35f * std::sin (juce::MathConstants<float>::twoPi * 110.0f * t)
                 * std::exp (-std::fmod (t, 0.7f) * 3.0f);
        }
    }

    float SourcePool::digest (const juce::AudioBuffer<float>& incoming, double incomingRate,
                              const juce::String& name)
    {
        const int n = digestBuffer.getNumSamples();
        const int inN = incoming.getNumSamples();
        if (n <= 0 || inN <= 4) return 0.0f;

        // ---- mono-sum and resample the incoming material -----------------
        const double ratio = incomingRate > 0.0 ? incomingRate / sr : 1.0;
        const int chans = juce::jmax (1, incoming.getNumChannels());

        auto readIncoming = [&] (double pos) -> float
        {
            if (pos < 0.0) pos = 0.0;
            const int i0 = (int) pos;
            if (i0 >= inN - 1) return 0.0f;
            const float f = (float) (pos - (double) i0);
            float a = 0.0f, b = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                a += incoming.getReadPointer (c)[i0];
                b += incoming.getReadPointer (c)[i0 + 1];
            }
            a /= (float) chans; b /= (float) chans;
            return a + (b - a) * f;
        };

        const int availableResampled = (int) ((double) inN / ratio);
        if (availableResampled < 64) return 0.0f;

        // ---- peak-normalise the incoming so one quiet drop is not ignored
        float peak = 0.0f;
        for (int c = 0; c < chans; ++c)
            peak = juce::jmax (peak, incoming.getMagnitude (c, 0, inN));
        const float inGain = peak > 1.0e-5f ? 0.9f / peak : 1.0f;

        /*  How much of the digest this drop is allowed to take.

            The first sample gets most of the buffer; later ones get less and
            less. This is what stops the twentieth drop from being the only
            thing you can hear, and it is why the colony keeps "some form of"
            everything it has eaten.                                        */
        const float share = 0.60f / (1.0f + 0.55f * (float) numDigested);

        // ---- stitch segments in, crossfaded. Never summed. ----------------
        auto* d = digestBuffer.getWritePointer (0);

        const int segLen = juce::jlimit (1024, n / 3, (int) (sr * (0.12f + rnd.nextFloat() * 0.35f)));
        const int fade   = juce::jmin (segLen / 3, (int) (sr * 0.02));
        int placed = 0;
        const int wantPlaced = (int) ((float) n * share);
        int guard = 0;

        while (placed < wantPlaced && guard++ < 64)
        {
            const int dstStart = rnd.nextInt (juce::jmax (1, n - segLen));
            const double srcStart = rnd.nextDouble() * (double) juce::jmax (1, availableResampled - segLen - 2);

            for (int i = 0; i < segLen; ++i)
            {
                const int di = dstStart + i;
                if (di >= n) break;

                const float s = readIncoming ((srcStart + (double) i) * ratio) * inGain;

                // equal-power crossfade at both ends of the segment so the
                // seams do not click - clicks would read as noise and the
                // whole point is to avoid manufacturing noise
                float w = 1.0f;
                if (i < fade)                 w = (float) i / (float) fade;
                else if (i > segLen - fade)   w = (float) (segLen - i) / (float) fade;
                w = std::sin (w * juce::MathConstants<float>::halfPi);

                d[di] = d[di] * (1.0f - w) + s * w;
            }
            placed += segLen;
        }

        // ---- gentle global normalise --------------------------------------
        const float mag = digestBuffer.getMagnitude (0, 0, n);
        if (mag > 1.0e-5f && mag > 0.99f)
            digestBuffer.applyGain (0.99f / mag);

        ++numDigested;
        names.add (name);
        if (names.size() > maxSamples) names.remove (0);

        return share;
    }

    // =====================================================================
    //  MicInput
    // =====================================================================

    void MicInput::prepare (double sampleRate, int maxBlock)
    {
        juce::ignoreUnused (maxBlock);
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;

        captureBuffer.setSize (1, (int) (sr * 10.0), false, true, false);
        captureBuffer.clear();

        // Log-spaced bandpass bank for howl detection.
        for (int i = 0; i < numBands; ++i)
        {
            const float t = (float) i / (float) (numBands - 1);
            const float hz = bandLo * std::pow (bandHi / bandLo, t);
            bandHz[(size_t) i] = hz;
            auto& b = bands[(size_t) i];
            b.coeff = juce::jlimit (0.002f, 0.45f, 2.0f * hz / (float) sr);
            b.q = 0.035f;          // narrow: feedback tones are narrow
            b.z1 = b.z2 = 0.0f;
            b.energy = 0.0f;
        }
        reset();
    }

    void MicInput::reset()
    {
        state.store (State::idle);
        capturedSamples = 0;
        writePos = 0;
        wantSamples = 0;
        suspectBand = -1;
        suspectSeconds = 0.0f;
        rmsSlow = rmsFast = 0.0f;
        notchHz = 0.0f;
        notchGain = 1.0f;
        duck = 1.0f;
        howlHz.store (0.0f);
        aborted.store (false);
        ready.store (false);
        for (auto& b : bands) { b.z1 = b.z2 = 0.0f; b.energy = 0.0f; }
    }

    void MicInput::arm (bool shouldArm)
    {
        if (shouldArm)
        {
            if (state.load() == State::idle || state.load() == State::aborted)
                state.store (State::armed);
        }
        else
        {
            state.store (State::idle);
            writePos = 0;
        }
    }

    void MicInput::startCapture (float seconds)
    {
        if (state.load() != State::armed) return;
        wantSamples = juce::jlimit (1024, captureBuffer.getNumSamples(),
                                    (int) (seconds * sr));
        writePos = 0;
        capturedSamples = 0;
        suspectBand = -1;
        suspectSeconds = 0.0f;
        captureBuffer.clear();
        state.store (State::capturing);
    }

    void MicInput::cancel()
    {
        if (state.load() == State::capturing) state.store (State::armed);
        writePos = 0;
    }

    bool MicInput::shouldMuteOutput() const noexcept
    {
        // Layer 1: if we are not monitoring, the loop is physically open.
        return state.load() == State::capturing && ! liveMonitor;
    }

    float MicInput::captureProgress() const noexcept
    {
        if (wantSamples <= 0) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, (float) writePos / (float) wantSamples);
    }

    // ---------------------------------------------------------------------

    bool MicInput::detectHowl (const float* input, int numSamples) noexcept
    {
        /*  A feedback tone is three things at once: narrow-band, persistent in
            the *same* band, and growing. Music is regularly one or two of
            those; it is almost never all three for half a second. Requiring
            the conjunction is what keeps this from aborting on a sustained
            note.                                                            */

        const float dt = (float) numSamples / (float) sr;
        const float decay = std::exp (-dt / 0.10f);

        float total = 0.0f;
        int   loudest = 0;
        float loudestEnergy = 0.0f;

        for (int i = 0; i < numBands; ++i)
        {
            auto& b = bands[(size_t) i];
            float acc = 0.0f;
            for (int n = 0; n < numSamples; ++n)
            {
                // state-variable style bandpass
                b.z1 += b.coeff * (input[n] - b.z1 - b.z2 / (b.q + 0.0001f) * 0.02f);
                b.z2 += b.coeff * b.z1;
                acc += b.z1 * b.z1;
            }
            acc = std::sqrt (acc / (float) numSamples);
            b.energy = b.energy * decay + acc * (1.0f - decay);

            total += b.energy;
            if (b.energy > loudestEnergy) { loudestEnergy = b.energy; loudest = i; }
        }

        const float mean = total / (float) numBands;

        // overall level, two time constants
        float sum = 0.0f;
        for (int n = 0; n < numSamples; ++n) sum += input[n] * input[n];
        const float rms = std::sqrt (sum / (float) juce::jmax (1, numSamples));
        inputLevel.store (juce::jlimit (0.0f, 1.0f, rms * 4.0f));

        rmsFast += (rms - rmsFast) * (1.0f - std::exp (-dt / 0.08f));
        rmsSlow += (rms - rmsSlow) * (1.0f - std::exp (-dt / 0.90f));

        const bool narrow    = mean > 1.0e-7f && (loudestEnergy / mean) > 7.0f;
        const bool growing   = rmsFast > rmsSlow * 1.25f && rms > 0.02f;
        const bool sameBand  = loudest == suspectBand;

        if (narrow && sameBand) suspectSeconds += dt;
        else if (narrow)        { suspectBand = loudest; suspectSeconds = 0.0f; }
        else                    { suspectSeconds = juce::jmax (0.0f, suspectSeconds - dt * 2.0f); }

        if (suspectSeconds > 0.45f && growing)
        {
            howlHz.store (bandHz[(size_t) juce::jlimit (0, numBands - 1, loudest)]);
            return true;
        }
        return false;
    }

    bool MicInput::process (const float* input, int numSamples) noexcept
    {
        if (input == nullptr || numSamples <= 0) return false;

        const auto st = state.load();
        if (st == State::idle) return false;

        // Layer 2 runs whenever the mic is armed, not only while capturing -
        // the room can start ringing before the player presses capture.
        if (detectHowl (input, numSamples))
        {
            notchHz = howlHz.load();
            notchGain = 0.0f;                 // full notch, relaxes back
            duck = 0.25f;
            if (st == State::capturing)
            {
                state.store (State::aborted);
                aborted.store (true);
                writePos = 0;
                return false;
            }
        }

        if (st != State::capturing) return false;

        const int room = juce::jmin (numSamples, wantSamples - writePos);
        if (room > 0)
        {
            auto* d = captureBuffer.getWritePointer (0);
            for (int n = 0; n < room; ++n) d[writePos + n] = input[n];
            writePos += room;
        }

        if (writePos >= wantSamples)
        {
            capturedSamples = writePos;
            state.store (State::armed);
            ready.store (true);
            return false;
        }
        return true;
    }

    void MicInput::protectOutput (juce::AudioBuffer<float>& out) noexcept
    {
        const int n = out.getNumSamples();
        const int chans = juce::jmin (2, out.getNumChannels());
        if (n <= 0 || chans <= 0) return;

        // Layer 1: silence while capturing without monitoring.
        if (shouldMuteOutput())
        {
            out.clear();
            return;
        }

        if (duck >= 0.999f && notchGain >= 0.999f) return;

        // Layer 2 aftermath: notch the offending frequency and duck, both
        // relaxing back over a second or so. If the howl is still there the
        // detector simply re-arms them.
        const float w = notchHz > 20.0f
            ? juce::jlimit (0.001f, 0.45f, 2.0f * notchHz / (float) sr) : 0.0f;

        for (int c = 0; c < chans; ++c)
        {
            float* d = out.getWritePointer (c);
            float z1 = notchZ1[c], z2 = notchZ2[c];

            for (int i = 0; i < n; ++i)
            {
                float v = d[i];

                if (w > 0.0f)
                {
                    z1 += w * (v - z1 - z2 * 0.5f);
                    z2 += w * z1;
                    v -= z1 * (1.0f - notchGain);      // subtract the band
                }

                d[i] = v * duck;
            }
            notchZ1[c] = std::isfinite (z1) ? z1 : 0.0f;
            notchZ2[c] = std::isfinite (z2) ? z2 : 0.0f;
        }

        // Layer 3: a hard ceiling underneath everything else.
        for (int c = 0; c < chans; ++c)
        {
            float* d = out.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = juce::jlimit (-0.98f, 0.98f, d[i]);
        }

        const float relax = (float) n / (float) sr;
        duck += (1.0f - duck) * juce::jmin (1.0f, relax / 1.2f);
        notchGain += (1.0f - notchGain) * juce::jmin (1.0f, relax / 2.5f);
        if (notchGain > 0.995f) { notchHz = 0.0f; notchGain = 1.0f; }
    }
}
