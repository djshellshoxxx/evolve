#include "Parameters.h"
#include <map>

/*  =====================================================================
    One sentence per control, for the hover tooltips.

    These are deliberately kept apart from createLayout(): the layout is the
    contract with the host and should stay readable as a list of ranges,
    while this is prose that will be edited far more often than the ranges
    are. Keeping them together would mean re-reading the whole parameter
    surface every time a sentence is reworded.

    Anything without an entry falls back to its parameter name, so a missing
    line degrades to the old behaviour rather than to an empty tooltip.
    ===================================================================== */
namespace mutagen::params
{
    namespace
    {
        const std::map<juce::String, juce::String>& table()
        {
            static const std::map<juce::String, juce::String> t =
            {
                // ---- the five large environment controls -----------------
                { nutrients,   "How much food the dish holds. More food means more cells "
                               "survive and reproduce, so the texture thickens." },
                { mutation,    "How often a mutation lands. Low values drift; high values "
                               "keep rewriting the colony underneath you." },
                { selection,   "How hard the environment pushes cells toward the selection "
                               "targets below. At zero, evolution is undirected." },
                { metabolism,  "The speed the colony lives at - how fast generations turn "
                               "over and how quickly cells react to a change." },
                { stability,   "Resistance to change. High stability keeps a sound you like; "
                               "low stability lets it fall apart and re-form." },

                // ---- the deeper ecology -----------------------------------
                { fertility,     "How readily surviving cells reproduce." },
                { mutationDepth, "How large each mutation is when one lands. Rate says how "
                                 "often, depth says how far." },
                { radiation,     "Background damage. Rare, large, mostly harmful mutations - "
                                 "the slow version of the RADIATE button." },
                { temperature,   "Agitation. Warm colonies move, modulate and mutate faster; "
                                 "cold ones settle." },
                { competition,   "How much cells fight for the same food. High competition "
                                 "kills the weak quickly and sharpens the sound." },
                { symbiosis,     "How much cells reinforce each other instead of competing. "
                                 "The opposite hand on the same dial as competition." },
                { lifespan,      "How long a cell lives before it dies of age." },
                { apoptosis,     "Programmed death: the chance a cell removes itself even "
                                 "while healthy. Keeps the dish from clogging." },
                { diversity,     "Pressure to stay different from each other. This is the "
                                 "control that stops the colony collapsing into one voice." },
                { migration,     "How often traits jump between the colony's islands. High "
                                 "migration spreads a discovery through everything." },

                // ---- selection targets -------------------------------------
                { selBrightness,  "What fitness rewards: dark on the left, bright on the right." },
                { selDensity,     "What fitness rewards: sparse on the left, dense on the right." },
                { selHarmonicity, "What fitness rewards: noisy on the left, harmonic on the right." },
                { selAggression,  "What fitness rewards: calm on the left, aggressive on the right." },
                { selDivergence,  "How much the colony is rewarded for finding something it "
                                  "has not found before, rather than for hitting the targets." },

                // ---- germination -------------------------------------------
                { captureLength,     "How many seconds of the source the colony feeds on." },
                { transientSens,     "How eagerly the source is cut at transients when it is "
                                     "sliced up for the founders." },
                { initialPopulation, "How many cells a new colony germinates with." },
                { distGrain,         "Share of founders that are grain cells: granular, "
                                     "textural, closest to the source material." },
                { distSpectral,      "Share of founders that are spectral cells: partial "
                                     "banks, tonal, the part that sings." },
                { distResonator,     "Share of founders that are resonator cells: struck "
                                     "and ringing, the part with a body." },
                { sourceMode,        "What the colony feeds on: a dropped sample, the live "
                                     "input, one of the built-in primitives, or a preserved "
                                     "organism." },

                // ---- lifecycle ---------------------------------------------
                { exploreMode, "EXPLORING lets the colony keep evolving. PRESERVED freezes "
                               "evolution and holds the sound where it is." },
                { memory,      "How much of the colony survives from one note to the next." },
                { cpuQuality,  "How much CPU the colony may spend: a bigger budget allows a "
                               "larger population and more partials per cell." },
                { pluginRole,  "Instrument plays from MIDI, Effect feeds on the incoming "
                               "audio, Hybrid does both." },

                // ---- output -------------------------------------------------
                { masterGain, "Output level." },
                { dryWet,     "Blend between the incoming audio and the colony." },

                // ---- performance macros --------------------------------------
                { macroGrowth,   "One hand on food, fertility and lifespan together." },
                { macroMutation, "One hand on mutation rate and depth together." },
                { macroStress,   "One hand on competition, radiation and apoptosis together." },
                { macroDensity,  "Pushes the colony toward a denser or sparser texture." },
                { macroBody,     "Weight and low end." },
                { macroVoice,    "How vocal and formant-shaped the colony sounds." },
                { macroMovement, "How much everything wobbles: the depth of every cell's "
                                 "own modulation at once." },
                { macroDecay,    "How long cells ring after they are excited." },
                { xyStability,   "The XY pad's vertical axis: stability against chaos." },
                { xyRepro,       "The XY pad's horizontal axis: reproduction rate." },

                // ---- the post-colony rack -------------------------------------
                { oscLevel,    "Level of the oscillator bank feeding the voice." },
                { oscKeytrack, "Oscillators follow the last MIDI note instead of a fixed "
                               "frequency." },
                { oscFreeHz,   "Oscillator frequency when key-tracking is off." },
                { oscSpread,   "Stereo width of the oscillator bank." },
                { oscBlend,    "Negative carves the oscillator voice out of the colony, "
                               "zero silences it, positive layers it on top." },
                { synthAttack,  "Oscillator voice attack time." },
                { synthDecay,   "Oscillator voice decay time." },
                { synthSustain, "Oscillator voice sustain level." },
                { synthRelease, "Oscillator voice release time." },
                { synthDrone,   "Ignore the note gate and let the oscillator voice run "
                                "continuously." },

                { filterOn,     "Switch the filter in." },
                { filterType,   "Low pass, high pass, band pass or notch." },
                { filterCutoff, "Filter cutoff frequency. LFO 1 modulates this." },
                { filterRes,    "Filter resonance." },
                { filterDrive,  "Drive into the filter - saturation, not level." },

                { eqOn,       "Switch the three-band EQ in." },
                { eqLowFreq,  "Low shelf corner frequency." },
                { eqLowGain,  "Low shelf gain." },
                { eqMidFreq,  "Mid peak centre frequency." },
                { eqMidGain,  "Mid peak gain." },
                { eqMidQ,     "Mid peak width - higher Q is narrower." },
                { eqHighFreq, "High shelf corner frequency." },
                { eqHighGain, "High shelf gain." },

                { gatorOn,      "Switch the rhythmic gate in." },
                { gatorSync,    "Lock the gate to the host tempo." },
                { gatorRate,    "Gate speed when it is not tempo-locked." },
                { gatorDiv,     "Step length when the gate is tempo-locked." },
                { gatorLength,  "How many of the sixteen steps the pattern uses." },
                { gatorAttack,  "How quickly each open step fades in." },
                { gatorRelease, "How quickly each closed step fades out." },
                { gatorDepth,   "How far the gate closes. Below full depth it ducks "
                                "rather than cuts." },

                { glitchOn,      "Switch the beat-repeat and stutter section in." },
                { glitchAmount,  "The chance that any given slice is glitched at all." },
                { glitchSync,    "Lock the slice length to the host tempo." },
                { glitchRate,    "Slice rate when it is not tempo-locked." },
                { glitchDiv,     "Slice length when it is tempo-locked." },
                { glitchRepeat,  "How finely a repeated slice is subdivided." },
                { glitchReverse, "The chance a slice plays backwards." },
                { glitchCrush,   "Sample-rate and bit reduction on glitched slices." },
                { glitchTape,    "The chance a slice tape-stops, and how hard." },
                { glitchMix,     "How much of the glitched signal reaches the output." },

                // ---- MIDI ------------------------------------------------------
                { midiReactive,    "Master switch for everything on this page." },
                { lfoKeyRetrigger, "Reset every LFO's phase when a note starts." },
                { gatorRetrigger,  "Restart the gate pattern when a note starts." },
                { midiBendRange,   "How many semitones the pitch wheel covers." },
                { modWheelDest,    "What the mod wheel drives." },
                { modWheelAmount,  "How far the mod wheel moves its destination." },
                { velToSynth,      "How much note velocity raises the oscillator voice." },
                { velToFilter,     "How much note velocity opens the filter." },
            };
            return t;
        }

