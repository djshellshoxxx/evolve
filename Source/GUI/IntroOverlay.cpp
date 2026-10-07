// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "IntroOverlay.h"
#include "MutagenLookAndFeel.h"

namespace mutagen
{
    namespace
    {
        struct Page { const char* kicker; const char* title; const char* body; juce::uint32 colour; };

        const Page pages[] = {
            { "WELCOME", "MUTAGEN is an art project.",
              "It is not a product and it does not have a correct way to be used. It is a living sound colony: a population of small "
              "synthetic organisms that breed, compete, infect each other and die. What you hear is the population, not a patch.\n\n"
              "You do not edit a sound here. You keep an ecosystem interesting. Nothing you do can break it, and no two runs will ever "
              "sound or play the same.", 0xffe8532a },
            { "THE CULTURE CHAMBER", "The big panel is alive. Touch it.",
              "LEFT CLICK mutates the cells under the cursor - the picture and the sound change together.\n"
              "DRAG sends a wave through the dish: a mass mutation.\n"
              "RIGHT CLICK is damage: it removes things. Use it when the sound gets crowded or noisy.\n"
              "DROP audio files onto the window and the colony eats them. ARM MIC lets it eat the room (with feedback protection).", 0xff4fb6c4 },
            { "THE GAME", "Variety scores. Noise freezes.",
              "The score climbs faster the more the colony varies. If the sound collapses into noise, the score stops - right-click "
              "to carve it back into something with shape and it starts again.\n\n"
              "The panels around the chamber are the lab: GERMINATION seeds the colony, ENVIRONMENT shapes evolution, the INSPECTOR "
              "edits genes, the TIMELINE keeps history. Lab games - roulette, monte, slots, dice, twenty-one, scratch cards - will "
              "offer themselves now and then. NO THANKS is always allowed.", 0xff7bc96f },
            { "THE RESONANCE ACTS", "The narrator strip tells a story.",
              "Under the chamber, characters from the lab talk to you: an acoustician, a spectrum analyser, a composer, something "
              "living below 20 Hz, an echo, a gatekeeper - and the Tuner, who wants every sound at 440 Hz.\n\n"
              "Everything they say is true about sound, and the colony plays it as they say it. Facts you hear fill your LEXICON, "
              "and the lexicon opens new acts. Each run has its own key, its own twist and its own pacing.", 0xff9b7bd6 },
            { "YOUR SOUNDS", "Collect what you like. Take it with you.",
              "COLLECT records the last three seconds of the colony. Story moments, secret rooms and game prizes are collected "
              "automatically. Every sound is cleaned up like a sample-library one-shot and saved as a 24-bit WAV.\n\n"
              "EXPORT WAV copies your whole collection into any folder, ready for your own music.", 0xfff2c14e },
            { "SECRETS", "There are 39 hidden rooms.",
              "Some open by accident in the first few minutes. Some need patience, a steady hand, or a particular time on the clock. "
              "A few need the calendar on your side. Each one has its own room, skill, orbs and sound.\n\n"
              "Odd visitors wander through, too - some with mirrored names, some upside down. They do not make sense and they "
              "launch bottle rockets. And sometimes the lab speaks another language, for no reason at all.", 0xffe8556f },
            { "BEGIN", "That is everything you need.",
              "Click the chamber. Listen. Follow what sounds good, carve away what does not.\n\n"
              "Press INTRO under the chamber whenever you want to read this again.", 0xff4fc4a8 }
        };
        constexpr int pageCount = (int) (sizeof (pages) / sizeof (pages[0]));
    }

    IntroOverlay::IntroOverlay()
    {
        setWantsKeyboardFocus (true);
        for (auto* b : { &back, &next, &skip }) addAndMakeVisible (*b);
        back.onClick = [this] { show (page - 1); };
        next.onClick = [this] { if (page + 1 >= pageCount) close(); else show (page + 1); };
        skip.onClick = [this] { close(); };
        setVisible (false);
    }

    void IntroOverlay::open()
    {
        setVisible (true);
        toFront (true);
        show (0);
    }

    void IntroOverlay::close()
    {
        setVisible (false);
        if (onClosed) onClosed();
    }

