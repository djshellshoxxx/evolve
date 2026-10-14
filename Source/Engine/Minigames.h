#pragma once

// Two chance minigames (docs/specs/minigames-spec.md). Pure standard C++ (no JUCE).
//   Pulse: keep time with the colony's heartbeat after it goes silent.
//   Hunt:  name the interval the colony just played.

#include "Storyline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
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

    // ---- Seek: hot and cold by ear -----------------------------------------------

    class Seek
    {
    public:
        static constexpr int kRounds = 3, kToWin = 2;
        static constexpr double kTimeLimit = 12.0, kLockRadius = 0.06;

        void start (std::uint64_t seed)
        {
            rng = Rng (seed);
            round = 0; found = 0; done = false;
            newRound();
        }

        int roundNumber() const { return std::min (round + 1, kRounds); }
        int foundCount() const { return found; }
        bool finished() const { return done; }
        bool won() const { return found >= kToWin; }
        double timeLeft() const { return std::max (0.0, kTimeLimit - elapsed); }
        double target() const { return goal; }
        double lastDistance() const { return dist; }

        /** Ping spacing: fast when close, slow when far. */
        static double pingInterval (double distance) { return 0.12 + std::clamp (distance, 0.0, 1.0) * 1.2; }

        /** Returns true when a ping should sound now (pitch the caller from the position). */
        bool update (double dt, double pos)
        {
            if (done) return false;
            elapsed += dt;
            dist = std::abs (pos - goal);
            if (elapsed >= kTimeLimit) { advance (false); return false; }
            sincePing += dt;
            if (sincePing >= pingInterval (dist)) { sincePing = 0.0; return true; }
            return false;
        }

        /** Lock in the current position. Returns whether it was close enough. */
        bool lock (double pos)
        {
            if (done) return false;
            const bool ok = std::abs (pos - goal) <= kLockRadius;
            advance (ok);
            return ok;
        }

    private:
        void advance (bool ok)
        {
            if (ok) ++found;
            if (++round >= kRounds) done = true; else newRound();
        }
        void newRound() { goal = 0.1 + 0.8 * rng.unit(); elapsed = 0.0; sincePing = 0.0; dist = 1.0; }

        Rng rng { 1 };
        double goal = 0.5, elapsed = 0.0, sincePing = 0.0, dist = 1.0;
        int round = 0, found = 0;
        bool done = false;
    };

    // ---- Flash: chord quality, against the clock -----------------------------------

    enum class Quality : int { major, minor, diminished, augmented, count };

    inline const char* qualityName (Quality q)
    {
        static const char* n[] = { "MAJOR", "MINOR", "DIMINISHED", "AUGMENTED" };
        return n[(int) q];
    }

    /** Semitone offsets of the three chord tones above the root. */
    inline std::array<int, 3> qualityTones (Quality q)
    {
        switch (q)
        {
            case Quality::major:      return { 0, 4, 7 };
            case Quality::minor:      return { 0, 3, 7 };
            case Quality::diminished: return { 0, 3, 6 };
            default:                  return { 0, 4, 8 };
        }
    }

    class Flash
    {
    public:
        static constexpr int kQuestions = 8, kToWin = 6;
        static constexpr double kPerQuestion = 4.0;

        void start (std::uint64_t seed)
        {
            rng = Rng (seed);
            index = 0; right = 0; playPending = true;
            ask();
        }

        int questionNumber() const { return std::min (index + 1, kQuestions); }
        int correctCount() const { return right; }
        bool finished() const { return index >= kQuestions; }
        bool won() const { return right >= kToWin; }
        Quality answerQuality() const { return current; }
        int rootOffset() const { return base; }
        double timeLeft() const { return std::max (0.0, kPerQuestion - elapsed); }

        bool consumePlay() { const bool p = playPending; playPending = false; return p; }

        /** A timeout counts as wrong. */
        void update (double dt)
        {
            if (finished()) return;
            elapsed += dt;
            if (elapsed >= kPerQuestion) next (false);
        }

        bool answer (int option)
        {
            if (finished() || option < 0 || option >= (int) Quality::count) return false;
            const bool ok = option == (int) current;
            next (ok);
            return ok;
        }

    private:
        void next (bool ok)
        {
            if (ok) ++right;
            ++index;
            if (! finished()) ask();
        }
        void ask() { current = (Quality) rng.range (0, (int) Quality::count - 1); base = rng.range (-5, 5); elapsed = 0.0; playPending = true; }

        Rng rng { 1 };
        Quality current = Quality::major;
        double elapsed = 0.0;
        int index = 0, right = 0, base = 0;
        bool playPending = false;
    };

    // ---- Frequency Lab: pitch is hertz -------------------------------------------

    class FrequencyLab
    {
    public:
        static constexpr int kQuestions = 5, kToWin = 3;
        static constexpr double kLowHz = 55.0, kOctaves = 5.0, kOkCents = 50.0, kPerfectCents = 15.0;

        struct Question
        {
            std::string prompt, lesson;
            double refHz = 0.0, targetHz = 440.0;
        };

        struct Result { bool ok = false, perfect = false; double cents = 0.0; };

        static double hzAt (double pos) { return kLowHz * std::pow (2.0, std::clamp (pos, 0.0, 1.0) * kOctaves); }
        static double posOf (double hz) { return std::clamp (std::log2 (hz / kLowHz) / kOctaves, 0.0, 1.0); }
        static double centsBetween (double a, double b) { return 1200.0 * std::log2 (a / b); }
        static int midiFor (double hz) { return (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)); }
        static double hzForMidi (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

        void start (std::uint64_t seed)
        {
            Rng rng (seed);
            questions.clear();
            auto pick = [&] (std::initializer_list<double> v) { return *(v.begin() + rng.range (0, (int) v.size() - 1)); };
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

            double r = pick ({ 110.0, 220.0, 440.0 });
            questions.push_back ({ "Here is " + fmt (r) + " Hz. Find the note one octave ABOVE it.",
                                   "Doubling the frequency raises the pitch by exactly one octave: " + fmt (r) + " Hz becomes " + fmt (2 * r) + " Hz.", r, 2 * r });
            r = pick ({ 110.0, 220.0 });
            questions.push_back ({ "Here is " + fmt (r) + " Hz. Find the perfect FIFTH above it.",
                                   "A perfect fifth is a 3:2 ratio, so " + fmt (r) + " x 1.5 = " + fmt (1.5 * r) + " Hz.", r, 1.5 * r });
            r = pick ({ 440.0, 880.0 });
            questions.push_back ({ "Here is " + fmt (r) + " Hz. Find the note one octave BELOW it.",
                                   "Halving the frequency drops one octave: " + fmt (r) + " Hz becomes " + fmt (r / 2) + " Hz.", r, r / 2 });
            for (int i = 0; i < 2; ++i)
            {
                const int m = (int) pick ({ 60.0, 62.0, 64.0, 65.0, 67.0, 69.0, 71.0, 72.0 });
                const std::string n = std::string (names[m % 12]) + std::to_string (m / 12 - 1);
                questions.push_back ({ "Find " + n + " without a reference. Concert A4 is 440 Hz.",
                                       n + " is " + fmt (hzForMidi (m)) + " Hz. Each semitone multiplies the frequency by 1.0595 (the 12th root of 2).", 0.0, hzForMidi (m) });
            }
            index = 0; right = 0; perfectCount = 0; lastLesson.clear();
        }

        bool finished() const { return index >= kQuestions; }
        bool won() const { return right >= kToWin; }
        int questionNumber() const { return std::min (index + 1, kQuestions); }
        int correctCount() const { return right; }
        int perfects() const { return perfectCount; }
        const Question& current() const { return questions[(std::size_t) std::min (index, kQuestions - 1)]; }
        const std::string& lastLessonText() const { return lastLesson; }

        Result submit (double pos)
        {
            Result res;
            if (finished()) return res;
            const auto& q = current();
            res.cents = centsBetween (hzAt (pos), q.targetHz);
            res.ok = std::abs (res.cents) <= kOkCents;
            res.perfect = std::abs (res.cents) <= kPerfectCents;
            if (res.ok) ++right;
            if (res.perfect) ++perfectCount;
            lastLesson = q.lesson;
            ++index;
            return res;
        }

        static std::string fmt (double hz)
        {
            char buf[32];
            std::snprintf (buf, sizeof buf, hz < 100.0 || std::abs (hz - std::round (hz)) > 0.05 ? "%.1f" : "%.0f", hz);
            return buf;
        }

    private:
        std::vector<Question> questions;
        std::string lastLesson;
        int index = 0, right = 0, perfectCount = 0;
    };
}
