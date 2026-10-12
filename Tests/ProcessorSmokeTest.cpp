// Drives the real MutagenProcessor headlessly in Effect role with the Colony Morph
// turned up, feeds it audio, fires the game events that colour the effect, and
// checks that the output stays finite and bounded and that the morph changes the sound.

#include "../Source/PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <memory>

using namespace mutagen;

namespace
{
    void setParam (MutagenProcessor& p, const char* id, float actual)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (actual));
    }

    struct Run { double energy = 0.0; bool ok = true; float peak = 0.0f; };

    Run render (float morphMix, bool withEvents)
    {
        // Heap, as a real host does: the processor holds several megabytes of history and
        // payload arrays, which overflows Windows' 1 MB default stack.
        auto owned = std::make_unique<MutagenProcessor>();
        MutagenProcessor& p = *owned;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        setParam (p, "pluginRole", 1.0f);   // Effect
        setParam (p, "dryWet", 0.0f);       // 0 = fully dry: with the morph off the input passes straight through
        setParam (p, "morphMix", morphMix);
        setParam (p, "morphReact", 1.0f);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        juce::Random rng (1234);
        Run r;
        double phase = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            if (withEvents)
            {
                EngineCommand c;
                if (block == 40)  { c.type = CommandType::addEnzyme;   p.pushCommand (c); }
                if (block == 100) { c.type = CommandType::addCatalyst; p.pushCommand (c); }
                if (block == 160) { c.type = CommandType::addHeat; c.fa = 1.0f; p.pushCommand (c); }
                if (block == 220) { c.type = CommandType::gameEvent; c.ia = 5; p.pushCommand (c); }
                if (block == 260) { c.type = CommandType::gameEvent; c.ia = 7; p.pushCommand (c); }
            }
            for (int n = 0; n < 512; ++n)
            {
                phase += 2.0 * 3.14159265358979 * 220.0 / 48000.0;
                const float s = 0.4f * (float) std::sin (phase) + 0.1f * (rng.nextFloat() - 0.5f);
                buf.setSample (0, n, s);
                buf.setSample (1, n, s * 0.9f);
            }
            midi.clear();
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < 512; ++n)
                {
                    const float v = buf.getSample (ch, n);
                    if (! std::isfinite (v)) r.ok = false;
                    r.peak = std::fmax (r.peak, std::fabs (v));
                    if (block >= 20) r.energy += (double) v * (double) v;
                }
        }
        p.releaseResources();
        return r;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf ("processor object: %zu KB (Windows default stack is 1024 KB)\n", sizeof (MutagenProcessor) / 1024);
    const auto off = render (0.0f, false);
    const auto on  = render (1.0f, true);

    std::printf ("morph off: peak %.3f energy %.3f | morph on: peak %.3f energy %.3f\n",
                 off.peak, off.energy, on.peak, on.energy);

    const bool finite = off.ok && on.ok;
    const bool bounded = on.peak <= 4.0f;
    const bool passesWhenOff = off.energy > 1000.0;                          // morph off: the input comes through
    const double change = std::fabs (on.energy - off.energy) / (off.energy + 1.0e-9);
    const bool audibleWhenOn = on.energy > 1000.0 && change > 0.05;           // morph on: re-made, audible, different

    std::printf ("finite=%d bounded=%d passesWhenOff=%d audibleWhenOn=%d (change %.0f%%)\n",
                 (int) finite, (int) bounded, (int) passesWhenOff, (int) audibleWhenOn, change * 100.0);
    auto processor = std::make_unique<MutagenProcessor>();
    processor->setPlayConfigDetails (2, 2, 48000.0, 512);
    processor->prepareToPlay (48000.0, 512);
    setParam (*processor, "pluginRole", 1); setParam (*processor, "dryWet", 0); setParam (*processor, "morphMix", 0);
    juce::AudioBuffer<float> input (2, 512); input.clear();
    for (int i = 0; i < 512; ++i) { input.setSample (0, i, 0.2f); input.setSample (1, i, -0.1f); }
    juce::MidiBuffer midi;
    processor->processBlock (input, midi);
    juce::AudioBuffer<float> recorded;
    processor->copyRecentOutput (recorded, 512.0f / 48000.0f);
    const bool stereoCapture = recorded.getNumChannels() == 2 && recorded.getNumSamples() > 0
        && std::abs (recorded.getSample (0, recorded.getNumSamples() - 1) - recorded.getSample (1, recorded.getNumSamples() - 1)) > 0.1f;
    Genome genome;
    bool payloadBounded = true;
    for (int i = 0; i < 24; ++i)
    {
        EngineCommand c; c.type = CommandType::injectGenome; c.ia = 0; c.ib = 1;
        c.payloadIndex = processor->stageGenomePayload (genome);
        payloadBounded &= c.payloadIndex >= 0 && processor->pushCommand (c);
    }
    payloadBounded &= processor->stageGenomePayload (genome) == -1;
    processor->processBlock (input, midi);
    EngineCommand reuse; reuse.type = CommandType::injectGenome; reuse.ib = 1;
    reuse.payloadIndex = processor->stageGenomePayload (genome);
    payloadBounded &= reuse.payloadIndex >= 0 && processor->pushCommand (reuse);
    processor->savedRunScore.store (123456); processor->savedRunSeconds.store (78.5); processor->savedRunId.store (9876);
    juce::MemoryBlock state; processor->getStateInformation (state);
    auto restored = std::make_unique<MutagenProcessor>();
    restored->setStateInformation (state.getData(), (int) state.getSize());
    const bool scoreRestored = restored->savedRunScore.load() == 123456 && restored->savedRunSeconds.load() == 78.5
        && restored->savedRunId.load() == 9876;
    std::printf ("Integration: stereo=%d payloadOwnership=%d scoreState=%d\n", stereoCapture, payloadBounded, scoreRestored);
    return (stereoCapture && payloadBounded && scoreRestored && finite && bounded && passesWhenOff && audibleWhenOn) ? 0 : 1;
}
