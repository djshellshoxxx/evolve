#include "LabGameOverlay.h"
#include <cmath>

namespace mutagen
{
    LabGameOverlay::LabGameOverlay()
    {
        setOpaque (false);
        for (auto* button : { &a, &b, &c, &decline })
            addAndMakeVisible (*button);

        decline.onClick = [this]
        {
            setVisible (false);
            mode = Mode::none;
            if (onDecline) onDecline();
        };

        startTimerHz (60);
        setVisible (false);
    }

    void LabGameOverlay::showRoulette (juce::int64 score, bool alternate)
    {
        mode = alternate ? Mode::skillRoulette : Mode::roulette;
        currentGame = alternate ? labgames::Game::skillRoulette : labgames::Game::roulette;
        title = alternate ? "SKILL ROULETTE" : "LAB ROULETTE";
        detail = alternate
            ? "Every fifth skill can wake this second wheel."
            : "Nine wedge types. The two bad wedges vanish whenever the current score ends in 7.";
        if (! alternate && score >= 0 && score % 10 == 7)
            detail << " 7-PROTECTION ACTIVE.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showMonte()
    {
        mode = Mode::monte;
        currentGame = labgames::Game::monte;
        title = "THREE CARD MONTE";
        detail = "One card is right. Pick one, or walk away.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showSlots()
    {
        mode = Mode::slots;
        currentGame = labgames::Game::slots;
        title = "COLONY SLOTS";
        detail = "One spin. 777 wakes the glowing rainbow brood.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showDice()
    {
        mode = Mode::dice;
        currentGame = labgames::Game::dice;
        title = "DICE AGAINST THE LAB";
        detail = "High roll wins. Ties belong to the lab.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showTwentyOne (int playerTotal, bool canHit)
    {
        mode = Mode::twentyOne;
        currentGame = labgames::Game::twentyOne;
        title = "21";
        detail = "Your hand: " + juce::String (playerTotal)
               + (canHit ? "  HIT or STAND." : "  STAND to resolve.");
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        if (! canHit) a.setEnabled (false);
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showScratch (int ticketsAvailable)
    {
        mode = Mode::scratch;
        currentGame = labgames::Game::scratch;
        title = "SCRATCH & WIN";
        detail = juce::String (ticketsAvailable)
               + " ticket" + (ticketsAvailable == 1 ? "" : "s")
               + " available. Five symbols, five possible types.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showMutationSlots (bool sideGame)
    {
        mode = sideGame ? Mode::sideMutationSlots : Mode::mutationSlots;
        currentGame = sideGame ? labgames::Game::sideMutationSlots
                               : labgames::Game::mutationSlots;
        title = sideGame ? "SIDE MUTATION SLOTS" : "MUTATION SLOTS";
        detail = sideGame
            ? "Permanent side machine. Same prize grammar, different sounds."
            : "Skill-milestone machine. Jackpot breeds things that should not breed.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showPetChoice()
    {
        mode = Mode::petChoice;
        currentGame = labgames::Game::sideMutationSlots;
        title = "CHOOSE A LAB PET";
        detail = "The pet persists and follows yellow orbs around the chamber.";
        anim = 0.0f;
        resultPositive = true;
        configureButtons();
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::showWolfermean()
    {
        mode = Mode::wolfermean;
        currentGame = labgames::Game::slots;
        title = "WOLFERMEAN";
        detail = "snit snit";
        anim = 0.0f;
        resultPositive = true;
        a.setVisible (false);
        b.setVisible (false);
        c.setVisible (false);
        decline.setVisible (false);
        setVisible (true);
        toFront (true);
    }

    void LabGameOverlay::configureButtons()
    {
        a.setEnabled (true);
        a.setVisible (true);
        b.setVisible (false);
        c.setVisible (false);
        decline.setButtonText ("NO THANKS");
        decline.setVisible (true);
        juce::Component::SafePointer<LabGameOverlay> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->resized(); });

        if (mode == Mode::roulette)
        {
            a.setButtonText ("SPIN");
            a.onClick = [this] { if (onRouletteSpin) onRouletteSpin(); };
        }
        else if (mode == Mode::skillRoulette)
        {
            a.setButtonText ("SPIN");
            a.onClick = [this] { if (onSkillRouletteSpin) onSkillRouletteSpin(); };
        }
        else if (mode == Mode::monte)
        {
            a.setButtonText ("CARD 1");
            b.setButtonText ("CARD 2");
            c.setButtonText ("CARD 3");
            b.setVisible (true);
            c.setVisible (true);
            a.onClick = [this] { if (onMontePick) onMontePick (0); };
            b.onClick = [this] { if (onMontePick) onMontePick (1); };
            c.onClick = [this] { if (onMontePick) onMontePick (2); };
        }
        else if (mode == Mode::slots)
        {
            a.setButtonText ("SPIN");
            a.onClick = [this] { if (onSlotsSpin) onSlotsSpin(); };
        }
        else if (mode == Mode::dice)
        {
            a.setButtonText ("ROLL");
            a.onClick = [this] { if (onDiceRoll) onDiceRoll(); };
        }
        else if (mode == Mode::twentyOne)
        {
            a.setButtonText ("HIT");
            b.setButtonText ("STAND");
            b.setVisible (true);
            a.onClick = [this] { if (onTwentyOneHit) onTwentyOneHit(); };
            b.onClick = [this] { if (onTwentyOneStand) onTwentyOneStand(); };
        }
        else if (mode == Mode::scratch)
        {
            a.setButtonText ("SCRATCH");
            a.onClick = [this] { if (onScratch) onScratch(); };
        }
        else if (mode == Mode::mutationSlots)
        {
            a.setButtonText ("SPIN");
            a.onClick = [this] { if (onMutationSpin) onMutationSpin(); };
        }
        else if (mode == Mode::sideMutationSlots)
        {
            a.setButtonText ("SPIN");
            a.onClick = [this] { if (onSideMutationSpin) onSideMutationSpin(); };
        }
        else if (mode == Mode::petChoice)
        {
            a.setButtonText ("CAT");
            b.setButtonText ("DOG");
            b.setVisible (true);
            decline.setButtonText ("LATER");
            a.onClick = [this] { if (onPetChosen) onPetChosen (true); };
            b.onClick = [this] { if (onPetChosen) onPetChosen (false); };
        }
    }

    void LabGameOverlay::resolve (const juce::String& t, const juce::String& d,
                                  labgames::Game game, labgames::SoundMoment moment,
                                  bool positive)
    {
        mode = Mode::result;
        currentGame = game;
        currentMoment = moment;
        title = t;
        detail = d;
        resultPositive = positive;
        anim = 0.0f;
        a.setVisible (false);
        b.setVisible (false);
        c.setVisible (false);
        decline.setButtonText ("CLOSE");
        resized();
        decline.setVisible (true);
        repaint();
    }

    void LabGameOverlay::timerCallback()
    {
        if (! isVisible()) return;
        anim += 1.0f / 60.0f;
        if (mode == Mode::wolfermean && anim > 2.5f)
        {
            setVisible (false);
            mode = Mode::none;
            if (onDecline) onDecline();
            return;
        }
        repaint();
    }

    void LabGameOverlay::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.82f));
        g.fillRoundedRectangle (r.reduced (6.0f), 18.0f);

        const float pulse = 0.5f + 0.5f * std::sin (anim * 5.0f);
        juce::Colour accent =
            currentGame == labgames::Game::roulette ? juce::Colours::violet
          : currentGame == labgames::Game::skillRoulette ? juce::Colour (0xffff66d9)
          : currentGame == labgames::Game::monte ? juce::Colours::orange
          : currentGame == labgames::Game::slots ? juce::Colours::cyan
          : currentGame == labgames::Game::dice ? juce::Colours::lime
          : currentGame == labgames::Game::twentyOne ? juce::Colour (0xffff4545)
          : currentGame == labgames::Game::mutationSlots ? juce::Colour (0xff8cff49)
          : currentGame == labgames::Game::sideMutationSlots ? juce::Colour (0xff44ffd8)
                                                             : juce::Colour (0xffffd65a);

        if (mode == Mode::result && ! resultPositive)
            accent = juce::Colours::grey;

        g.setColour (accent.withAlpha (0.18f + pulse * 0.18f));

        if (currentGame == labgames::Game::roulette
            || currentGame == labgames::Game::skillRoulette)
        {
            const auto centre = r.getCentre();
            const int wedges = currentGame == labgames::Game::skillRoulette ? 8 : 12;
            const float rad = 54.0f + pulse * 9.0f;
            for (int i = 0; i < wedges; ++i)
            {
                const float theta = anim * (currentGame == labgames::Game::skillRoulette ? -2.4f : 1.8f)
                                  + i * juce::MathConstants<float>::twoPi / (float) wedges;
                const float rr = (i & 1) ? rad * 0.74f : rad;
                g.fillEllipse (centre.x + std::cos (theta) * rr - 5.0f,
                               centre.y + std::sin (theta) * rr - 5.0f, 10.0f, 10.0f);
            }
        }
        else if (currentGame == labgames::Game::monte)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float y = 88.0f + std::sin (anim * 4.2f + i * 1.7f) * 10.0f;
                const float x = 62.0f + i * 82.0f
                              + std::sin (anim * 3.1f + i) * 9.0f;
                g.drawRoundedRectangle ({ x, y, 52.0f, 76.0f }, 6.0f, 3.0f);
            }
        }
        else if (currentGame == labgames::Game::slots)
        {
            for (int i = 0; i < 3; ++i)
            {
                auto box = juce::Rectangle<float> (66.0f + i * 72.0f, 96.0f, 52.0f, 52.0f);
                g.drawRoundedRectangle (box, 5.0f, 3.0f);
                g.drawText (juce::String ((int) (anim * 17 + i * 3) % 10),
                            box.toNearestInt(), juce::Justification::centred);
            }
        }
        else if (currentGame == labgames::Game::dice)
        {
            for (int i = 0; i < 2; ++i)
            {
                const float spin = anim * (i ? -3.0f : 3.4f);
                auto box = juce::Rectangle<float> (92.0f + i * 96.0f,
                                                    100.0f + std::sin (spin) * 9.0f,
                                                    58.0f, 58.0f);
                g.drawRoundedRectangle (box, 8.0f, 3.0f);
                g.fillEllipse (box.getCentreX() - 4, box.getCentreY() - 4, 8, 8);
            }
        }
        else if (currentGame == labgames::Game::twentyOne)
        {
            // A small red dog silhouette backflips on positive results; a grey
            // cloud drifts down on a loss. During play, two cards sway.
            if (mode == Mode::result && resultPositive)
            {
                const float x = 30.0f + std::fmod (anim * 125.0f, juce::jmax (1.0f, r.getWidth() - 90.0f));
                const float y = 128.0f + std::sin (anim * 8.0f) * 18.0f;
                g.setColour (juce::Colours::red.withAlpha (0.85f));
                g.fillEllipse (x, y, 42.0f, 24.0f);
                g.fillEllipse (x + 31.0f, y - 8.0f, 18.0f, 18.0f);
                g.drawLine (x + 6.0f, y + 22.0f, x - 4.0f, y + 34.0f, 4.0f);
                g.drawLine (x + 30.0f, y + 22.0f, x + 42.0f, y + 34.0f, 4.0f);
            }
            else if (mode == Mode::result)
            {
                g.setColour (juce::Colours::grey.withAlpha (0.58f + 0.18f * pulse));
                for (int i = 0; i < 5; ++i)
                    g.fillEllipse (70.0f + i * 34.0f + std::sin (anim + i) * 8.0f,
                                   112.0f + std::cos (anim * 0.7f + i) * 5.0f,
                                   60.0f, 34.0f);
            }
            else
            {
                for (int i = 0; i < 2; ++i)
                    g.drawRoundedRectangle ({ 104.0f + i * 76.0f,
                                              98.0f + std::sin (anim * 2.7f + i) * 7.0f,
                                              52.0f, 76.0f }, 5.0f, 3.0f);
            }
        }
        else if (currentGame == labgames::Game::scratch)
        {
            for (int i = 0; i < 5; ++i)
            {
                auto box = juce::Rectangle<float> (42.0f + i * 56.0f, 104.0f, 42.0f, 42.0f);
                g.drawRoundedRectangle (box, 4.0f, 2.0f);
                const float reveal = std::fmod (anim * 0.42f + i * 0.17f, 1.0f);
                if (reveal > 0.45f)
                    g.fillEllipse (box.reduced (12.0f));
                else
                    for (int s = 0; s < 4; ++s)
                        g.drawLine (box.getX() + 4.0f, box.getY() + 6.0f + s * 8.0f,
                                    box.getRight() - 4.0f, box.getY() + 2.0f + s * 8.0f, 1.0f);
            }
        }
        else
        {
            // Mutation slots / pet choice / Wolfermean share a strange
            // biological-machine visual vocabulary.
            if (mode == Mode::petChoice)
            {
                g.setColour (juce::Colour (0xffffdf70).withAlpha (0.8f));
                g.fillEllipse (72.0f, 112.0f, 58.0f, 34.0f);
                g.drawLine (82.0f, 110.0f, 74.0f, 98.0f, 3.0f);
                g.drawLine (120.0f, 110.0f, 128.0f, 98.0f, 3.0f);
                g.setColour (juce::Colour (0xffff704f).withAlpha (0.8f));
                g.fillEllipse (188.0f, 112.0f, 68.0f, 36.0f);
            }
            else if (mode == Mode::wolfermean)
            {
                const float t = std::fmod (anim * anim * 52.0f, juce::jmax (1.0f, r.getWidth() + 100.0f));
                const float x = r.getRight() - t;
                const float y = 122.0f + std::sin (anim * 6.0f) * 8.0f;
                g.saveState();
                juce::AffineTransform tr = juce::AffineTransform::translation (-x - 24.0f, -y - 14.0f)
                    .rotated (juce::MathConstants<float>::pi)
                    .translated (x + 24.0f, y + 14.0f);
                g.addTransform (tr);
                g.setColour (juce::Colour (0xff8a775f).withAlpha (0.92f));
                g.fillEllipse (x, y, 48.0f, 28.0f);
                g.fillEllipse (x + 36.0f, y - 8.0f, 20.0f, 20.0f);
                g.restoreState();
            }
            else
            {
                for (int i = 0; i < 3; ++i)
                {
                    auto box = juce::Rectangle<float> (64.0f + i * 72.0f, 96.0f, 52.0f, 52.0f);
                    g.drawRoundedRectangle (box, 9.0f, 3.0f);
                    const float q = std::fmod (anim * (9.0f + i * 2.0f), 1.0f);
                    g.setColour (juce::Colour::fromHSV (q, 0.88f, 1.0f, 0.7f));
                    g.fillEllipse (box.reduced (10.0f));
                }
            }
        }

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (20.0f, juce::Font::bold));
        g.drawText (title, 20, 18, getWidth() - 40, 28, juce::Justification::centred);

        g.setFont (12.5f);
        g.setColour (juce::Colours::white.withAlpha (0.78f));
        g.drawFittedText (detail, 28, 50, getWidth() - 56, 58,
                          juce::Justification::centred, 3);
    }

    void LabGameOverlay::resized()
    {
        auto r = getLocalBounds().reduced (24);
        auto buttons = r.removeFromBottom (44);
        decline.setBounds (buttons.removeFromRight (110).reduced (4));
        // Fixed slots, laid out right-to-left over the visible buttons only, and
        // recomputed every time the visible set changes (configureButtons/resolve).
        for (auto* btn : { &c, &b, &a })
            if (btn->isVisible()) btn->setBounds (buttons.removeFromRight (100).reduced (4));
    }
}
