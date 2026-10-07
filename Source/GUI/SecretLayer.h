// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/Secrets.h"
#include <functional>
#include <vector>

namespace mutagen
{
    class MutagenProcessor;
    class CultureChamber;
    class StoryPanel;

    /** A transparent layer over the culture chamber.

        It listens to the chamber's clicks without taking them, decides whether
        one of them opened a secret, and draws what follows: the secret room,
        the odd visitors (three with mirrored names, four upside down) who speak
        gibberish and fire bottle rockets, and the polyglot banners that turn up
        in another language for no reason at all. */
    class SecretLayer : public juce::Component,
                        private juce::Timer
    {
    public:
        SecretLayer (MutagenProcessor&, CultureChamber&, StoryPanel&);
        ~SecretLayer() override;

        /** Live game state, from the editor's UI timer. */
        void update (double dt, juce::int64 score, int generation, int population, bool paused);

        std::function<void (float, const juce::String&)> onReward;
        std::function<void (juce::int64, const juce::String&)> onPoints;
        std::function<void()> onLoseSkills;

        /** Plays the fate the player's name drew at the start of a game. */
        void playFate (secrets::Fate);

        /** Test hook: open a secret room directly. */
        void openSecretForTest (int id) { if (id >= 0 && id < secrets::secretCount) openSecret (id); }

        class Roamer;

        void paint (juce::Graphics&) override;

    private:
        void mouseDown (const juce::MouseEvent&) override;
        void timerCallback() override;

        void openSecret (int id);
        void applySkill (secrets::Skill, int param);
        void applyBurst (secrets::Burst, int count);
        void spawnVisitor (int which);
        void launchRocket (juce::Point<float> from, juce::Colour, int recipe);
        void startPolyglot (int which);
        void ensureAnimating();

        MutagenProcessor& processor;
        CultureChamber& chamber;
        StoryPanel& story;

        secrets::Tracker tracker;
        juce::Random rng;
        double clock = 0.0;
        juce::int64 liveScore = 0;
        int liveGeneration = 0, livePopulation = 1;

        // ---- the room currently open ----
        int roomId = -1;
        float roomAge = 0.0f;
        static constexpr float roomLife = 6.5f;
        std::vector<int> roomVisitors;

        // ---- odd visitors and their rockets ----
        struct Walker { int who; float x, targetX, age, life, nextRocket; juce::String speech; };
        std::vector<Walker> walkers;

        struct Spark { juce::Point<float> p, v; juce::Colour c; float life; };
        struct Rocket { juce::Point<float> p, v; juce::Colour c; float fuse; int recipe; std::vector<juce::Point<float>> trail; };
        std::vector<Rocket> rockets;
        std::vector<Spark> sparks;

        // ---- polyglot banner ----
        int polyglot = -1;
        float polyAge = 0.0f;

        double nextVisitorAt = 0.0, nextPolyglotAt = 0.0;

        // ---- name fates ----
        struct Dot { float x, y, vx, vy; juce::uint32 argb; };
        std::vector<Dot> dots;               // J: the hundred-thousand wall rain
        int dotsToSpawn = 0;
        juce::int64 dotPointsPending = 0;
        juce::Image dotImage;
        std::vector<Dot> swarm;              // T: 9,448 black orbs, bouncing
        float swarmLife = 0.0f;

        struct BigOrb { juce::Point<float> p, v; float life; };
        std::vector<BigOrb> bigOrbs;         // H: nine white moons

        float moonAge = -1.0f;               // K: the moon falls in
        bool moonLanded = false;

        std::unique_ptr<juce::Component> roamer;   // P / A: orbs loose on the desktop
        juce::String fateTitle;
        float fateTitleAge = 99.0f;

        struct Groaner { juce::Point<float> p; float heading, life; };
        std::vector<Groaner> groaners;       // 7,797,766 points: turquoise mites
        bool groanChecked = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SecretLayer)
    };
}
