#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/LabGames.h"
#include <functional>

namespace mutagen
{
    class LabGameOverlay : public juce::Component,
                           private juce::Timer
    {
    public:
        LabGameOverlay();

        void showRoulette (juce::int64 score, bool skillRoulette = false);
        void showMonte();
        void showSlots();
        void showDice();
        void showTwentyOne (int playerTotal, bool canHit);
        void showScratch (int ticketsAvailable);
        void showMutationSlots (bool sideGame);
        void showPetChoice();
        void showWolfermean();

        bool active() const { return mode != Mode::none; }

        std::function<void()> onDecline;
        std::function<void()> onRouletteSpin;
        std::function<void()> onSkillRouletteSpin;
        std::function<void(int card)> onMontePick;
        std::function<void()> onSlotsSpin;
        std::function<void()> onDiceRoll;
        std::function<void()> onTwentyOneHit;
        std::function<void()> onTwentyOneStand;
        std::function<void()> onScratch;
        std::function<void()> onMutationSpin;
        std::function<void()> onSideMutationSpin;
        std::function<void(bool cat)> onPetChosen;

        void resolve (const juce::String& title, const juce::String& detail,
                      labgames::Game game, labgames::SoundMoment moment,
                      bool positive);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        enum class Mode
        {
            none, roulette, skillRoulette, monte, slots, dice,
            twentyOne, scratch, mutationSlots, sideMutationSlots,
            petChoice, wolfermean, result
        };

        void timerCallback() override;
        void configureButtons();

        Mode mode = Mode::none;
        labgames::Game currentGame { labgames::Game::roulette };
        labgames::SoundMoment currentMoment { labgames::SoundMoment::appear };
        juce::String title, detail;
        float anim = 0.0f;
        bool resultPositive = true;

        juce::TextButton a, b, c, decline { "NO THANKS" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabGameOverlay)
    };
}
