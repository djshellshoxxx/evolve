// MUTAGEN - Dream Rooms pure-logic and offline DSP tests (no JUCE).
#include "Engine/DreamRooms.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace mutagen::story;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Enter, wait for exploring, then step with `drive` (returns the input for each step).
template <typename Drive>
static bool runRoom (RoomId id, std::uint64_t seed, Drive drive, double maxSec = 200.0)
{
    DreamRoom r (id, seed);
    r.enter();
    const double dt = 0.05;
    for (double t = 0.0; t < maxSec && ! r.solved(); t += dt)
    {
        if (r.phase() == RoomPhase::exploring) r.update (dt, drive (r, t));
        else r.update (dt, RoomInput {});
        if (r.phase() == RoomPhase::failed || r.phase() == RoomPhase::idle) break;
    }
    return r.solved();
}

static void testCombHallSolvable()
{
    for (std::uint64_t seed = 1; seed <= 5; ++seed)
    {
        const bool ok = runRoom (RoomId::combHall, seed, [] (DreamRoom& r, double) {
            return RoomInput { r.combSolvePos(), false };
        });
        CHECK (ok);
    }
}

static void testDopplerSolvable()
{
    // Clicking on a steady rhythm catches each cell as its ratio swings past the threshold.
    for (std::uint64_t seed = 1; seed <= 5; ++seed)
    {
        const bool ok = runRoom (RoomId::dopplerCarousel, seed, [] (DreamRoom&, double t) {
            return RoomInput { 0.5, std::fmod (t, 0.5) < 0.05 };
        }, 400.0);
        CHECK (ok);
    }
}

static void testCathedralSolvable()
{
    for (std::uint64_t seed = 1; seed <= 3; ++seed)
    {
        // A click every 3 s clears the 2.5 s tail, so eight notes land.
        const bool ok = runRoom (RoomId::cathedral, seed, [] (DreamRoom&, double t) {
            return RoomInput { 0.5, std::fmod (t, 3.0) < 0.05 };
        }, 200.0);
        CHECK (ok);
    }
}

static void testStandingWaveSolvable()
{
    // Sweep the position and click at every step: the four spots are hit as the sweep passes them.
    for (std::uint64_t seed = 1; seed <= 3; ++seed)
    {
        const bool ok = runRoom (RoomId::standingWave, seed, [] (DreamRoom&, double t) {
            const double p = std::fmod (t * 0.2, 1.0);
            return RoomInput { p, true };
        }, 200.0);
        CHECK (ok);
    }
}

static void testPhaseGardenSolvable()
{
    for (std::uint64_t seed = 1; seed <= 3; ++seed)
    {
        const bool ok = runRoom (RoomId::phaseGarden, seed, [] (DreamRoom& r, double) {
            return RoomInput { r.gardenDriftNow(), false };
        }, 200.0);
        CHECK (ok);
    }
}

static void testHintAndFailure()
{
    DreamRoom r (RoomId::combHall, 9);
    r.enter();
    for (double t = 0.0; t < 2.0; t += 0.05) r.update (0.05, RoomInput { 0.0, false });
    CHECK (r.phase() == RoomPhase::exploring);
    CHECK (! r.hintActive());
    // No progress for 20 s: the hint must appear.
    for (double t = 0.0; t < DreamRoom::kHintSec + 1.0; t += 0.05) r.update (0.05, RoomInput { 0.0, false });
    CHECK (r.hintActive());
    // Never solving fails the room once the time limit passes.
    for (double t = 0.0; t < r.timeLimit() + 5.0 && r.phase() != RoomPhase::failed; t += 0.05)
        r.update (0.05, RoomInput { 0.0, false });
    CHECK (r.phase() == RoomPhase::failed || r.phase() == RoomPhase::leaving || r.phase() == RoomPhase::idle);
    CHECK (! r.solved());
}

static void testExitRevertsAndFinishes()
{
    DreamRoom r (RoomId::cathedral, 2);
    r.enter();
    for (double t = 0.0; t < 2.0; t += 0.05) r.update (0.05, RoomInput {});
    r.exit();
    CHECK (r.phase() == RoomPhase::leaving);
    for (double t = 0.0; t < 2.0; t += 0.05) r.update (0.05, RoomInput {});
    CHECK (r.finished());
}

static void testTargetsBoundedAndDeterministic()
{
    DreamRoom a (RoomId::standingWave, 77), b (RoomId::standingWave, 77);
    CHECK (std::fabs (a.combNotchDelayMs() - b.combNotchDelayMs()) < 1e-6f);
    const auto ta = a.targets();
    CHECK (ta.lowShelfDb >= -15.0f && ta.lowShelfDb <= 12.0f);
    CHECK (ta.dopplerRatio >= 0.8f && ta.dopplerRatio <= 1.25f);
}

// Offline render: every room through the DSP stays finite and below full scale.
static void testOfflineRenderBounded()
{
    const double sr = 48000.0;
    const int n = (int) (sr * 3.0), block = 256;
    for (int rid = 0; rid < (int) RoomId::count; ++rid)
    {
        DreamRoomFx fx;
        fx.prepare (sr, block);
        DreamRoom room ((RoomId) rid, 5);
        room.enter();

        std::vector<float> L ((std::size_t) n), R ((std::size_t) n);
        std::uint32_t seed = 12345u + (std::uint32_t) rid;
        double phase = 0.0;
        bool finite = true;
        float peak = 0.0f;
        for (int i = 0; i < n; i += block)
        {
            const int m = std::min (block, n - i);
            for (int k = 0; k < m; ++k)
            {
                seed = seed * 1664525u + 1013904223u;
                const float noise = ((float) (seed >> 8) / 8388608.0f - 1.0f) * 0.3f;
                const float tone = 0.4f * (float) std::sin (phase);
                phase += 2.0 * 3.14159265358979 * 220.0 / sr;
                L[(std::size_t) (i + k)] = tone + noise;
                R[(std::size_t) (i + k)] = 0.8f * tone - noise;
            }
            room.update (block / sr, RoomInput { 0.5, false });
            fx.process (L.data() + i, R.data() + i, m, room.targets());
            for (int k = 0; k < m; ++k)
            {
                const float a = std::fabs (L[(std::size_t) (i + k)]), b = std::fabs (R[(std::size_t) (i + k)]);
                if (! std::isfinite (a) || ! std::isfinite (b)) finite = false;
                peak = std::max (peak, std::max (a, b));
            }
        }
        CHECK (finite);
        CHECK (peak <= 1.0f);
    }
}

int main()
{
    testCombHallSolvable();
    testDopplerSolvable();
    testCathedralSolvable();
    testStandingWaveSolvable();
    testPhaseGardenSolvable();
    testHintAndFailure();
    testExitRevertsAndFinishes();
    testTargetsBoundedAndDeterministic();
    testOfflineRenderBounded();

    if (failures == 0) std::printf ("DreamRoomsTest: all passed\n");
    else std::printf ("DreamRoomsTest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
