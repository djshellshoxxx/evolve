// MUTAGEN - "phreak" key matcher tests (no JUCE).
#include "Engine/Phreak.h"
#include <cstdio>
#include <cstring>

using mutagen::phreak::SequenceMatcher;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool type (SequenceMatcher& m, const char* s, double& t, double step = 0.2)
{
    bool fired = false;
    for (; *s != 0; ++s) { t += step; fired = m.push (*s, t); }
    return fired;
}

int main()
{
    double t = 0.0;
    { SequenceMatcher m; CHECK (type (m, "phreak", t)); }
    { SequenceMatcher m; CHECK (type (m, "PhReAk", t)); }
    { SequenceMatcher m; CHECK (type (m, "xxphreak", t)); }                  // preceded by other keys
    { SequenceMatcher m; CHECK (! type (m, "phreek", t)); }
    { SequenceMatcher m; CHECK (type (m, "pphreak", t)); }                   // a restart on the first letter
    { SequenceMatcher m; CHECK (! type (m, "phrea", t)); CHECK (type (m, "k", t)); }
    { SequenceMatcher m; CHECK (! type (m, "phr", t)); t += 5.0; CHECK (! type (m, "eak", t)); }   // too slow
    { SequenceMatcher m; CHECK (type (m, "phreak", t)); CHECK (! m.push ('k', t + 0.1)); CHECK (type (m, "phreak", t)); }   // re-arms
    { SequenceMatcher m; type (m, "phr", t); m.reset(); CHECK (! type (m, "eak", t)); }
    { SequenceMatcher m (""); CHECK (! m.push ('a', 1.0)); }
    std::printf (failures == 0 ? "All Phreak tests passed\n" : "Phreak tests FAILED\n");
    return failures == 0 ? 0 : 1;
}
