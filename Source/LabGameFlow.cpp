// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

// The optional lab games: when they are offered, how they resolve, and what
// they pay out. Every instrument a game awards is also recorded into the sound
// collection (so it can be exported as WAV), and the character who hands it
// over says one true thing about that instrument.

#include "PluginEditor.h"

namespace mutagen
{
    namespace
    {
        struct InstrumentNote { const char* label; story::Speaker who; const char* fact; };

        const InstrumentNote& instrumentNote (int kind)
        {
            static const InstrumentNote notes[] = {
                { "snare", story::Speaker::lyra, "A snare's rattle is metal wires pressed against the drum's bottom head." },
                { "tom", story::Speaker::lyra, "Toms have no wires, so you hear the pitch fall as the head relaxes: a pitch envelope." },
                { "bass drum", story::Speaker::sub, "A kick's weight lives around 50 to 100 Hz; the click that cuts through sits near 2 to 5 kHz." },
                { "strings", story::Speaker::lyra, "Halve a string's length and it sounds an octave higher. Tighten it and the pitch rises too." },
                { "tambourine", story::Speaker::echo, "Tambourine jingles are tiny cymbals called zills. Their shimmer lives high in the spectrum." },
                { "kazoo", story::Speaker::fourier, "A kazoo makes no pitch of its own. It buzzes a membrane over your hum: a filter with a personality." },
                { "bass drop", story::Speaker::sub, "A drop works by contrast: take the low end away for a few bars and its return feels bigger." },
                { "piano key", story::Speaker::lyra, "A piano's 88 keys run from A0 at 27.5 Hz to C8 at about 4186 Hz." },
                { "clap", story::Speaker::cadence, "Drum machines fake a clap with several noise bursts a few milliseconds apart." },
                { "pad", story::Speaker::cadence, "Pads use long attack and release so they fill space without asking for attention." },
                { "up sweep", story::Speaker::fourier, "A riser sweeps pitch or a filter upward. The ear reads rising as tension." },
                { "sound", story::Speaker::moth, "SPECIMEN NOTE: every sound you keep is saved as 24-bit WAV. EXPORT copies the jar." }
            };
            return notes[juce::jlimit (0, 11, kind)];
        }

        enum Instrument { snare, tom, bassDrum, strings, tambourine, kazoo, bassDrop, pianoKey, clap, pad, upSweep, genericSound };
    }

