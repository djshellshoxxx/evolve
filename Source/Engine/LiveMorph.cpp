// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "LiveMorph.h"

#include <algorithm>
#include <cmath>

namespace mutagen
{
namespace
{
    constexpr float kPi = 3.14159265358979f;
    constexpr double kTwoPi = 6.28318530717958647692;
    constexpr float kOnsetRatio = 2.0f;
    constexpr float kOnsetRelease = 1.3f;
    constexpr float kOnsetFloor = 1.0e-6f;
    constexpr float kFreezeGreyness = 0.85f;
    constexpr double kWarmSeconds = 0.25;
    constexpr double kGrainReachSec = 0.45;   // base read-behind distance of grains
    constexpr double kGrainMarginSec = 0.05;

    enum Ctl
    {
        cVariety, cAppeal, cGreyness, cRoughness, cCentroid, cTonal,
        cHeat, cPressure, cInfection, cScore, cSpeciesUp, cSpeciesDown
    };
    static_assert (cSpeciesDown + 1 == 12, "LiveMorph::kCtl must match the Ctl enum");

    inline float clampf (float v, float lo, float hi)
    {
        return ! (v >= lo) ? lo : (v > hi ? hi : v); // NaN maps to lo
    }

    inline float clamp01 (float v) { return clampf (v, 0.0f, 1.0f); }

    inline float flushF (float v) { return std::fabs (v) < 1.0e-20f ? 0.0f : v; }

    inline float sanitize (float v) { return (v > -1.0e6f && v < 1.0e6f) ? v : 0.0f; }

    inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }

    inline float onePoleCoef (double sr, double seconds)
    {
        return (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
    }

    inline float softLimit (float x)
    {
        if (! (x > -1.0e4f && x < 1.0e4f))
            return 0.0f;

        const float a = std::fabs (x);
        if (a <= 0.8f)
            return x;

        const float y = std::min (1.0f, 0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f));
        return x < 0.0f ? -y : y;
    }

