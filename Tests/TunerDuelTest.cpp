// MUTAGEN - Tuner Duel pure-logic tests (no JUCE).
#include "Engine/TunerDuel.h"
#include <cmath>
#include <cstdio>

using namespace mutagen::story;
using namespace mutagen::story::duel;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Plays a duel through to the resolve phase, driven purely by update().
static void toResolve (Duel& d)
{
    d.update (Duel::quantiseSeconds + 0.01);
    d.update (Duel::tensionSeconds + 0.01);
    CHECK (d.state() == State::resolve);
}

// Returns the first accepted pitch for a given unstable cell, from the library's own table.
static int correctPitch (const Duel& d, int cell)
{
    for (const auto& r : resolutionsFor (d.round().chord, d.round().mode))
        if (r.tone == cell && r.count > 0) return r.accepted[0];
    return -1;
}

static void testStartsInQuantise()
{
    Duel d;
    bool used = false;
    d.start (1234, 1, 0, 48);
    CHECK (d.state() == State::quantise);
    CHECK (d.roundCount() == 1);
    CHECK (d.ignore (used));           // ignoring is allowed once
    CHECK (d.state() == State::skipped);
    CHECK (used);
    CHECK (! d.ignore (used));         // and never twice in a run
}

static void testWinPath()
{
    Duel d;
    d.start (42, 1, 0, 48, 0);
    toResolve (d);
    for (int c = 0; c < d.cellCount(); ++c)
    {
        if (d.round().chord.kind[(std::size_t) c] == Tension::stable) continue;
        const int p = correctPitch (d, c);
        CHECK (p >= 0);
        CHECK (d.dropPitch (c, p) == DropResult::correct);
    }
    CHECK (d.state() == State::cadence);
    d.update (Duel::cadenceSeconds + 0.01);
    CHECK (d.state() == State::won);
    CHECK (d.finished());
    CHECK (d.rewardRelic() >= 0);
    CHECK (hasRelic (d.relicMask(), d.rewardRelic()));
}

static void testWrongDropsLose()
{
    Duel d;
    d.start (7, 1, 0, 48, 0);
    toResolve (d);
    int wrongs = 0;
    int cell = -1;
    for (int c = 0; c < d.cellCount(); ++c)
        if (d.grab (c)) { cell = c; break; }
    CHECK (cell >= 0);
    // A pitch this cell does not accept (the library's own accepted list decides).
    int bad = -1;
    for (int off = 1; off < 12 && bad < 0; ++off)
    {
        const int cand = d.round().chord.pitch[(std::size_t) cell] + off;
        bool ok = true;
        for (const auto& r : resolutionsFor (d.round().chord, d.round().mode))
            if (r.tone == cell)
                for (int i = 0; i < r.count; ++i) if (r.accepted[(std::size_t) i] == cand) ok = false;
        if (ok) bad = cand;
    }
    CHECK (bad >= 0);
    for (int i = 0; i < triesPerRound && d.state() == State::resolve; ++i)
    {
        CHECK (d.dropPitch (cell, bad) == DropResult::wrong);
        ++wrongs;
    }
    CHECK (wrongs == triesPerRound);
    CHECK (d.state() == State::lost);
    CHECK (d.sterileRemaining() > 0.0);
}

static void testTimeoutLoses()
{
    Duel d;
    d.start (99, 1, 0, 48, 0);
    toResolve (d);
    d.update (d.difficulty().resolveSeconds + 1.0);
    CHECK (d.state() == State::lost);
}

static void testDeterministicPerSeed()
{
    Duel a, b;
    a.start (555, 5, 2, 50);
    b.start (555, 5, 2, 50);
    CHECK (a.roundCount() == b.roundCount());
    CHECK (a.round().chord.pitch[0] == b.round().chord.pitch[0]);
    CHECK (a.round().unstable == b.round().unstable);
}

static void testDifficultyByAct()
{
    CHECK (difficultyFor (1).tier == 1);
    CHECK (difficultyFor (4).tier == 2);
    CHECK (difficultyFor (9).tier == 3);
    CHECK (difficultyFor (9).rounds >= difficultyFor (1).rounds);
}

static void testRelicsBoundedAndPersistent()
{
    std::uint32_t m = 0;
    for (int i = 0; i < relicCount; ++i) m = withRelic (m, i);
    CHECK (relicsOwned (m) == relicCount);
    CHECK (relicsOwned (withRelic (m, 0)) == relicCount);   // idempotent
    CHECK (std::abs (centsForDegree (0, 0)) < 1e-6f);        // no relic, no detune
}

static void testNormalisedPitchAxis()
{
    Duel d;
    d.start (3, 1, 0, 48, 0);
    CHECK (d.pitchToX (d.xToPitch (0.0f)) <= 0.05f);
    CHECK (d.pitchToX (d.xToPitch (1.0f)) >= 0.95f);
    CHECK (d.xToPitch (0.5f) >= d.pitchLow());
}

int main()
{
    testStartsInQuantise();
    testWinPath();
    testWrongDropsLose();
    testTimeoutLoses();
    testDeterministicPerSeed();
    testDifficultyByAct();
    testRelicsBoundedAndPersistent();
    testNormalisedPitchAxis();

    if (failures == 0) std::printf ("TunerDuelTest: all passed\n");
    else std::printf ("TunerDuelTest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
