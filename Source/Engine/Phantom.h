#pragma once
// MUTAGEN Phantom accompanist: pure C++20 call/answer engine (no JUCE).
// Degrees and phrase notes are semitone offsets from the run's root.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Storyline.h"

namespace mutagen::story::phantom
{
    /** Assumed tempo of a played call: seconds per beat. Used only to tell
        augmentation / diminution (2x / 0.5x) from ordinary tempo drift. */
    inline constexpr float kBeatSec = 0.5f;

    struct Motif
    {
        std::vector<int> degrees;   // semitone offsets from root, within the mode
        std::vector<float> beats;   // onset times in beats
    };

    struct PhraseEvent { double timeSec; int semitone; };
    using Phrase = std::vector<PhraseEvent>;

    enum class Classification { silence, noise, echo, inversion, augmentation,
                                diminution, sequence, variation, answer, count };

    inline float scoreFor (Classification c)
    {
        switch (c)
        {
            case Classification::echo:         return 0.3f;
            case Classification::inversion:    return 0.8f;
            case Classification::augmentation: return 0.8f;
            case Classification::diminution:   return 0.8f;
            case Classification::sequence:     return 0.7f;
            case Classification::variation:    return 1.0f;
            case Classification::answer:       return 0.9f;
            default:                           return 0.0f;   // silence, noise, count
        }
    }

    inline void markTechnique (std::uint32_t& seen, Classification c)
    {
        if (c != Classification::count) seen |= (1u << (unsigned) c);
    }
    inline bool hasTechnique (std::uint32_t seen, Classification c)
    {
        return c != Classification::count && (seen & (1u << (unsigned) c)) != 0;
    }

    namespace detail
    {
        inline int floorDiv (int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
        inline int pcOf (int s) { return ((s % 12) + 12) % 12; }

        /** Distinct in-octave steps of the mode (the trailing 12 and -99 dropped). */
        inline std::vector<int> pool (const std::vector<int>& modeSteps)
        {
            std::vector<int> p;
            for (int s : modeSteps)
                if (s >= 0 && s < 12 && std::find (p.begin(), p.end(), s) == p.end()) p.push_back (s);
            std::sort (p.begin(), p.end());
            if (p.empty()) p.push_back (0);
            return p;
        }

        inline bool inMode (int semitone, const std::vector<int>& p)
        {
            return std::find (p.begin(), p.end(), pcOf (semitone)) != p.end();
        }

        /** Absolute scale position (octave * size + index) of the mode note nearest a semitone. */
        inline int snapIndex (int semitone, const std::vector<int>& p)
        {
            const int n = (int) p.size(), oct = floorDiv (semitone, 12), pc = pcOf (semitone);
            int best = 0, bestD = 1000;
            for (int i = -1; i <= n; ++i)   // include neighbours across the octave seam
            {
                const int v = (i < 0) ? p[(std::size_t) (n - 1)] - 12 : (i == n ? p[0] + 12 : p[(std::size_t) i]);
                const int d = std::abs (v - pc);
                if (d < bestD) { bestD = d; best = i; }
            }
            return oct * n + best;
        }

        inline int degreeAt (int absIdx, const std::vector<int>& p)
        {
            const int n = (int) p.size();
            return floorDiv (absIdx, n) * 12 + p[(std::size_t) (((absIdx % n) + n) % n)];
        }

        inline std::vector<int> intervals (const std::vector<int>& v)
        {
            std::vector<int> out;
            for (std::size_t i = 1; i < v.size(); ++i) out.push_back (v[i] - v[i - 1]);
            return out;
        }

        inline std::vector<double> iois (const std::vector<double>& t)
        {
            std::vector<double> out;
            for (std::size_t i = 1; i < t.size(); ++i) out.push_back (t[i] - t[i - 1]);
            return out;
        }

        /** Same inter-onset *proportions* within +-25 % (tempo invariant). */
        inline bool rhythmMatches (const std::vector<double>& a, const std::vector<double>& c)
        {
            if (a.size() != c.size() || a.empty()) return false;
            double sa = 0, sc = 0;
            for (double x : a) sa += x;
            for (double x : c) sc += x;
            if (sa <= 1.0e-6 || sc <= 1.0e-6) return false;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                const double na = a[i] / sa, nc = c[i] / sc;
                if (std::abs (na - nc) > 0.25 * nc + 0.02) return false;
            }
            return true;
        }
    }