    // Event amount over age: full for 3 s, eased to zero by 6 s, neutral after that.
    inline float eventDecay (float age, int kind)
    {
        if (kind == 0 || ! (age <= 6.0f))
            return 0.0f;

        if (age <= 3.0f)
            return 1.0f;

        const float t = (age - 3.0f) / 3.0f;
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    inline std::size_t nextPow2 (std::size_t v)
    {
        std::size_t p = 1;
        while (p < v)
            p <<= 1;
        return p;
    }

    // Pitch class (root + k) in octaves 2..5 at concert pitch.
    inline double combHz (int root, int k)
    {
        const int pc = (root + k) % 12;
        const int octave = 2 + (k % 4);
        const int midi = 12 * (octave + 1) + pc;
        return 440.0 * std::exp2 ((double) (midi - 69) / 12.0);
    }

    inline std::uint64_t seedState (std::uint32_t seed)
    {
        const std::uint64_t s = 0x9E3779B97F4A7C15ull ^ (((std::uint64_t) seed << 32) | (std::uint64_t) seed);
        return s != 0 ? s : 1;
    }
}

LiveMorph::LiveMorph()
{
    for (int i = 0; i <= kHannSize; ++i)
        hann_[(std::size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / kHannSize));

    applyState (MorphState {});
    cur_ = tgt_;
}

void LiveMorph::prepare (double sampleRate, int maxBlock)
{
    sr_ = sampleRate > 1000.0 ? sampleRate : 48000.0;
    maxBlock_ = std::max (maxBlock, 1);

    const std::size_t ringSize = nextPow2 ((std::size_t) (2.0 * sr_) + 64);
    ringL_.assign (ringSize, 0.0f);
    ringR_.assign (ringSize, 0.0f);
    ringMask_ = (std::uint64_t) ringSize - 1;

    combSize_ = nextPow2 ((std::size_t) (sr_ / 60.0) + 256);
    combMask_ = combSize_ - 1;
    combBuf_.assign (combSize_ * (std::size_t) kCombs, 0.0f);

    aCtl_ = onePoleCoef (sr_, 0.02);
    aFast_ = onePoleCoef (sr_, 0.003);
    aSlow_ = onePoleCoef (sr_, 0.15);
    aRms_ = onePoleCoef (sr_, 0.05);
    aLong_ = onePoleCoef (sr_, 0.4);
    aBright_ = onePoleCoef (sr_, 0.03);
    aComp_ = onePoleCoef (sr_, 0.3);
    aClose_ = onePoleCoef (sr_, 0.4);
    aFz_ = (float) (1.0 - std::exp (-kTwoPi * 1200.0 / sr_));
    aT1_ = (float) (1.0 - std::exp (-kTwoPi * 400.0 / sr_));
    aT2_ = (float) (1.0 - std::exp (-kTwoPi * 3000.0 / sr_));
    clickDecay_ = (float) std::exp (-1.0 / (0.002 * sr_));
    stDecay_ = (float) std::exp (-1.0 / (0.08 * sr_));

    shMin_ = 0.005 * sr_;
    shWin_ = 0.05 * sr_;
    warmSamples_ = (std::uint64_t) (kWarmSeconds * sr_);

    applyState (st_);
    reset();
    prepared_ = true;
}

void LiveMorph::reset()
{
    std::fill (ringL_.begin(), ringL_.end(), 0.0f);
    std::fill (ringR_.begin(), ringR_.end(), 0.0f);
    std::fill (combBuf_.begin(), combBuf_.end(), 0.0f);
    wAbs_ = 0;
    combW_ = 0;

    for (Comb& c : combs_)
    {
        c.d = c.dTarget;
        c.amp = c.ampTarget;
        c.lp = 0.0f;
    }

    for (Grain& g : grains_) g.active = false;
    for (Grain& g : sparkles_) g.active = false;
    for (Grain& g : twists_) g.active = false;
    spawnTimer_ = 0.0;
    twistTimer_ = 0.0;

    for (Shifter& s : shifters_)
    {
        s.d1 = shMin_;
        s.d2 = shMin_ + 0.5 * shWin_;
        stepShifter (s, 1.0);
    }
    vibPhase_ = 0.0;

    powFast_ = powSlow_ = powRms_ = powLong_ = powWet_ = 0.0f;
    mPrev_ = dEma_ = eEma_ = 0.0f;
    onsetArmed_ = false;
    refractory_ = 0;

    tlp1L_ = tlp1R_ = tlp2L_ = tlp2R_ = 0.0f;
    holdCount_ = 0;
    holdL_ = holdR_ = clickVal_ = 0.0f;
    stEnv_ = 0.0f;
    compGain_ = 1.0f;
    fLpL_ = fLpR_ = 0.0f;
    closeCur_ = 0.0f;

    cur_ = tgt_;
    ev_.fill (0.0f);
    elapsed_ = 0.0;

    rng_ = seedState (st_.seed);
    seedInit_ = true;
    seedApplied_ = st_.seed;

    level_ = 0.0f;
    bright_ = 0.0f;
    onsetBlock_ = false;
}

void LiveMorph::setState (const MorphState& s)
{
    applyState (s);
}

void LiveMorph::applyState (const MorphState& s)
{
    st_ = s;

    tgt_[cVariety] = clamp01 (s.variety);
    tgt_[cAppeal] = clamp01 (s.appeal);
    tgt_[cGreyness] = clamp01 (s.greyness);
    tgt_[cRoughness] = clamp01 (s.roughness);
    tgt_[cCentroid] = clamp01 (s.centroid);
    tgt_[cTonal] = clamp01 (s.tonalness);
    tgt_[cHeat] = clamp01 (s.heat);
    tgt_[cPressure] = clamp01 (s.pressure);
    tgt_[cInfection] = clamp01 (s.infection);
    tgt_[cScore] = clamp01 (s.score01);
    tgt_[cSpeciesUp] = clamp01 (s.species[0]);
    tgt_[cSpeciesDown] = clamp01 (s.species[2]);

    kind_ = std::min (std::max (s.eventKind, 0), kEvents - 1);

    float age = s.eventAge;
    if (! (age == age))
        age = 99.0f; // NaN
    ageBase_ = (double) std::min (std::max (age, 0.0f), 1.0e6f);
    elapsed_ = 0.0;

    evStrength_ = 0.35f + 0.65f * clamp01 (s.eventEnergy);
    bpm_ = clampf (s.bpm, 0.0f, 300.0f);

    const int root = ((s.rootPc % 12) + 12) % 12;
    const unsigned mask = (unsigned) s.scaleMask & 0x0FFFu;
    for (int k = 0; k < kCombs; ++k)
    {
        combs_[(std::size_t) k].ampTarget = ((mask >> k) & 1u) != 0u ? 1.0f : 0.0f;
        combs_[(std::size_t) k].dTarget = std::min (sr_ / combHz (root, k), (double) combSize_ - 8.0);
    }

    vibHz_ = 6.0f + 3.0f * (float) std::fmod ((double) s.seed * 0.618034, 1.0);

    if (! seedInit_ || s.seed != seedApplied_)
    {
        seedApplied_ = s.seed;
        seedInit_ = true;
        rng_ = seedState (s.seed);
    }
}

float LiveMorph::uniform()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 7;
    rng_ ^= rng_ << 17;
    return (float) ((rng_ >> 40) * (1.0 / 16777216.0));
}

