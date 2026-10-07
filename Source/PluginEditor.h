// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/MutagenLookAndFeel.h"
#include "GUI/CultureChamber.h"
#include "GUI/GerminationPanel.h"
#include "GUI/EnvironmentPanel.h"
#include "GUI/GenomeInspector.h"
#include "GUI/EvolutionTimeline.h"
#include "GUI/BreedingLabView.h"
#include "GUI/PerformanceView.h"
#include "GUI/FxRackView.h"
#include "GUI/TopBar.h"
#include "GUI/ScoreHud.h"
#include "GUI/GameBar.h"
#include "GUI/HelpView.h"
#include "GUI/OptionsView.h"
#include "GUI/ProgressionView.h"
#include "GUI/HauntedOverlay.h"
#include "GUI/LabGameOverlay.h"
#include "GUI/StoryPanel.h"
#include "Engine/ScoreSystem.h"
#include "Engine/HauntedEvents.h"
#include "Engine/HiddenDiscoveries.h"
#include "Engine/PhysicalOddities.h"
#include "Engine/LabGames.h"
#include "Engine/ProgressionSystem.h"
#include "Engine/RenderEngine.h"
#include <deque>

namespace mutagen
{
    class MutagenEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
    {
    public:
        explicit MutagenEditor (MutagenProcessor&);
        ~MutagenEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void openBreedingLab (const OrganismState& parent);
        void doRender();
        void layoutMain();

        void wireGameLayer();
        void wireChrome();
        void applyTooltipSetting (bool enabled);
        void sendSimple (CommandType t);
        void saveRun();
        void loadRun();
        void handleDroppedFiles (const juce::StringArray& files);
        void handleHiddenClick (const juce::MouseEvent&);
        void triggerCreature (int creatureId, bool newlyDiscovered);
        void triggerMilestoneArtifact (int index, bool replay);
        void applyMilestoneSkill (int index);
        void serviceHauntedMilestones (juce::int64 beforeScore, juce::int64 afterScore);
        void serviceSpontaneousPhantom();

        enum class PendingGame { roulette, monte, slots, dice, twentyOne, scratch, skillRoulette };
        void wireLabGames();
        void rewardInstrument (int kind, int count);
        void serviceLabGameSchedule (juce::int64 beforeScore, juce::int64 afterScore);
        void queueLabGame (PendingGame);
        void presentNextLabGame();
        void playLabSound (labgames::Game, labgames::SoundMoment, int variation = 0, float intensity = 0.8f);
        void awardGameSkill (int index, int count = 1);
        void checkSkillRouletteTrigger (int skillsBefore, int skillsAfter);
        void resolveTwentyOne();
        int drawCardValue();

        MutagenProcessor& processor;
        haunted::DesktopPhantomManager phantomManager;
        MutagenLookAndFeel lnf;

        TopBar            topBar;
        GerminationPanel  germination;
        EnvironmentPanel  environment;
        CultureChamber    chamber;
        GenomeInspector   inspector;
        EvolutionTimeline timeline;
        BreedingLabView   breedingLab;
        PerformanceView   performance;
        FxRackView        fxRack;
        LabGameOverlay    labGameOverlay;
        StoryPanel        storyPanel { processor };

        // ---- the chrome every plugin in the range carries ----
        HelpView          helpView;
        OptionsView       optionsView;
        std::unique_ptr<juce::TooltipWindow> tooltips;

        // ---- the persistent meta-game ----
        ProgressionSystem progressionSystem;
        ProgressionView   progressionView;

        // ---- the game layer ----
        ScoreSystem       scoreSystem;
        ScoreHud          scoreHud;
        GameBar           gameBar;
        uint64_t          lastRadiationCounter = 0;

        // ---- hidden / haunted laboratory layer -------------------------
        haunted::SessionRng hauntedRng;
        haunted::ClickSequence hiddenClicks;
        haunted::CdTrayPulser cdTrayPulser;
        bool cdTrayRollDone = false;
        bool micReverseRollDone = false;
        bool hiddenMicAwaitingReplay = false;
        int spontaneousPhantomTicks = 0;
        double lastCreatureDiscoverySec = -100.0;
        double lastHiddenEffectSec = -100.0;

        // ---- optional lab games ----------------------------------------
        std::deque<PendingGame> pendingGames;
        int lastGameMinuteBucket = 0;
        int scratchTickets = 0;
        bool skillRoulettePendingRoll = false;
        int monteWinningCard = 0;
        int twentyOnePlayer = 0;
        int twentyOneHouse = 0;
        bool twentyOneActive = false;
        double playSeconds = 0.0;

        RenderEngine      renderEngine;
        std::unique_ptr<juce::FileChooser> chooser;

        EngineSnapshot snapshot;
        double lastTimeSec = 0.0;
        double inspectorAccum = 0.0;
        double progressionFlushAccum = 0.0;
        bool   showInspector = true;
        bool   showPerformance = false;
        bool   showFx = false;
        int    labParentFilled = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenEditor)
    };
}