    /** Classify a player's answer against the call, by intervals and inter-onset
        proportions only (transposition and tempo invariant). Augmentation and
        diminution are a uniform 2x / 0.5x stretch relative to the call at kBeatSec. */
    inline Classification judge (const Motif& call, const Phrase& answerIn, const std::vector<int>& modeSteps)
    {
        using C = Classification;
        using namespace detail;
        if (answerIn.empty()) return C::silence;
        if (answerIn.size() < 2 || call.degrees.size() < 2 || call.beats.size() != call.degrees.size())
            return C::noise;

        Phrase ans = answerIn;
        std::stable_sort (ans.begin(), ans.end(), [] (const PhraseEvent& a, const PhraseEvent& b) { return a.timeSec < b.timeSec; });

        std::vector<int> an; std::vector<double> at, ct;
        for (auto& e : ans) { an.push_back (e.semitone); at.push_back (e.timeSec); }
        for (float b : call.beats) ct.push_back ((double) b * kBeatSec);

        const auto ai = intervals (an), ci = intervals (call.degrees);
        const auto aio = iois (at), cio = iois (ct);
        const bool sameLen = an.size() == call.degrees.size();
        const bool rhythmOk = sameLen && rhythmMatches (aio, cio);

        if (sameLen)
        {
            bool same = true, mirrored = true, allZero = true;
            for (std::size_t i = 0; i < ci.size(); ++i)
            {
                same = same && ai[i] == ci[i];
                mirrored = mirrored && ai[i] == -ci[i];
                allZero = allZero && ci[i] == 0;
            }

            if (same && rhythmOk)
            {
                const double scale = (at.back() - at.front()) / (ct.back() - ct.front());
                if (scale >= 1.6) return C::augmentation;
                if (scale <= 0.625) return C::diminution;
                return pcOf (an[0]) == pcOf (call.degrees[0]) ? C::echo : C::sequence;
            }
            if (mirrored && ! allZero && rhythmOk) return C::inversion;
            if (same) return C::variation;   // right notes, different rhythm
        }

        // Share at least half the intervals (positionally), differ elsewhere.
        std::size_t shared = 0;
        for (std::size_t i = 0; i < std::min (ai.size(), ci.size()); ++i)
            if (ai[i] == ci[i]) ++shared;
        if (shared > 0 && shared * 2 >= ci.size()) return C::variation;

        const auto p = pool (modeSteps);
        bool fits = true;
        for (int s : an) fits = fits && inMode (s, p);
        return fits ? C::answer : C::noise;
    }

    /** Act-based difficulty: 1-2 three diatonic steps; 3-5 four notes with a leap;
        6-9 five notes, syncopated. Every degree lies in the mode. */
    inline Motif makeCall (Rng& rng, int act, const std::vector<int>& modeSteps)
    {
        using namespace detail;
        const auto p = pool (modeSteps);
        const int n = (int) p.size();
        Motif m;
        int count = 3;
        std::vector<float> beats { 0.f, 1.f, 2.f };
        if (act >= 6)
        {
            count = 5;
            static const std::vector<std::vector<float>> bank {
                { 0.f, 0.5f, 1.5f, 2.f, 3.5f }, { 0.f, 1.f, 1.5f, 2.5f, 3.f }, { 0.f, 0.5f, 1.f, 2.5f, 3.f } };
            beats = bank[(std::size_t) rng.range (0, (int) bank.size() - 1)];
        }
        else if (act >= 3)
        {
            count = 4;
            static const std::vector<std::vector<float>> bank {
                { 0.f, 1.f, 2.f, 3.f }, { 0.f, 1.f, 1.5f, 3.f }, { 0.f, 0.5f, 2.f, 3.f } };
            beats = bank[(std::size_t) rng.range (0, (int) bank.size() - 1)];
        }
        const bool leaps = act >= 3;
        int idx = rng.range (0, n - 1) + n;   // second octave, room to go down
        const int base = idx;
        std::vector<int> pos { idx };
        bool leapped = false;
        for (int i = 1; i < count; ++i)
        {
            int step = rng.range (0, 1) ? 1 : -1;
            if (leaps && ! leapped && (i == count - 1 || rng.range (0, 2) == 0))
            { step *= rng.range (2, 3); leapped = true; }
            int next = idx + step;
            if (next < 0 || next > 3 * n - 1) next = idx - step;   // reflect at the edges
            idx = next;
            pos.push_back (idx);
        }
        for (int q : pos) m.degrees.push_back (degreeAt (q, p) - 12 * floorDiv (base, n));
        m.beats = beats;
        return m;
    }

