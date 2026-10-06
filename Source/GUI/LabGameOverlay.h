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

        void showRoulette (juce::int64 score);
        void showMonte();
        void showSlots();
        void showDice();

        bool active() const { return mode != Mode::none; }

        std::function<void()> onDecline;
        std::function<void()> onRouletteSpin;
        std::function<void(int card)> onMontePick;
        std::function<void()> onSlotsSpin;
        std::function<void()> onDiceRoll;

        void resolve (const juce::String& title, const juce::String& detail,
                      labgames::Game game, labgames::SoundMoment moment,
                      bool positive);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        enum class Mode { none, roulette, monte, slots, dice, result };
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