    void MutagenEditor::wireLabGames()
    {
        addChildComponent (labGameOverlay);

        labGameOverlay.onDecline = [this] { presentNextLabGame(); };

        chamber.onGreenOrbsBorn = [this] (int n) { progressionSystem.addGreenOrbs (n); };
        chamber.onTemporaryGator = [this] (int bpm) { processor.triggerTemporaryGator (bpm, 20); };

        labGameOverlay.onRouletteSpin = [this]
        {
            const auto o = labgames::rouletteOutcome (scoreSystem.score(), hauntedRng.rollInclusive (1, 12));
            if (o.points != 0) scoreSystem.adjustScore (o.points, "ROULETTE");
            if (o.skillDelta > 0) awardGameSkill ((int) o.kind % 5, o.skillDelta);
            if (o.skillDelta < 0) progressionSystem.removeRandomGameSkill (hauntedRng.rollInclusive (0, 99));
            if (o.specialOrbs > 0) { progressionSystem.addSpecialOrbs (o.specialOrbs); chamber.spawnSpecialOrbs (o.specialOrbs); }
            if (o.soundUnlocks > 0) rewardInstrument (genericSound, o.soundUnlocks);
            const bool good = o.points >= 0 && o.skillDelta >= 0;
            labGameOverlay.resolve ("ROULETTE", labgames::roulettePrizeName (o.kind),
                                    labgames::Game::roulette,
                                    good ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty, good);
            playLabSound (labgames::Game::roulette, good ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty);
            progressionSystem.recordGameCompleted();
        };

        labGameOverlay.onSkillRouletteSpin = [this]
        {
            const auto o = labgames::skillRouletteOutcome (hauntedRng.rollInclusive (1, 8));
            if (o.points != 0) scoreSystem.adjustScore (o.points, "SKILL ROULETTE");
            if (o.skillDelta < 0) progressionSystem.removeRandomGameSkill (hauntedRng.rollInclusive (0, 99));
            if (o.pianoKeys > 0)   { progressionSystem.addPianoKeySounds (o.pianoKeys); rewardInstrument (pianoKey, o.pianoKeys); }
            if (o.snareSounds > 0) { progressionSystem.addSnareSounds (o.snareSounds); rewardInstrument (snare, o.snareSounds); }
            if (o.claps > 0)       { progressionSystem.addClapSounds (o.claps); rewardInstrument (clap, o.claps); }
            if (o.pads > 0)        { progressionSystem.addPadSounds (o.pads); rewardInstrument (pad, o.pads); }
            if (o.tomSounds > 0)   { progressionSystem.addTomSounds (o.tomSounds); rewardInstrument (tom, o.tomSounds); }
            if (o.upSweeps > 0)    { progressionSystem.addUpSweepSounds (o.upSweeps); rewardInstrument (upSweep, o.upSweeps); }
            const bool good = o.points >= 0 && o.skillDelta >= 0;
            labGameOverlay.resolve ("SKILL ROULETTE", good ? "The wheel paid out in sound." : "The wheel took something back.",
                                    labgames::Game::skillRoulette,
                                    good ? labgames::SoundMoment::bigWin : labgames::SoundMoment::penalty, good);
            playLabSound (labgames::Game::skillRoulette, good ? labgames::SoundMoment::bigWin : labgames::SoundMoment::penalty);
            progressionSystem.recordGameCompleted();
        };

        labGameOverlay.onMontePick = [this] (int card)
        {
            const bool won = card == monteWinningCard;
            const auto o = labgames::monteOutcome (won, hauntedRng.rollInclusive (1, 2));
            if (o.points != 0) scoreSystem.adjustScore (o.points, "THREE-CARD MONTE");
            if (o.rainbowOrbs > 0) { progressionSystem.addRainbowOrbs (o.rainbowOrbs); chamber.spawnRainbowOrbs (o.rainbowOrbs, false); }
            if (o.skillDelta > 0) awardGameSkill (hauntedRng.rollInclusive (0, 4), o.skillDelta);
            if (o.soundUnlocks > 0) rewardInstrument (genericSound, o.soundUnlocks);
            labGameOverlay.resolve ("THREE-CARD MONTE",
                                    won ? "You followed the card." : "Card " + juce::String (monteWinningCard + 1) + " was the one.",
                                    labgames::Game::monte,
                                    won ? labgames::SoundMoment::bigWin : labgames::SoundMoment::penalty, won);
            playLabSound (labgames::Game::monte, won ? labgames::SoundMoment::bigWin : labgames::SoundMoment::penalty);
            progressionSystem.recordGameCompleted();
        };

        labGameOverlay.onSlotsSpin = [this]
        {
            const auto o = labgames::slotOutcome (hauntedRng.rollInclusive (0, 999));
            if (o.points != 0) scoreSystem.adjustScore (o.points, "SLOTS");
            if (o.specialOrbs > 0) { progressionSystem.addSpecialOrbs (o.specialOrbs); chamber.spawnSpecialOrbs (o.specialOrbs); }
            if (o.glowingRainbowOrbs > 0) { progressionSystem.addRainbowOrbs (o.glowingRainbowOrbs, true); chamber.spawnRainbowOrbs (o.glowingRainbowOrbs, true, o.rainbowMultiplier); }
            if (o.skillDelta > 0) awardGameSkill (hauntedRng.rollInclusive (0, 4), o.skillDelta);
            if (o.soundUnlocks > 0) rewardInstrument (genericSound, o.soundUnlocks);
            const auto moment = o.jackpot ? labgames::SoundMoment::jackpot : labgames::SoundMoment::smallWin;
            labGameOverlay.resolve (o.jackpot ? "JACKPOT" : "SLOTS",
                                    o.jackpot ? "777. The whole lab lights up." : "+" + juce::String (o.points) + " points",
                                    labgames::Game::slots, moment, true);
            playLabSound (labgames::Game::slots, moment);
            progressionSystem.recordGameCompleted();
        };

        labGameOverlay.onDiceRoll = [this]
        {
            const int mine = hauntedRng.rollInclusive (1, 6), lab = hauntedRng.rollInclusive (1, 6);
            const auto o = labgames::diceOutcome (mine, lab);
            if (o.won)
            {
                progressionSystem.addSpecialOrbs (o.monsterOrbs + o.miniOrbs);
                chamber.spawnMonsterOrbs (o.monsterOrbs);
                chamber.spawnMiniOrbs (o.miniOrbs);
            }
            if (o.orbInversionPenalty) chamber.setOrbInversion (20.0f);
            // Three dice losses in a row cost 390,094 points.
            diceLossStreak = o.won ? 0 : diceLossStreak + 1;
            if (diceLossStreak >= 3)
            {
                diceLossStreak = 0;
                scoreSystem.adjustScore (-390094, "THREE DICE LOSSES");
            }
            labGameOverlay.resolve ("DICE", "You " + juce::String (mine) + "  /  Lab " + juce::String (lab),
                                    labgames::Game::dice,
                                    o.won ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty, o.won);
            playLabSound (labgames::Game::dice, o.won ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty);
            progressionSystem.recordGameCompleted();
        };

        labGameOverlay.onTwentyOneHit = [this]
        {
            dealCard (twentyOnePlayer, twentyOnePlayerAces);
            if (twentyOnePlayer >= 21) resolveTwentyOne();
            else labGameOverlay.showTwentyOne (twentyOnePlayer, true);
        };
        labGameOverlay.onTwentyOneStand = [this] { resolveTwentyOne(); };

        labGameOverlay.onScratch = [this]
        {
            if (scratchTickets <= 0) { presentNextLabGame(); return; }
            --scratchTickets;
            std::array<int, 5> symbols {};
            for (auto& s : symbols) s = hauntedRng.rollInclusive (0, 4);
            const auto o = labgames::scratchOutcome (symbols);
            if (o.points != 0) scoreSystem.adjustScore (o.points, "SCRATCH CARD");
            if (o.bassDrumSounds > 0)   { progressionSystem.addBassDrumSounds (o.bassDrumSounds); rewardInstrument (bassDrum, o.bassDrumSounds); }
            if (o.bassDrops > 0)        { progressionSystem.addBassDrops (o.bassDrops); rewardInstrument (bassDrop, o.bassDrops); }
            if (o.kazooSounds > 0)      { progressionSystem.addKazooSounds (o.kazooSounds); rewardInstrument (kazoo, o.kazooSounds); }
            if (o.stringSounds > 0)     { progressionSystem.addStringSounds (o.stringSounds); rewardInstrument (strings, o.stringSounds); }
            if (o.tambourineSounds > 0) { progressionSystem.addTambourineSounds (o.tambourineSounds); rewardInstrument (tambourine, o.tambourineSounds); }
            const bool good = o.matchCount >= 3;
            // Four winning cards in a row: four times in five, 495 orbs flood in and die fast.
            scratchStreak = good ? scratchStreak + 1 : 0;
            if (! good) scoreSystem.adjustScore (-494, "SCRATCH CARD LOST");
            if (scratchStreak >= 4)
            {
                scratchStreak = 0;
                if (hauntedRng.rollInclusive (1, 5) <= 4) chamber.spawnDoomedOrbs (495);
            }
            labGameOverlay.resolve ("SCRATCH CARD", juce::String (o.matchCount) + " matching symbols",
                                    labgames::Game::scratch,
                                    good ? labgames::SoundMoment::bigWin : labgames::SoundMoment::appear, good);
            playLabSound (labgames::Game::scratch, good ? labgames::SoundMoment::bigWin : labgames::SoundMoment::appear);
            progressionSystem.recordGameCompleted();
        };
    }

