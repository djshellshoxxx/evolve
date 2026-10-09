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

        std::uint64_t factSeedFor (story::Progress& p);

        std::uint64_t freshSeed()
        {
            return (std::uint64_t) juce::Random::getSystemRandom().nextInt64()
                 ^ (std::uint64_t) juce::Time::currentTimeMillis() * 0x9e3779b97f4a7c15ull;
        }

        std::uint64_t factSeedFor (story::Progress& p)
        {
            if (p.factSeed == 0) p.factSeed = freshSeed() | 1ull;
            return p.factSeed;
        }
    }

    StoryPanel::StoryPanel (MutagenProcessor& p)
        : processor (p), prog (storyio::load()), runStory (freshSeed()), chanceDir (freshSeed()),
          factDeck (factSeedFor (prog), prog.factCounter)
    {
        setOpaque (false);
        ++prog.runsPlayed;
        factSeen = facts::SeenSet::fromBase64 (prog.factSeen);

        // Tell the effect mode which key tonight's run is in.
        {
            int mask = 0;
            for (auto step : story::modes()[(std::size_t) runStory.mode].steps)
                if (step != story::kNo) mask |= 1 << (((step % 12) + 12) % 12);
            processor.setMorphScale (mask, runStory.root % 12);
        }

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
        introButton.setTooltip ("What everything is, and what MUTAGEN is for");
        introButton.onClick = [this] { if (onIntro) onIntro(); };
        addAndMakeVisible (introButton);

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
        speakerId = s;
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

        // Every 30 minutes of play the plot moves on by itself: the next act
        // opens, or, once the last act is open, a fresh twist lands.
        if ((playClock += dt) >= 1800.0)
        {
            playClock = 0.0;
            if (prog.plotFloor < story::actCount && story::actFor (prog) < story::actCount)
            {
                prog.plotFloor = story::actFor (prog) + 1;
                persist();
            }
            else
            {
                runStory.twist = (runStory.twist + 1 + (int) (clock * 7.0) % 7) % (int) story::twists().size();
                revealTwist();
                return;
            }
        }

        const int act = story::actFor (prog);
        if (act != currentAct)
        {
            openAct (act);
            return;
        }

        // The second half of an exchange, or the closing lines of an ending.
        if (! lineQueue.empty())
        {
            if (clock >= lineQueue.front().at)
            {
                const auto q = lineQueue.front();
                lineQueue.erase (lineQueue.begin());
                say (q.who, q.text);
                perform (q.fx, q.notes, q.param);
            }
            return;
        }

        // Every five minutes of unpaused play one fact is shown; a won game or an
        // unlocked skill brings an extra one a few seconds later.
        factClock += dt;
        sinceLastFact += dt;
        if (factClock >= 300.0 || (pendingFactRewards > 0 && sinceLastFact >= 8.0))
        {
            if (pendingFactRewards > 0 && factClock < 300.0) --pendingFactRewards;
            else factClock = 0.0;
            presentFact (true);
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

        if (finaleAt > 0.0 && ! runStory.finaleDone && clock >= finaleAt)
        {
            playFinale();
            return;
        }

        if (clock >= runStory.nextQuizSec)
        {
            askQuiz();
            return;
        }

        if (clock >= chanceDir.nextAtSec)
        {
            fireChance();
            return;
        }

        if (clock >= runStory.nextBanterSec)
        {
            fireBanter();
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

        if (act == story::actCount && ! runStory.finaleDone) finaleAt = clock + 80.0;
        if (act > 1 && onBanner)
            onBanner (juce::String (a.title).upToFirstOccurrenceOf ("  ", false, false),
                      juce::String (a.title).fromFirstOccurrenceOf ("  ", false, false), colourFor (a.speaker));

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
        twistClock = clock;
        const auto& t = story::twists()[(std::size_t) runStory.twist];
        prog.twistsSeen |= 1u << runStory.twist;
        topic = juce::String ("TWIST: ") + t.name;
        say (t.speaker, t.reveal);
        perform (t.effect, t.notes, t.param);
        { EngineCommand ev; ev.type = CommandType::gameEvent; ev.ia = 7; processor.pushCommand (ev); }
        if (onGlitch) onGlitch (colourFor (t.speaker));
        if (onBanner) onBanner ("TWIST", t.name, colourFor (t.speaker));
        if (onReward) onReward (0.8f, juce::String ("TWIST  ") + t.name);
        collectSoon (juce::String ("twist ") + t.name, 0.7, 3.0f);
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 40.0);
        persist();
    }

    // ---- facts ------------------------------------------------------------------

    void StoryPanel::presentFact (bool bonus)
    {
        const int slotCounter = (int) factDeck.counter();
        const int id = factDeck.next();
        const auto f = facts::factAt (id);
        const int points = facts::knowledgePoints (prog.factSeed, (uint32_t) slotCounter);

        factSeen.mark (id);
        prog.knowledge += (std::uint64_t) points;
        prog.factCounter = factDeck.counter();
        prog.factSeen = factSeen.toBase64();
        sinceLastFact = 0.0;

        const int bonusScore = f.demonstrable ? facts::kDemoBonusScore : 0;
        { EngineCommand ev; ev.type = CommandType::gameEvent; ev.ia = 6; processor.pushCommand (ev); }
        if (onFact) onFact (f.category, f.text, points, bonusScore, bonus ? theme::resonator : theme::spectralV);
        if (f.demonstrable)
        {
            playDemo (f.demo);
            if (onPoints) onPoints (bonusScore, "FACT DEMONSTRATION");
        }
        else
        {
            // a small two-note chime in tonight's key, so every fact is heard as well as read
            const auto sc = runStory.scaleNotes (-1);
            if (sc.size() >= 5) playNotes ({ sc[0], sc[4] }, 0.22, 0.5);
        }
        persist();
    }

    void StoryPanel::playDemo (const facts::Demo& d)
    {
        using K = facts::Demo::Kind;
        std::vector<int> semis;
        for (int i = 0; i < d.noteCount && i < (int) d.notes.size(); ++i)
            if (d.notes[(std::size_t) i] >= 0) semis.push_back (d.notes[(std::size_t) i] - runStory.root);
        if (semis.empty()) return;

        const double hold = juce::jlimit (0.8, 3.0, d.durationMs / 1000.0);
        switch (d.kind)
        {
            case K::none: break;
            case K::note: case K::pianoKey: case K::ghostNote: case K::instrumentColour:
                playNotes ({ semis[0] }, 0.0, hold); break;
            case K::keySound:
                if (semis.size() > 1) playNotes (semis, 0.28, 0.4); else playNotes ({ semis[0] }, 0.0, 2.5);
                break;
            case K::interval:      playNotes (semis, 0.55, 1.6); break;
            case K::chord:         playNotes (semis, 0.0, 2.4); break;
            case K::octavePair:    playNotes (semis, 0.7, 1.2); break;
            case K::scale:
            {
                std::vector<int> run;
                for (auto s : facts::scaleSemitones (d.fxId)) run.push_back (semis[0] + s);
                run.push_back (semis[0] + 12);
                playNotes (run, 0.26, 0.4);
                break;
            }
            case K::circleOfFifths:
            {
                std::vector<int> run;
                int s = semis[0];
                for (int i = 0; i < 6; ++i) { run.push_back (s); s += 7; if (s - semis[0] > 12) s -= 12; }
                playNotes (run, 0.4, 0.5);
                break;
            }
            case K::dopplerPass:   playNotes (semis, 0.35, 1.2); break;
            case K::beats:         playNotes (semis, 0.0, 2.5); break;
            case K::effect:
            {
                // Timed note helper: a note that sounds at `at` seconds from now for `len` seconds.
                const auto note = [this] (double at, int midi, double len)
                {
                    const int n = juce::jlimit (12, 108, midi);
                    noteQueue.push_back ({ clock + 0.05 + at, n, true });
                    noteQueue.push_back ({ clock + 0.05 + at + len, n, false });
                };
                const int a = semis[0] + runStory.root;                       // first note given by the fact
                const int b = semis.size() > 1 ? semis[1] + runStory.root : a;
                switch (d.fxId)
                {
                    case facts::kFxGate:        // a held chord chopped by the tempo gate
                    case facts::kFxSidechain:   // the same, pumping harder and lower
                    {
                        const int root = d.fxId == facts::kFxSidechain ? runStory.root - 12 : runStory.root;
                        for (int i : { 0, 4, 7 }) note (0.0, root + i, 5.0);
                        processor.triggerTemporaryGator (d.fxId == facts::kFxSidechain ? 124 : 120, 6, 2);
                        break;
                    }
                    case facts::kFxRiser:       // a rising run that speeds up
                    {
                        double t = 0.0, gap = 0.34;
                        for (int i = 0; i < 16; ++i) { note (t, runStory.root + i * 2, gap * 1.6); t += gap; gap = juce::jmax (0.05, gap * 0.84); }
                        break;
                    }
                    case facts::kFxSweep:       // a fast chromatic glide, like a filter opening
                        for (int i = 0; i <= 24; ++i) note (i * 0.09, runStory.root - 12 + i, 0.16);
                        break;
                    case facts::kFxBassDrop:    // a held note that falls into the sub
                    {
                        const int from = a > b ? a : juce::jmax (a, runStory.root);
                        const int to = a > b ? b : from - 24;
                        note (0.0, from, 1.0);
                        const int steps = juce::jmax (4, from - to);
                        for (int i = 1; i <= steps; ++i) note (1.0 + i * 0.06, from - (from - to) * i / steps, 0.12);
                        note (1.0 + steps * 0.06 + 0.1, to, 2.0);
                        break;
                    }
                    case facts::kFxTapeStop:    // pitch sags while the steps slow down
                    {
                        double t = 0.0, gap = 0.06;
                        int m = a;
                        for (int i = 0; i < 14; ++i) { note (t, m, gap * 1.4); t += gap; gap *= 1.28; if (i % 2 == 1) --m; }
                        break;
                    }
                    case facts::kFxEchoThrow:   // a short note thrown into the delay
                        note (0.0, a, 0.25);
                        note (0.5, a + 7, 0.25);
                        processor.triggerTripDelay (10);
                        break;
                    case facts::kFxReverseSwell: // a chord that builds in, one voice at a time
                    default:
                        for (int i = 0; i < 4; ++i) note (i * 0.5, runStory.root + (int) (i * 3.5f), 3.0 - i * 0.5);
                        break;
                }
                break;
            }
            case K::songMotif:     playNotes (semis, 0.4, 0.5); break;
        }
    }

    // ---- chance events, banter, endings ---------------------------------------

    void StoryPanel::fireChance (int forced)
    {
        chanceDir.nextAtSec = clock + chanceDir.gap();
        const int id = forced >= 0 ? forced : chanceDir.draw (currentAct);
        if (id < 0) return;

        const auto& c = chance::all()[(std::size_t) id];
        const bool fresh = ((prog.chanceSeen >> id) & 1u) == 0;
        prog.chanceSeen |= 1u << id;

        using R = chance::Rarity;
        const juce::Colour colour = c.rarity == R::common   ? theme::spectralV
                                  : c.rarity == R::uncommon ? theme::resonator
                                  : c.rarity == R::rare     ? theme::warning
                                                            : juce::Colour (0xffffd36b);
        const float intensity = c.rarity == R::common ? 0.8f : c.rarity == R::uncommon ? 1.0f
                              : c.rarity == R::rare ? 1.3f : 1.7f;

        topic = juce::String ("CHANCE  ") + chance::rarityName (c.rarity) + "  " + c.name;
        say (story::Speaker::moth, c.text);
        if (onAnim) onAnim (c.anim, colour, intensity);
        if (c.rarity >= R::rare && onBanner)
            onBanner (juce::String (chance::rarityName (c.rarity)) + " EVENT", c.name, colour);

        { EngineCommand ev; ev.type = CommandType::gameEvent; ev.ia = 5; processor.pushCommand (ev); }
        applyChance (c);
        if (onReward) onReward (c.reward, juce::String ("CHANCE  ") + c.name + (fresh ? "  (NEW)" : ""));
        if (c.rarity >= R::rare && c.kind != chance::Kind::collect)
            collectSoon (juce::String ("chance ") + c.name, 1.0, 4.0f);

        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 14.0);
        persist();
    }

    void StoryPanel::applyChance (const chance::Chance& c)
    {
        using K = chance::Kind;
        auto push = [this] (CommandType t, float a = 0.0f, float b = 0.0f, float r = 0.0f, float s = 0.0f)
        {
            EngineCommand cmd;
            cmd.type = t; cmd.fa = a; cmd.fb = b; cmd.fc = r; cmd.fd = s;
            processor.pushCommand (cmd);
        };
        auto rand01 = [] { return juce::Random::getSystemRandom().nextFloat(); };

        switch (c.kind)
        {
            case K::none: break;
            case K::mutate:   push (CommandType::mutateNow); break;
            case K::meteor:
                for (int i = 0; i < c.param; ++i)
                    push (CommandType::mutateAt, 0.1f + 0.8f * rand01(), 0.1f + 0.8f * rand01(), 0.13f, 0.8f);
                break;
            case K::sweep:
                for (int i = 0; i < 8; ++i)
                    push (CommandType::mutateAt, (float) i / 7.0f, 0.5f + 0.25f * std::sin ((float) i * 0.9f), 0.22f, 0.6f);
                break;
            case K::prune:
                push (CommandType::apoptosis, (float) c.param / 100.0f);
                push (CommandType::cure);
                break;
            case K::enzyme:   push (CommandType::addEnzyme); break;
            case K::catalyst: push (CommandType::addCatalyst); break;
            case K::warm:     push (CommandType::addHeat, 1.0f); break;
            case K::cool:     push (CommandType::addHeat, -1.0f); break;
            case K::cure:
                push (CommandType::cure);
                push (CommandType::addEnzyme);
                break;
            case K::notes:
            {
                std::vector<int> semis;
                for (auto n : c.notes) if (n != story::kNo) semis.push_back (n);
                playNotes (semis, 0.18, 2.2);
                break;
            }
            case K::scale:    playNotes (runStory.scaleNotes (-1), 0.24, 0.35); break;
            case K::haunted:  processor.triggerHauntedSound (c.param, 0.6f); break;
            case K::gate:     processor.triggerTemporaryGator (c.param, 8, 4); break;
            case K::delay:    processor.triggerTripDelay (c.param); break;
            case K::collect:
                push (CommandType::mutateNow);
                collectSoon (juce::String ("chance ") + c.name, 0.4, 4.0f);
                break;
        }
    }

    void StoryPanel::fireBanter()
    {
        const int id = runStory.drawBanter (currentAct);
        runStory.nextBanterSec = clock + runStory.gapAfterBanter();
        if (id < 0) return;

        const auto& b = story::banters()[(std::size_t) id];
        topic = "CONVERSATION";
        say (b.a, b.lineA);
        lineQueue.push_back ({ clock + 7.5, b.b, b.lineB, b.effect, b.notes, b.param });
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 20.0);
    }

    void StoryPanel::playFinale()
    {
        runStory.finaleDone = true;
        const int idx = story::endingFor (prog);
        const auto& e = story::endings()[(std::size_t) idx];
        const bool fresh = ((prog.endingsSeen >> idx) & 1u) == 0;
        prog.endingsSeen |= 1u << idx;

        topic = juce::String ("ENDING: ") + e.name;
        say (e.speaker, e.text);
        static const std::array<std::array<int, 4>, 4> chords {{
            { 0, 4, 7, 12 }, { 0, 3, 6, 9 }, { 0, 7, 12, 16 }, { 7, 11, 14, 17 } }};
        perform (story::Effect::chord, chords[(std::size_t) idx], 0);
        if (onAnim) onAnim (chance::Anim::goldDust, colourFor (e.speaker), 1.5f);
        if (onBanner) onBanner ("ENDING", e.name, colourFor (e.speaker));
        if (onReward) onReward (fresh ? 2.5f : 1.0f, juce::String ("ENDING  ") + e.name);
        collectSoon (juce::String ("ending ") + e.name, 1.0, 5.0f);

        const juce::String who = prog.playerName.empty()
            ? juce::String ("listener") : juce::String (juce::CharPointer_UTF8 (prog.playerName.c_str()));
        lineQueue.push_back ({ clock + 12.0, story::Speaker::lyra,
                               "Thank you for listening, " + who + ". The colony will still be here when you come back. It never learned how to leave.",
                               story::Effect::none, { story::kNo, story::kNo, story::kNo, story::kNo }, 0 });
        runStory.nextEventSec = juce::jmax (runStory.nextEventSec, clock + 30.0);
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
        std::vector<int> used;
        for (auto s : semis)
        {
            const int note = juce::jlimit (12, 108, runStory.root + s);
            // One voice group per pitch in the colony: skip a pitch that is
            // repeated or still sounding from an earlier demonstration.
            const bool busy = std::find (used.begin(), used.end(), note) != used.end()
                           || std::any_of (noteQueue.begin(), noteQueue.end(),
                                           [note] (const NoteEvent& e) { return e.note == note; });
            if (busy) { t += spacing; continue; }
            used.push_back (note);
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
        sigilPhase += 1.0f / 30.0f;
        repaint (sigilBounds());
        if (typed < (float) line.length()) { typed += 2.2f; dirty = true; }
        if (flash > 0.0f) { flash = juce::jmax (0.0f, flash - 0.04f); dirty = true; }
        if (dirty) repaint();
    }

    void StoryPanel::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto right = r.removeFromRight (112);
        const int third = r.getHeight() / 3;
        collectButton.setBounds (right.removeFromTop (third).reduced (0, 2));
        exportButton.setBounds (right.removeFromTop (third).reduced (0, 2));
        introButton.setBounds (right.reduced (0, 2));

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
        drawSigil (g, sigilBounds().toFloat());
        area.removeFromLeft (52);

        const int act = juce::jmax (1, currentAct);
        const auto status = juce::String (story::acts()[(std::size_t) (act - 1)].title)
                          + "  |  KEY " + juce::String (runStory.keyName())
                          + "  |  LEXICON " + juce::String (prog.lexiconCount()) + "/" + juce::String (story::lexiconSize)
                          + "  |  JAR " + juce::String (jar.size())
                          + "  |  SECRETS " + juce::String (prog.secretCount()) + "/39"
                          + "  |  EVENTS " + juce::String (prog.chanceCount()) + "/" + juce::String (chance::count)
                          + "  |  ENDINGS " + juce::String (prog.endingCount()) + "/4";
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

    // ---- the speaker's animated sigil ----------------------------------------

    juce::Rectangle<int> StoryPanel::sigilBounds() const
    {
        return { 12, 22, 40, 40 };
    }

    void StoryPanel::drawSigil (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const float ph = sigilPhase;
        const auto c = speakerColour;
        const auto mid = r.getCentre();
        const float R = r.getWidth() * 0.5f;
        const float amp = typed < (float) line.length() ? 1.0f : 0.35f;   // livelier while speaking

        g.setColour (c.withAlpha (0.10f + 0.22f * flash));
        g.fillEllipse (r);
        g.setColour (c.withAlpha (0.55f));
        g.drawEllipse (r.reduced (1.0f), 1.2f);
        g.setColour (c);

        juce::Path p;
        switch (speakerId)
        {
            case story::Speaker::cadence:      // a tuning fork, prongs ringing
            {
                const float v = std::sin (ph * 52.0f) * 1.6f * amp;
                p.startNewSubPath (mid.x - 5.0f - v, mid.y - 13.0f);
                p.lineTo (mid.x - 5.0f, mid.y + 1.0f);
                p.quadraticTo (mid.x, mid.y + 7.0f, mid.x + 5.0f, mid.y + 1.0f);
                p.lineTo (mid.x + 5.0f + v, mid.y - 13.0f);
                p.startNewSubPath (mid.x, mid.y + 5.0f);
                p.lineTo (mid.x, mid.y + 14.0f);
                g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
            case story::Speaker::fourier:      // three stacked sines, scrolling
                for (int k = 1; k <= 3; ++k)
                {
                    juce::Path w;
                    for (int i = 0; i <= 16; ++i)
                    {
                        const float u = i / 16.0f;
                        const float x = r.getX() + 6.0f + u * (r.getWidth() - 12.0f);
                        const float y = mid.y + (k - 2) * 9.0f + std::sin (u * 6.2832f * k + ph * 3.0f * k) * 3.2f * amp;
                        if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
                    }
                    g.setColour (c.withAlpha (0.45f + 0.18f * k));
                    g.strokePath (w, juce::PathStrokeType (1.3f));
                }
                break;
            case story::Speaker::lyra:         // a staff and a bobbing note
                for (int i = 0; i < 5; ++i)
                    g.drawLine (r.getX() + 6.0f, mid.y - 8.0f + i * 4.0f, r.getRight() - 6.0f, mid.y - 8.0f + i * 4.0f, 0.7f);
                {
                    const float ny = mid.y - 8.0f + (1.0f + 0.5f * std::sin (ph * 2.2f)) * 4.0f * (1.0f + amp) * 0.8f;
                    g.fillEllipse (mid.x - 3.5f, ny - 2.5f, 7.0f, 5.0f);
                    g.drawLine (mid.x + 3.3f, ny, mid.x + 3.3f, ny - 11.0f, 1.3f);
                }
                break;
            case story::Speaker::tuner:        // a needle that cannot settle
            {
                g.drawEllipse (mid.x - 11.0f, mid.y - 11.0f, 22.0f, 22.0f, 1.0f);
                g.drawLine (mid.x - 14.0f, mid.y, mid.x + 14.0f, mid.y, 0.6f);
                g.drawLine (mid.x, mid.y - 14.0f, mid.x, mid.y + 14.0f, 0.6f);
                const float a = std::sin (ph * 5.0f) * 0.45f * amp + std::sin (ph * 17.0f) * 0.06f;
                g.drawLine (mid.x, mid.y + 9.0f, mid.x + std::sin (a) * 17.0f, mid.y + 9.0f - std::cos (a) * 17.0f, 1.8f);
                break;
            }
            case story::Speaker::sub:          // slow rings you feel more than see
                for (int i = 0; i < 3; ++i)
                {
                    const float u = std::fmod (ph * 0.45f + i / 3.0f, 1.0f);
                    g.setColour (c.withAlpha ((1.0f - u) * 0.8f));
                    const float rr = u * R * 0.95f;
                    g.drawEllipse (mid.x - rr, mid.y - rr, rr * 2.0f, rr * 2.0f, 1.6f);
                }
                break;
            case story::Speaker::echo:         // one dot and its late copies
                for (int i = 0; i < 5; ++i)
                {
                    g.setColour (c.withAlpha (1.0f - i * 0.2f));
                    const float x = mid.x - 12.0f + i * 6.0f, y = mid.y + std::sin (ph * 2.5f - i * 0.6f) * 8.0f * (0.5f + amp);
                    g.fillEllipse (x - 2.5f + i * 0.0f, y - 2.5f, 5.0f - i * 0.5f, 5.0f - i * 0.5f);
                }
                break;
            case story::Speaker::nyquist:      // a wave folding at the gate
                g.drawLine (mid.x, r.getY() + 5.0f, mid.x, r.getBottom() - 5.0f, 1.0f);
                for (int side = 0; side < 2; ++side)
                {
                    juce::Path w;
                    for (int i = 0; i <= 10; ++i)
                    {
                        const float u = i / 10.0f;
                        const float x = side == 0 ? r.getX() + 5.0f + u * (R - 5.0f) : r.getRight() - 5.0f - u * (R - 5.0f);
                        const float y = mid.y + std::sin (u * 9.0f + ph * 4.0f) * 7.0f * amp * (0.4f + u * 0.6f);
                        if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
                    }
                    g.setColour (c.withAlpha (side == 0 ? 0.95f : 0.45f));
                    g.strokePath (w, juce::PathStrokeType (1.4f));
                }
                break;
            case story::Speaker::moth:         // wings, flapping
            {
                const float flap = 0.35f + 0.65f * std::abs (std::sin (ph * (3.0f + 6.0f * amp)));
                for (int side = -1; side <= 1; side += 2)
                {
                    g.fillEllipse (mid.x + (side > 0 ? 1.0f : -1.0f - 12.0f * flap), mid.y - 10.0f, 12.0f * flap, 12.0f);
                    g.fillEllipse (mid.x + (side > 0 ? 1.0f : -1.0f - 9.0f * flap), mid.y + 1.0f, 9.0f * flap, 9.0f);
                }
                g.setColour (theme::text);
                g.drawLine (mid.x, mid.y - 8.0f, mid.x, mid.y + 10.0f, 1.8f);
                break;
            }
            case story::Speaker::visitor:      // an eye, blinking now and then
            {
                const float blink = std::fmod (ph, 4.0f) > 3.85f ? 0.15f : 1.0f;
                p.startNewSubPath (r.getX() + 4.0f, mid.y);
                p.quadraticTo (mid.x, mid.y - 15.0f * blink, r.getRight() - 4.0f, mid.y);
                p.quadraticTo (mid.x, mid.y + 15.0f * blink, r.getX() + 4.0f, mid.y);
                g.strokePath (p, juce::PathStrokeType (1.5f));
                g.fillEllipse (mid.x - 4.0f + std::sin (ph * 0.9f) * 4.0f, mid.y - 4.0f * blink, 8.0f, 8.0f * blink);
                break;
            }
            case story::Speaker::count: break;
        }
    }
}
