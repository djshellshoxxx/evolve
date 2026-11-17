#pragma once

// The Echo Chorale: the post-ending encore (docs/specs/encore-spec.md).
//
// Pure standard C++ (no JUCE). The colony sings a growing phrase on four pads;
// the player echoes it back. Three lives, twelve rounds, a faster tempo each
// round. The GUI drives update(dt) / press(lane) and plays the notes it is told to.

#include "Storyline.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace mutagen::story::encore
{
    constexpr int kLanes = 4, kRounds = 12, kLives = 3, kFirstLength = 3;
    constexpr double kInputTimeout = 6.0, kPause = 1.0;

    enum class State : int { demo, listen, cleared, won, lost };
    enum class Press : int { ignored, correct, roundDone, wrong, lostGame };

    /** Four scale steps spread across the mode, ascending. Never fewer than 1 distinct. */
    inline std::array<int, kLanes> laneSteps (const std::vector<int>& steps)
    {
        std::array<int, kLanes> out {};
        if (steps.empty()) { out = { 0, 4, 7, 12 }; return out; }
        for (int i = 0; i < kLanes; ++i)
            out[(std::size_t) i] = steps[(std::size_t) i * steps.size() / (std::size_t) kLanes];
        return out;
    }

    /** Seconds between demo notes; tightens each round, floor 0.26. */
    inline double beatSeconds (int round) { return std::max (0.26, 0.62 - 0.03 * (double) round); }

    /** Rank earned by the best round cleared, lifetime. */
    inline const char* rankFor (int best)
    {
        if (best >= kRounds) return "MAESTRO";
        if (best >= 9) return "CONDUCTOR";
        if (best >= 6) return "SOLOIST";
        if (best >= 3) return "CHORISTER";
        return best >= 1 ? "APPRENTICE" : "UNHEARD";
    }

    /** Points for a finished chorale: rounds cleared, a bonus per life kept on a full clear. */
    inline long long pointsFor (int roundsCleared, int livesLeft)
    {
        const long long base = 400LL * std::clamp (roundsCleared, 0, kRounds);
        return base + (roundsCleared >= kRounds ? 2000LL * std::clamp (livesLeft, 0, kLives) : 0LL);
    }

    class Chorale
    {
    public:
        void start (std::uint64_t seed)
        {
            rng = Rng (seed);
            phrase.clear();
            roundsDone = 0; lifeCount = kLives;
            beginRound (false);
        }

        State state() const { return st; }
        int roundsCleared() const { return roundsDone; }
        int lives() const { return lifeCount; }
        int phraseLength() const { return (int) phrase.size(); }
        int inputIndex() const { return pos; }
        const std::vector<int>& notes() const { return phrase; }
        bool finished() const { return st == State::won || st == State::lost; }

        /** Lane lit by the demo right now, or -1. */
        int litLane() const { return st == State::demo && lit > 0.0 ? phrase[(std::size_t) std::max (0, demoPos - 1)] : -1; }

        /** Lanes the demo has just sounded; the GUI plays and clears them. */
        std::vector<int> takeSounded() { auto v = sounded; sounded.clear(); return v; }

        void update (double dt)
        {
            clock += dt;
            lit = std::max (0.0, lit - dt);
            if (st == State::demo)
            {
                demoWait -= dt;
                while (demoWait <= 0.0 && st == State::demo)
                {
                    if (demoPos < (int) phrase.size())
                    {
                        sounded.push_back (phrase[(std::size_t) demoPos++]);
                        lit = beatSeconds (roundsDone) * 0.7;
                        demoWait += beatSeconds (roundsDone);
                    }
                    else { st = State::listen; pos = 0; idle = 0.0; }
                }
            }
            else if (st == State::listen)
            {
                idle += dt;
                if (idle >= kInputTimeout) fail();
            }
            else if (st == State::cleared)
            {
                demoWait -= dt;
                if (demoWait <= 0.0)
                {
                    if (roundsDone >= kRounds) st = State::won;
                    else beginRound (true);
                }
            }
        }

        Press press (int lane)
        {
            if (st != State::listen || lane < 0 || lane >= kLanes) return Press::ignored;
            idle = 0.0;
            if (phrase[(std::size_t) pos] != lane) return fail() ? Press::lostGame : Press::wrong;
            if (++pos < (int) phrase.size()) return Press::correct;
            ++roundsDone; st = State::cleared; demoWait = kPause;
            return Press::roundDone;
        }

    private:
        /** Lose a life: replay the same phrase, or end the game on the last one. */
        bool fail()
        {
            if (--lifeCount <= 0) { lifeCount = 0; st = State::lost; return true; }
            beginRound (false);
            return false;
        }

        void beginRound (bool grow)
        {
            const int length = kFirstLength + roundsDone;
            if (grow || phrase.empty())
                for (int i = (int) phrase.size(); i < length; ++i) phrase.push_back (rng.range (0, kLanes - 1));
            st = State::demo; demoPos = 0; pos = 0; demoWait = 0.8; lit = 0.0; idle = 0.0;
            sounded.clear();
        }

        Rng rng { 1 };
        std::vector<int> phrase, sounded;
        State st = State::demo;
        int roundsDone = 0, lifeCount = kLives, demoPos = 0, pos = 0;
        double clock = 0.0, demoWait = 0.0, lit = 0.0, idle = 0.0;
    };
}