    void MutagenEditor::rewardInstrument (int kind, int count)
    {
        const auto& n = instrumentNote (kind);
        storyPanel.say (n.who, n.fact);
        storyPanel.collectSoon (juce::String (n.label) + " x" + juce::String (count), 0.25, 2.5f);
        if (kind != genericSound) progressionSystem.addGameSounds (count);
    }

    void MutagenEditor::serviceLabGameSchedule (juce::int64 beforeScore, juce::int64 afterScore)
    {
        // Scratch tickets come from score milestones, the rest from play time.
        if (const int t = labgames::scratchTicketsCrossed (beforeScore, afterScore); t > 0)
        {
            scratchTickets += t;
            queueLabGame (PendingGame::scratch);
        }

        // Score divisible by 12: half the time, orbs mutate into mice or squids.
        // Divisible by 6: one time in seven, yellow mites evolve. The score lands
        // on such numbers constantly, so each roll is spaced at least 25 s apart.
        if (afterScore != beforeScore && afterScore > 0 && playSeconds - lastDivisibleEventSec > 25.0)
        {
            if (afterScore % 12 == 0)
            {
                lastDivisibleEventSec = playSeconds;
                if (hauntedRng.rollInclusive (1, 2) == 1) chamber.mutateOrbsIntoCreatures (false);
            }
            else if (afterScore % 6 == 0)
            {
                lastDivisibleEventSec = playSeconds;
                if (hauntedRng.rollInclusive (1, 7) == 1) chamber.mutateOrbsIntoCreatures (true);
            }
        }

        // Crossing 3,048 points: one time in twelve, 345 orbs that turn into squids.
        if (! squidsChecked && afterScore >= 3048)
        {
            squidsChecked = true;
            if (hauntedRng.rollInclusive (1, 12) == 1)
                chamber.spawnOrbsThatBecomeSquids (345);
        }

        // Past 49,940,949 points: four times in five, giants shrink and multiply.
        if (! giantsChecked && afterScore > 49940949)
        {
            giantsChecked = true;
            if (hauntedRng.rollInclusive (1, 5) <= 4)
                chamber.spawnShrinkingGiants (24);
        }

        // Past a million points: three times in seven, a bonus and three
        // roulette spins back to back. Once per game.
        if (! millionChecked && afterScore > 1000000)
        {
            millionChecked = true;
            if (hauntedRng.rollInclusive (1, 7) <= 3)
            {
                scoreSystem.adjustScore (930943, "MILLION CLUB");
                for (int i = 0; i < 3; ++i) queueLabGame (PendingGame::roulette, true);
            }
        }

        const int minute = (int) (playSeconds / 60.0);
        if (minute == lastGameMinuteBucket) return;
        lastGameMinuteBucket = minute;

        if (labgames::shouldOfferSlots (minute, hauntedRng.rollInclusive (1, 10)))       queueLabGame (PendingGame::slots);
        if (labgames::shouldOfferRoulette (minute, hauntedRng.rollInclusive (1, 5)))     queueLabGame (PendingGame::roulette);
        if (labgames::shouldOfferTwentyOne (minute, hauntedRng.rollInclusive (1, 11)))   queueLabGame (PendingGame::twentyOne);
        if (labgames::shouldOfferMonte (minute, hauntedRng.rollInclusive (1, 3)))        queueLabGame (PendingGame::monte);
        if (labgames::shouldOfferDice (minute, hauntedRng.rollInclusive (1, 30)))        queueLabGame (PendingGame::dice);
    }

