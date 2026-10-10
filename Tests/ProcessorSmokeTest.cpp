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
    return (finite && bounded && passesWhenOff && audibleWhenOn) ? 0 : 1;
}
