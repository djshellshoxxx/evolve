// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

// The "phreak" cheat code (docs/specs/phreak-spec.md): typing the word opens a
// feature tree that plays any minigame, lab game, skill, story event or major
// unlock on demand. Every entry calls the same code path the game itself uses.

#include "PluginEditor.h"
#include "Engine/Entropy.h"
#include "Engine/Secrets.h"

namespace mutagen
{
    void MutagenEditor::wirePhreak()
    {
        addChildComponent (phreak);
        phreak.setTree (buildPhreakTree());
    }

    bool MutagenEditor::phreakKey (const juce::KeyPress& key)
    {
        if (phreak.isVisible()) return false;
        const auto c = key.getTextCharacter();
        if (c <= 0 || c > 127) return false;
        if (! phreakKeys.push ((char) c, juce::Time::getMillisecondCounterHiRes() * 0.001)) return false;
        phreak.setBounds (getLocalBounds());
        phreak.open();
        return true;
    }

    std::vector<PhreakOverlay::Category> MutagenEditor::buildPhreakTree()
    {
        using Cat = PhreakOverlay::Category;
        std::vector<Cat> tree;

        const auto startEncounter = [this] (EncounterOverlay::Kind k)
        {
            return [this, k]
            {
                encounter.begin (k, globalEntropy().nextSeed(), storyPanel.act(), storyPanel.run().mode,
                                 storyPanel.run().root, storyPanel.progress().relics);
            };
        };

        // ---- minigames and encounters ------------------------------------------
        {
            using K = EncounterOverlay::Kind;
            Cat c { "MINIGAMES AND ENCOUNTERS", {} };
            c.items.push_back ({ "Dream Room (random room)", startEncounter (K::room) });
            c.items.push_back ({ "Phantom: Call and Answer", startEncounter (K::phantom) });
            c.items.push_back ({ "Tuner Duel", startEncounter (K::tuner) });
            c.items.push_back ({ "Encore: The Echo Chorale", startEncounter (K::chorale) });
            c.items.push_back ({ "Pulse: Keep the Heartbeat", startEncounter (K::pulse) });
            c.items.push_back ({ "Interval Hunt", startEncounter (K::hunt) });
            c.items.push_back ({ "Seek: Hot and Cold", startEncounter (K::seek) });
            c.items.push_back ({ "Chord Flash", startEncounter (K::flash) });
            c.items.push_back ({ "Frequency Lab", startEncounter (K::freq) });
            tree.push_back (std::move (c));
        }

        // ---- lab games ----------------------------------------------------------
        {
            Cat c { "LAB GAMES", {} };
            const std::pair<const char*, PendingGame> games[] = {
                { "Roulette", PendingGame::roulette }, { "Monte", PendingGame::monte }, { "Slots", PendingGame::slots },
                { "Dice", PendingGame::dice }, { "Twenty-One", PendingGame::twentyOne }, { "Scratch Ticket", PendingGame::scratch },
                { "Skill Roulette", PendingGame::skillRoulette }, { "Phantom Monte", PendingGame::phantomMonte },
                { "Eclipse Slots", PendingGame::eclipseSlots }, { "Spectral Dice", PendingGame::spectralDice } };
            for (const auto& [name, game] : games)
                c.items.push_back ({ name, [this, game = game]
                {
                    if (game == PendingGame::scratch) scratchTickets = juce::jmax (1, scratchTickets);
                    queueLabGame (game, true);
                } });
            tree.push_back (std::move (c));
        }

        // ---- skills ---------------------------------------------------------------
        {
            Cat c { "SKILLS", {} };
            for (int i = 0; i < 5; ++i)
                c.items.push_back ({ "Game skill " + juce::String (i + 1) + " (+1)", [this, i] { awardGameSkill (i, 1); } });
            for (int i = 1; i <= 16; ++i)
                c.items.push_back ({ "Milestone skill " + juce::String (i) + " (artifact " + juce::String (i) + " million)",
                                     [this, i] { triggerMilestoneArtifact (i, false); } });
            tree.push_back (std::move (c));
        }

        // ---- instrument unlocks ------------------------------------------------
        {
            Cat c { "INSTRUMENT UNLOCKS", {} };
            const char* names[] = { "Snare", "Tom", "Bass drum", "Strings", "Tambourine", "Kazoo", "Bass drop",
                                    "Piano key", "Clap", "Pad", "Up sweep", "Collected sound" };
            for (int i = 0; i < 12; ++i)
                c.items.push_back ({ names[i], [this, i] { rewardInstrument (i, 1); } });
            tree.push_back (std::move (c));
        }

        // ---- story --------------------------------------------------------------
        {
            Cat c { "STORY", {} };
            for (int a = 1; a <= story::actCount; ++a)
                c.items.push_back ({ "Open " + juce::String (story::acts()[(std::size_t) (a - 1)].title).trim(), [this, a] { storyPanel.cheatAct (a); } });
            for (int e = 0; e < 4; ++e)
                c.items.push_back ({ "Ending: " + juce::String (story::endings()[(std::size_t) e].name), [this, e] { storyPanel.cheatEnding (e); } });
            c.items.push_back ({ "Reveal this run's twist", [this] { storyPanel.cheatTwist(); } });
            c.items.push_back ({ "Ear check (quiz)", [this] { storyPanel.cheatQuiz(); } });
            c.items.push_back ({ "Conversation", [this] { storyPanel.cheatBanter(); } });
            c.items.push_back ({ "Bonus fact", [this] { storyPanel.cheatFact(); } });
            c.items.push_back ({ "UNLOCK: all endings, events, secrets, twists and lexicon", [this] { storyPanel.cheatUnlockAll(); } });
            tree.push_back (std::move (c));
        }

        // ---- chance events --------------------------------------------------------
        {
            Cat c { "CHANCE EVENTS", {} };
            for (int i = 0; i < chance::count; ++i)
                c.items.push_back ({ juce::String (chance::all()[(std::size_t) i].name), [this, i] { storyPanel.fireChanceForTest (i); } });
            tree.push_back (std::move (c));
        }

        // ---- secret rooms ---------------------------------------------------------
        {
            Cat c { "SECRET ROOMS", {} };
            for (int i = 0; i < secrets::secretCount; ++i)
                c.items.push_back ({ juce::String (secrets::all()[(std::size_t) i].room) + "  /  " + secrets::all()[(std::size_t) i].name,
                                     [this, i] { secretLayer.openSecretRoom (i); } });
            tree.push_back (std::move (c));
        }

        // ---- haunted lab ----------------------------------------------------------
        {
            Cat c { "HAUNTED LAB", {} };
            for (int i = 0; i < 100; ++i)
                c.items.push_back ({ "Creature " + juce::String (i + 1) + ": " + juce::String (haunted::creatureName (i)),
                                     [this, i] { triggerCreature (i, false); } });
            for (int i = 0; i < 14; ++i)
                c.items.push_back ({ "Name fate " + juce::String (i + 1), [this, i] { secretLayer.playFate ((secrets::Fate) (i + 1)); } });
            tree.push_back (std::move (c));
        }

        // ---- orbs -------------------------------------------------------------------
        {
            Cat c { "ORB EVENTS", {} };
            c.items.push_back ({ "40 special orbs", [this] { chamber.spawnSpecialOrbs (40); } });
            c.items.push_back ({ "6 monster orbs", [this] { chamber.spawnMonsterOrbs (6); } });
            c.items.push_back ({ "6 giant orbs that become mites", [this] { chamber.spawnGiantsThatBecomeMites (6); } });
            c.items.push_back ({ "Reward burst", [this] { chamber.triggerReward (0.3f); } });
            c.items.push_back ({ "Temporary gator (120 bpm)", [this] { processor.triggerTemporaryGator (120, 20); } });
            tree.push_back (std::move (c));
        }
        return tree;
    }
}