float LiveMorph::hannAt (float x) const
{
    const float f = clampf (x, 0.0f, 0.99999f) * (float) kHannSize;
    const int i = (int) f;
    const float t = f - (float) i;
    const float a = hann_[(std::size_t) i];
    const float b = hann_[(std::size_t) i + 1];
    return a + (b - a) * t;
}

float LiveMorph::window (float x) const
{
    // roughness sharpens the Hann window towards a narrower, more abrupt grain
    const float h = hannAt (x);
    const float r = cur_[cRoughness];
    return h * (1.0f - r) + h * h * r;
}

float LiveMorph::readRing (const std::vector<float>& buf, double pos) const
{
    const double fl = std::floor (pos);
    const std::uint64_t base = (std::uint64_t) (std::int64_t) fl;
    const std::size_t i0 = (std::size_t) (base & ringMask_);
    const std::size_t i1 = (std::size_t) ((base + 1u) & ringMask_);
    const float t = (float) (pos - fl);
    return buf[i0] + (buf[i1] - buf[i0]) * t;
}

void LiveMorph::stepShifter (Shifter& s, double ratio) const
{
    const double dd = ratio - 1.0;
    const double hi = shMin_ + shWin_;

    s.d1 -= dd;
    s.d2 -= dd;

    if (s.d1 < shMin_) s.d1 += shWin_;
    else if (s.d1 >= hi) s.d1 -= shWin_;

    if (s.d2 < shMin_) s.d2 += shWin_;
    else if (s.d2 >= hi) s.d2 -= shWin_;

    s.w1 = hannAt ((float) ((s.d1 - shMin_) / shWin_));
    s.w2 = hannAt ((float) ((s.d2 - shMin_) / shWin_));
}

float LiveMorph::readShifter (const std::vector<float>& buf, const Shifter& s) const
{
    const double head = (double) wAbs_;
    return s.w1 * readRing (buf, head - s.d1) + s.w2 * readRing (buf, head - s.d2);
}

int LiveMorph::mainTarget() const
{
    return std::clamp ((int) (4.0f + 8.0f * cur_[cVariety] + 0.5f), 4, 12);
}

