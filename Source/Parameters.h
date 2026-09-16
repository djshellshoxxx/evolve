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

    // =====================================================================
    //  Post-colony processing rack: oscillators, filter + LFOs, EQ, gator.
    //  The colony is still the instrument - this is the signal it flows into.
    // =====================================================================

    inline constexpr int numOscillators = 4;
    inline constexpr int numLfos        = 4;   // 0 filter, 1 volume, 2 pan, 3 environment
    inline constexpr int gatorSteps     = 16;
    inline constexpr int envLfoIndex    = 3;   // the LFO that modulates the colony's ecology

    // Oscillator bank (id built as "osc1_wave" etc.)
    juce::String oscParam (int index, const char* leaf);   // index 0..3
    inline constexpr auto oscLevel     = "oscLevel";        // bank output level
    inline constexpr auto oscKeytrack  = "oscKeytrack";     // follow last MIDI note
    inline constexpr auto oscFreeHz    = "oscFreeHz";       // base freq when not key-tracking
    inline constexpr auto oscSpread    = "oscSpread";       // stereo width of the bank

    // Subtractive-synth blend: the osc bank is a full subtractive voice
    // (oscillators -> shared filter -> ADSR). oscBlend is bipolar:
    //   -1  subtract the voice from the colony (phase-cancel / carve)
    //    0  voice silent in the mix
    //   +1  add the voice as an extra layer
    inline constexpr auto oscBlend     = "oscBlend";
    inline constexpr auto synthAttack  = "synthAttack";
    inline constexpr auto synthDecay   = "synthDecay";
    inline constexpr auto synthSustain = "synthSustain";
    inline constexpr auto synthRelease = "synthRelease";
    inline constexpr auto synthDrone   = "synthDrone";      // ignore the gate, always on

    // Filter (LFO-modulated, LFO 0)
    inline constexpr auto filterOn     = "filterOn";
    inline constexpr auto filterType   = "filterType";      // choice
    inline constexpr auto filterCutoff = "filterCutoff";    // Hz
    inline constexpr auto filterRes    = "filterRes";
    inline constexpr auto filterDrive  = "filterDrive";

    // LFOs (id built as "lfo1_rate" etc., index 0..2)
    juce::String lfoParam (int index, const char* leaf);
    // leaves: "sync" (bool), "rate" (Hz), "div" (choice), "depth", "shape" (choice), "phase"
    // LFO index envLfoIndex also has "dest" (choice) - which ecology control it drives.

    enum class EnvLfoDest { mutation = 0, mutationDepth, nutrients, selection,
                            radiation, temperature, fertility };
    juce::StringArray envLfoDestChoices();

    // EQ (3 band: low shelf / mid peak / high shelf)
    inline constexpr auto eqOn        = "eqOn";
    inline constexpr auto eqLowFreq   = "eqLowFreq";
    inline constexpr auto eqLowGain   = "eqLowGain";
    inline constexpr auto eqMidFreq   = "eqMidFreq";
    inline constexpr auto eqMidGain   = "eqMidGain";
    inline constexpr auto eqMidQ      = "eqMidQ";
    inline constexpr auto eqHighFreq  = "eqHighFreq";
    inline constexpr auto eqHighGain  = "eqHighGain";

    // Gator (rhythmic gate)
    inline constexpr auto gatorOn      = "gatorOn";
    inline constexpr auto gatorSync    = "gatorSync";
    inline constexpr auto gatorRate    = "gatorRate";       // Hz when free-running
    inline constexpr auto gatorDiv     = "gatorDiv";        // choice (BPM-synced step length)
    inline constexpr auto gatorLength  = "gatorLength";     // active pattern length 2..16
    inline constexpr auto gatorAttack  = "gatorAttack";
    inline constexpr auto gatorRelease = "gatorRelease";
    inline constexpr auto gatorDepth   = "gatorDepth";
    juce::String gatorStepParam (int step);                 // step 0..15 (bool)

    // ---- Glitch (beat-repeat / stutter / crush / reverse / tape-stop) --
    inline constexpr auto glitchOn      = "glitchOn";
    inline constexpr auto glitchAmount  = "glitchAmount";   // probability a slice glitches
    inline constexpr auto glitchSync    = "glitchSync";
    inline constexpr auto glitchRate    = "glitchRate";     // Hz when free-running
    inline constexpr auto glitchDiv     = "glitchDiv";      // choice (slice length)
    inline constexpr auto glitchRepeat  = "glitchRepeat";   // stutter subdivision depth
    inline constexpr auto glitchReverse = "glitchReverse";  // chance a slice reverses
    inline constexpr auto glitchCrush   = "glitchCrush";    // sample-rate / bit reduction
    inline constexpr auto glitchTape    = "glitchTape";     // tape-stop chance / strength
    inline constexpr auto glitchMix     = "glitchMix";

    // ---- MIDI reactivity -------------------------------------------------
    inline constexpr auto lfoKeyRetrigger = "lfoKeyRetrigger"; // reset LFO phases on note-on
    inline constexpr auto gatorRetrigger  = "gatorRetrigger";  // reset gator pattern on note-on
    inline constexpr auto midiBendRange   = "midiBendRange";   // pitch-bend range, semitones
    inline constexpr auto modWheelDest    = "modWheelDest";    // choice
    inline constexpr auto modWheelAmount  = "modWheelAmount";
    inline constexpr auto velToSynth      = "velToSynth";      // note velocity -> synth level
    inline constexpr auto velToFilter     = "velToFilter";     // note velocity -> filter cutoff
    inline constexpr auto midiReactive    = "midiReactive";    // master enable for the above

    enum class ModDest { off = 0, filterCutoff, synthBlend, mutation, nutrients, lfoDepth, gatorDepth_ };
    juce::StringArray modWheelDestChoices();

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** One sentence explaining a control, for the hover tooltips. Empty when
        the parameter has no entry, in which case the caller should fall back
        to the parameter's own name. Implemented in ParamHelp.cpp. */
    juce::String describe (const juce::String& id);

    /*  primitiveTone is appended rather than inserted so that saved states
        keep referring to the same modes they were written with. */
    enum class SourceMode  { sample = 0, liveInput, primitiveNoise, primitiveImpulse,
                             preservedOrganism, primitiveTone };
    enum class CpuQuality   { eco = 0, balanced, pristine };
    enum class PluginRole   { instrument = 0, effect, hybrid };
    enum class OscWave      { sine = 0, triangle, saw, square, pulse, noise };
    enum class FilterType   { lowpass = 0, highpass, bandpass, notch };
    enum class LfoShape     { sine = 0, triangle, sawUp, sawDown, square, sampleHold, randomSmooth };

    int   maxCellsFor (CpuQuality q);
    juce::StringArray sourceModeChoices();
    juce::StringArray cpuQualityChoices();
    juce::StringArray pluginRoleChoices();
    juce::StringArray oscWaveChoices();
    juce::StringArray filterTypeChoices();
    juce::StringArray lfoShapeChoices();
    juce::StringArray syncDivChoices();

    /** Beats per cycle/step for a syncDivChoices() index. */
    double syncDivBeats (int index);
}