    void MutagenEditor::queueLabGame (PendingGame g, bool allowRepeat)
    {
        if (pendingGames.size() >= (allowRepeat ? 6u : 3u)) return;
        if (! allowRepeat && std::find (pendingGames.begin(), pendingGames.end(), g) != pendingGames.end()) return;
        pendingGames.push_back (g);
        if (! labGameOverlay.isVisible()) presentNextLabGame();
    }

    void MutagenEditor::presentNextLabGame()
    {
        if (pendingGames.empty())
        {
            labGameOverlay.setVisible (false);
            return;
        }

        const auto g = pendingGames.front();
        pendingGames.pop_front();
        labGameOverlay.setVisible (true);
        labGameOverlay.toFront (false);

        switch (g)
        {
            case PendingGame::roulette:      labGameOverlay.showRoulette (scoreSystem.score()); break;
            case PendingGame::skillRoulette: labGameOverlay.showRoulette (scoreSystem.score(), true); break;
            case PendingGame::monte:         monteWinningCard = hauntedRng.rollInclusive (0, 2); labGameOverlay.showMonte(); break;
            case PendingGame::slots:         labGameOverlay.showSlots(); break;
            case PendingGame::dice:          labGameOverlay.showDice(); break;
            case PendingGame::scratch:       labGameOverlay.showScratch (scratchTickets); break;
            case PendingGame::twentyOne:
                twentyOneActive = true;
                twentyOnePlayer = twentyOneHouse = twentyOnePlayerAces = twentyOneHouseAces = 0;
                for (int i = 0; i < 2; ++i)
                {
                    dealCard (twentyOnePlayer, twentyOnePlayerAces);
                    dealCard (twentyOneHouse, twentyOneHouseAces);
                }
                labGameOverlay.showTwentyOne (twentyOnePlayer, twentyOnePlayer < 21);
                break;
        }
        playLabSound (g == PendingGame::twentyOne ? labgames::Game::twentyOne : labgames::Game::roulette,
                      labgames::SoundMoment::appear, (int) g, 0.5f);
    }

