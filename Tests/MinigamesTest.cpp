// MUTAGEN - Pulse and Hunt pure-logic tests (no JUCE).
#include "Engine/Minigames.h"
#include <cstdio>

using namespace mutagen::story::mini;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void advanceTo (Pulse& p, double t) { while (p.clockNow() < t) p.update (0.001); }

static void testPulsePerfect()
{
    Pulse p; p.start (4);
    for (int i = 0; i < Pulse::kBeats; ++i) { advanceTo (p, p.beatTime (i)); p.tap(); }
    CHECK (p.hits() == Pulse::kBeats && p.won());
    p.update (5.0);
    CHECK (p.finished());
}

static void testPulseMissesAndWindow()
{
    Pulse p; p.start (4);
    CHECK (p.tap() > 1.0e8);                          // nothing near a beat yet
    advanceTo (p, p.beatTime (0) + 0.2);
    const double err = p.tap();                       // inside the window, outside the hit zone
    CHECK (err > 0.1 && p.hits() == 0);
    CHECK (p.tap() > 1.0e8);                          // a beat can only be judged once
    p.update (30.0);
    CHECK (p.finished() && ! p.won());
}

static void testPulseSoundAndPhase()
{
    Pulse p; p.start (9);
    int sounded = 0;
    while (! p.finished()) { p.update (0.01); sounded += (int) p.takeSounded().size(); }
    CHECK (sounded == Pulse::kAudible);               // the rest is silent: keep time yourself
    CHECK (p.phase() >= 0.0 && p.phase() < 1.0);
    Pulse q; q.start (9);
    CHECK (q.beatSeconds() == p.beatSeconds() && q.beatSeconds() >= 0.5 && q.beatSeconds() <= 0.8);
}

static void testHunt()
{
    Hunt h; h.start (3, 1);
    CHECK (h.consumePlay() && ! h.consumePlay());
    int rounds = 0;
    while (! h.finished() && rounds++ < 20)
    {
        const auto& o = h.options();
        int hits = 0;
        for (int v : o) hits += (v == h.answerSemitones());
        CHECK (hits == 1);                            // exactly one right answer, no duplicates
        for (std::size_t a = 0; a < o.size(); ++a) for (std::size_t b = a + 1; b < o.size(); ++b) CHECK (o[a] != o[b]);
        h.replay(); CHECK (h.consumePlay());
        int pick = 0; for (int i = 0; i < Hunt::kOptions; ++i) if (o[(std::size_t) i] == h.answerSemitones()) pick = i;
        CHECK (h.answer (pick));
    }
    CHECK (h.finished() && h.won() && h.correctCount() == Hunt::kQuestions);
    CHECK (! h.answer (0));                           // nothing after the end
}

static void testHuntLoses()
{
    Hunt h; h.start (8, 9);
    while (! h.finished())
    {
        int wrong = 0; for (int i = 0; i < Hunt::kOptions; ++i) if (h.options()[(std::size_t) i] != h.answerSemitones()) wrong = i;
        CHECK (! h.answer (wrong));
    }
    CHECK (! h.won() && h.correctCount() == 0);
    CHECK (std::string (intervalName (7)) == "PERFECT 5TH" && std::string (intervalName (99)) == "?");
}

static void testSeek()
{
    Seek s; s.start (5);
    CHECK (Seek::pingInterval (0.0) < Seek::pingInterval (1.0));
    int pings = 0;
    for (int i = 0; i < 100; ++i) pings += s.update (0.01, s.target()) ? 1 : 0;
    CHECK (pings >= 5 && s.lastDistance() == 0.0);
    CHECK (s.lock (s.target()) && s.foundCount() == 1 && s.roundNumber() == 2);
    CHECK (! s.lock (s.target() < 0.5 ? 0.99 : 0.01));
    s.update (Seek::kTimeLimit + 0.1, 0.5);          // timeout spends the last round
    CHECK (s.finished() && s.foundCount() == 1 && ! s.won());
    CHECK (! s.lock (0.5));
}

static void testFlash()
{
    Flash f; f.start (2);
    CHECK (f.consumePlay() && ! f.consumePlay());
    for (int i = 0; i < Flash::kQuestions; ++i)
    {
        CHECK (! f.finished());
        f.answer ((int) f.answerQuality());
        if (! f.finished()) CHECK (f.consumePlay());
    }
    CHECK (f.finished() && f.won() && f.correctCount() == Flash::kQuestions);
    Flash g; g.start (2);
    for (int i = 0; i < Flash::kQuestions; ++i) g.update (Flash::kPerQuestion + 0.1);   // every one times out
    CHECK (g.finished() && ! g.won() && g.correctCount() == 0);
    CHECK (std::string (qualityName (Quality::augmented)) == "AUGMENTED" && qualityTones (Quality::minor)[1] == 3);
    CHECK (! g.answer (0) && ! f.answer (9));
}

int main()
{
    testPulsePerfect(); testPulseMissesAndWindow(); testPulseSoundAndPhase(); testHunt(); testHuntLoses(); testSeek(); testFlash();
    if (failures == 0) std::printf("All Minigames tests passed\n");
    return failures == 0 ? 0 : 1;
}