void LiveMorph::spawnMainGrain (Grain& g)
{
    const float variety = cur_[cVariety];
    const float heat = cur_[cHeat];
    const float heatEff = clamp01 (heat + 0.6f * ev_[4]);
    const int target = mainTarget();

    const float lenSec = clampf ((0.02f + 0.23f * (1.0f - 0.6f * variety) * uniform())
                                     * (1.0f - 0.4f * heatEff),
                                 0.02f, 0.25f);
    const float spread = 7.0f * clamp01 (0.5f * heat + 0.5f * cur_[cPressure]);
    const float bias = 6.0f * (cur_[cSpeciesUp] - cur_[cSpeciesDown]);
    const float semis = bias + spread * (2.0f * uniform() - 1.0f);
    const double ratio = std::exp2 ((double) semis / 12.0);
    const bool reverse = uniform() < 0.6f * cur_[cInfection];
    const float jitter = cur_[cGreyness] * 0.3f * uniform();
    const float len = lenSec * (float) sr_;
    const double inc = reverse ? -ratio : ratio;
    const double startDist = std::max (kGrainReachSec * sr_ + jitter * sr_,
                                       (double) len * ratio + kGrainMarginSec * sr_);

    const float ang = (clampf (2.0f * uniform() - 1.0f, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;

    g.active = true;
    g.pos = (double) wAbs_ - startDist;
    g.inc = inc;
    g.phase = 0.0f;
    g.len = len;
    g.amp = 1.0f / std::sqrt ((float) target);
    g.gl = std::cos (ang);
    g.gr = std::sin (ang);

    // time until the next spawn: average spacing of len / target, jittered, quicker with heat
    const float interval = lenSec * (float) sr_ / (float) target * (0.6f + 0.8f * uniform()) / (1.0f + heatEff);
    spawnTimer_ = (double) interval;
}

void LiveMorph::spawnSparkle (Grain& g, float strength)
{
    const double ratio = uniform() < 0.5f ? 2.0 : 3.0; // octave, or octave plus fifth
    const float len = (0.05f + 0.07f * uniform()) * (float) sr_;
    const double startDist = std::max (kGrainReachSec * sr_, (double) len * ratio + kGrainMarginSec * sr_);
    const float ang = (clampf (2.0f * uniform() - 1.0f, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;

    g.active = true;
    g.pos = (double) wAbs_ - startDist;
    g.inc = ratio;
    g.phase = 0.0f;
    g.len = len;
    g.amp = 0.5f * strength;
    g.gl = std::cos (ang);
    g.gr = std::sin (ang);
}

void LiveMorph::spawnTwist (Grain& g, float strength)
{
    // slow reversed swell: reads backwards over 0.8 s, all of it behind the write head
    g.active = true;
    g.pos = (double) wAbs_ - kGrainMarginSec * sr_;
    g.inc = -1.0;
    g.phase = 0.0f;
    g.len = 0.8f * (float) sr_;
    g.amp = 0.6f * strength;
    g.gl = 0.7071f;
    g.gr = 0.7071f;
}

void LiveMorph::renderGrain (Grain& g, float& l, float& r)
{
    if (! g.active)
        return;

    const float w = window (g.phase / g.len) * g.amp;
    l += w * g.gl * readRing (ringL_, g.pos);
    r += w * g.gr * readRing (ringR_, g.pos);

    g.pos += g.inc;
    g.phase += 1.0f;

    if (g.phase >= g.len)
        g.active = false;
}

float LiveMorph::inputLevel() const
{
    return level_;
}

float LiveMorph::inputBrightness() const
{
    return bright_;
}

bool LiveMorph::onsetThisBlock() const
{
    return onsetBlock_;
}

void LiveMorph::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (n <= 0)
        return;

    if (! prepared_)
    {
        for (int i = 0; i < n; ++i)
            outL[i] = outR[i] = 0.0f;
        return;
    }

    bool onsetSeen = false;
    float brightNow = bright_;

    for (int i = 0; i < n; ++i)
    {
        const float xl = sanitize (inL[i]);
        const float xr = sanitize (inR[i]);
        const float m = 0.5f * (xl + xr);

        // 1. input analysis: level, brightness, onset with hysteresis
        const float p = m * m;
        powFast_ = flushF (powFast_ + aFast_ * (p - powFast_));
        powSlow_ = flushF (powSlow_ + aSlow_ * (p - powSlow_));
        powRms_ = flushF (powRms_ + aRms_ * (p - powRms_));
        powLong_ = flushF (powLong_ + aLong_ * (p - powLong_));

        const float dm = m - mPrev_;
        mPrev_ = m;
        dEma_ = flushF (dEma_ + aBright_ * (dm * dm - dEma_));
        eEma_ = flushF (eEma_ + aBright_ * (p - eEma_));
        brightNow = clamp01 (2.0f * dEma_ / (eEma_ + 1.0e-9f));

        if (refractory_ > 0)
            --refractory_;

        bool onset = false;
        if (! onsetArmed_)
        {
            if (refractory_ == 0 && powFast_ > kOnsetRatio * powSlow_ + kOnsetFloor)
            {
                onsetArmed_ = true;
                onset = true;
                refractory_ = (int) (0.05 * sr_);
            }
        }
        else if (powFast_ < kOnsetRelease * powSlow_ + kOnsetFloor)
        {
            onsetArmed_ = false;
        }

        if (onset)
        {
            onsetSeen = true;
            stEnv_ = 1.0f;
            const double stutSec = bpm_ > 1.0f ? std::clamp (60.0 / (double) bpm_ / 4.0, 0.03, 0.25) : 0.08;
            stutLen_ = (int) (stutSec * sr_);
        }

        // 2. smooth the controls (one-pole, about 20 ms)
        for (int c = 0; c < kCtl; ++c)
            cur_[(std::size_t) c] = flushF (cur_[(std::size_t) c] + aCtl_ * (tgt_[(std::size_t) c] - cur_[(std::size_t) c]));

        const float age = (float) (ageBase_ + elapsed_);
        elapsed_ += 1.0 / sr_;

        const float strength = evStrength_ * eventDecay (age, kind_);
        for (int k = 1; k < kEvents; ++k)
        {
            const float target = k == kind_ ? strength : 0.0f;
            float v = ev_[(std::size_t) k] + aCtl_ * (target - ev_[(std::size_t) k]);
            if (target == 0.0f && v < 1.0e-6f)
                v = 0.0f;
            ev_[(std::size_t) k] = v;
        }

        // 3. freeze: the ring stops writing (greyness, or frozenNoise while the event is young)
        const bool freezeWanted = tgt_[cGreyness] > kFreezeGreyness || (kind_ == 8 && age <= 6.0f);
        const bool frozen = freezeWanted && wAbs_ >= warmSamples_;
        closeCur_ = flushF (closeCur_ + aClose_ * ((frozen ? 1.0f : 0.0f) - closeCur_));

        if (! frozen)
        {
            const std::size_t idx = (std::size_t) (wAbs_ & ringMask_);
            ringL_[idx] = xl;
            ringR_[idx] = xr;
            ++wAbs_;
        }

        // 4. granular recomposer (main voices, sparkles, twist swells)
        float wetL = 0.0f;
        float wetR = 0.0f;

        spawnTimer_ -= 1.0;
        if (spawnTimer_ <= 0.0)
        {
            int active = 0;
            for (const Grain& g : grains_)
                active += g.active ? 1 : 0;

            if (active < mainTarget())
            {
                for (Grain& g : grains_)
                {
                    if (! g.active)
                    {
                        spawnMainGrain (g);
                        break;
                    }
                }
            }
            else
            {
                spawnTimer_ = 0.005 * sr_;
            }
        }

        for (Grain& g : grains_)
            renderGrain (g, wetL, wetR);

        const float spark = ev_[5];
        if (spark > 1.0e-5f && uniform() < 6.0f * spark / (float) sr_)
        {
            for (Grain& g : sparkles_)
            {
                if (! g.active)
                {
                    spawnSparkle (g, spark);
                    break;
                }
            }
        }

        for (Grain& g : sparkles_)
            renderGrain (g, wetL, wetR);

        const float twist = ev_[7];
        if (twist > 1.0e-5f)
        {
            twistTimer_ -= 1.0;
            if (twistTimer_ <= 0.0)
            {
                for (Grain& g : twists_)
                {
                    if (! g.active)
                    {
                        spawnTwist (g, twist);
                        break;
                    }
                }
                twistTimer_ = 0.4 * sr_;
            }
        }

        for (Grain& g : twists_)
            renderGrain (g, wetL, wetR);

        // 5. scale resonators: feedback combs excited by the input, one per enabled pitch class
        const float tonal = cur_[cTonal];
        const float appeal = cur_[cAppeal];
        const float greyness = cur_[cGreyness];
        const float ta = clamp01 (0.6f * tonal + 0.4f * appeal);
        const float resAmt = ta * (0.7f + 0.3f * cur_[cScore]) * (1.0f - 0.8f * greyness);
        const float combG = 0.55f + 0.4f * ta;
        const float combLp = 0.15f + 0.7f * (1.0f - greyness);
        const float resScale = resAmt * (1.0f - combG) * 0.5f;

        float sumEven = 0.0f;
        float sumOdd = 0.0f;
        for (int k = 0; k < kCombs; ++k)
        {
            Comb& c = combs_[(std::size_t) k];
            c.d += (double) aCtl_ * (c.dTarget - c.d);
            c.amp = flushF (c.amp + aCtl_ * (c.ampTarget - c.amp));

            const std::size_t base = (std::size_t) k * combSize_;
            const double rp = (double) combW_ - c.d;
            const double fl = std::floor (rp);
            const std::size_t i0 = (std::size_t) ((std::uint64_t) (std::int64_t) fl & (std::uint64_t) combMask_);
            const std::size_t i1 = (std::size_t) ((std::uint64_t) ((std::int64_t) fl + 1) & (std::uint64_t) combMask_);
            const float yd = combBuf_[base + i0] + (combBuf_[base + i1] - combBuf_[base + i0]) * (float) (rp - fl);

            c.lp = flushF (c.lp + combLp * (yd - c.lp));
            const float y = flushF (m + combG * c.lp);
            combBuf_[base + (std::size_t) (combW_ & (std::uint64_t) combMask_)] = y;

            const float v = c.amp * y;
            if ((k & 1) != 0)
                sumOdd += v;
            else
                sumEven += v;
        }
        ++combW_;

        wetL += resScale * (0.8f * sumEven + 0.2f * sumOdd);
        wetR += resScale * (0.8f * sumOdd + 0.2f * sumEven);

        // 6. event colourings
        const float enz = ev_[1];
        if (enz > 1.0e-5f)
        {
            stepShifter (shifters_[0], 2.0);
            wetL += enz * 0.45f * readShifter (ringL_, shifters_[0]);
            wetR += enz * 0.45f * readShifter (ringR_, shifters_[0]);
        }

        const float cat = ev_[2];
        if (cat > 1.0e-5f)
        {
            vibPhase_ += (double) vibHz_ / sr_;
            if (vibPhase_ >= 1.0)
                vibPhase_ -= 1.0;

            const double cents = 40.0 * std::sin (kTwoPi * vibPhase_);
            stepShifter (shifters_[1], std::exp2 (cents / 1200.0));
            const float a = 0.5f * cat;
            wetL = (1.0f - a) * wetL + a * readShifter (ringL_, shifters_[1]);
            wetR = (1.0f - a) * wetR + a * readShifter (ringR_, shifters_[1]);
        }

        const float rad = ev_[3];
        if (rad > 1.0e-5f)
        {
            const int hold = 1 + (int) (rad * 7.0f + 0.5f);
            const float step = std::exp2 (1.0f - std::round (lerpf (16.0f, 5.0f, rad)));
            if (holdCount_ <= 0)
            {
                holdL_ = step * std::round (wetL / step);
                holdR_ = step * std::round (wetR / step);
                holdCount_ = hold;
            }
            --holdCount_;

            wetL = lerpf (wetL, holdL_, rad);
            wetR = lerpf (wetR, holdR_, rad);

            if (uniform() < 0.0007f * rad)
                clickVal_ += (uniform() - 0.5f) * 0.8f * rad;

            wetL += clickVal_;
            wetR += clickVal_;
            clickVal_ = flushF (clickVal_ * clickDecay_);
        }
        else
        {
            holdCount_ = 0;
            clickVal_ = 0.0f;
        }

        // 7. tone: three-band tilt from centroid and appeal, soft saturation when dull
        const float tilt = clampf ((cur_[cCentroid] - 0.35f) * 1.2f + (appeal - 0.5f) * 0.8f, -1.0f, 1.0f);
        const float gLow = clampf (1.0f - 0.6f * tilt, 0.2f, 2.0f);
        const float gHigh = clampf (1.0f + 0.9f * tilt + 0.3f * enz, 0.2f, 2.5f); // enzyme adds air
        const float sat = clampf ((0.5f - appeal) * 1.2f + 0.3f * ev_[4] + 0.15f * cur_[cRoughness], 0.0f, 0.7f);

        tlp1L_ = flushF (tlp1L_ + aT1_ * (wetL - tlp1L_));
        tlp2L_ = flushF (tlp2L_ + aT2_ * (wetL - tlp2L_));
        tlp1R_ = flushF (tlp1R_ + aT1_ * (wetR - tlp1R_));
        tlp2R_ = flushF (tlp2R_ + aT2_ * (wetR - tlp2R_));

        float yL = tlp1L_ * gLow + (tlp2L_ - tlp1L_) + (wetL - tlp2L_) * gHigh;
        float yR = tlp1R_ * gLow + (tlp2R_ - tlp1R_) + (wetR - tlp2R_) * gHigh;
        yL = (1.0f - sat) * yL + sat * std::tanh (2.5f * yL) / 2.5f;
        yR = (1.0f - sat) * yR + sat * std::tanh (2.5f * yR) / 2.5f;
        wetL = yL;
        wetR = yR;

        // 8. onset stutter: a transient echo one beat division back, sized by pressure
        stEnv_ = flushF (stEnv_ * stDecay_);
        const float pressure = cur_[cPressure];
        if (stEnv_ > 1.0e-5f && pressure > 1.0e-5f)
        {
            const double head = (double) wAbs_;
            const float gain = 0.6f * pressure * stEnv_;
            const float tapL = readRing (ringL_, head - (double) stutLen_)
                             + 0.5f * readRing (ringL_, head - 2.0 * (double) stutLen_);
            const float tapR = readRing (ringR_, head - (double) stutLen_)
                             + 0.5f * readRing (ringR_, head - 2.0 * (double) stutLen_);
            wetL += gain * tapL;
            wetR += gain * tapR;
        }

        // 9. level compensation: wet RMS tracks input RMS within +-6 dB (measured before the duck)
        const float wetP = 0.5f * (wetL * wetL + wetR * wetR);
        powWet_ = flushF (powWet_ + aLong_ * (wetP - powWet_));
        const float ratio = std::sqrt ((powLong_ + 1.0e-12f) / (powWet_ + 1.0e-12f));
        compGain_ += aComp_ * (clampf (ratio, 0.5f, 2.0f) - compGain_);
        wetL *= compGain_;
        wetR *= compGain_;

        // 10. fact demo ducks the wet briefly to make room
        const float duck = 1.0f - 0.6f * ev_[6];
        wetL *= duck;
        wetR *= duck;

        // 11. frozen colony: a gentle low-pass closes over the wet
        fLpL_ = flushF (fLpL_ + aFz_ * (wetL - fLpL_));
        fLpR_ = flushF (fLpR_ + aFz_ * (wetR - fLpR_));
        wetL += closeCur_ * (fLpL_ - wetL);
        wetR += closeCur_ * (fLpR_ - wetR);

        // 12. output, soft limited so it never exceeds 1.0
        outL[i] = softLimit (wetL);
        outR[i] = softLimit (wetR);
    }

    level_ = std::sqrt (powRms_);
    bright_ = brightNow;
    onsetBlock_ = onsetSeen;
}
}
