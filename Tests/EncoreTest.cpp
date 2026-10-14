// MUTAGEN - Echo Chorale pure-logic tests (no JUCE).
#include "Engine/Encore.h"
#include <cstdio>

using namespace mutagen::story;
using namespace mutagen::story::encore;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void runDemo (Chorale& c)
{
    int guard = 0;
    while (c.state() == State::demo && ++guard < 100000) c.update (0.05);
    CHECK (c.state() == State::listen);
}

static void testFullClear()
{
    Chorale c; c.start (7);
    for (int r = 0; r < kRounds; ++r)
    {
        CHECK (c.phraseLength() == kFirstLength + r);
        runDemo (c);
        const auto phrase = c.notes();
        for (std::size_t i = 0; i < phrase.size(); ++i)
        {
            const auto p = c.press (phrase[i]);
            CHECK (p == (i + 1 == phrase.size() ? Press::roundDone : Press::correct));
        }
        CHECK (c.roundsCleared() == r + 1);
        c.update (kPause + 0.01);
    }
    CHECK (c.state() == State::won && c.finished() && c.lives() == kLives);
    CHECK (pointsFor (c.roundsCleared(), c.lives()) == 400 * kRounds + 2000 * kLives);
}

static void testWrongAndLoss()
{
    Chorale c; c.start (3);
    runDemo (c);
    const auto first = c.notes();
    const int bad = (first[0] + 1) % kLanes;
    CHECK (c.press (bad) == Press::wrong && c.lives() == kLives - 1);
    CHECK (c.state() == State::demo && c.notes() == first);   // same phrase replayed
    runDemo (c);
    CHECK (c.press ((first[0] + 1) % kLanes) == Press::wrong);
    runDemo (c);
    CHECK (c.press ((first[0] + 1) % kLanes) == Press::lostGame);
    CHECK (c.state() == State::lost && c.lives() == 0 && c.finished());
    CHECK (c.press (0) == Press::ignored);
}

static void testTimeoutAndBounds()
{
    Chorale c; c.start (9);
    CHECK (c.press (0) == Press::ignored);         // demo still playing
    runDemo (c);
    CHECK (c.press (-1) == Press::ignored && c.press (kLanes) == Press::ignored);
    c.update (kInputTimeout + 0.1);
    CHECK (c.lives() == kLives - 1 && c.state() == State::demo);
}

static void testDemoSoundsEveryNote()
{
    Chorale c; c.start (11);
    std::vector<int> heard;
    while (c.state() == State::demo)
    {
        c.update (0.02);
        for (int l : c.takeSounded()) heard.push_back (l);
    }
    CHECK (heard == c.notes());
}

static void testDeterminismAndHelpers()
{
    Chorale a, b; a.start (5); b.start (5);
    CHECK (a.notes() == b.notes());
    CHECK (beatSeconds (0) > beatSeconds (11) && beatSeconds (99) >= 0.26);
    const auto s = laneSteps ({ 0, 2, 4, 5, 7, 9, 11 });
    CHECK (s[0] == 0 && s[1] == 2 && s[2] == 5 && s[3] == 9);
    CHECK (laneSteps ({})[3] == 12 && laneSteps ({ 3 })[2] == 3);
    CHECK (std::string (rankFor (0)) == "UNHEARD" && std::string (rankFor (12)) == "MAESTRO");
    CHECK (pointsFor (5, 3) == 2000 && pointsFor (99, 0) == 4800);
}

int main()
{
    testFullClear(); testWrongAndLoss(); testTimeoutAndBounds();
    testDemoSoundsEveryNote(); testDeterminismAndHelpers();
    if (failures == 0) std::printf("All Encore tests passed\n");
    return failures == 0 ? 0 : 1;
}
