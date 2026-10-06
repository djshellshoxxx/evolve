#include "ProgressionSystem.h"

namespace mutagen
{
    namespace
    {
        int readInt (const juce::var& root, const char* key)
        {
            if (auto* obj = root.getDynamicObject())
                return (int) obj->getProperty (key);
            return 0;
        }

        juce::int64 readInt64 (const juce::var& root, const char* key)
        {
            if (auto* obj = root.getDynamicObject())
                return (juce::int64) obj->getProperty (key);
            return 0;
        }

        void readSet (const juce::var& root, const char* key, std::set<juce::String>& out)
        {
            if (auto* obj = root.getDynamicObject())
                if (auto* arr = obj->getProperty (key).getArray())
                    for (const auto& item : *arr)
                        out.insert (item.toString());
        }

        void readIntSet (const juce::var& root, const char* key, std::set<int>& out)
        {
            if (auto* obj = root.getDynamicObject())
                if (auto* arr = obj->getProperty (key).getArray())
                    for (const auto& item : *arr)
                        out.insert ((int) item);
        }
    }

    ProgressionSystem::ProgressionSystem()
    {
        load();
    }

    ProgressionSystem::~ProgressionSystem()
    {
        flush();
    }

    juce::File ProgressionSystem::progressFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("MUTAGEN")
            .getChildFile ("field-journal.json");
    }

    void ProgressionSystem::load()
    {
        const auto file = progressFile();
        if (! file.existsAsFile())
            return;

        const auto root = juce::JSON::parse (file.loadFileAsString());
        if (! root.isObject())
            return;

        lifetime.lifetimeScore = readInt64 (root, "lifetimeScore");
        lifetime.generationsObserved = readInt (root, "generationsObserved");
        lifetime.lifetimeDiscoveries = readInt (root, "lifetimeDiscoveries");
        lifetime.worldsVisited = readInt (root, "worldsVisited");
        lifetime.noiseRescues = readInt (root, "noiseRescues");
        lifetime.samplesDigested = readInt (root, "samplesDigested");
        lifetime.radiationExposures = readInt (root, "radiationExposures");
        lifetime.breedingOperations = readInt (root, "breedingOperations");
        lifetime.steeringInterventions = readInt (root, "steeringInterventions");
        lifetime.anomaliesFound = readInt (root, "anomaliesFound");
        lifetime.relicsFound = readInt (root, "relicsFound");
        lifetime.savedRuns = readInt (root, "savedRuns");
        lifetime.peakCombo = readInt (root, "peakCombo");

        readSet (root, "anomalies", anomalyIds);
        readSet (root, "relics", relicIds);
        readSet (root, "worlds", worldIds);
        readIntSet (root, "creatures", creatureIds);
        readIntSet (root, "milestones", milestoneIds);

        for (int i = 0; i < 5; ++i)
        {
            const auto key = "gameSkill" + juce::String (i);
            gameSkillCounts[(size_t) i] = readInt (root, key.toRawUTF8());
        }
        gameSoundCount = readInt (root, "gameSounds");
        specialOrbCount = readInt (root, "specialOrbs");
        rainbowOrbCount = readInt (root, "rainbowOrbs");
        glowingRainbowOrbCount = readInt (root, "glowingRainbowOrbs");
        greenOrbCount = readInt (root, "greenOrbs");
        redOrbCount = readInt (root, "redOrbs");
        pinkOrbCount = readInt (root, "pinkOrbs");

        // Sets are authoritative where available. Keep the legacy world count
        // so an older journal does not lose progression during migration.
        lifetime.anomaliesFound = (int) anomalyIds.size();
        lifetime.relicsFound = (int) relicIds.size();
        lifetime.worldsVisited = std::max (lifetime.worldsVisited, (int) worldIds.size());
    }

    void ProgressionSystem::markDirty (const juce::String& notice)
    {
        dirty = true;
        if (notice.isNotEmpty())
            pendingNotice = notice;
    }

    bool ProgressionSystem::awardAnomaly (const juce::String& id, bool condition)
    {
        if (! condition || anomalyIds.count (id) != 0)
            return false;

        anomalyIds.insert (id);
        lifetime.anomaliesFound = (int) anomalyIds.size();
        markDirty ("ANOMALY RECORDED: " + id.replaceCharacter ('_', ' ').toUpperCase());
        return true;
    }

    bool ProgressionSystem::awardRelic (const juce::String& id, bool condition)
    {
        if (! condition || relicIds.count (id) != 0)
            return false;

        relicIds.insert (id);
        lifetime.relicsFound = (int) relicIds.size();
        markDirty ("RELIC UNLOCKED: " + id.replaceCharacter ('_', ' ').toUpperCase());
        return true;
    }

    void ProgressionSystem::updateRelics()
    {
        awardRelic ("rescue_badge", lifetime.noiseRescues >= 1);
        awardRelic ("century_lineage", lifetime.generationsObserved >= 100);
        awardRelic ("atlas_ten", lifetime.worldsVisited >= 10);
        awardRelic ("six_figures", lifetime.lifetimeScore >= 100000);
        awardRelic ("anomaly_six", lifetime.anomaliesFound >= 6);
        awardRelic ("breeder_five", lifetime.breedingOperations >= 5);
        awardRelic ("outside_dna", lifetime.samplesDigested >= 5);
        awardRelic ("combo_20", lifetime.peakCombo >= 20);
    }

    void ProgressionSystem::updateUnlockNotice (const progression::ResearchState& before)
    {
        const auto after = state();
        if (pendingNotice.isNotEmpty())
            return;

        if (after.level > before.level)
        {
            pendingNotice = "RESEARCH LEVEL " + juce::String (after.level) + " UNLOCKED";
            return;
        }

        if (after.storyChapter > before.storyChapter)
        {
            pendingNotice = "STORY: " + juce::String (progression::storyTitle (after.storyChapter)).toUpperCase();
            return;
        }

        for (std::size_t i = 0; i < after.characters.size(); ++i)
            if (after.characters[i] && ! before.characters[i])
            {
                pendingNotice = "RESEARCHER: "
                    + juce::String (progression::characterName ((progression::Character) i)).toUpperCase();
                return;
            }

        for (std::size_t i = 0; i < after.features.size(); ++i)
            if (after.features[i] && ! before.features[i])
            {
                pendingNotice = "SYSTEM UNLOCKED: "
                    + juce::String (progression::featureName ((progression::Feature) i)).toUpperCase();
                return;
            }
    }

    void ProgressionSystem::observe (const EngineSnapshot& snap, const ScoreSystem& score)
    {
        const auto before = state();
        bool changed = false;

        if (! primed)
        {
            primed = true;
            lastScore = score.score();
            lastGeneration = snap.generation;
            lastDiscoveries = score.discoveries();
            lastWorldSeed = snap.seed;
            wasNoiseLocked = snap.noiseLocked;

            if (snap.seed != 0)
            {
                const auto worldId = juce::String::toHexString ((juce::int64) snap.seed)
                    + "  " + juce::String (snap.worldName);
                if (worldIds.insert (worldId).second)
                {
                    lifetime.worldsVisited = std::max (lifetime.worldsVisited + 1,
                                                       (int) worldIds.size());
                    changed = true;
                }
            }
        }
        else
        {
            const auto nowScore = score.score();
            if (nowScore >= lastScore)
            {
                const auto delta = nowScore - lastScore;
                if (delta > 0)
                {
                    lifetime.lifetimeScore += delta;
                    changed = true;
                }
            }
            lastScore = nowScore;

            if (snap.generation >= lastGeneration)
            {
                const int delta = snap.generation - lastGeneration;
                if (delta > 0)
                {
                    lifetime.generationsObserved += delta;
                    changed = true;
                }
            }
            lastGeneration = snap.generation;

            const int discoveries = score.discoveries();
            if (discoveries >= lastDiscoveries)
            {
                const int delta = discoveries - lastDiscoveries;
                if (delta > 0)
                {
                    lifetime.lifetimeDiscoveries += delta;
                    changed = true;
                }
            }
            lastDiscoveries = discoveries;

            if (snap.seed != 0 && lastWorldSeed != 0 && snap.seed != lastWorldSeed)
            {
                const auto worldId = juce::String::toHexString ((juce::int64) snap.seed)
                    + "  " + juce::String (snap.worldName);
                if (worldIds.insert (worldId).second)
                {
                    lifetime.worldsVisited = std::max (lifetime.worldsVisited + 1,
                                                       (int) worldIds.size());
                    changed = true;
                    pendingNotice = "WORLD ATLAS EXPANDED";
                }
            }
            if (snap.seed != 0)
                lastWorldSeed = snap.seed;

            if (wasNoiseLocked && ! snap.noiseLocked)
            {
                ++lifetime.noiseRescues;
                changed = true;
                pendingNotice = "RESCUE LOGGED";
            }
            wasNoiseLocked = snap.noiseLocked;
        }

        if (score.combo() > lifetime.peakCombo)
        {
            lifetime.peakCombo = score.combo();
            changed = true;
        }

        // Catalogue conditions deliberately combine independent descriptors.
        // Each can only be awarded once across the lifetime profile.
        changed |= awardAnomaly ("crystal_bloom",
            snap.appeal > 0.82f && snap.variety > 0.62f && snap.tonalness > 0.76f);
        changed |= awardAnomaly ("grey_choir",
            snap.greyness > 0.45f && snap.tonalness > 0.68f && snap.population > 20);
        changed |= awardAnomaly ("knife_garden",
            snap.roughness > 0.68f && snap.appeal > 0.60f);
        changed |= awardAnomaly ("quiet_storm",
            snap.outputRms < 0.08f && snap.variety > 0.55f && snap.population > 8);
        changed |= awardAnomaly ("crowded_silence",
            snap.population > 70 && snap.outputRms < 0.05f);
        changed |= awardAnomaly ("living_fossil",
            snap.preservedMode && snap.generation >= 80);
        changed |= awardAnomaly ("many_islands",
            snap.nicheCount >= 7 && snap.coverage > 0.42f);
        changed |= awardAnomaly ("edge_of_noise",
            snap.noiseLocked && snap.appeal > 0.60f);

        if (changed)
            markDirty();

        updateRelics();
        updateUnlockNotice (before);
    }

    void ProgressionSystem::record (Action action)
    {
        const auto before = state();
        switch (action)
        {
            case Action::sampleDigested:       ++lifetime.samplesDigested; break;
            case Action::radiationExposure:    ++lifetime.radiationExposures; break;
            case Action::breedingOperation:    ++lifetime.breedingOperations; break;
            case Action::steeringIntervention: ++lifetime.steeringInterventions; break;
            case Action::savedRun:             ++lifetime.savedRuns; break;
        }
        markDirty();
        updateRelics();
        updateUnlockNotice (before);
    }

    bool ProgressionSystem::discoverCreature (int id)
    {
        if (id < 0 || id >= 100 || ! creatureIds.insert (id).second)
            return false;
        markDirty ("CREATURE DISCOVERED " + juce::String (id + 1) + "/100");
        return true;
    }

    bool ProgressionSystem::unlockMilestoneArtifact (int index)
    {
        if (index <= 0 || ! milestoneIds.insert (index).second)
            return false;
        markDirty ("MILESTONE ARTIFACT " + juce::String (index) + " UNLOCKED");
        return true;
    }

    void ProgressionSystem::changeGameSkill (int index, int delta)
    {
        if (index < 0 || index >= 5 || delta == 0) return;
        gameSkillCounts[(size_t) index] = juce::jmax (0, gameSkillCounts[(size_t) index] + delta);
        markDirty();
    }

    int ProgressionSystem::removeRandomGameSkill (int selector)
    {
        std::array<int, 5> owned {};
        int count = 0;
        for (int i = 0; i < 5; ++i)
            if (gameSkillCounts[(size_t) i] > 0)
                owned[(size_t) count++] = i;
        if (count == 0) return -1;
        const int index = owned[(size_t) (std::abs (selector) % count)];
        --gameSkillCounts[(size_t) index];
        markDirty();
        return index;
    }

    void ProgressionSystem::addGameSounds (int count)
    {
        gameSoundCount = juce::jmax (0, gameSoundCount + count);
        markDirty();
    }

    void ProgressionSystem::addSpecialOrbs (int count)
    {
        specialOrbCount = juce::jmax (0, specialOrbCount + count);
        markDirty();
    }

    void ProgressionSystem::addRainbowOrbs (int count, bool glowing)
    {
        if (glowing) glowingRainbowOrbCount = juce::jmax (0, glowingRainbowOrbCount + count);
        else rainbowOrbCount = juce::jmax (0, rainbowOrbCount + count);
        markDirty();
    }

    void ProgressionSystem::addGreenOrbs (int count)
    {
        greenOrbCount = juce::jmax (0, greenOrbCount + count);
        markDirty();
    }

    void ProgressionSystem::addRedOrbs (int count)
    {
        redOrbCount = juce::jmax (0, redOrbCount + count);
        markDirty();
    }

    void ProgressionSystem::addPinkOrbs (int count)
    {
        pinkOrbCount = juce::jmax (0, pinkOrbCount + count);
        markDirty();
    }

    juce::String ProgressionSystem::consumeNotice()
    {
        auto result = pendingNotice;
        pendingNotice.clear();
        return result;
    }

    void ProgressionSystem::flush()
    {
        if (! dirty)
            return;

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty ("version", 1);
        root->setProperty ("lifetimeScore", lifetime.lifetimeScore);
        root->setProperty ("generationsObserved", lifetime.generationsObserved);
        root->setProperty ("lifetimeDiscoveries", lifetime.lifetimeDiscoveries);
        root->setProperty ("worldsVisited", lifetime.worldsVisited);
        root->setProperty ("noiseRescues", lifetime.noiseRescues);
        root->setProperty ("samplesDigested", lifetime.samplesDigested);
        root->setProperty ("radiationExposures", lifetime.radiationExposures);
        root->setProperty ("breedingOperations", lifetime.breedingOperations);
        root->setProperty ("steeringInterventions", lifetime.steeringInterventions);
        root->setProperty ("anomaliesFound", lifetime.anomaliesFound);
        root->setProperty ("relicsFound", lifetime.relicsFound);
        root->setProperty ("savedRuns", lifetime.savedRuns);
        root->setProperty ("peakCombo", lifetime.peakCombo);

        juce::Array<juce::var> anomaliesArray;
        for (const auto& id : anomalyIds) anomaliesArray.add (id);
        root->setProperty ("anomalies", anomaliesArray);

        juce::Array<juce::var> relicsArray;
        for (const auto& id : relicIds) relicsArray.add (id);
        root->setProperty ("relics", relicsArray);

        juce::Array<juce::var> worldsArray;
        for (const auto& id : worldIds) worldsArray.add (id);
        root->setProperty ("worlds", worldsArray);

        juce::Array<juce::var> creaturesArray;
        for (const auto id : creatureIds) creaturesArray.add (id);
        root->setProperty ("creatures", creaturesArray);

        juce::Array<juce::var> milestonesArray;
        for (const auto id : milestoneIds) milestonesArray.add (id);
        root->setProperty ("milestones", milestonesArray);

        for (int i = 0; i < 5; ++i)
            root->setProperty ("gameSkill" + juce::String (i), gameSkillCounts[(size_t) i]);
        root->setProperty ("gameSounds", gameSoundCount);
        root->setProperty ("specialOrbs", specialOrbCount);
        root->setProperty ("rainbowOrbs", rainbowOrbCount);
        root->setProperty ("glowingRainbowOrbs", glowingRainbowOrbCount);
        root->setProperty ("greenOrbs", greenOrbCount);
        root->setProperty ("redOrbs", redOrbCount);
        root->setProperty ("pinkOrbs", pinkOrbCount);

        const auto file = progressFile();
        file.getParentDirectory().createDirectory();
        file.replaceWithText (juce::JSON::toString (juce::var (root.release()), true));
        dirty = false;
    }
}
