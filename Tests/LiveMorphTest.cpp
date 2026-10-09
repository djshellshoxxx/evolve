#include "../Source/Engine/LiveMorph.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

using namespace mutagen;

#define CHECK(condition) do { if (! (condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " << #condition << '\n'; \
    return 1; \
} } while (false)

// ---- allocation counter: every global operator new in the test binary is counted

static std::size_t gAllocCount = 0;

void* operator new (std::size_t n)
{
    ++gAllocCount;
    if (void* p = std::malloc (n ? n : 1))
        return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t n)
{
    ++gAllocCount;
    if (void* p = std::malloc (n ? n : 1))
        return p;
    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

// ---- signal helpers

static constexpr double kSr = 48000.0;

struct Stereo
{
    std::vector<float> l, r;
};

static Stereo sine (double freq, double secs, float amp)
{
    Stereo s;
    const std::size_t n = (std::size_t) (secs * kSr);
    s.l.resize (n);
    s.r.resize (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const float v = amp * (float) std::sin (2.0 * 3.14159265358979 * freq * (double) i / kSr);
        s.l[i] = v;
        s.r[i] = v * 0.9f;
    }
    return s;
}

static Stereo whiteNoise (double secs, float amp, std::uint32_t seed)
{
    Stereo s;
    const std::size_t n = (std::size_t) (secs * kSr);
    s.l.resize (n);
    s.r.resize (n);
    std::uint32_t x = seed * 2654435761u + 1u;
    auto next = [&x]() {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return (float) ((double) x / 4294967296.0 * 2.0 - 1.0);
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        s.l[i] = amp * next();
        s.r[i] = amp * next();
    }
    return s;
}

// Pink-ish noise: Paul Kellet's three-pole approximation of 1/f.
static Stereo pinkNoise (double secs, float amp, std::uint32_t seed)
{
    Stereo s;
    const std::size_t n = (std::size_t) (secs * kSr);
    s.l.resize (n);
    s.r.resize (n);
    std::uint32_t x = seed * 2246822519u + 7u;
    auto white = [&x]() {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return (float) ((double) x / 4294967296.0 * 2.0 - 1.0);
    };
    float b0 = 0, b1 = 0, b2 = 0, c0 = 0, c1 = 0, c2 = 0;
    auto pink = [&](float w, float& a0, float& a1, float& a2) {
        a0 = 0.99765f * a0 + w * 0.0990460f;
        a1 = 0.96300f * a1 + w * 0.2965164f;
        a2 = 0.57000f * a2 + w * 1.0526913f;
        return (a0 + a1 + a2 + w * 0.1848f) * 0.25f;
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        s.l[i] = amp * pink (white(), b0, b1, b2);
        s.r[i] = amp * pink (white(), c0, c1, c2);
    }
    return s;
}

// Drum-like clicks: a 200 Hz body decaying over 25 ms, one per period, starting at 0.1 s.
static Stereo clickTrain (double secs, double period, float amp)
{
    Stereo s;
    const std::size_t n = (std::size_t) (secs * kSr);
    s.l.assign (n, 0.0f);
    s.r.assign (n, 0.0f);
    const std::size_t hit = (std::size_t) (0.1 * kSr);
    const std::size_t step = (std::size_t) (period * kSr);
    const std::size_t len = (std::size_t) (0.025 * kSr);
    for (std::size_t start = hit; start < n; start += step)
    {
        for (std::size_t j = 0; j < len && start + j < n; ++j)
        {
            const double t = (double) j / kSr;
            const float v = amp * (float) (std::sin (2.0 * 3.14159265358979 * 200.0 * t) * std::exp (-t * 150.0));
            s.l[start + j] += v;
            s.r[start + j] += v;
        }
    }
    return s;
}

struct Out
{
    std::vector<float> l, r;
};

static Out run (LiveMorph& lm, const Stereo& in, int block)
{
    Out o;
    o.l.assign (in.l.size(), 0.0f);
    o.r.assign (in.r.size(), 0.0f);
    std::size_t pos = 0;
    while (pos < in.l.size())
    {
        const int n = (int) std::min<std::size_t> ((std::size_t) block, in.l.size() - pos);
        lm.process (in.l.data() + pos, in.r.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        pos += (std::size_t) n;
    }
    return o;
}

static double rmsOf (const std::vector<float>& a, const std::vector<float>& b)
{
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
        s += 0.5 * ((double) a[i] * a[i] + (double) b[i] * b[i]);
    return std::sqrt (s / (double) a.size());
}

static double maxAbsOf (const std::vector<float>& a)
{
    double m = 0.0;
    for (float v : a)
        m = std::max (m, (double) std::fabs (v));
    return m;
}

static double maxDiff (const Out& a, const Out& b, std::size_t from = 0)
{
    double m = 0.0;
    for (std::size_t i = from; i < a.l.size(); ++i)
    {
        m = std::max (m, (double) std::fabs (a.l[i] - b.l[i]));
        m = std::max (m, (double) std::fabs (a.r[i] - b.r[i]));
    }
    return m;
}

// Normalised difference: RMS of (a - b) over RMS of b.
static double relDiff (const Out& a, const Out& b, std::size_t from = 0)
{
    double num = 0.0, den = 0.0;
    for (std::size_t i = from; i < a.l.size(); ++i)
    {
        const double dl = (double) a.l[i] - b.l[i];
        const double dr = (double) a.r[i] - b.r[i];
        num += dl * dl + dr * dr;
        den += (double) b.l[i] * b.l[i] + (double) b.r[i] * b.r[i];
    }
    return den > 0.0 ? std::sqrt (num / den) : (num > 0.0 ? 1.0e9 : 0.0);
}

static bool allBounded (const Out& o)
{
    for (std::size_t i = 0; i < o.l.size(); ++i)
    {
        if (! std::isfinite (o.l[i]) || ! std::isfinite (o.r[i]))
            return false;
        if (std::fabs (o.l[i]) > 1.0f || std::fabs (o.r[i]) > 1.0f)
            return false;
    }
    return true;
}

static MorphState eventState (int kind, float age, float energy = 1.0f)
{
    MorphState s;
    s.eventKind = kind;
    s.eventAge = age;
    s.eventEnergy = energy;
    return s;
}

int main()
{
    // Silence in, silence out.
    {
        LiveMorph lm;
        lm.prepare (kSr, 512);
        Stereo zero;
        zero.l.assign ((std::size_t) kSr, 0.0f);
        zero.r.assign ((std::size_t) kSr, 0.0f);
        const Out o = run (lm, zero, 512);
        CHECK (maxAbsOf (o.l) < 1.0e-6 && maxAbsOf (o.r) < 1.0e-6);
    }

    // Finite and bounded for neutral, every event kind and extreme states.
    {
        const Stereo inputs[] = { sine (440.0, 1.0, 0.8f), whiteNoise (1.0, 0.9f, 3), clickTrain (1.0, 0.25, 0.9f) };

        std::vector<MorphState> states;
        states.push_back (MorphState {});
        for (int k = 1; k <= 8; ++k)
            states.push_back (eventState (k, 0.1f));

        MorphState lo;
        lo.variety = lo.appeal = lo.greyness = lo.roughness = lo.centroid = 0.0f;
        lo.tonalness = lo.heat = lo.pressure = lo.infection = lo.score01 = 0.0f;
        lo.eventEnergy = 0.0f;
        lo.eventKind = 0;
        lo.eventAge = 0.0f;
        lo.species = { 0.0f, 0.0f, 0.0f };
        lo.scaleMask = 0;
        lo.rootPc = 0;
        lo.bpm = 0.0f;
        lo.beatPhase = 0.0f;
        lo.seed = 0;
        states.push_back (lo);

        MorphState hi;
        hi.variety = hi.appeal = hi.greyness = hi.roughness = hi.centroid = 1.0f;
        hi.tonalness = hi.heat = hi.pressure = hi.infection = hi.score01 = 1.0f;
        hi.eventEnergy = 1.0f;
        hi.eventKind = 0;
        hi.eventAge = 0.0f;
        hi.species = { 1.0f, 1.0f, 1.0f };
        hi.scaleMask = 0xFFFF;
        hi.rootPc = 11;
        hi.bpm = 300.0f;
        hi.beatPhase = 1.0f;
        hi.seed = 0xFFFFFFFFu;
        states.push_back (hi);
        for (int k = 1; k <= 8; ++k)
        {
            MorphState s = hi;
            s.eventKind = k;
            s.eventAge = 0.1f;
            states.push_back (s);
        }

        MorphState nanState;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        nanState.variety = nanState.appeal = nanState.greyness = nanState.heat = nan;
        nanState.eventAge = nan;
        nanState.bpm = nan;
        nanState.eventKind = 3;
        states.push_back (nanState);

        for (const Stereo& in : inputs)
        {
            for (const MorphState& s : states)
            {
                LiveMorph lm;
                lm.prepare (kSr, 512);
                lm.setState (s);
                const Out o = run (lm, in, 512);
                CHECK (allBounded (o));
            }
        }
    }

    // Wet RMS within 12 dB of input RMS over 3 s of pink-ish noise.
    {
        const Stereo in = pinkNoise (3.0, 0.2f, 11);
        const double inRms = rmsOf (in.l, in.r);
        for (int variant = 0; variant < 2; ++variant)
        {
            LiveMorph lm;
            lm.prepare (kSr, 512);
            MorphState s;
            if (variant == 1)
            {
                s.pressure = 0.5f;
                s.infection = 0.3f;
                s.heat = 0.8f;
            }
            lm.setState (s);
            const Out o = run (lm, in, 512);
            const double outRms = rmsOf (o.l, o.r);
            const double db = 20.0 * std::log10 (outRms / inRms);
            CHECK (std::fabs (db) <= 12.0);
        }
    }

    // Responsiveness: each event changes the output; after 6 s the event is neutral again.
    {
        const Stereo in = pinkNoise (2.0, 0.2f, 5);
        LiveMorph neutral;
        neutral.prepare (kSr, 512);
        const Out base = run (neutral, in, 512);

        for (int k = 1; k <= 8; ++k)
        {
            LiveMorph lm;
            lm.prepare (kSr, 512);
            lm.setState (eventState (k, 0.1f));
            const Out o = run (lm, in, 512);
            const double d = relDiff (o, base);
            if (! (d > 0.05))
                std::cerr << "event " << k << " normalised difference " << d << '\n';
            CHECK (d > 0.05);
            CHECK (allBounded (o));

            LiveMorph late;
            late.prepare (kSr, 512);
            late.setState (eventState (k, 6.5f));
            const Out lo = run (late, in, 512);
            CHECK (maxDiff (lo, base) <= 1.0e-6);
        }
    }

    // Two different states give different output.
    {
        const Stereo in = pinkNoise (2.0, 0.2f, 9);
        LiveMorph a, b;
        a.prepare (kSr, 512);
        b.prepare (kSr, 512);
        MorphState sb;
        sb.variety = 0.9f;
        sb.heat = 0.9f;
        sb.pressure = 0.5f;
        sb.greyness = 0.3f;
        b.setState (sb);
        const Out oa = run (a, in, 512);
        const Out ob = run (b, in, 512);
        CHECK (relDiff (oa, ob) > 0.05);
    }

    // Determinism: same seed and input give bit-identical output; another seed differs.
    {
        const Stereo in = pinkNoise (2.0, 0.2f, 21);
        MorphState s;
        s.seed = 7;
        s.pressure = 0.5f;
        s.variety = 0.8f;
        s.heat = 0.6f;
        s.infection = 0.4f;

        LiveMorph a, b;
        a.prepare (kSr, 512);
        b.prepare (kSr, 512);
        a.setState (s);
        b.setState (s);
        const Out oa = run (a, in, 512);
        const Out ob = run (b, in, 512);
        CHECK (oa.l.size() == ob.l.size());
        CHECK (std::memcmp (oa.l.data(), ob.l.data(), oa.l.size() * sizeof (float)) == 0);
        CHECK (std::memcmp (oa.r.data(), ob.r.data(), oa.r.size() * sizeof (float)) == 0);

        MorphState other = s;
        other.seed = 8;
        LiveMorph c;
        c.prepare (kSr, 512);
        c.setState (other);
        const Out oc = run (c, in, 512);
        CHECK (relDiff (oa, oc) > 0.01);
    }

    // Block size does not change the output (state held constant).
    {
        const Stereo in = pinkNoise (2.0, 0.2f, 33);
        MorphState s;
        s.pressure = 0.5f;
        s.infection = 0.3f;
        s.greyness = 0.2f;
        s.eventKind = 5;
        s.eventAge = 0.5f;
        s.eventEnergy = 1.0f;
        s.bpm = 120.0f;

        LiveMorph ref;
        ref.prepare (kSr, 512);
        ref.setState (s);
        const Out oref = run (ref, in, 512);

        const int blocks[] = { 1, 64, 2048 };
        for (int block : blocks)
        {
            LiveMorph lm;
            lm.prepare (kSr, 512);
            lm.setState (s);
            const Out o = run (lm, in, block);
            CHECK (maxDiff (o, oref, 4096) < 1.0e-3);
        }
    }

    // Onsets: fires on the click train, not on a steady sine.
    {
        const Stereo clicks = clickTrain (2.0, 0.5, 0.9f);
        LiveMorph lm;
        lm.prepare (kSr, 512);
        int onsetBlocks = 0;
        for (std::size_t pos = 0; pos + 512 <= clicks.l.size(); pos += 512)
        {
            std::vector<float> ol (512), orr (512);
            lm.process (clicks.l.data() + pos, clicks.r.data() + pos, ol.data(), orr.data(), 512);
            if (lm.onsetThisBlock())
                ++onsetBlocks;
        }
        CHECK (onsetBlocks >= 3);

        const Stereo tone = sine (440.0, 2.0, 0.5f);
        LiveMorph steady;
        steady.prepare (kSr, 512);
        std::vector<float> ol (512), orr (512);
        int falseOnsets = 0;
        for (std::size_t pos = 0; pos + 512 <= tone.l.size(); pos += 512)
        {
            steady.process (tone.l.data() + pos, tone.r.data() + pos, ol.data(), orr.data(), 512);
            if (pos >= (std::size_t) (0.5 * kSr) && steady.onsetThisBlock())
                ++falseOnsets;
        }
        CHECK (falseOnsets == 0);
    }

    // No heap allocation inside process() after prepare, even while events and states change.
    {
        const Stereo in = pinkNoise (2.0, 0.2f, 44);
        LiveMorph lm;
        lm.prepare (kSr, 512);
        MorphState s;
        s.pressure = 0.5f;
        s.infection = 0.4f;
        s.greyness = 0.2f;
        s.bpm = 110.0f;
        s.eventAge = 0.0f;
        s.eventEnergy = 1.0f;
        lm.setState (s);
        std::vector<float> ol (512), orr (512);

        const std::size_t before = gAllocCount;
        for (int blockIndex = 0; blockIndex * 512 + 512 <= (int) in.l.size(); ++blockIndex)
        {
            s.eventKind = blockIndex % 9;
            lm.setState (s);
            const std::size_t pos = (std::size_t) blockIndex * 512;
            lm.process (in.l.data() + pos, in.r.data() + pos, ol.data(), orr.data(), 512);
        }
        CHECK (gAllocCount == before);
    }

    // Speed: 20 s of stereo at 48 kHz under -O2.
    {
        const Stereo in = pinkNoise (20.0, 0.2f, 55);
        LiveMorph lm;
        lm.prepare (kSr, 512);
        MorphState s;
        s.pressure = 0.6f;
        s.infection = 0.4f;
        s.greyness = 0.3f;
        s.heat = 0.7f;
        s.variety = 0.9f;
        s.eventKind = 1;
        s.eventAge = 0.5f;
        s.eventEnergy = 1.0f;
        s.bpm = 120.0f;
        lm.setState (s);
        std::vector<float> ol (512), orr (512);

        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t pos = 0; pos + 512 <= in.l.size(); pos += 512)
            lm.process (in.l.data() + pos, in.r.data() + pos, ol.data(), orr.data(), 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::cout << "speed: " << secs << " s for 20 s stereo = " << (20.0 / secs) << "x realtime\n";
        CHECK (secs < 4.0);
    }

    std::cout << "LiveMorph tests passed\n";
    return 0;
}
