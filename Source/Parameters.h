#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  MUTAGEN parameter surface.

    The plugin exposes the *environment* of a living colony rather than a
    synthesis graph. Every knob here nudges an ecology; it never addresses an
    oscillator. IDs are stable strings so host automation survives refactors.
*/
namespace mutagen::params
{
    // ---- Environment (the five large controls) -----------------------------
    inline constexpr auto nutrients       = "nutrients";
    inline constexpr auto mutation        = "mutation";        // master mutation rate
    inline constexpr auto selection       = "selection";       // selection-pressure amount
    inline constexpr auto metabolism      = "metabolism";
    inline constexpr auto stability       = "stability";

    // ---- Environment (deeper ecological controls) -------------------------
    inline constexpr auto fertility       = "fertility";
    inline constexpr auto mutationDepth   = "mutationDepth";
    inline constexpr auto radiation       = "radiation";
    inline constexpr auto temperature     = "temperature";
    inline constexpr auto competition     = "competition";
    inline constexpr auto symbiosis       = "symbiosis";
    inline constexpr auto lifespan        = "lifespan";
    inline constexpr auto apoptosis       = "apoptosis";
    inline constexpr auto diversity       = "diversity";
    inline constexpr auto migration       = "migration";

    // ---- Selection targets (what "fitness" means right now) --------------
    inline constexpr auto selBrightness   = "selBrightness";   // -1 dark .. +1 bright
    inline constexpr auto selDensity      = "selDensity";      // -1 sparse .. +1 dense
    inline constexpr auto selHarmonicity  = "selHarmonicity";  // -1 noisy .. +1 harmonic
    inline constexpr auto selAggression   = "selAggression";   // -1 calm .. +1 aggressive
    inline constexpr auto selDivergence   = "selDivergence";   // 0 familiar .. 1 divergent

    // ---- Germination ---------------------------------------------------
    inline constexpr auto captureLength      = "captureLength";      // seconds
    inline constexpr auto transientSens      = "transientSens";
    inline constexpr auto initialPopulation  = "initialPopulation";
    inline constexpr auto distGrain          = "distGrain";
    inline constexpr auto distSpectral       = "distSpectral";
    inline constexpr auto distResonator      = "distResonator";
    inline constexpr auto sourceMode         = "sourceMode";        // choice

    // ---- Lifecycle / identity ---------------------------------------
    inline constexpr auto exploreMode     = "exploreMode";     // true = explore, false = preserve
    inline constexpr auto memory          = "memory";          // colony inheritance across notes
    inline constexpr auto cpuQuality      = "cpuQuality";      // choice: Eco / Balanced / Pristine
    inline constexpr auto pluginRole      = "pluginRole";      // choice: Instrument / Effect / Hybrid

    // ---- Output ------------------------------------------------------
    inline constexpr auto masterGain      = "masterGain";
    inline constexpr auto dryWet          = "dryWet";

    // ---- Performance macros ---------------------------------------
    inline constexpr auto macroGrowth     = "macroGrowth";
    inline constexpr auto macroMutation   = "macroMutation";
    inline constexpr auto macroStress     = "macroStress";
    inline constexpr auto macroDensity    = "macroDensity";
    inline constexpr auto macroBody       = "macroBody";
    inline constexpr auto macroVoice      = "macroVoice";
    inline constexpr auto macroMovement   = "macroMovement";
    inline constexpr auto macroDecay      = "macroDecay";
    inline constexpr auto xyStability     = "xyStability";
    inline constexpr auto xyRepro         = "xyRepro";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    enum class SourceMode  { sample = 0, liveInput, primitiveNoise, primitiveImpulse, preservedOrganism };
    enum class CpuQuality   { eco = 0, balanced, pristine };
    enum class PluginRole   { instrument = 0, effect, hybrid };

    int   maxCellsFor (CpuQuality q);
    juce::StringArray sourceModeChoices();
    juce::StringArray cpuQualityChoices();
    juce::StringArray pluginRoleChoices();
}
