// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/Storyline.h"
#include "../Engine/SoundCollection.h"
#include <functional>
#include <vector>

namespace mutagen
{
    class MutagenProcessor;

    /** The narrator strip under the culture chamber, and the director behind it.

        It runs the Resonance Acts: opens each act, fires seeded story events
        (which make the colony sing intervals, chords, scales and tempo gates
        while a character speaks), springs the run's twist, asks the occasional
        ear check, and keeps the specimen jar - COLLECT records the last few
        seconds of the colony, EXPORT copies every collected sound out as WAV. */
    class StoryPanel : public juce::Component,
                       private juce::Timer
    {
    public:
        explicit StoryPanel (MutagenProcessor&);
        ~StoryPanel() override;

        /** Advance the director. Call from the editor's UI timer. */
        void tick (double dt, bool paused);

        /** Queue a capture of the colony `delaySec` from now (so a reward
            sound that has just been triggered is inside the recording). */
        void collectSoon (const juce::String& label, double delaySec = 0.6, float seconds = 3.0f);

        /** Story-relevant moments reported by the rest of the game. */
        void say (story::Speaker, const juce::String& line);

        /** Scores the player for engaging with the story (weight, label). */
        std::function<void (float, const juce::String&)> onReward;

        const story::Progress& progress() const { return prog; }
        const story::RunStory& run() const { return runStory; }
        SoundCollection& collection() { return jar; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void fireEvent (int id);
        void openAct (int act);
        void revealTwist();
        void askQuiz();
        void answer (int choice);
        void perform (story::Effect, const std::array<int, 4>& notes, int param);
        void playNotes (const std::vector<int>& semis, double spacing, double hold);
        void releaseAll();
        void doCollect (const juce::String& label, float seconds);
        void exportAll();
        void persist();

        MutagenProcessor& processor;
        story::Progress prog;
        story::RunStory runStory;
        SoundCollection jar;

        double clock = 0.0;
        int currentAct = 0;
        int quizIndex = -1;

        juce::String speaker, role, line, topic;
        juce::Colour speakerColour;
        float typed = 0.0f;          // characters revealed so far
        float flash = 0.0f;

        struct NoteEvent { double at; int note; bool on; };
        std::vector<NoteEvent> noteQueue;

        struct Capture { double at; juce::String label; float seconds; };
        std::vector<Capture> captures;

        juce::TextButton collectButton { "COLLECT" }, exportButton { "EXPORT WAV" };
        juce::TextButton answerButtons[3];
        std::unique_ptr<juce::FileChooser> chooser;
        double lastManualCollect = -10.0;
        double keyIntroAt = 14.0;
        bool keyIntroDone = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StoryPanel)
    };
}
