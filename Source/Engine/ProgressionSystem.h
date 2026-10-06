#pragma once

#include "Progression.h"
#include "OrganismState.h"
#include "ScoreSystem.h"
#include <juce_core/juce_core.h>
#include <set>

namespace mutagen
{
    class ProgressionSystem
    {
    public:
        enum class Action
        {
            sampleDigested,
            radiationExposure,
            breedingOperation,
            steeringIntervention,
            savedRun
        };

        ProgressionSystem();
        ~ProgressionSystem();

        void observe (const EngineSnapshot&, const ScoreSystem&);
        void record (Action);
        void flush();

        const progression::LifetimeStats& stats() const { return lifetime; }
        progression::ResearchState state() const { return progression::evaluate (lifetime); }

        const std::set<juce::String>& anomalies() const { return anomalyIds; }
        const std::set<juce::String>& relics() const { return relicIds; }
        const std::set<juce::String>& worlds() const { return worldIds; }
        const std::set<int>& creatures() const { return creatureIds; }
        const std::set<int>& milestoneArtifacts() const { return milestoneIds; }

        bool discoverCreature (int id);
        bool unlockMilestoneArtifact (int index);

        // Persistent lab-game inventory.
        const std::array<int, 5>& gameSkills() const { return gameSkillCounts; }
        int unlockedGameSounds() const { return gameSoundCount; }
        int specialOrbs() const { return specialOrbCount; }
        int rainbowOrbs() const { return rainbowOrbCount; }
        int glowingRainbowOrbs() const { return glowingRainbowOrbCount; }
        int greenOrbs() const { return greenOrbCount; }
        int redOrbs() const { return redOrbCount; }
        int pinkOrbs() const { return pinkOrbCount; }
        int snareSounds() const { return snareSoundCount; }
        int tomSounds() const { return tomSoundCount; }
        int bassDrumSounds() const { return bassDrumSoundCount; }
        int pianoKeySounds() const { return pianoKeySoundCount; }
        int stringSounds() const { return stringSoundCount; }
        int tambourineSounds() const { return tambourineSoundCount; }
        int bassDrops() const { return bassDropCount; }
        int kazooSounds() const { return kazooSoundCount; }
        int clapSounds() const { return clapSoundCount; }
        int padSounds() const { return padSoundCount; }
        int upSweepSounds() const { return upSweepSoundCount; }
        int totalGameSkills() const
        {
            int total = 0;
            for (const auto n : gameSkillCounts) total += n;
            return total;
        }

        void changeGameSkill (int index, int delta);
        int removeRandomGameSkill (int selector);
        void addGameSounds (int count);
        void addSpecialOrbs (int count);
        void addRainbowOrbs (int count, bool glowing = false);
        void addGreenOrbs (int count);
        void addRedOrbs (int count);
        void addPinkOrbs (int count);
        void addSnareSounds (int count);
        void addTomSounds (int count);
        void addBassDrumSounds (int count);
        bool tradeBassDrumsForPianoKey();
        void addStringSounds (int count);
        void addTambourineSounds (int count);
        void addBassDrops (int count);
        void addKazooSounds (int count);
        void addClapSounds (int count);
        void addPadSounds (int count);
        void addUpSweepSounds (int count);

        /** Returns and clears the newest unlock/event notice. */
        juce::String consumeNotice();

        static juce::File progressFile();

    private:
        void load();
        void markDirty (const juce::String& notice = {});
        bool awardAnomaly (const juce::String& id, bool condition);
        bool awardRelic (const juce::String& id, bool condition);
        void updateRelics();
        void updateUnlockNotice (const progression::ResearchState& before);

        progression::LifetimeStats lifetime;
        std::set<juce::String> anomalyIds;
        std::set<juce::String> relicIds;
        std::set<juce::String> worldIds;
        std::set<int> creatureIds;
        std::set<int> milestoneIds;

        std::array<int, 5> gameSkillCounts {};
        int gameSoundCount = 0;
        int specialOrbCount = 0;
        int rainbowOrbCount = 0;
        int glowingRainbowOrbCount = 0;
        int greenOrbCount = 0;
        int redOrbCount = 0;
        int pinkOrbCount = 0;
        int snareSoundCount = 0;
        int tomSoundCount = 0;
        int bassDrumSoundCount = 0;
        int pianoKeySoundCount = 0;
        int stringSoundCount = 0;
        int tambourineSoundCount = 0;
        int bassDropCount = 0;
        int kazooSoundCount = 0;
        int clapSoundCount = 0;
        int padSoundCount = 0;
        int upSweepSoundCount = 0;

        juce::String pendingNotice;

        juce::int64 lastScore = 0;
        int lastGeneration = 0;
        int lastDiscoveries = 0;
        uint64_t lastWorldSeed = 0;
        bool wasNoiseLocked = false;
        bool primed = false;
        bool dirty = false;
    };
}
