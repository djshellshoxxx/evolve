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
