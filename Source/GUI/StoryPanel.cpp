// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "StoryPanel.h"
#include "MutagenLookAndFeel.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    namespace
    {
        juce::Colour colourFor (story::Speaker s)
        {
            using story::Speaker;
            switch (s)
            {
                case Speaker::cadence: return theme::accent2;
                case Speaker::fourier: return theme::spectralV;
                case Speaker::lyra:    return theme::resonator;
                case Speaker::tuner:   return theme::danger;
                case Speaker::sub:     return theme::nutrient;
                case Speaker::echo:    return theme::warning;
                case Speaker::nyquist: return theme::infection;
                case Speaker::moth:    return theme::textDim;
                case Speaker::visitor: return theme::text;
                case Speaker::count:   break;
            }
            return theme::text;
        }

        std::uint64_t freshSeed()
        {
            return (std::uint64_t) juce::Random::getSystemRandom().nextInt64()
                 ^ (std::uint64_t) juce::Time::currentTimeMillis() * 0x9e3779b97f4a7c15ull;
        }
    }

    StoryPanel::StoryPanel (MutagenProcessor& p)
        : processor (p), prog (storyio::load()), runStory (freshSeed())
    {
        setOpaque (false);
        ++prog.runsPlayed;

        collectButton.setTooltip ("Record the last 3 seconds of the colony into your sound collection");
        exportButton.setTooltip ("Copy every collected sound to a folder as 24-bit WAV files");
        collectButton.onClick = [this]
        {
            if (clock - lastManualCollect < 2.0) return;
            lastManualCollect = clock;
            doCollect ("specimen " + juce::String (runStory.keyName()), 3.0f);
        };
        exportButton.onClick = [this] { exportAll(); };
        addAndMakeVisible (collectButton);
        addAndMakeVisible (exportButton);

        for (int i = 0; i < 3; ++i)
        {
            answerButtons[i].onClick = [this, i] { answer (i); };
            addChildComponent (answerButtons[i]);
        }

        startTimerHz (30);
    }

    StoryPanel::~StoryPanel()
    {
        releaseAll();
        persist();
    }

    void StoryPanel::persist() { storyio::save (prog); }

    // -------------------------------------------------------------------------

    void StoryPanel::say (story::Speaker s, const juce::String& text)
    {
        speaker = story::speakerName (s);
        role = story::speakerRole (s);
        speakerColour = colourFor (s);
        line = text;
        typed = 0.0f;
        flash = 1.0f;
        repaint();
    }

    void StoryPanel::tick (double dt, bool paused)
    {
        clock += dt;

        // Scheduled notes and captures run even when the story is paused, so
        // nothing is left hanging.
        for (auto it = noteQueue.begin(); it != noteQueue.end();)
        {
            if (it->at <= clock)
            {
                EngineCommand c;
                c.type = it->on ? CommandType::noteBurst : CommandType::noteRelease;
                c.ia = it->note;
                c.fa = 0.75f;
                processor.pushCommand (c);
                it = noteQueue.erase (it);
            }
            else ++it;
        }

        for (auto it = captures.begin(); it != captures.end();)
        {
            if (it->at <= clock) { doCollect (it->label, it->seconds); it = captures.erase (it); }
            else ++it;
        }

        if (paused || quizIndex >= 0) return;

        const int act = story::actFor (prog);
        if (act != currentAct)
        {
            openAct (act);
            return;
        }

        // Once the act has opened, Lyra names this run's key - different every run.
        if (! keyIntroDone && clock >= keyIntroAt)
        {
            keyIntroDone = true;
            const auto& m = story::modes()[(std::size_t) runStory.mode];
            topic = "Scales";
            say (story::Speaker::lyra, "Tonight the colony is tuned to " + juce::String (runStory.keyName())
                                       + ". " + m.character);
            playNotes (runStory.scaleNotes (-1), 0.30, 0.45);
            return;
        }

        if (! runStory.twistRevealed && clock >= runStory.twistAtSec)
        {
            revealTwist();
            return;
        }

        if (clock >= runStory.nextQuizSec)
        {
            askQuiz();
            return;
        }

        if (clock >= runStory.nextEventSec)
        {
            const int id = runStory.drawEvent (currentAct, prog);
            runStory.nextEventSec = clock + runStory.gapAfterEvent (currentAct);
            if (id >= 0) fireEvent (id);
        }
    }

    void StoryPanel::openAct (int act)
    {
        const bool firstTime = act > prog.highestActSeen;
        currentAct = act;
        prog.highestActSeen = juce::jmax (prog.highestActSeen, act);

        const auto& a = story::acts()[(std::size_t) (act - 1)];
        topic = a.title;
        say (a.speaker, a.opening);
        perform (story::Effect::chord, a.notes, 0);

        // Give the opening line room to breathe before the next event.
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 30.0);

        if (firstTime && act > 1)
        {
            if (onReward) onReward (1.0f, juce::String (a.title).upToFirstOccurrenceOf ("  ", false, false) + " REACHED");
            collectSoon (juce::String (a.title).fromFirstOccurrenceOf ("  ", false, false) + " motif", 0.8, 3.5f);
        }
        persist();
    }

    void StoryPanel::fireEvent (int id)
    {
        const auto& e = story::events()[(std::size_t) id];
        const bool fresh = ! prog.heard (id);
        prog.markHeard (id);

        topic = e.topic;
        say (e.speaker, e.line);
        perform (e.effect, e.notes, e.param);

        if (fresh && onReward) onReward (0.35f, "LEXICON +1  " + juce::String (e.topic).toUpperCase());
        persist();
    }

    void StoryPanel::revealTwist()
    {
        runStory.twistRevealed = true;
        const auto& t = story::twists()[(std::size_t) runStory.twist];
        prog.twistsSeen |= 1u << runStory.twist;
        topic = juce::String ("TWIST: ") + t.name;
        say (t.speaker, t.reveal);
        perform (t.effect, t.notes, t.param);
        if (onReward) onReward (0.8f, juce::String ("TWIST  ") + t.name);
        collectSoon (juce::String ("twist ") + t.name, 0.7, 3.0f);
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 40.0);
        persist();
    }

    // ---- ear checks ---------------------------------------------------------

    void StoryPanel::askQuiz()
    {
        quizIndex = runStory.drawQuiz (currentAct);
        if (quizIndex < 0) { runStory.nextQuizSec = clock + runStory.gapAfterQuiz(); return; }

        const auto& q = story::quizzes()[(std::size_t) quizIndex];
        topic = "EAR CHECK";
        say (q.speaker, q.question);
        for (int i = 0; i < 3; ++i)
        {
            answerButtons[i].setButtonText (q.answers[(std::size_t) i]);
            answerButtons[i].setVisible (true);
        }
        resized();
    }

    void StoryPanel::answer (int choice)
    {
        if (quizIndex < 0) return;
        const auto& q = story::quizzes()[(std::size_t) quizIndex];
        const bool right = choice == q.correct;
        ++prog.quizzesAsked;

        for (auto& b : answerButtons) b.setVisible (false);
        quizIndex = -1;
        runStory.nextQuizSec = clock + runStory.gapAfterQuiz();
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 15.0);

        if (right)
        {
            ++prog.quizzesCorrect;
            say (q.speaker, juce::String ("Exactly. ") + q.why);
            perform (story::Effect::chord, { 0, 4, 7, 12 }, 0);   // a resolved major chord
            if (onReward) onReward (0.9f, "EAR CHECK PASSED");
        }
        else
        {
            say (q.speaker, juce::String ("Not quite - ") + q.answers[(std::size_t) q.correct] + ". " + q.why);
            perform (story::Effect::interval, { 0, 1, story::kNo, story::kNo }, 0); // a semitone rub
        }
        persist();
        resized();
    }

    // ---- sound --------------------------------------------------------------

    void StoryPanel::perform (story::Effect fx, const std::array<int, 4>& notes, int param)
    {
        using story::Effect;
        std::vector<int> semis;
        for (auto n : notes) if (n != story::kNo) semis.push_back (n);

        EngineCommand c;
        switch (fx)
        {
            case Effect::none: break;
            case Effect::interval: playNotes (semis, 0.55, 1.6); break;   // melodic, then held together
            case Effect::chord:    playNotes (semis, 0.0, 2.4); break;
            case Effect::scale:    playNotes (runStory.scaleNotes (param), 0.26, 0.4); break;
            case Effect::mutate:   c.type = CommandType::mutateNow; processor.pushCommand (c); break;
            case Effect::radiate:  c.type = CommandType::radiate; processor.pushCommand (c); break;
            case Effect::gate:     processor.triggerTemporaryGator (param > 0 ? param : 120, 16, 6); break;
            case Effect::delay:    processor.triggerTripDelay (param > 0 ? param : 40); break;
            case Effect::haunted:  processor.triggerHauntedSound (param, 0.7f); break;
            case Effect::collect:  collectSoon ("lesson specimen", 0.2, 3.0f); break;
        }
    }

    void StoryPanel::playNotes (const std::vector<int>& semis, double spacing, double hold)
    {
        // Intervals and chords ring together until the end; scale runs are
        // played legato, each note releasing as the next has settled.
        const bool sustainAll = spacing * 2.0 >= hold || spacing <= 0.0;
        double t = clock + 0.05;
        const double end = t + spacing * (double) semis.size() + hold;
        for (auto s : semis)
        {
            const int note = juce::jlimit (12, 108, runStory.root + s);
            noteQueue.push_back ({ t, note, true });
            noteQueue.push_back ({ sustainAll ? end : t + hold, note, false });
            t += spacing;
        }
    }

    void StoryPanel::releaseAll()
    {
        for (const auto& n : noteQueue)
            if (! n.on)
            {
                EngineCommand c;
                c.type = CommandType::noteRelease;
                c.ia = n.note;
                processor.pushCommand (c);
            }
        noteQueue.clear();
    }

    // ---- the specimen jar ---------------------------------------------------

    void StoryPanel::collectSoon (const juce::String& label, double delaySec, float seconds)
    {
        if (captures.size() < 16)
            captures.push_back ({ clock + delaySec + (double) seconds, label, seconds });
    }

    void StoryPanel::doCollect (const juce::String& label, float seconds)
    {
        juce::AudioBuffer<float> buf;
        processor.copyRecentOutput (buf, seconds);
        if (jar.collect (buf, processor.currentSampleRate(), label))
        {
            ++prog.collected;
            persist();
            flash = 1.0f;
            if (onReward) onReward (0.25f, "SOUND COLLECTED  #" + juce::String (jar.size()));
        }
        else if (label.startsWith ("specimen"))
        {
            say (story::Speaker::cadence, "Nothing above the noise floor to keep. Let the colony make a sound first.");
        }
        repaint();
    }

    void StoryPanel::exportAll()
    {
        if (jar.size() == 0)
        {
            say (story::Speaker::cadence, "The jar is empty. Press COLLECT while the colony is doing something you like.");
            return;
        }

        chooser = std::make_unique<juce::FileChooser> ("Export collected sounds to...",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<StoryPanel> (this)] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto dir = fc.getResult();
                if (dir == juce::File()) return;
                const int n = safe->jar.exportTo (dir.getChildFile ("MUTAGEN Sounds"));
                safe->say (story::Speaker::cadence,
                           juce::String (n) + " sounds exported as 24-bit WAV to " + dir.getFullPathName()
                           + ". 24 bits gives about 144 dB of range - far more than any room you will play them in.");
            });
    }

    // ---- drawing -------------------------------------------------------------

    void StoryPanel::timerCallback()
    {
        bool dirty = false;
        if (typed < (float) line.length()) { typed += 2.2f; dirty = true; }
        if (flash > 0.0f) { flash = juce::jmax (0.0f, flash - 0.04f); dirty = true; }
        if (dirty) repaint();
    }

    void StoryPanel::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto right = r.removeFromRight (112);
        collectButton.setBounds (right.removeFromTop (r.getHeight() / 2).reduced (0, 2));
        exportButton.setBounds (right.reduced (0, 2));

        if (quizIndex >= 0)
        {
            auto row = r.removeFromBottom (24);
            const int w = row.getWidth() / 3;
            for (auto& b : answerButtons) b.setBounds (row.removeFromLeft (w).reduced (3, 0));
        }
    }

    void StoryPanel::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::stroke.interpolatedWith (speakerColour, flash * 0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto area = getLocalBounds().reduced (10, 6);
        area.removeFromRight (120);
        if (quizIndex >= 0) area.removeFromBottom (26);

        const int act = juce::jmax (1, currentAct);
        const auto status = juce::String (story::acts()[(std::size_t) (act - 1)].title)
                          + "  |  KEY " + juce::String (runStory.keyName())
                          + "  |  LEXICON " + juce::String (prog.lexiconCount()) + "/" + juce::String (story::lexiconSize)
                          + "  |  JAR " + juce::String (jar.size());
        g.setFont (theme::monoFont (9.5f));
        g.setColour (theme::textDim);
        g.drawText (status, area.removeFromTop (12), juce::Justification::centredLeft, true);

        auto header = area.removeFromTop (16);
        g.setFont (theme::uiFont (11.5f, true));
        g.setColour (speakerColour);
        g.drawText (speaker, header, juce::Justification::centredLeft, true);
        const int nameW = juce::GlyphArrangement::getStringWidthInt (theme::uiFont (11.5f, true), speaker);
        g.setFont (theme::uiFont (10.0f));
        g.setColour (theme::textDim);
        g.drawText (role + "  /  " + topic, header.withTrimmedLeft (nameW + 8), juce::Justification::centredLeft, true);

        g.setFont (theme::uiFont (13.0f));
        g.setColour (theme::text);
        g.drawFittedText (line.substring (0, (int) typed), area.reduced (0, 2),
                          juce::Justification::topLeft, 3, 0.9f);
    }
}
