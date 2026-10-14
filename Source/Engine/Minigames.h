#pragma once

// Two chance minigames (docs/specs/minigames-spec.md). Pure standard C++ (no JUCE).
//   Pulse: keep time with the colony's heartbeat after it goes silent.
//   Hunt:  name the interval the colony just played.

#include "Storyline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace mutagen::story::mini
{
    // ---- Pulse ---------------------------------------------------------------

    class Pulse
    {
    public:
        static constexpr int kBeats = 8, kAudible = 4, kToWin = 6;
        static constexpr double kLead = 1.5, kWindow = 0.25, kHit = 0.12, kPerfect = 0.05;

        void start (std::uint64_t seed)
        {
            Rng rng (seed);
            beat = 0.5 + 0.05 * (double) rng.range (0, 6);      // 120 to 75 bpm
            clock = 0.0; hitCount = perfectCount = 0; nextSound = 0;
            judged.fill (false);
        }

        double beatSeconds() const { return beat; }
        double beatTime (int i) const { return kLead + beat * (double) i; }
        double clockNow() const { return clock; }
        int hits() const { return hitCount; }
        int perfects() const { return perfectCount; }
        bool won() const { return hitCount >= kToWin; }
        bool finished() const { return clock > beatTime (kBeats - 1) + kWindow + 0.4; }

        /** 0 just after a beat, 1 on the next one: drives the shrinking ring. */
        double phase() const
        {
            const double p = (clock - kLead) / beat;
            return p < 0.0 ? 0.0 : p - std::floor (p);
        }

        void update (double dt)
        {
            clock += dt;
            for (int i = 0; i < kBeats; ++i)
                if (! judged[(std::size_t) i] && clock > beatTime (i) + kWindow) judged[(std::size_t) i] = true;
        }

        /** Beat indices whose click should sound now (only the first kAudible do). */
        std::vector<int> takeSounded()
        {
            std::vector<int> out;
            while (nextSound < kAudible && clock >= beatTime (nextSound)) out.push_back (nextSound++);
            return out;
        }

        /** A tap now. Returns seconds early(-)/late(+) when it landed on a beat, else a large number. */
        double tap()
        {
            int best = -1; double bestErr = 1.0e9;
            for (int i = 0; i < kBeats; ++i)
            {
                const double err = clock - beatTime (i);
                if (! judged[(std::size_t) i] && std::abs (err) <= kWindow && std::abs (err) < std::abs (bestErr))
                { best = i; bestErr = err; }
            }
            if (best < 0) return 1.0e9;
            judged[(std::size_t) best] = true;
            if (std::abs (bestErr) <= kHit) { ++hitCount; if (std::abs (bestErr) <= kPerfect) ++perfectCount; }
            return bestErr;
        }

    private:
        double beat = 0.5, clock = 0.0;
        int hitCount = 0, perfectCount = 0, nextSound = 0;
        std::array<bool, kBeats> judged {};
    };

    // ---- Hunt ----------------------------------------------------------------

    inline const char* intervalName (int semitones)
    {
        static const char* n[] = { "UNISON", "MINOR 2ND", "MAJOR 2ND", "MINOR 3RD", "MAJOR 3RD", "PERFECT 4TH",
                                   "TRITONE", "PERFECT 5TH", "MINOR 6TH", "MAJOR 6TH", "MINOR 7TH", "MAJOR 7TH", "OCTAVE" };
        return semitones >= 0 && semitones <= 12 ? n[semitones] : "?";
    }

    class Hunt
    {
    public:
        static constexpr int kQuestions = 5, kOptions = 4, kToWin = 4;

        void start (std::uint64_t seed, int act)
        {
            rng = Rng (seed);
            // Early acts use the easy, widely spaced intervals; later acts add the confusable ones.
            pool = { 7, 4, 12, 5, 3 };
            if (act >= 3) { pool.push_back (2); pool.push_back (9); pool.push_back (10); }
            if (act >= 6) { pool.push_back (1); pool.push_back (6); pool.push_back (11); pool.push_back (8); }
            index = 0; right = 0; playPending = true;
            base = rng.range (0, 5);
            ask();
        }

        int questionNumber() const { return index + 1; }
        int correctCount() const { return right; }
        bool finished() const { return index >= kQuestions; }
        bool won() const { return right >= kToWin; }
        int answerSemitones() const { return target; }
        const std::array<int, kOptions>& options() const { return opts; }
        int rootOffset() const { return base; }

        /** True once whenever the root and interval should be played (new question or replay). */
        bool consumePlay() { const bool p = playPending; playPending = false; return p; }
        void replay() { if (! finished()) playPending = true; }

        /** Returns whether option `i` was right, and moves to the next question. */
        bool answer (int i)
        {
            if (finished() || i < 0 || i >= kOptions) return false;
            const bool ok = opts[(std::size_t) i] == target;
            if (ok) ++right;
            ++index;
            if (! finished()) { base = rng.range (0, 5); ask(); playPending = true; }
            return ok;
        }

    private:
        void ask()
        {
            target = pool[(std::size_t) rng.range (0, (int) pool.size() - 1)];
            std::vector<int> choices { target };
            while ((int) choices.size() < kOptions)
            {
                const int c = pool[(std::size_t) rng.range (0, (int) pool.size() - 1)];
                if (std::find (choices.begin(), choices.end(), c) == choices.end()) choices.push_back (c);
            }
            for (int i = (int) choices.size() - 1; i > 0; --i) std::swap (choices[(std::size_t) i], choices[(std::size_t) rng.range (0, i)]);
            for (int i = 0; i < kOptions; ++i) opts[(std::size_t) i] = choices[(std::size_t) i];
        }

        Rng rng { 1 };
        std::vector<int> pool;
        std::array<int, kOptions> opts {};
        int index = 0, right = 0, target = 0, base = 0;
        bool playPending = false;
    };
}
