// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Dream Rooms: acoustic spaces whose one altered law of sound is the puzzle.
// Pure standard C++ (no JUCE): room definitions, the state machine, solve
// predicates, per-room audio targets, a smoothing helper and an allocation-free
// DSP class (DreamRoomFx) a processor can run on the post chain.

#include "Storyline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mutagen::story
{
    // Append-only: ids are persisted in Progress.roomsSolved bit positions.
    enum class RoomId : int { combHall, dopplerCarousel, cathedral, standingWave, phaseGarden, count };
    constexpr int kNumRooms = (int) RoomId::count;

    // Append-only.
    enum class RoomPhase : int { idle, entering, exploring, solved, failed, leaving };

    inline const char* roomName (RoomId r)
    {
        constexpr const char* n[] = { "Comb Hall", "Doppler Carousel", "Cathedral of Long Tails",
                                      "Standing-Wave Corridor", "Phase Garden" };
        return n[std::clamp ((int) r, 0, kNumRooms - 1)];
    }

    inline const char* roomLaw (RoomId r)
    {
        constexpr const char* n[] = {
            "The walls are teeth: a comb filter follows you.",
            "The cells orbit, and pitch bends with their speed.",
            "Every note returns late, from far away.",
            "The bass swells and dies as you walk.",
            "Two copies of one tone drift out of step." };
        return n[std::clamp ((int) r, 0, kNumRooms - 1)];
    }

    /** Bounded audio parameters one room asks of the post chain. */
    struct RoomTargets
    {
        float combDelayMs   = 5.0f;    // 0.5 .. 20
        float combFeedback  = 0.0f;    // 0 .. 0.7
        float combMix       = 0.0f;    // 0 .. 0.9 (feed-forward notch depth)
        float dopplerRatio  = 1.0f;    // 0.8 .. 1.25 (pitch ratio)
        float dopplerDepth  = 0.0f;    // 0 .. 1 (wet amount)
        float reverbMacro   = 0.0f;    // 0 .. 1 (tail length and level)
        float lowShelfDb    = 0.0f;    // -15 .. +12 at ~200 Hz
        float phaseOffset   = 0.0f;    // 0 .. 1 (R-channel delay, up to 1 ms)
        float width         = 1.0f;    // 0 .. 1.5

        static RoomTargets baseline() { return {}; }

        void clampAll()
        {
            if (! std::isfinite (combDelayMs + combFeedback + combMix + dopplerRatio + dopplerDepth
                                 + reverbMacro + lowShelfDb + phaseOffset + width))
            {
                *this = baseline();
                return;
            }
            combDelayMs  = std::clamp (combDelayMs, 0.5f, 20.0f);
            combFeedback = std::clamp (combFeedback, 0.0f, 0.7f);
            combMix      = std::clamp (combMix, 0.0f, 0.9f);
            dopplerRatio = std::clamp (dopplerRatio, 0.8f, 1.25f);
            dopplerDepth = std::clamp (dopplerDepth, 0.0f, 1.0f);
            reverbMacro  = std::clamp (reverbMacro, 0.0f, 1.0f);
            lowShelfDb   = std::clamp (lowShelfDb, -15.0f, 12.0f);
            phaseOffset  = std::clamp (phaseOffset, 0.0f, 1.0f);
            width        = std::clamp (width, 0.0f, 1.5f);
        }

        static RoomTargets lerp (const RoomTargets& a, const RoomTargets& b, float t)
        {
            t = std::clamp (t, 0.0f, 1.0f);
            auto m = [t] (float x, float y) { return x + (y - x) * t; };
            RoomTargets r;
            r.combDelayMs = m (a.combDelayMs, b.combDelayMs);    r.combFeedback = m (a.combFeedback, b.combFeedback);
            r.combMix = m (a.combMix, b.combMix);                r.dopplerRatio = m (a.dopplerRatio, b.dopplerRatio);
            r.dopplerDepth = m (a.dopplerDepth, b.dopplerDepth); r.reverbMacro = m (a.reverbMacro, b.reverbMacro);
            r.lowShelfDb = m (a.lowShelfDb, b.lowShelfDb);       r.phaseOffset = m (a.phaseOffset, b.phaseOffset);
            r.width = m (a.width, b.width);
            r.clampAll();
            return r;
        }
    };

    /** One-pole smoother over a whole RoomTargets, advanced per block. */
    class TargetSmoother
    {
    public:
        void reset (const RoomTargets& t = RoomTargets::baseline()) { cur = t; cur.clampAll(); }
        const RoomTargets& current() const { return cur; }

        /** Advance toward `target` by `seconds` with time constant `tauSec`. */
        const RoomTargets& step (const RoomTargets& target, double seconds, double tauSec = 0.05)
        {
            const float a = tauSec <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-std::max (0.0, seconds) / tauSec));
            cur = RoomTargets::lerp (cur, target, a);
            return cur;
        }

    private:
        RoomTargets cur;
    };

    /** Per-frame player input. pos is a normalised 0..1 cursor/arc position. */
    struct RoomInput
    {
        double pos = 0.5;
        bool click = false;
    };

    class DreamRoom
    {
    public:
        static constexpr double kEnterSec = 1.5, kSolvedSec = 1.5, kFailedSec = 1.0, kLeaveSec = 0.8;
        static constexpr double kHintSec = 20.0;
        static constexpr double kTailSec = 2.5;

        DreamRoom (RoomId r, std::uint64_t seed) : room (r), rng (seed * 31u + (std::uint64_t) r + 1u)
        {
            duration = 45.0 + rng.unit() * 30.0;
            combNotchMs = 2.0f + (float) rng.unit() * 14.0f;
            ph0 = (float) (0.3 + rng.unit() * 2.5);
            driftSeed = rng.unit() * 6.28;
            for (int i = 0; i < 3; ++i) cellPhase[(size_t) i] = (float) (rng.unit() * 6.28);
        }

        RoomId id() const { return room; }
        RoomPhase phase() const { return ph; }
        bool solved() const { return wasSolved; }
        double exploreTime() const { return exploreClock; }
        double timeLimit() const { return duration; }
        std::uint32_t solvedMaskBit() const { return wasSolved ? (1u << (int) room) : 0u; }
        bool finished() const { return ph == RoomPhase::idle && started; }

        void enter()
        {
            if (ph != RoomPhase::idle || started) return;
            started = true; ph = RoomPhase::entering; phaseClock = 0.0;
        }

        /** Esc: leave immediately (crossfade out). */
        void exit()
        {
            if (ph == RoomPhase::idle || ph == RoomPhase::leaving) return;
            leaveFrom = blend();
            ph = RoomPhase::leaving; phaseClock = 0.0;
        }

        bool hintActive() const { return ph == RoomPhase::exploring && sinceProgress >= kHintSec; }

        /** 0..1 weight of the room's processing (entering ramps up, leaving ramps down). */
        float blend() const
        {
            switch (ph)
            {
                case RoomPhase::entering: return (float) std::clamp (phaseClock / kEnterSec, 0.0, 1.0);
                case RoomPhase::leaving:  return leaveFrom * (float) (1.0 - std::clamp (phaseClock / kLeaveSec, 0.0, 1.0));
                case RoomPhase::idle:     return 0.0f;
                default:                  return 1.0f;
            }
        }

        // ---- solve helpers (also drive the hint glow) ----
        float combNotchDelayMs() const { return combNotchMs; }
        static float posToDelayMs (double pos) { return 0.5f + (float) std::clamp (pos, 0.0, 1.0) * 19.5f; }
        double combSolvePos() const { return (combNotchMs - 0.5) / 19.5; }
        float cellRatio (int i, double t) const
        {
            return 1.0f + kDopplerAmp * (float) std::sin (kCellW[(size_t) i] * t + cellPhase[(size_t) i]);
        }
        float cellRatio (int i) const { return cellRatio (i, exploreClock); }
        bool cellCaught (int i) const { return caught[(size_t) i]; }
        int caughtCount() const { return (int) std::count (caught.begin(), caught.end(), true); }
        int notesPlayed() const { return notes; }
        double tailRemaining() const { return tailLeft; }
        float standingLevel (double p) const
        {
            return 0.5f + 0.5f * (float) std::cos (4.0 * kPi * p + ph0);
        }
        /** Extremum positions: [0],[1] loudest, [2],[3] quietest. */
        std::array<double, 4> standingSpots() const
        {
            return { (2 * kPi - ph0) / (4 * kPi), (4 * kPi - ph0) / (4 * kPi),
                     (kPi - ph0) / (4 * kPi), (3 * kPi - ph0) / (4 * kPi) };
        }
        int spotsFound() const { return (int) std::count (spotDone.begin(), spotDone.end(), true); }
        double gardenDrift (double t) const { return 0.5 + 0.3 * std::sin (0.35 * t + driftSeed); }
        double gardenDriftNow() const { return gardenDrift (exploreClock); }
        double holdTime() const { return hold; }
        float lastLevel() const { return level; }

        void update (double dt, const RoomInput& in)
        {
            dt = std::clamp (dt, 0.0, 0.5);
            pos = std::clamp (in.pos, 0.0, 1.0);
            phaseClock += dt;
            switch (ph)
            {
                case RoomPhase::entering:
                    if (phaseClock >= kEnterSec) { ph = RoomPhase::exploring; phaseClock = 0.0; }
                    break;
                case RoomPhase::exploring:
                    exploreClock += dt; sinceProgress += dt;
                    if (tailLeft > 0) tailLeft = std::max (0.0, tailLeft - dt);
                    explore (dt, in);
                    if (ph == RoomPhase::exploring && exploreClock >= duration)
                    { ph = RoomPhase::failed; phaseClock = 0.0; }
                    break;
                case RoomPhase::solved:
                    if (phaseClock >= kSolvedSec) beginLeave();
                    break;
                case RoomPhase::failed:
                    if (phaseClock >= kFailedSec) beginLeave();
                    break;
                case RoomPhase::leaving:
                    if (phaseClock >= kLeaveSec) { ph = RoomPhase::idle; phaseClock = 0.0; }
                    break;
                default: break;
            }
        }

        /** Room targets at full strength for the current state. */
        RoomTargets roomTargets() const
        {
            RoomTargets t;
            switch (room)
            {
                case RoomId::combHall:
                    t.combDelayMs = posToDelayMs (pos); t.combFeedback = 0.3f; t.combMix = 0.85f; break;
                case RoomId::dopplerCarousel:
                {
                    int best = 0; float bd = -1;
                    for (int i = 0; i < 3; ++i)
                    {
                        const float d = std::abs (cellRatio (i) - 1.0f) + (caught[(size_t) i] ? 0.0f : 1.0f);
                        if (d > bd) { bd = d; best = i; }
                    }
                    t.dopplerRatio = cellRatio (best); t.dopplerDepth = 1.0f; break;
                }
                case RoomId::cathedral:
                    t.reverbMacro = 0.85f + 0.15f * (float) (tailLeft / kTailSec); break;
                case RoomId::standingWave:
                    t.lowShelfDb = 12.0f * (2.0f * standingLevel (pos) - 1.0f); break;
                case RoomId::phaseGarden:
                    t.phaseOffset = (float) circDist (pos, gardenDriftNow()) * 2.0f; break;
                default: break;
            }
            t.clampAll();
            return t;
        }

        /** What the DSP should chase: baseline crossfaded to the room by blend(). */
        RoomTargets targets() const
        {
            return RoomTargets::lerp (RoomTargets::baseline(), roomTargets(), blend());
        }

    private:
        static constexpr double kPi = 3.14159265358979323846;
        static constexpr float kDopplerAmp = 0.06f;
        static constexpr double kCellW[3] = { 1.2566370614, 0.9666438934, 0.7853981634 };

        static double circDist (double a, double b)
        {
            double d = std::fabs (a - b);
            d -= std::floor (d);
            return std::min (d, 1.0 - d);
        }

        void progress() { sinceProgress = 0.0; }
        void solve() { wasSolved = true; ph = RoomPhase::solved; phaseClock = 0.0; }
        void beginLeave() { leaveFrom = 1.0f; ph = RoomPhase::leaving; phaseClock = 0.0; }

        void explore (double dt, const RoomInput& in)
        {
            switch (room)
            {
                case RoomId::combHall:
                {
                    const bool on = std::fabs (posToDelayMs (pos) - combNotchMs) <= 0.6f;
                    level = on ? 0.0f : 1.0f;   // hum cell level
                    if (on) { if (hold <= 0.0) progress(); hold += dt; } else hold = 0.0;
                    if (hold >= 2.0) solve();
                    break;
                }
                case RoomId::dopplerCarousel:
                    if (in.click)
                        for (int i = 0; i < 3; ++i)
                            if (! caught[(size_t) i] && cellRatio (i) >= 1.0f + kDopplerAmp * 0.92f)
                            { caught[(size_t) i] = true; progress(); break; }
                    if (caughtCount() >= 3) solve();
                    break;
                case RoomId::cathedral:
                    if (in.click && tailLeft <= 0.0) { ++notes; tailLeft = kTailSec; progress(); }
                    if (notes >= 8) solve();
                    break;
                case RoomId::standingWave:
                    if (in.click)
                    {
                        const auto s = standingSpots();
                        for (size_t i = 0; i < 4; ++i)
                            if (! spotDone[i] && std::fabs (pos - s[i]) <= 0.04) { spotDone[i] = true; progress(); }
                    }
                    if (spotsFound() >= 4) solve();
                    break;
                case RoomId::phaseGarden:
                {
                    const double d = circDist (pos, gardenDriftNow());
                    level = (float) std::pow (std::cos (kPi * d), 2.0);
                    if (d <= 0.03) { if (hold <= 0.0) progress(); hold += dt; } else hold = 0.0;
                    if (hold >= 3.0) solve();
                    break;
                }
                default: break;
            }
        }

        RoomId room;
        story::Rng rng;
        RoomPhase ph = RoomPhase::idle;
        bool started = false, wasSolved = false;
        double phaseClock = 0.0, exploreClock = 0.0, sinceProgress = 0.0, duration = 60.0;
        double pos = 0.5, hold = 0.0, tailLeft = 0.0, driftSeed = 0.0;
        float combNotchMs = 5.0f, ph0 = 1.0f, level = 1.0f, leaveFrom = 1.0f;
        int notes = 0;
        std::array<float, 3> cellPhase {};
        std::array<bool, 3> caught {};
        std::array<bool, 4> spotDone {};
    };

    // ------------------------------------------------------------------------
    /** Allocation-free stereo effect applying RoomTargets. prepare() allocates;
        process() never does. Output is finite and soft-clipped below 1.0. */
    class DreamRoomFx
    {
    public:
        void prepare (double sampleRate, int /*maxBlock*/)
        {
            sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
            for (auto* d : { &combL, &combR, &dopL, &dopR, &phaseR, &tailL, &tailR })
                d->init ((int) (sr * 0.25) + 8);
            smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
            reset (RoomTargets::baseline());
        }

        void reset (const RoomTargets& t)
        {
            cur = t; cur.clampAll();
            for (auto* d : { &combL, &combR, &dopL, &dopR, &phaseR, &tailL, &tailR }) d->clear();
            dopDelay = (float) (0.02 * sr);
            for (auto& s : shelf) s = {};
            sc = {};
            shelfDb = -1000.0f;
        }

        void process (float* L, float* R, int n, const RoomTargets& targetIn)
        {
            if (L == nullptr || R == nullptr || combL.buf.empty()) return;
            RoomTargets tg = targetIn;
            tg.clampAll();
            const float centre = (float) (0.02 * sr), maxD = (float) (0.04 * sr), minD = 2.0f;
            for (int i = 0; i < n; ++i)
            {
                cur.combDelayMs  += smoothCoef * (tg.combDelayMs  - cur.combDelayMs);
                cur.combFeedback += smoothCoef * (tg.combFeedback - cur.combFeedback);
                cur.combMix      += smoothCoef * (tg.combMix      - cur.combMix);
                cur.dopplerRatio += smoothCoef * (tg.dopplerRatio - cur.dopplerRatio);
                cur.dopplerDepth += smoothCoef * (tg.dopplerDepth - cur.dopplerDepth);
                cur.reverbMacro  += smoothCoef * (tg.reverbMacro  - cur.reverbMacro);
                cur.lowShelfDb   += smoothCoef * (tg.lowShelfDb   - cur.lowShelfDb);
                cur.phaseOffset  += smoothCoef * (tg.phaseOffset  - cur.phaseOffset);
                cur.width        += smoothCoef * (tg.width        - cur.width);

                if ((i & 31) == 0) updateShelf (cur.lowShelfDb);

                float l = clean (L[i]), r = clean (R[i]);

                // Doppler-ish: delay slews at (1 - ratio) samples per sample, gently recentred.
                dopDelay += (1.0f - cur.dopplerRatio) + 0.0005f * (centre - dopDelay);
                dopDelay = std::clamp (dopDelay, minD, maxD);
                dopL.write (l); dopR.write (r);
                const float dl = dopL.read (dopDelay), dr = dopR.read (dopDelay);
                l += cur.dopplerDepth * (dl - l);
                r += cur.dopplerDepth * (dr - r);

                // Comb notch: y = x - mix * x[n-d], with a feedback tap for ringing.
                const float cd = std::clamp (cur.combDelayMs * 0.001f * (float) sr, 1.0f, (float) (0.021 * sr));
                const float cl = combL.read (cd), cr = combR.read (cd);
                combL.write (l + cur.combFeedback * cl);
                combR.write (r + cur.combFeedback * cr);
                l -= cur.combMix * cl;
                r -= cur.combMix * cr;

                // Long tail: two feedback delays whose level follows the macro.
                const float m = cur.reverbMacro;
                if (m > 0.0001f)
                {
                    const float fb = 0.2f + 0.55f * m;
                    const float tl = tailL.read ((float) (0.097 * sr)), tr = tailR.read ((float) (0.113 * sr));
                    tailL.write (l + fb * tl); tailR.write (r + fb * tr);
                    l += 0.5f * m * tl; r += 0.5f * m * tr;
                }
                else { tailL.write (0.0f); tailR.write (0.0f); }

                l = shelf[0].run (l, sc); r = shelf[1].run (r, sc);

                // Phase offset: R lags by up to 1 ms. Width via mid/side.
                phaseR.write (r);
                r = phaseR.read (cur.phaseOffset * 0.001f * (float) sr);
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * cur.width;
                l = mid + side; r = mid - side;

                L[i] = softClip (l); R[i] = softClip (r);
            }
        }

    private:
        struct Delay
        {
            std::vector<float> buf;
            int mask = 0, w = 0;
            void init (int minSize)
            {
                int s = 16; while (s < minSize) s <<= 1;
                buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0;
            }
            void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
            void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
            // delay in samples (>= 0), linear interpolation; 0 returns the newest written sample.
            float read (float d) const
            {
                d = std::clamp (d, 0.0f, (float) mask - 2.0f);
                const int di = (int) d; const float f = d - (float) di;
                const float a = buf[(size_t) ((w - 1 - di) & mask)];
                const float b = buf[(size_t) ((w - 2 - di) & mask)];
                return a + f * (b - a);
            }
        };

        struct Coef { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
        struct Biquad
        {
            float z1 = 0, z2 = 0;
            float run (float x, const Coef& c)
            {
                const float y = c.b0 * x + z1;
                z1 = c.b1 * x - c.a1 * y + z2;
                z2 = c.b2 * x - c.a2 * y;
                return y;
            }
        };

        static float clean (float x) { return std::isfinite (x) ? std::clamp (x, -4.0f, 4.0f) : 0.0f; }
        static float softClip (float x) { return std::isfinite (x) ? std::tanh (x) * 0.98f : 0.0f; }

        void updateShelf (float db)
        {
            if (std::fabs (db - shelfDb) < 0.01f) return;
            shelfDb = db;
            const double A = std::pow (10.0, db / 40.0), w0 = 2.0 * 3.14159265358979 * 200.0 / sr;
            const double cw = std::cos (w0), sw = std::sin (w0), alpha = sw / 2.0 * std::sqrt (2.0);
            const double tA = 2.0 * std::sqrt (A) * alpha;
            const double b0 = A * ((A + 1) - (A - 1) * cw + tA), b1 = 2 * A * ((A - 1) - (A + 1) * cw),
                         b2 = A * ((A + 1) - (A - 1) * cw - tA), a0 = (A + 1) + (A - 1) * cw + tA,
                         a1 = -2 * ((A - 1) + (A + 1) * cw), a2 = (A + 1) + (A - 1) * cw - tA;
            sc = { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
        }

        double sr = 44100.0;
        float smoothCoef = 0.001f, dopDelay = 0.0f, shelfDb = -1000.0f;
        RoomTargets cur;
        Delay combL, combR, dopL, dopR, phaseR, tailL, tailR;
        std::array<Biquad, 2> shelf {};
        Coef sc;
    };
}
