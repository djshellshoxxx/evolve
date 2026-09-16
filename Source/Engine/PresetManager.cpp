#include "PresetManager.h"
#include "../Parameters.h"

namespace mutagen
{
    using namespace params;

    // =====================================================================
    //  The factory bank.
    //
    //  Each entry lists only the parameters it actually cares about; every
    //  other parameter is returned to its default before the list is applied.
    //  A preset that silently inherited half its state from whatever was
    //  loaded before is not a preset, it is a diff.
    // =====================================================================

    const std::vector<FactoryPreset>& PresetManager::factoryPresets()
    {
        static const std::vector<FactoryPreset> bank =
        {
            // ---- Foundations -------------------------------------------
            { "Init", "Foundations",
              "Factory defaults. A balanced colony on the tonal seed - the "
              "starting point every other preset is a departure from.",
              {} },

            { "First Culture", "Foundations",
              "A slow, well-fed colony that drifts rather than lurches. The "
              "gentlest way to hear what evolution is doing.",
              { { nutrients, 0.70f }, { mutation, 0.16f }, { mutationDepth, 0.22f },
                { selection, 0.30f }, { metabolism, 0.38f }, { stability, 0.78f },
                { lifespan, 0.70f }, { diversity, 0.45f }, { selHarmonicity, 0.35f } } },

            { "Petri Dish", "Foundations",
              "Small population, high turnover. Generations pass quickly, so "
              "the colony reaches an identity in under a minute.",
              { { initialPopulation, 0.22f }, { fertility, 0.72f }, { lifespan, 0.26f },
                { apoptosis, 0.38f }, { metabolism, 0.66f }, { mutation, 0.40f },
                { competition, 0.60f } } },

            // ---- Textures -----------------------------------------------
            { "Slow Bloom", "Textures",
              "Long-lived resonator cells over a quiet grain bed. Swells and "
              "recedes on its own without ever quite settling.",
              { { distGrain, 0.22f }, { distSpectral, 0.20f }, { distResonator, 0.58f },
                { nutrients, 0.64f }, { mutation, 0.14f }, { metabolism, 0.26f },
                { stability, 0.82f }, { lifespan, 0.84f }, { apoptosis, 0.08f },
                { selHarmonicity, 0.55f }, { selDensity, -0.30f }, { macroDecay, 0.78f } } },

            { "Glass Colony", "Textures",
              "Bright, thin and metallic. Selection pushes hard toward the top "
              "of the spectrum and keeps the population sparse.",
              { { distSpectral, 0.70f }, { distGrain, 0.18f }, { distResonator, 0.12f },
                { selBrightness, 0.80f }, { selDensity, -0.45f }, { selHarmonicity, 0.40f },
                { stability, 0.66f }, { mutation, 0.26f }, { temperature, 0.42f },
                { eqOn, 1.0f }, { eqHighGain, 4.5f }, { eqLowGain, -6.0f } } },

            { "Deep Sediment", "Textures",
              "Dark, dense and slow-moving. Everything bright is selected "
              "against until only the low end survives.",
              { { selBrightness, -0.75f }, { selDensity, 0.55f }, { distGrain, 0.55f },
                { distResonator, 0.33f }, { distSpectral, 0.12f },
                { metabolism, 0.24f }, { stability, 0.72f }, { nutrients, 0.66f },
                { filterOn, 1.0f }, { filterCutoff, 700.0f }, { filterRes, 0.18f },
                { eqOn, 1.0f }, { eqLowGain, 5.0f }, { eqHighGain, -5.0f } } },

            { "Humid Air", "Textures",
              "Warm, wet and crowded. High symbiosis means cells reinforce "
              "each other instead of competing, so the texture thickens.",
              { { symbiosis, 0.80f }, { competition, 0.16f }, { nutrients, 0.74f },
                { temperature, 0.58f }, { diversity, 0.36f }, { migration, 0.55f },
                { selDensity, 0.40f }, { selBrightness, -0.20f }, { stability, 0.58f },
                { macroBody, 0.72f } } },

            // ---- Rhythmic ------------------------------------------------
            { "Mitosis Clock", "Rhythmic",
              "The gator cuts the colony into sixteenths while the population "
              "doubles and thins underneath it.",
              { { gatorOn, 1.0f }, { gatorSync, 1.0f }, { gatorDiv, 6.0f },
                { gatorDepth, 1.0f }, { gatorAttack, 0.06f }, { gatorRelease, 0.16f },
                { fertility, 0.68f }, { lifespan, 0.32f }, { metabolism, 0.62f },
                { selAggression, 0.35f } } },

            { "Stutter Culture", "Rhythmic",
              "Beat-repeat and reversal on a colony that is already unstable. "
              "Two kinds of motion fighting for the same bar.",
              { { glitchOn, 1.0f }, { glitchAmount, 0.55f }, { glitchRepeat, 0.65f },
                { glitchReverse, 0.45f }, { glitchTape, 0.22f }, { glitchDiv, 6.0f },
                { mutation, 0.46f }, { stability, 0.34f }, { selAggression, 0.45f },
                { radiation, 0.14f } } },

            { "Pulse Feeder", "Rhythmic",
              "The environment LFO drives nutrients, so the colony is fed in "
              "waves and grows and starves in time with the track.",
              { { "lfo4_depth", 0.85f }, { "lfo4_sync", 1.0f }, { "lfo4_div", 1.0f },
                { "lfo4_dest", 2.0f }, { "lfo4_shape", 1.0f },
                { fertility, 0.62f }, { metabolism, 0.58f }, { lifespan, 0.40f },
                { nutrients, 0.40f } } },

            // ---- Aggressive -----------------------------------------------
            { "Hot Zone", "Aggressive",
              "High radiation and high temperature. Mutations land constantly "
              "and some of them are fatal - the colony is always recovering.",
              { { radiation, 0.42f }, { temperature, 0.74f }, { mutation, 0.62f },
                { mutationDepth, 0.66f }, { stability, 0.22f }, { apoptosis, 0.36f },
                { selAggression, 0.65f }, { selDivergence, 0.55f }, { fertility, 0.70f } } },

            { "Predator Pressure", "Aggressive",
              "Ruthless selection on a crowded dish. Only the loudest few cells "
              "survive each generation, so the sound keeps snapping into focus.",
              { { competition, 0.92f }, { selection, 0.85f }, { symbiosis, 0.08f },
                { apoptosis, 0.45f }, { lifespan, 0.30f }, { fertility, 0.75f },
                { diversity, 0.22f }, { selAggression, 0.55f } } },

            { "Runaway Growth", "Aggressive",
              "Feed it everything and remove the brakes. Population climbs to "
              "the CPU ceiling and the texture goes opaque.",
              { { nutrients, 1.0f }, { fertility, 0.95f }, { metabolism, 0.80f },
                { apoptosis, 0.02f }, { lifespan, 0.90f }, { competition, 0.20f },
                { selDensity, 0.75f }, { initialPopulation, 0.80f },
                { filterOn, 1.0f }, { filterCutoff, 5000.0f }, { filterDrive, 0.35f } } },

            { "Infection Bloom", "Aggressive",
              "Maximum mutation depth with migration wide open, so traits jump "
              "between islands and spread through the whole colony at once.",
              { { mutationDepth, 0.95f }, { mutation, 0.70f }, { migration, 0.88f },
                { diversity, 0.80f }, { selDivergence, 0.80f }, { stability, 0.18f },
                { glitchOn, 1.0f }, { glitchAmount, 0.30f }, { glitchCrush, 0.25f } } },

            // ---- Ambient --------------------------------------------------
            { "Frozen Specimen", "Ambient",
              "Preserve mode with almost no mutation. The colony holds what it "
              "has become and simply plays it.",
              { { exploreMode, 0.0f }, { mutation, 0.02f }, { mutationDepth, 0.05f },
                { radiation, 0.0f }, { stability, 0.95f }, { lifespan, 0.95f },
                { apoptosis, 0.0f }, { memory, 0.9f }, { metabolism, 0.30f } } },

            { "Tidal Drone", "Ambient",
              "A sustained bed with a slow filter sweep. The colony changes "
              "over minutes, not seconds.",
              { { synthDrone, 1.0f }, { oscBlend, 0.62f }, { oscLevel, 0.45f },
                { filterOn, 1.0f }, { filterCutoff, 900.0f }, { filterRes, 0.32f },
                { "lfo1_depth", 0.70f }, { "lfo1_rate", 0.06f }, { "lfo1_shape", 0.0f },
                { mutation, 0.10f }, { metabolism, 0.18f }, { stability, 0.88f },
                { selHarmonicity, 0.60f }, { macroDecay, 0.85f } } },

            { "Long Decay", "Ambient",
              "Resonator-heavy and starved. Cells ring out far longer than "
              "they are fed, so the dish is mostly tails.",
              { { distResonator, 0.72f }, { distGrain, 0.16f }, { distSpectral, 0.12f },
                { nutrients, 0.30f }, { metabolism, 0.14f }, { macroDecay, 0.95f },
                { lifespan, 0.88f }, { selDensity, -0.55f }, { selHarmonicity, 0.50f },
                { eqOn, 1.0f }, { eqMidGain, -3.0f } } },

            // ---- Experimental ----------------------------------------------
            { "Divergence Engine", "Experimental",
              "Selection rewards being unlike everything the colony has already "
              "been. It will not repeat itself, and it will not settle.",
              { { selDivergence, 1.0f }, { diversity, 0.92f }, { migration, 0.15f },
                { competition, 0.35f }, { mutation, 0.45f }, { mutationDepth, 0.55f },
                { stability, 0.40f }, { symbiosis, 0.25f } } },

            { "Feedback Organism", "Experimental",
              "Built to be fed. Drop samples or arm the mic - short lifespans "
              "and high fertility let ingested material take hold fast.",
              { { sourceMode, 0.0f }, { fertility, 0.85f }, { lifespan, 0.28f },
                { metabolism, 0.70f }, { mutation, 0.38f }, { transientSens, 0.72f },
                { captureLength, 4.0f }, { memory, 0.25f } } },

            { "Carved Voice", "Experimental",
              "The oscillator bank is phase-inverted against the colony, so the "
              "synth subtracts from the organism instead of layering over it.",
              { { oscBlend, -0.80f }, { oscLevel, 0.70f }, { "osc1_on", 1.0f },
                { "osc2_on", 1.0f }, { "osc2_level", 0.55f }, { "osc2_tune", -12.0f },
                { filterOn, 1.0f }, { filterCutoff, 2400.0f }, { filterRes, 0.45f },
                { synthDrone, 1.0f }, { selHarmonicity, 0.40f } } },
        };
        return bank;
    }

