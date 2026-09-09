#include "RenderEngine.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    RenderEngine::RenderEngine() : juce::Thread ("MutagenRender") {}

    RenderEngine::~RenderEngine()
    {
        stopThread (4000);
    }

    void RenderEngine::start (MutagenProcessor& processor, Job job)
    {
        if (isThreadRunning()) return;
        proc = &processor;
        current = job;
        progress01.store (0.0f);
        startThread (juce::Thread::Priority::normal);
    }

    void RenderEngine::run()
    {
        const float total = juce::jlimit (0.5f, 30.0f, current.seconds);
        const int steps = juce::jmax (1, (int) (total * 20.0f));

        for (int i = 0; i < steps && ! threadShouldExit(); ++i)
        {
            wait (50);
            progress01.store ((float) (i + 1) / (float) steps);
        }

        bool ok = false;
        juce::File out = current.destination;

        if (! threadShouldExit() && proc != nullptr)
        {
            juce::AudioBuffer<float> mono;
            proc->copyRecentOutput (mono, total);

            const double sr = proc->currentSampleRate();
            juce::AudioBuffer<float> stereo (2, mono.getNumSamples());
            stereo.copyFrom (0, 0, mono, 0, 0, mono.getNumSamples());
            stereo.copyFrom (1, 0, mono, 0, 0, mono.getNumSamples());

            if (out.getFileExtension().isEmpty()) out = out.withFileExtension ("wav");
            out.deleteFile();

            juce::WavAudioFormat wav;
            if (auto* stream = out.createOutputStream().release())
            {
                std::unique_ptr<juce::AudioFormatWriter> writer (
                    wav.createWriterFor (stream, sr, 2, 24, {}, 0));
                if (writer != nullptr)
                {
                    ok = writer->writeFromAudioSampleBuffer (stereo, 0, stereo.getNumSamples());
                }
                else
                {
                    delete stream;
                }
            }
        }

        progress01.store (1.0f);
        auto cb = onFinished;
        juce::MessageManager::callAsync ([cb, ok, out]
        {
            if (cb) cb (ok, out);
        });
    }
}
