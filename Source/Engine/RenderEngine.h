#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  "Render" for a living instrument. Rather than trying to re-simulate the
        colony offline (which would lose the loaded source audio), this records
        the colony's real output going forward for a chosen duration and writes
        it to a WAV - exactly what the musician just heard the colony become.   */
    class RenderEngine : private juce::Thread
    {
    public:
        RenderEngine();
        ~RenderEngine() override;

        struct Job
        {
            juce::File   destination;
            float        seconds = 8.0f;
            bool         alsoStems = false;     // reserved for species stems
        };

        void start (MutagenProcessor& processor, Job job);
        bool isBusy() const { return isThreadRunning(); }
        float progress() const { return progress01.load(); }

        std::function<void (bool ok, juce::File)> onFinished;   // called on the message thread

    private:
        void run() override;

        MutagenProcessor* proc = nullptr;
        Job current;
        std::atomic<float> progress01 { 0.0f };
    };
}
