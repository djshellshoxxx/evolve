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

        return layout;
    }
}