    void MutagenEditor::playLabSound (labgames::Game game, labgames::SoundMoment moment, int variation, float)
    {
        processor.triggerSkillSound (labgames::soundRecipe (game, moment, variation));
    }

    void MutagenEditor::awardGameSkill (int index, int count)
    {
        const int before = progressionSystem.totalGameSkills();
        progressionSystem.changeGameSkill (juce::jlimit (0, 4, index), count);
        if (count > 0) storyPanel.grantFactReward();
        scoreSystem.adjustScore (labgames::skillDiscoveryPoints (count), "SKILL +" + juce::String (count));
        // Every second skill earned is worth another 93 points.
        for (int i = 0; i < count; ++i)
            if (++skillsEarnedThisGame % 2 == 0)
                scoreSystem.adjustScore (93, "EVERY SECOND SKILL");
        checkSkillRouletteTrigger (before, progressionSystem.totalGameSkills());
    }

    void MutagenEditor::checkSkillRouletteTrigger (int before, int after)
    {
        if (labgames::skillRouletteOfferDue (before, after, hauntedRng.rollInclusive (1, 4)))
            queueLabGame (PendingGame::skillRoulette);
    }

    int MutagenEditor::drawCardValue()
    {
        const int r = hauntedRng.rollInclusive (1, 13);
        return r == 1 ? 11 : juce::jmin (10, r);
    }

    void MutagenEditor::dealCard (int& total, int& softAces)
    {
        const int v = drawCardValue();
        total += v;
        if (v == 11) ++softAces;
        // An ace counts 11 until that would bust the hand, then 1.
        while (total > 21 && softAces > 0) { total -= 10; --softAces; }
    }

    void MutagenEditor::resolveTwentyOne()
    {
        if (! twentyOneActive) return;
        twentyOneActive = false;

        // The house draws to 17, like a casino dealer.
        while (twentyOnePlayer <= 21 && twentyOneHouse < 17)
            dealCard (twentyOneHouse, twentyOneHouseAces);

        const auto o = labgames::twentyOneOutcome (twentyOnePlayer, twentyOneHouse);
        if (o.points != 0) scoreSystem.adjustScore (o.points, "TWENTY-ONE");
        for (int i = 0; i < o.skillLoss; ++i)
            progressionSystem.removeRandomGameSkill (hauntedRng.rollInclusive (0, 99));
        if (o.snareSounds > 0) { progressionSystem.addSnareSounds (o.snareSounds); rewardInstrument (snare, o.snareSounds); }
        if (o.tomSounds > 0)   { progressionSystem.addTomSounds (o.tomSounds); rewardInstrument (tom, o.tomSounds); }

        const bool good = o.result != labgames::TwentyOneResult::loss;
        const auto text = "You " + juce::String (twentyOnePlayer) + "  /  House " + juce::String (twentyOneHouse);
        labGameOverlay.resolve (o.result == labgames::TwentyOneResult::win ? "TWENTY-ONE: WIN"
                              : o.result == labgames::TwentyOneResult::loss ? "TWENTY-ONE: LOSS" : "TWENTY-ONE: PUSH",
                                text, labgames::Game::twentyOne,
                                good ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty, good);
        playLabSound (labgames::Game::twentyOne, good ? labgames::SoundMoment::smallWin : labgames::SoundMoment::penalty);
        progressionSystem.recordGameCompleted();
    }
}
