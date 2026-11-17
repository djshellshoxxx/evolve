#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <array>
#include <cmath>

namespace mutagen
{
    // Three opt-in secret puzzles. Gameplay lives on the message thread only.
    class SecretArcade final : public juce::Component, private juce::Timer
    {
    public:
        std::function<void (int)> onAward;
        SecretArcade()
        {
            for (int i = 0; i < 3; ++i)
            {
                auto& b = choices[(size_t) i];
                addAndMakeVisible (b);
                b.onClick = [this, i] { choose (i); };
            }
            addAndMakeVisible (next);
            next.onClick = [this] { advance(); };
            addAndMakeVisible (close);
            close.setButtonText ("CLOSE");
            close.onClick = [this] { setVisible (false); };
            setVisible (false);
        }
        ~SecretArcade() override { stopTimer(); }

        void open (int number)
        {
            mode = juce::jlimit (0, 2, number);
            round = 0;
            finished = false;
            advance();
            setVisible (true);
            toFront (true);
            startTimerHz (30);
        }

        void visibilityChanged() override { if (! isVisible()) stopTimer(); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20);
            auto foot = r.removeFromBottom (42);
            close.setBounds (foot.removeFromRight (100));
            next.setBounds (foot.removeFromLeft (110));
            auto buttons = r.removeFromBottom (58);
            for (auto& b : choices) b.setBounds (buttons.removeFromLeft (juce::jmax (1, buttons.getWidth() / (3 - (int) (&b - choices.data())))).reduced (5));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff07121a));
            auto a = getLocalBounds().toFloat();
            auto accent = juce::Colour::fromHSV ((float) mode / 3.0f + 0.08f, 0.85f, 1.0f, 1.0f);
            g.setColour (accent);
            g.setFont (juce::Font (24.0f));
            static const char* titles[] = { "ECHO SEQUENCE", "SPECTRAL RIDDLE", "PHASE LOCK" };
            g.drawText (titles[mode], 16, 15, getWidth() - 32, 35, juce::Justification::centred);
            g.setColour (juce::Colours::white);
            g.setFont (16.0f);
            g.drawFittedText (prompt, 28, 60, getWidth() - 56, 70, juce::Justification::centred, 3);
            if (mode == 2 && ! finished)
            {
                auto track = a.reduced (55.0f).withHeight (35.0f).withCentre ({a.getCentreX(), a.getCentreY()});
                g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillRoundedRectangle (track, 9.0f);
                g.setColour (juce::Colours::lime.withAlpha (0.65f));
                g.fillRect (track.getCentreX() - track.getWidth() * 0.08f, track.getY(), track.getWidth() * 0.16f, track.getHeight());
                g.setColour (accent);
                const float x = track.getX() + phase * track.getWidth();
                g.fillEllipse (x - 10.0f, track.getCentreY() - 10.0f, 20.0f, 20.0f);
            }
            if (mode == 0 && ! finished)
            {
                for (int i = 0; i < 3; ++i)
                {
                    auto c = juce::Colour::fromHSV (i / 3.0f, 0.9f, 1.0f, 1.0f);
                    g.setColour (c.withAlpha (i == target ? 0.9f : 0.25f));
                    g.fillEllipse (a.getCentreX() - 100.0f + i * 80.0f, a.getCentreY() - 24.0f, 48.0f, 48.0f);
                }
            }
        }
    private:
        void advance()
        {
            finished = false;
            next.setVisible (false);
            for (auto& b : choices) b.setVisible (mode != 2);
            // Deterministic cycling: every round can be reproduced without RNG.
            target = (round * 7 + mode * 11 + 1) % 3;
            if (mode == 0)
            {
                prompt = "Remember the brightest orb. Select its colour.";
                choices[0].setButtonText ("RED");
                choices[1].setButtonText ("GREEN");
                choices[2].setButtonText ("BLUE");
            }
            else if (mode == 1)
            {
                prompt = "Which signal has the highest frequency?";
                for (int i = 0; i < 3; ++i)
                    choices[(size_t) i].setButtonText (juce::String (110 + ((i - target + 3) % 3) * 110) + " Hz");
                target = (target + 2) % 3; // Index of the 330 Hz option.
            }
            else
            {
                prompt = "Tap LOCK when the orbit reaches the green central band.";
                choices[0].setVisible (true);
                choices[0].setButtonText ("LOCK");
                choices[1].setVisible (false);
                choices[2].setVisible (false);
                phase = 0.0f;
            }
            repaint();
        }
        void choose (int index)
        {
            if (finished) return;
            const bool won = mode == 2 ? std::abs (phase - 0.5f) <= 0.08f : index == target;
            finished = true;
            prompt = won ? "PATTERN STABILIZED. +93 POINTS" : "SIGNAL LOST. TRY ANOTHER ROUND.";
            if (won && onAward) onAward (93);
            for (auto& b : choices) b.setVisible (false);
            next.setButtonText ("AGAIN");
            next.setVisible (true);
            ++round;
            resized();
            repaint();
        }
        void timerCallback() override
        {
            if (mode == 2 && ! finished)
            {
                phase = 0.5f + 0.5f * std::sin (++ticks * 0.065f);
                repaint();
            }
        }
        std::array<juce::TextButton, 3> choices;
        juce::TextButton next { "AGAIN" }, close { "CLOSE" };
        juce::String prompt;
        int mode = 0, round = 0, target = 0, ticks = 0;
        float phase = 0.0f;
        bool finished = false;
    };
}
