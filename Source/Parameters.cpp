#include "Parameters.h"

namespace mutagen::params
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;

    static Range unit()               { return { 0.0f, 1.0f, 0.0001f }; }
    static Range bipolar()            { return { -1.0f, 1.0f, 0.0001f }; }

    juce::StringArray sourceModeChoices()
    {
        return { "Sample", "Live Input", "Primitive: Noise", "Primitive: Impulse", "Preserved Organism" };
    }
    juce::StringArray cpuQualityChoices() { return { "Eco", "Balanced", "Pristine" }; }
    juce::StringArray pluginRoleChoices() { return { "Instrument", "Effect", "Hybrid" }; }
    juce::StringArray oscWaveChoices()    { return { "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise" }; }
    juce::StringArray filterTypeChoices() { return { "Low Pass", "High Pass", "Band Pass", "Notch" }; }
    juce::StringArray lfoShapeChoices()
    {
        return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Random Smooth" };
    }
    juce::StringArray syncDivChoices()
    {
        return { "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }
    double syncDivBeats (int index)
    {
        static const double beats[] = { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[juce::jlimit (0, 8, index)];
    }

    juce::String oscParam (int index, const char* leaf)
    {
        return "osc" + juce::String (index + 1) + "_" + leaf;
    }
    juce::String lfoParam (int index, const char* leaf)
    {
        return "lfo" + juce::String (index + 1) + "_" + leaf;
    }
    juce::String gatorStepParam (int step)
    {
        return "gatorStep" + juce::String (step + 1);
    }
    juce::StringArray modWheelDestChoices()
    {
        return { "Off", "Filter Cutoff", "Synth Blend", "Mutation", "Nutrients",
                 "LFO Depth", "Gator Depth" };
    }
    juce::StringArray envLfoDestChoices()
    {
        return { "Mutation", "Mutation Depth", "Nutrients", "Selection",
                 "Radiation", "Temperature", "Fertility" };
    }

    int maxCellsFor (CpuQuality q)
    {
        switch (q)
        {
            case CpuQuality::eco:      return 28;
            case CpuQuality::balanced: return 56;
            case CpuQuality::pristine: return 112;
        }
        return 56;
    }

    static auto pid (const char* s, int v = 1) { return juce::ParameterID { s, v }; }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto addF = [&] (const char* id, const juce::String& name, Range r, float def,
                         const juce::String& unitLabel = {})
        {
            auto attr = juce::AudioParameterFloatAttributes().withLabel (unitLabel);
            layout.add (std::make_unique<APF> (pid (id), name, r, def, attr));
        };

        // ---- five large environment controls ----
        addF (nutrients,  "Nutrients",  unit(), 0.55f);
        addF (mutation,   "Mutation",   unit(), 0.30f);
        addF (selection,  "Selection",  unit(), 0.35f);
        addF (metabolism, "Metabolism", unit(), 0.50f);
        addF (stability,  "Stability",  unit(), 0.60f);

        // ---- deeper ecology ----
        addF (fertility,     "Fertility",      unit(), 0.50f);
        addF (mutationDepth, "Mutation Depth", unit(), 0.35f);
        addF (radiation,     "Radiation",      unit(), 0.05f);
        addF (temperature,   "Temperature",    unit(), 0.25f);
        addF (competition,   "Competition",    unit(), 0.45f);
        addF (symbiosis,     "Symbiosis",      unit(), 0.35f);
        addF (lifespan,      "Lifespan",       unit(), 0.55f);
        addF (apoptosis,     "Apoptosis",      unit(), 0.20f);
        addF (diversity,     "Diversity",      unit(), 0.50f);
        addF (migration,     "Migration",      unit(), 0.30f);

        // ---- selection targets ----
        addF (selBrightness,  "Select: Bright",   bipolar(), 0.0f);
        addF (selDensity,     "Select: Dense",    bipolar(), 0.0f);
        addF (selHarmonicity, "Select: Harmonic", bipolar(), 0.0f);
        addF (selAggression,  "Select: Aggressive", bipolar(), 0.0f);
        addF (selDivergence,  "Select: Diverge",  unit(), 0.0f);

        // ---- germination ----
        addF (captureLength,     "Capture Length", Range { 0.1f, 12.0f, 0.01f, 0.5f }, 3.0f, "s");
        addF (transientSens,     "Transient Sensitivity", unit(), 0.5f);
        addF (initialPopulation, "Initial Population", Range { 0.0f, 1.0f, 0.001f }, 0.4f);
        addF (distGrain,         "Grain Share",     unit(), 0.34f);
        addF (distSpectral,      "Spectral Share",  unit(), 0.33f);
        addF (distResonator,     "Resonator Share", unit(), 0.33f);
        layout.add (std::make_unique<APC> (pid (sourceMode), "Source", sourceModeChoices(), 2));

        // ---- lifecycle / identity ----
        layout.add (std::make_unique<APB> (pid (exploreMode), "Explore Mode", true));
        addF (memory, "Memory", unit(), 0.5f);
        layout.add (std::make_unique<APC> (pid (cpuQuality), "CPU Quality", cpuQualityChoices(), 1));
        layout.add (std::make_unique<APC> (pid (pluginRole), "Role", pluginRoleChoices(), 2));

        // ---- output ----
        addF (masterGain, "Output", Range { 0.0f, 1.5f, 0.0001f }, 0.9f);
        addF (dryWet,     "Dry/Wet", unit(), 1.0f);

        // ---- performance macros ----
        addF (macroGrowth,   "M: Growth",   unit(), 0.5f);
        addF (macroMutation, "M: Mutation", unit(), 0.3f);
        addF (macroStress,   "M: Stress",   unit(), 0.2f);
        addF (macroDensity,  "M: Density",  unit(), 0.5f);
        addF (macroBody,     "M: Body",     unit(), 0.5f);
        addF (macroVoice,    "M: Voice",    unit(), 0.5f);
        addF (macroMovement, "M: Movement", unit(), 0.4f);
        addF (macroDecay,    "M: Decay",    unit(), 0.5f);
        addF (xyStability,   "XY: Stability", unit(), 0.6f);
        addF (xyRepro,       "XY: Reproductive Aggression", unit(), 0.4f);

        // =================================================================
        //  Post-colony rack
        // =================================================================
        auto addFS = [&] (const juce::String& id, const juce::String& name, Range r, float def,
                          const juce::String& unitLabel = {})
        {
            auto attr = juce::AudioParameterFloatAttributes().withLabel (unitLabel);
            layout.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, attr));
        };
        auto addBS = [&] (const juce::String& id, const juce::String& name, bool def)
        {
            layout.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def));
        };
        auto addCS = [&] (const juce::String& id, const juce::String& name,
                          const juce::StringArray& choices, int def)
        {
            layout.add (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, choices, def));
        };

        const Range freqRange { 20.0f, 20000.0f, 0.0f, 0.25f };
        const Range lfoRate   { 0.01f, 40.0f, 0.0f, 0.3f };

        // ---- subtractive-synth oscillator bank ----
        for (int i = 0; i < numOscillators; ++i)
        {
            const juce::String n = "Osc " + juce::String (i + 1) + " ";
            addBS (oscParam (i, "on"),    n + "On", i == 0);
            addCS (oscParam (i, "wave"),  n + "Wave", oscWaveChoices(), i == 2 ? 2 : 0);
            addFS (oscParam (i, "tune"),  n + "Tune", Range { -36.0f, 36.0f, 1.0f }, i == 1 ? -12.0f : 0.0f, "st");
            addFS (oscParam (i, "fine"),  n + "Fine", Range { -100.0f, 100.0f, 0.1f }, i == 3 ? 7.0f : 0.0f, "ct");
            addFS (oscParam (i, "level"), n + "Level", unit(), i == 0 ? 0.7f : 0.0f);
            addFS (oscParam (i, "pan"),   n + "Pan", bipolar(), 0.0f);
        }
        addFS (oscLevel,    "Synth Level", unit(), 0.6f);
        addBS (oscKeytrack, "Synth Key Track", true);
        addFS (oscFreeHz,   "Synth Free Freq", Range { 20.0f, 2000.0f, 0.0f, 0.3f }, 55.0f, "Hz");
        addFS (oscSpread,   "Synth Spread", unit(), 0.4f);
        addFS (oscBlend,    "Synth Blend (-sub / +add)", bipolar(), 0.5f);

        addFS (synthAttack,  "Synth Attack",  Range { 0.0f, 1.0f, 0.0001f, 0.5f }, 0.05f);
        addFS (synthDecay,   "Synth Decay",   Range { 0.0f, 1.0f, 0.0001f, 0.5f }, 0.2f);
        addFS (synthSustain, "Synth Sustain", unit(), 0.8f);
        addFS (synthRelease, "Synth Release", Range { 0.0f, 1.0f, 0.0001f, 0.5f }, 0.3f);
        addBS (synthDrone,   "Synth Drone", false);

        // ---- filter (modulated by LFO 1) ----
        addBS (filterOn,     "Filter On", false);
        addCS (filterType,   "Filter Type", filterTypeChoices(), 0);
        addFS (filterCutoff, "Filter Cutoff", freqRange, 1200.0f, "Hz");
        addFS (filterRes,    "Filter Resonance", unit(), 0.25f);
        addFS (filterDrive,  "Filter Drive", unit(), 0.0f);

        // ---- LFOs ----
        const char* lfoNames[numLfos] = { "LFO Filter", "LFO Volume", "LFO Pan", "LFO Mutation" };
        const float lfoDefRate[numLfos] = { 0.3f, 5.0f, 0.5f, 0.12f };
        const int   lfoDefDiv[numLfos]  = { 2, 6, 4, 1 };
        for (int i = 0; i < numLfos; ++i)
        {
            const juce::String n = juce::String (lfoNames[i]) + " ";
            addBS (lfoParam (i, "sync"),  n + "Sync", false);
            addFS (lfoParam (i, "rate"),  n + "Rate", lfoRate, lfoDefRate[i], "Hz");
            addCS (lfoParam (i, "div"),   n + "Division", syncDivChoices(), lfoDefDiv[i]);
            addFS (lfoParam (i, "depth"), n + "Depth", unit(), 0.0f);
            addCS (lfoParam (i, "shape"), n + "Shape", lfoShapeChoices(), 0);
            addFS (lfoParam (i, "phase"), n + "Phase", unit(), 0.0f);
        }
        addCS (lfoParam (envLfoIndex, "dest"), "LFO Mutation Dest", envLfoDestChoices(), 0);

        // ---- EQ ----
        addBS (eqOn,       "EQ On", false);
        addFS (eqLowFreq,  "EQ Low Freq",  Range { 20.0f, 500.0f, 0.0f, 0.4f }, 110.0f, "Hz");
        addFS (eqLowGain,  "EQ Low Gain",  Range { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");
        addFS (eqMidFreq,  "EQ Mid Freq",  Range { 150.0f, 8000.0f, 0.0f, 0.3f }, 900.0f, "Hz");
        addFS (eqMidGain,  "EQ Mid Gain",  Range { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");
        addFS (eqMidQ,     "EQ Mid Q",     Range { 0.2f, 8.0f, 0.0f, 0.4f }, 0.9f);
        addFS (eqHighFreq, "EQ High Freq", Range { 1500.0f, 20000.0f, 0.0f, 0.3f }, 6000.0f, "Hz");
        addFS (eqHighGain, "EQ High Gain", Range { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");

        // ---- gator ----
        addBS (gatorOn,      "Gator On", false);
        addBS (gatorSync,    "Gator Sync", true);
        addFS (gatorRate,    "Gator Rate", Range { 0.1f, 30.0f, 0.0f, 0.4f }, 8.0f, "Hz");
        addCS (gatorDiv,     "Gator Division", syncDivChoices(), 6);
        addFS (gatorLength,  "Gator Length", Range { 2.0f, 16.0f, 1.0f }, 16.0f);
        addFS (gatorAttack,  "Gator Attack",  unit(), 0.15f);
        addFS (gatorRelease, "Gator Release", unit(), 0.25f);
        addFS (gatorDepth,   "Gator Depth",   unit(), 1.0f);
        for (int s = 0; s < gatorSteps; ++s)
            addBS (gatorStepParam (s), "Gator Step " + juce::String (s + 1), (s % 2) == 0);

        // ---- glitch ----
        addBS (glitchOn,      "Glitch On", false);
        addFS (glitchAmount,  "Glitch Amount", unit(), 0.35f);
        addBS (glitchSync,    "Glitch Sync", true);
        addFS (glitchRate,    "Glitch Rate", Range { 0.5f, 30.0f, 0.0f, 0.4f }, 6.0f, "Hz");
        addCS (glitchDiv,     "Glitch Division", syncDivChoices(), 4);
        addFS (glitchRepeat,  "Glitch Repeat", unit(), 0.5f);
        addFS (glitchReverse, "Glitch Reverse", unit(), 0.3f);
        addFS (glitchCrush,   "Glitch Crush", unit(), 0.0f);
        addFS (glitchTape,    "Glitch Tape Stop", unit(), 0.15f);
        addFS (glitchMix,     "Glitch Mix", unit(), 1.0f);

        // ---- MIDI reactivity ----
        addBS (midiReactive,    "MIDI Reactive", true);
        addBS (lfoKeyRetrigger, "LFO Key Retrigger", false);
        addBS (gatorRetrigger,  "Gator Key Retrigger", true);
        addFS (midiBendRange,   "Pitch Bend Range", Range { 0.0f, 24.0f, 1.0f }, 2.0f, "st");
        addCS (modWheelDest,    "Mod Wheel Dest", modWheelDestChoices(), 1);
        addFS (modWheelAmount,  "Mod Wheel Amount", unit(), 0.5f);
        addFS (velToSynth,      "Velocity to Synth", unit(), 1.0f);
        addFS (velToFilter,     "Velocity to Filter", unit(), 0.0f);

        return layout;
    }
}