    void IntroOverlay::show (int p)
    {
        page = juce::jlimit (0, pageCount - 1, p);
        back.setEnabled (page > 0);
        next.setButtonText (page == pageCount - 1 ? "START" : "NEXT");
        repaint();
    }

    bool IntroOverlay::keyPressed (const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        if (k == juce::KeyPress::rightKey || k == juce::KeyPress::returnKey) { next.triggerClick(); return true; }
        if (k == juce::KeyPress::leftKey) { show (page - 1); return true; }
        return false;
    }

    void IntroOverlay::resized()
    {
        auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (720, getWidth() - 40), juce::jmin (440, getHeight() - 40));
        auto row = card.reduced (28).removeFromBottom (34);
        skip.setBounds (row.removeFromLeft (90));
        next.setBounds (row.removeFromRight (110));
        row.removeFromRight (8);
        back.setBounds (row.removeFromRight (90));
    }

    void IntroOverlay::paint (juce::Graphics& g)
    {
        g.fillAll (theme::bg0.withAlpha (0.86f));
        const auto& pg = pages[page];
        const juce::Colour c (pg.colour);

        auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (720, getWidth() - 40), juce::jmin (440, getHeight() - 40)).toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (c);
        g.fillRoundedRectangle (card.withHeight (5.0f), 2.0f);
        g.setColour (theme::stroke);
        g.drawRoundedRectangle (card, 8.0f, 1.0f);

        auto r = card.reduced (28.0f);
        r.removeFromBottom (44.0f);
        g.setColour (c);
        g.setFont (theme::monoFont (11.0f));
        g.drawText (juce::String (pg.kicker) + "   " + juce::String (page + 1) + " / " + juce::String (pageCount),
                    r.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (theme::text);
        g.setFont (theme::uiFont (26.0f, true));
        g.drawFittedText (pg.title, r.removeFromTop (44.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        r.removeFromTop (8.0f);
        g.setColour (theme::text.withAlpha (0.86f));
        g.setFont (theme::uiFont (15.0f));
        g.drawFittedText (pg.body, r.toNearestInt(), juce::Justification::topLeft, 14, 1.0f);

        // page dots
        const float dotsW = pageCount * 14.0f;
        for (int i = 0; i < pageCount; ++i)
        {
            g.setColour (i == page ? c : theme::stroke);
            g.fillEllipse (card.getCentreX() - dotsW / 2 + i * 14.0f, card.getBottom() - 22.0f, 7.0f, 7.0f);
        }
    }

    // ---- name prompt -------------------------------------------------------------

    NameOverlay::NameOverlay()
    {
        field.setInputRestrictions (32);
        field.setJustification (juce::Justification::centred);
        field.setFont (theme::uiFont (22.0f, true));
        field.setTextToShowWhenEmpty ("your name", theme::textDim);
        field.onReturnKey = [this] { submit(); };
        begin.onClick = [this] { submit(); };
        addAndMakeVisible (field);
        addAndMakeVisible (begin);
        setVisible (false);
    }

    void NameOverlay::open (const juce::String& lastName)
    {
        field.setText (lastName, false);
        field.selectAll();
        setVisible (true);
        toFront (true);
        field.grabKeyboardFocus();
    }

    void NameOverlay::submit()
    {
        auto name = field.getText().trim();
        if (name.isEmpty()) name = "Operator";
        setVisible (false);
        if (onName) onName (name);
    }

    void NameOverlay::resized()
    {
        auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (460, getWidth() - 40), 220);
        auto r = card.reduced (28);
        r.removeFromTop (78);
        field.setBounds (r.removeFromTop (44));
        r.removeFromTop (14);
        begin.setBounds (r.removeFromTop (34).withSizeKeepingCentre (140, 34));
    }

    void NameOverlay::paint (juce::Graphics& g)
    {
        g.fillAll (theme::bg0.withAlpha (0.86f));
        auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (460, getWidth() - 40), 220).toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (card.withHeight (5.0f), 2.0f);
        auto r = card.reduced (28.0f);
        g.setFont (theme::monoFont (11.0f));
        g.drawText ("NEW GAME", r.removeFromTop (18.0f), juce::Justification::centred);
        g.setColour (theme::text);
        g.setFont (theme::uiFont (22.0f, true));
        g.drawText ("Who is at the dish?", r.removeFromTop (40.0f), juce::Justification::centred);
    }
}