        /*  The rack parameters are generated - "osc3_tune", "lfo2_depth" - so
            they are described by their leaf rather than one entry each. */
        juce::String describeLeaf (const juce::String& id)
        {
            const auto leaf = id.fromFirstOccurrenceOf ("_", false, false);
            if (leaf.isEmpty()) return {};

            const bool isOsc = id.startsWith ("osc");
            const bool isLfo = id.startsWith ("lfo");

            if (isOsc)
            {
                if (leaf == "on")     return "Switch this oscillator on.";
                if (leaf == "wave")   return "This oscillator's waveform.";
                if (leaf == "tune")   return "Coarse tuning in semitones.";
                if (leaf == "fine")   return "Fine tuning in cents.";
                if (leaf == "level")  return "This oscillator's level in the bank.";
                if (leaf == "pan")    return "This oscillator's position in the stereo field.";
                if (leaf == "pw")     return "Pulse width, for the pulse wave.";
                if (leaf == "phase")  return "Start phase.";
            }

            if (isLfo)
            {
                if (leaf == "sync")  return "Lock this LFO to the host tempo.";
                if (leaf == "rate")  return "LFO speed when it is not tempo-locked.";
                if (leaf == "div")   return "LFO cycle length when it is tempo-locked.";
                if (leaf == "depth") return "How far this LFO moves its destination.";
                if (leaf == "shape") return "This LFO's waveform.";
                if (leaf == "phase") return "This LFO's start phase.";
                if (leaf == "dest")  return "Which ecology control this LFO drives - the "
                                            "environment breathing on its own.";
            }

            if (id.startsWith ("gatorStep"))
                return "One step of the gate pattern.";

            return {};
        }
    }

    juce::String describe (const juce::String& id)
    {
        const auto& t = table();
        if (const auto it = t.find (id); it != t.end())
            return it->second;

        return describeLeaf (id);
    }
}
