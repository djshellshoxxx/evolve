#include <cstdio>
#include "Engine/Phantom.h"

using namespace mutagen::story;
using namespace mutagen::story::phantom;

static int g_fail = 0;
#define CHECK(c) do { if (! (c)) { ++g_fail; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::vector<int> steps (int m)
{
    std::vector<int> out;
    for (auto s : modes()[(std::size_t) m].steps) if (s != kNo) out.push_back (s);
    return out;
}

static bool fitsMode (int deg, const std::vector<int>& ms)
{
    for (int s : ms) if (((deg % 12) + 12) % 12 == s % 12) return true;
    return false;
}

static Phrase play (const std::vector<int>& deg, const std::vector<float>& beats, int transpose, double tempo)
{
    Phrase p;
    for (std::size_t i = 0; i < deg.size(); ++i)
        p.push_back ({ 1.0 + beats[i] * kBeatSec * tempo, deg[i] + transpose });
    return p;
}

int main()
{
    const auto major = steps (0);
    const Motif call { { 0, 2, 4, 2 }, { 0, 1, 2, 3 } };
    using C = Classification;

    // Every classification reachable.
    CHECK (judge (call, {}, major) == C::silence);
    CHECK (judge (call, { { 1.0, 4 } }, major) == C::noise);
    CHECK (judge (call, play ({ 1, 6, 1, 6 }, { 0, 1, 2, 3 }, 0, 1.0), major) == C::noise);
    CHECK (judge (call, play (call.degrees, call.beats, 0, 1.0), major) == C::echo);
    CHECK (judge (call, play ({ 0, -2, -4, -2 }, call.beats, 0, 1.0), major) == C::inversion);
    CHECK (judge (call, play (call.degrees, call.beats, 0, 2.0), major) == C::augmentation);
    CHECK (judge (call, play (call.degrees, call.beats, 0, 0.5), major) == C::diminution);
    CHECK (judge (call, play (call.degrees, call.beats, 2, 1.0), major) == C::sequence);
    CHECK (judge (call, play ({ 0, 2, 4, 7 }, call.beats, 0, 1.0), major) == C::variation);
    CHECK (judge (call, play ({ 7, 0, 9, 4, 0 }, { 0, 1, 2, 3, 4 }, 0, 1.0), major) == C::answer);

    // Transposition and tempo invariance.
    for (int t : { -12, -5, 3, 5, 7, 12 })
        CHECK (judge (call, play (call.degrees, call.beats, t, 1.0), major)
               == (t % 12 == 0 ? C::echo : C::sequence));
    for (double tempo : { 0.85, 1.0, 1.2 })
        CHECK (judge (call, play (call.degrees, call.beats, 0, tempo), major) == C::echo);
    // Rhythm jitter within +-25 % still counts.
    {
        Phrase p = play (call.degrees, call.beats, 0, 1.0);
        p[2].timeSec += 0.1; p[3].timeSec -= 0.05;
        CHECK (judge (call, p, major) == C::echo);
    }

    // Scores.
    for (int c = 0; c < (int) C::count; ++c)
    {
        const float s = scoreFor ((C) c);
        CHECK (s >= 0.0f && s <= 1.0f);
    }
    CHECK (scoreFor (C::silence) == 0.0f && scoreFor (C::noise) == 0.0f);

    // Calls: in mode, length per act, never consecutive duplicates, all modes.
    int dupes = 0;
    for (int m = 0; m < (int) modes().size(); ++m)
    {
        const auto ms = steps (m);
        Director d (1000u + (std::uint64_t) m);
        Motif prev;
        for (int act = 1; act <= 9; ++act)
            for (int i = 0; i < 40; ++i)
            {
                const Motif c = d.nextCall (act, ms);
                const std::size_t want = act <= 2 ? 3 : act <= 5 ? 4 : 5;
                CHECK (c.degrees.size() == want && c.beats.size() == want);
                for (int deg : c.degrees) CHECK (fitsMode (deg, ms));
                if (c.degrees == prev.degrees && c.beats == prev.beats) ++dupes;
                prev = c;
            }
    }
    CHECK (dupes == 0);

    // A call played back verbatim is always an echo.
    {
        Rng r (7);
        for (int act = 1; act <= 9; ++act)
        {
            const Motif c = makeCall (r, act, major);
            CHECK (judge (c, play (c.degrees, c.beats, 0, 1.0), major) == C::echo);
        }
    }

    // Replies and degreeForX stay in mode.
    {
        Rng r (3);
        const Motif rep = makeReply (play ({ 0, 2, 4, 2 }, call.beats, 0, 1.0), C::answer, r, major);
        CHECK (rep.degrees.size() == rep.beats.size() && rep.degrees.size() >= 4);
        for (int deg : rep.degrees) CHECK (fitsMode (deg, major));
        CHECK (makeReply ({}, C::silence, r, major).degrees.empty());
        CHECK (degreeForX (0.0f, major) == 0);
        CHECK (degreeForX (1.0f, major) == 24);
        for (int i = 0; i <= 100; ++i)
        {
            const int d = degreeForX ((float) i / 100.0f, major);
            CHECK (fitsMode (d, major) && d >= 0 && d <= 24);
        }
        CHECK (degreeForX (-3.0f, major) == 0 && degreeForX (9.0f, major) == 24);
    }

    // Techniques bitmask.
    {
        std::uint32_t seen = 0;
        markTechnique (seen, C::echo); markTechnique (seen, C::answer);
        CHECK (hasTechnique (seen, C::echo) && hasTechnique (seen, C::answer) && ! hasTechnique (seen, C::inversion));
        CHECK (seen == ((1u << (int) C::echo) | (1u << (int) C::answer)));
    }

    // Director timing: gap 4-8 min of unpaused play, pauses do not count.
    {
        for (std::uint64_t seed = 1; seed <= 20; ++seed)
        {
            Director d (seed);
            CHECK (d.gapSec() >= 240.0 && d.gapSec() <= 480.0);
            double t = 0, lastDone = 0; int fires = 0;
            for (int i = 0; i < 6000 && fires < 4; ++i)
            {
                t += 1.0;
                const bool paused = (i >= 10 && i < 110);
                if (d.update (t, paused, false))
                {
                    const double unpaused = (t - lastDone) - (fires == 0 ? 100.0 : 0.0);
                    CHECK (unpaused >= 239.0 && unpaused <= 481.0);
                    CHECK (! d.update (t + 1, false, false));   // once per encounter
                    ++fires; t += 1; lastDone = t; d.markDone();
                }
            }
            CHECK (fires == 4);
        }
        Director d (5);
        CHECK (! d.update (10000.0, false, true));   // blocked by another encounter
        CHECK (d.update (10001.0, false, false));
        d.markDone();
        CHECK (! d.update (10002.0, false, false));
    }

    std::printf (g_fail ? "PhantomTest: %d FAILURES\n" : "PhantomTest: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
