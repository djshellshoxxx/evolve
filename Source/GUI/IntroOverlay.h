// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mutagen
{
    /** The first-run walkthrough: what everything is, and what MUTAGEN is for.
        Shown automatically once, and again whenever INTRO is pressed. */
    class IntroOverlay : public juce::Component
    {
    public:
        IntroOverlay();

        void open();
        std::function<void()> onClosed;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        void show (int page);
        void close();

        int page = 0;
        juce::TextButton back { "BACK" }, next { "NEXT" }, skip { "SKIP" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntroOverlay)
    };

    /** Asks for the player's name at the start of every game. */
    class NameOverlay : public juce::Component
    {
    public:
        NameOverlay();

        void open (const juce::String& lastName);
        std::function<void (const juce::String&)> onName;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void submit();

        juce::TextEditor field;
        juce::TextButton begin { "BEGIN" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NameOverlay)
    };
}
