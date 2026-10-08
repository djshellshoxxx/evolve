// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/ChanceEvents.h"
#include <vector>

namespace mutagen
{
    /** A transparent animation layer over the culture chamber.

        It draws what the story does to the dish: the chance-event animations
        (meteors, spores, aurora, eclipse, lightning...), the act and twist
        banners, and the ending card. Every effect is a pure function of its age
        and seed, so nothing is allocated per frame and the layer stops its timer
        the moment nothing is playing. It never takes a mouse click. */
    class StoryFx : public juce::Component,
                    private juce::Timer
    {
    public:
        StoryFx();
        ~StoryFx() override;

        void play (chance::Anim, juce::Colour, float intensity = 1.0f);

        /** A title that wipes across the dish, holds, and wipes away. */
        void banner (const juce::String& title, const juce::String& subtitle, juce::Colour);

        /** The fact card: category tag, typed text, knowledge points, and the
            demonstration bonus line. Fades by itself after about seven seconds. */
        void factToast (const juce::String& category, const juce::String& text,
                        int knowledgePoints, int demoBonus, juce::Colour);

        /** Glitched static with a split-colour flash, for twists. */
        void glitch (juce::Colour);

        /** Advances every effect by `dt` seconds (the timer calls this; tests drive it directly). */
        void advance (float dt);

        bool isPlaying() const { return ! fx.empty() || ! banners.empty() || ! toasts.empty(); }

        void paint (juce::Graphics&) override;
        bool hitTest (int, int) override { return false; }

    private:
        struct Fx { chance::Anim anim; juce::Colour colour; float age, life, intensity; std::uint32_t seed; bool glitch; };
        struct Banner { juce::String title, subtitle; juce::Colour colour; float age; };
        struct Toast { juce::String category, text; int points, bonus; juce::Colour colour; float age; };

        void timerCallback() override;
        void drawFx (juce::Graphics&, const Fx&, juce::Rectangle<float>) const;
        void drawBanner (juce::Graphics&, const Banner&, juce::Rectangle<float>) const;
        void drawToast (juce::Graphics&, const Toast&, juce::Rectangle<float>) const;

        std::vector<Fx> fx;
        std::vector<Banner> banners;
        std::vector<Toast> toasts;
        std::uint32_t counter = 1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StoryFx)
    };
}