    /** Phantom's answer: the player's notes snapped to the mode and harmonised
        (a third, sometimes a fifth, above), resolving to the root after good answers.
        Empty for silence. */
    inline Motif makeReply (const Phrase& phrase, Classification c, Rng& rng, const std::vector<int>& modeSteps)
    {
        using namespace detail;
        Motif r;
        if (phrase.empty() || c == Classification::silence) return r;
        Phrase ans = phrase;
        std::stable_sort (ans.begin(), ans.end(), [] (const PhraseEvent& a, const PhraseEvent& b) { return a.timeSec < b.timeSec; });
        const auto p = pool (modeSteps);
        const int up = rng.range (0, 3) == 0 ? 4 : 2;   // fifth or (usually) third, in scale steps
        const double t0 = ans.front().timeSec;
        for (auto& e : ans)
        {
            r.degrees.push_back (degreeAt (snapIndex (e.semitone, p) + up, p));
            r.beats.push_back ((float) ((e.timeSec - t0) / kBeatSec));
        }
        if (c == Classification::answer || c == Classification::variation)
        {
            r.degrees.push_back (12 * floorDiv (r.degrees.back(), 12) + p[0]);
            r.beats.push_back (r.beats.back() + 1.f);
        }
        return r;
    }

    /** Maps a click x in [0,1] to a mode semitone across `octaves` octaves. */
    inline int degreeForX (float x01, const std::vector<int>& modeSteps, int octaves = 2)
    {
        using namespace detail;
        const auto p = pool (modeSteps);
        const int total = (int) p.size() * std::max (1, octaves) + 1;
        const float x = std::min (1.0f, std::max (0.0f, x01));
        return degreeAt (std::min (total - 1, (int) (x * (float) total)), p);
    }

    /** Seeded summon timing. Feed it the play clock; gap is 4-8 min of unpaused play. */
    class Director
    {
    public:
        explicit Director (std::uint64_t seed) : rng (seed) { rollGap(); }

        /** True exactly once per encounter, until markDone(). */
        bool update (double playSec, bool paused, bool otherEncounterActive)
        {
            const double dt = std::max (0.0, playSec - lastClock);
            lastClock = playSec;
            if (! paused) unpausedSec += dt;
            if (active || paused || otherEncounterActive) return false;
            if (unpausedSec < gap) return false;
            active = true;
            return true;
        }

        void markDone() { active = false; unpausedSec = 0.0; rollGap(); }

        /** Next call, never identical to the previous one. */
        Motif nextCall (int act, const std::vector<int>& modeSteps)
        {
            Motif m;
            for (int tries = 0; tries < 64; ++tries)
            {
                m = makeCall (rng, act, modeSteps);
                if (! same (m, last)) break;
            }
            if (same (m, last) && ! m.beats.empty()) m.beats.back() += 0.5f;   // last resort
            last = m;
            return m;
        }

        double gapSec() const { return gap; }
        double unpausedPlaySec() const { return unpausedSec; }
        bool encounterActive() const { return active; }

    private:
        static bool same (const Motif& a, const Motif& b) { return a.degrees == b.degrees && a.beats == b.beats; }
        void rollGap() { gap = 240.0 + rng.unit() * 240.0; }

        Rng rng;
        double gap = 300.0, unpausedSec = 0.0, lastClock = 0.0;
        bool active = false;
        Motif last;
    };
}
