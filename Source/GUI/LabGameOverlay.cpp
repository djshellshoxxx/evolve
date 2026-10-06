#include "LabGameOverlay.h"
#include "MutagenLookAndFeel.h"
#include <cmath>

namespace mutagen
{
    LabGameOverlay::LabGameOverlay()
    {
        setOpaque (false);
        for (auto* button : { &a, &b, &c, &decline }) addAndMakeVisible (*button);
        decline.onClick = [this] { setVisible (false); mode = Mode::none; if (onDecline) onDecline(); };
        startTimerHz (60);
        setVisible (false);
    }

    void LabGameOverlay::showRoulette (juce::int64 score)
    {
        mode = Mode::roulette; currentGame = labgames::Game::roulette;
        title = "LAB ROULETTE";
        detail = "The wheel has nine kinds of wedges." + juce::String ((score % 10) == 7 ? " Score ends in 7: the two bad wedges are absent." : "");
        anim = 0.0f; resultPositive = true; configureButtons(); setVisible (true); toFront (true);
    }

    void LabGameOverlay::showMonte()
    {
        mode = Mode::monte; currentGame = labgames::Game::monte;
        title = "THREE CARD MONTE"; detail = "One card is right. You may walk away.";
        anim = 0.0f; resultPositive = true; configureButtons(); setVisible (true); toFront (true);
    }

    void LabGameOverlay::showSlots()
    {
        mode = Mode::slots; currentGame = labgames::Game::slots;
        title = "COLONY SLOTS"; detail = "Spin once. 777 wakes the rainbow brood.";
        anim = 0.0f; resultPositive = true; configureButtons(); setVisible (true); toFront (true);
    }

    void LabGameOverlay::showDice()
    {
        mode = Mode::dice; currentGame = labgames::Game::dice;
        title = "DICE AGAINST THE LAB"; detail = "High roll wins. Ties belong to the lab.";
        anim = 0.0f; resultPositive = true; configureButtons(); setVisible (true); toFront (true);
    }

    void LabGameOverlay::configureButtons()
    {
        a.setVisible (true); b.setVisible (false); c.setVisible (false); decline.setVisible (true);
        if (mode == Mode::roulette)
        {
            a.setButtonText ("SPIN"); a.onClick = [this] { if (onRouletteSpin) onRouletteSpin(); };
        }
        else if (mode == Mode::monte)
        {
            a.setButtonText ("CARD 1"); b.setButtonText ("CARD 2"); c.setButtonText ("CARD 3");
            b.setVisible (true); c.setVisible (true);
            a.onClick = [this] { if (onMontePick) onMontePick (0); };
            b.onClick = [this] { if (onMontePick) onMontePick (1); };
            c.onClick = [this] { if (onMontePick) onMontePick (2); };
        }
        else if (mode == Mode::slots)
        {
            a.setButtonText ("SPIN"); a.onClick = [this] { if (onSlotsSpin) onSlotsSpin(); };
        }
        else if (mode == Mode::dice)
        {
            a.setButtonText ("ROLL"); a.onClick = [this] { if (onDiceRoll) onDiceRoll(); };
        }
    }

    void LabGameOverlay::resolve (const juce::String& t, const juce::String& d,
                                  labgames::Game game, labgames::SoundMoment moment,
                                  bool positive)
    {
        mode = Mode::result; currentGame = game; currentMoment = moment;
        title = t; detail = d; resultPositive = positive; anim = 0.0f;
        a.setVisible (false); b.setVisible (false); c.setVisible (false);
        decline.setButtonText ("CLOSE");
        decline.setVisible (true);
        repaint();
    }

    void LabGameOverlay::timerCallback()
    {
        if (! isVisible()) return;
        anim += 1.0f / 60.0f;
        repaint();
    }

    void LabGameOverlay::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.78f));
        g.fillRoundedRectangle (r.reduced (6.0f), 18.0f);

        const float pulse = 0.5f + 0.5f * std::sin (anim * 5.0f);
        juce::Colour accent = currentGame == labgames::Game::roulette ? juce::Colours::violet
                           : currentGame == labgames::Game::monte ? juce::Colours::orange
                           : currentGame == labgames::Game::slots ? juce::Colours::cyan
                           : juce::Colours::lime;
        if (mode == Mode::result && ! resultPositive) accent = juce::Colours::red;

        g.setColour (accent.withAlpha (0.18f + pulse * 0.18f));
        if (currentGame == labgames::Game::roulette)
        {
            auto c = r.getCentre();
            float rad = 52.0f + pulse * 8.0f;
            for (int i=0;i<12;++i)
            {
                float a = anim * 1.8f + i * juce::MathConstants<float>::twoPi / 12.0f;
                g.fillEllipse (c.x + std::cos(a)*rad - 5.0f, c.y + std::sin(a)*rad - 5.0f, 10.0f, 10.0f);
            }
        }
        else if (currentGame == labgames::Game::monte)
        {
            for (int i=0;i<3;++i)
            {
                float y = 86.0f + std::sin(anim*4.0f + i*1.7f)*9.0f;
                g.drawRoundedRectangle ({ 48.0f + i*72.0f, y, 50.0f, 72.0f }, 6.0f, 3.0f);
            }
        }
        else if (currentGame == labgames::Game::slots)
        {
            for (int i=0;i<3;++i)
            {
                auto box = juce::Rectangle<float> (54.0f+i*68.0f, 92.0f, 50.0f, 50.0f);
                g.drawRoundedRectangle (box, 5.0f, 3.0f);
                g.drawText (juce::String ((int)(anim*13+i*3)%10), box.toNearestInt(), juce::Justification::centred);
            }
        }
        else
        {
            for (int i=0;i<2;++i)
            {
                float rot = anim * (i? -3.0f:3.4f);
                auto box = juce::Rectangle<float> (86.0f+i*88.0f, 98.0f + std::sin(rot)*8.0f, 54.0f,54.0f);
                g.drawRoundedRectangle (box, 8.0f, 3.0f);
                g.fillEllipse (box.getCentreX()-4, box.getCentreY()-4,8,8);
            }
        }

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (20.0f, juce::Font::bold));
        g.drawText (title, 20, 18, getWidth()-40, 28, juce::Justification::centred);
        g.setFont (12.5f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawFittedText (detail, 28, 50, getWidth()-56, 54, juce::Justification::centred, 3);
    }

    void LabGameOverlay::resized()
    {
        auto r = getLocalBounds().reduced (24);
        auto buttons = r.removeFromBottom (44);
        decline.setBounds (buttons.removeFromRight (110).reduced (4));
        if (c.isVisible()) c.setBounds (buttons.removeFromRight (100).reduced (4));
        if (b.isVisible()) b.setBounds (buttons.removeFromRight (100).reduced (4));
        if (a.isVisible()) a.setBounds (buttons.removeFromRight (100).reduced (4));
    }
}