    // =====================================================================

    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s)
        : apvts (s), rng (juce::Random::getSystemRandom().nextInt64())
    {
        userPresetDirectory().createDirectory();
    }

    juce::File PresetManager::userPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MUTAGEN")
                   .getChildFile ("Presets");
    }

    juce::StringArray PresetManager::factoryNames() const
    {
        juce::StringArray names;
        for (const auto& p : factoryPresets())
            names.add (juce::String (p.category) + ": " + juce::String (p.name));
        return names;
    }

    juce::StringArray PresetManager::userPresetNames() const
    {
        juce::StringArray names;
        for (const auto& f : userPresetDirectory().findChildFiles (juce::File::findFiles, false,
                                                                   wildcard))
            names.add (f.getFileNameWithoutExtension());
        names.sortNatural();
        return names;
    }

    // =====================================================================

    void PresetManager::setParam (const juce::String& id, float plainValue)
    {
        auto* p = apvts.getParameter (id);
        if (p == nullptr)
        {
            // A preset naming a parameter that no longer exists is a bug in
            // the bank, not in the user's session - skip it and carry on.
            jassertfalse;
            return;
        }

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (ranged->convertTo0to1 (plainValue));
            p->endChangeGesture();
        }
    }

    void PresetManager::applyPairs (const std::vector<std::pair<const char*, float>>& pairs)
    {
        for (const auto& [id, value] : pairs)
            setParam (id, value);
    }

    void PresetManager::resetToDefaults()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
    }

    void PresetManager::loadFactory (int index)
    {
        const auto& bank = factoryPresets();
        if (! juce::isPositiveAndBelow (index, (int) bank.size())) return;

        resetToDefaults();
        applyPairs (bank[(size_t) index].values);

        current = juce::File();
        currentDisplayName = bank[(size_t) index].name;
        modified = false;
        hasRandomised = false;
    }

    // =====================================================================

    bool PresetManager::saveToFile (const juce::File& f, const juce::String& displayName)
    {
        juce::ValueTree preset ("MUTAGEN_PRESET");
        preset.setProperty ("version", 1, nullptr);
        preset.setProperty ("name", displayName, nullptr);
        preset.setProperty ("plugin", "MUTAGEN", nullptr);
        preset.appendChild (apvts.copyState(), nullptr);

        auto xml = preset.createXml();
        if (xml == nullptr || ! xml->writeTo (f))
            return false;

        current = f;
        currentDisplayName = displayName;
        modified = false;
        return true;
    }

    bool PresetManager::loadFromFile (const juce::File& f)
    {
        if (! f.existsAsFile()) return false;

        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr) return false;

        auto preset = juce::ValueTree::fromXml (*xml);
        if (! preset.hasType ("MUTAGEN_PRESET")) return false;

        auto stateChild = preset.getChildWithName (apvts.state.getType());
        if (! stateChild.isValid()) return false;

        // Reset first for the same reason the factory bank does: a preset
        // written by an older build will not mention parameters added since,
        // and those should read as default rather than as leftovers.
        resetToDefaults();
        apvts.replaceState (stateChild);

        current = f;
        currentDisplayName = preset.getProperty ("name", f.getFileNameWithoutExtension()).toString();
        modified = false;
        hasRandomised = false;
        return true;
    }

    bool PresetManager::saveCurrent()
    {
        if (current == juce::File()) return false;
        return saveToFile (current, currentDisplayName);
    }

    // =====================================================================

    bool PresetManager::isRandomisable (const juce::String& id)
    {
        // Output level, dry/wet, CPU budget and role are the user's rig, not
        // the patch. Randomising them turns a fun button into a hazard.
        static const juce::StringArray excluded
        {
            masterGain, dryWet, cpuQuality, pluginRole, midiBendRange, midiReactive,
            modWheelDest, modWheelAmount, velToSynth, velToFilter,
            oscKeytrack, lfoKeyRetrigger, gatorRetrigger
        };
        return ! excluded.contains (id);
    }

    void PresetManager::randomise()
    {
        // The first press randomises from wherever you are. Every press after
        // that starts from defaults, so press two is a new set of settings
        // rather than a random walk away from press one.
        if (hasRandomised)
            resetToDefaults();
        hasRandomised = true;

        for (auto* p : apvts.processor.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr || ! isRandomisable (rp->paramID)) continue;

            float norm;
            if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
            {
                norm = rng.nextFloat() < 0.35f ? 1.0f : 0.0f;
            }
            else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
            {
                const int n = choice->choices.size();
                norm = n > 1 ? (float) rng.nextInt (n) / (float) (n - 1) : 0.0f;
            }
            else
            {
                // Biased toward the middle of the range. A uniform draw across
                // forty parameters reliably lands several of them at an
                // extreme, and the result is noise every time - which is not
                // "a unique sound", it is the same failure repeatedly.
                const float a = rng.nextFloat(), b = rng.nextFloat();
                norm = juce::jlimit (0.0f, 1.0f, 0.5f * (a + b));
            }

            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }

        // Species shares are a mix, so they need to stay plausible rather than
        // independently random - three near-zero draws would empty the dish.
        setParam (distGrain,     0.15f + rng.nextFloat() * 0.6f);
        setParam (distSpectral,  0.15f + rng.nextFloat() * 0.6f);
        setParam (distResonator, 0.15f + rng.nextFloat() * 0.6f);

        // Keep the colony alive and audible: a random draw that starves or
        // silences it is not a sound, it is a dead dish.
        setParam (nutrients,         0.35f + rng.nextFloat() * 0.55f);
        setParam (initialPopulation, 0.25f + rng.nextFloat() * 0.5f);
        setParam (fertility,         0.30f + rng.nextFloat() * 0.5f);
        setParam (apoptosis,         rng.nextFloat() * 0.35f);
        setParam (radiation,         rng.nextFloat() * 0.25f);

        current = juce::File();
        currentDisplayName = "Random";
        modified = true;
    }
}
