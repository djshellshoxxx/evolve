#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include "Rng.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        WorldSeed - the per-run character of the ecology.

        The old engine randomised only the *starting genomes*. The rules
        themselves were identical every run, and since those rules pulled
        every cell toward one attractor, every colony arrived at the same
        place from a different direction. Changing the seed changed the
        journey, not the destination.

        A WorldSeed randomises the rules. It decides the tuning system the
        colony hears in, how fast life runs, which modulation lanes reach
        which parts of the timbre, what counts as fitness, how many
        semi-isolated niches exist, and what the partials of a "note" even
        are. Two worlds are not two variations on a sound; they are two
        different instruments that happen to share an engine.

        It is a plain value type derived from one 64-bit number, so it
        serialises as that number and replays exactly.
        ------------------------------------------------------------------ */

    enum class PartialSet : int
    {
        harmonic = 0,   // 1, 2, 3, 4 ...      - classic, vocal, warm
        odd,            // 1, 3, 5, 7 ...      - hollow, clarinet-like
        stretched,      // n^1.0x              - piano-ish, slightly detuned highs
        golden,         // phi-spaced          - bell-like, unresolved
        subharmonic,    // 1, 1/2, 1/3 ...     - heavy, undertone
        formantic,      // clustered around formants - vowel-like
        count
    };

    enum class ScaleKind : int
    {
        chromatic = 0, major, minorNatural, dorian, phrygian, lydian,
        pentatonic, wholeTone, octatonic, harmonicMinor, justIntonation,
        quarterTone, bohlenPierce,
        count
    };

    struct WorldSeed
    {
        uint64_t seed = 0;

        // ---- tuning & pitch identity ------------------------------------
        ScaleKind scale       = ScaleKind::chromatic;
        float     rootHz      = 110.0f;   // 40 .. 320
        float     pitchSpread = 0.5f;     // how far cells roam from the root
        bool      quantisePitch = true;   // snap cell pitch to the scale
        float     detuneAmount = 0.02f;   // per-cell tuning error, 0 .. 0.12

        // ---- timbre palette ----------------------------------------------
        PartialSet partials   = PartialSet::harmonic;
        float      stretch    = 1.0f;     // exponent for the stretched set
        float      brightBias = 0.5f;     // where this world's spectra sit
        float      noiseCeiling = 0.55f;  // hard cap on how noisy a cell may get

        // ---- tempo of life -----------------------------------------------
        float lifeTempo    = 1.0f;        // 0.25 .. 3.0, scales the whole ecology
        float churn        = 0.5f;        // birth/death turnover

        // ---- modulation topology -----------------------------------------
        /*  Each of the 6 LFO lanes in a cell's ModBank is routed to a
            destination by this table. Randomising it per world is what makes
            one world wobble in pitch while another breathes in formant.     */
        static constexpr int numLanes = 6;
        uint8_t laneDest[numLanes] {};    // ModDest values
        float   rateCentre = 0.5f;        // 0 = all lanes slow, 1 = all fast
        float   rateSpread = 0.85f;       // 0 = all lanes the same rate, 1 = maximal spread
        float   modDepth   = 0.5f;        // world-wide modulation intensity

        // ---- ecology shape -----------------------------------------------
        int   nicheCount    = 4;          // 2 .. 8 semi-isolated sub-populations
        float nicheDrift    = 0.4f;       // how fast a niche's own target wanders
        float migrationRate = 0.25f;      // how often a cell changes niche
        float speciesMix[3] { 0.34f, 0.33f, 0.33f };   // grain / spectral / resonator

        // ---- what fitness means here -------------------------------------
        float wAppeal  = 0.40f;   // consonance / tonalness seeking
        float wNovelty = 0.40f;   // novelty search
        float wUser    = 0.20f;   // the user's clicks
        float crowdingPenalty = 0.5f;   // fitness sharing strength

        // ---- identity ------------------------------------------------------
        char name[32] { "WORLD" };

        // ------------------------------------------------------------------
        static WorldSeed fromSeed (uint64_t s)
        {
            WorldSeed w;
            w.seed = s;
            Rng r (s);

            w.scale         = (ScaleKind) r.intRange (0, (int) ScaleKind::count);
            w.rootHz        = 40.0f * std::pow (8.0f, r.nextFloat());          // 40 .. 320 Hz
            w.pitchSpread   = r.range (0.25f, 1.0f);
            w.quantisePitch = r.chance (0.72f);
            w.detuneAmount  = r.range (0.0f, 0.12f) * r.range (0.2f, 1.0f);

            w.partials    = (PartialSet) r.intRange (0, (int) PartialSet::count);
            w.stretch     = r.range (0.93f, 1.09f);
            w.brightBias  = r.range (0.2f, 0.85f);
            // Never let a world be *born* able to reach full noise. The guard
            // in Colony can still tighten this, but not loosen it.
            w.noiseCeiling = r.range (0.30f, 0.70f);

            w.lifeTempo = std::pow (2.0f, r.range (-2.0f, 1.6f));              // 0.25 .. ~3
            w.churn     = r.range (0.2f, 0.9f);

            w.rateCentre = r.range (0.2f, 0.8f);
            w.rateSpread = r.range (0.55f, 1.0f);
            w.modDepth   = r.range (0.3f, 1.0f);

            // Routing: shuffle the destination list so every lane is used,
            // then let a couple of lanes double up on a destination.
            uint8_t dests[numLanes];
            for (int i = 0; i < numLanes; ++i) dests[i] = (uint8_t) i;
            for (int i = numLanes - 1; i > 0; --i)
            {
                const int j = r.intRange (0, i + 1);
                const uint8_t t = dests[i]; dests[i] = dests[j]; dests[j] = t;
            }
            for (int i = 0; i < numLanes; ++i)
            {
                // 9 destinations exist (see ModDest); lanes past the first six
                // pick freely, and any lane may be re-pointed at random.
                w.laneDest[i] = r.chance (0.7f) ? dests[i]
                                                : (uint8_t) r.intRange (0, 9);
            }

            w.nicheCount    = r.intRange (2, 9);
            w.nicheDrift    = r.range (0.15f, 0.8f);
            w.migrationRate = r.range (0.05f, 0.45f);

            float a = r.range (0.15f, 1.0f), b = r.range (0.15f, 1.0f), c = r.range (0.15f, 1.0f);
            const float tot = a + b + c;
            w.speciesMix[0] = a / tot; w.speciesMix[1] = b / tot; w.speciesMix[2] = c / tot;

            float wa = r.range (0.25f, 0.75f);
            float wn = r.range (0.25f, 0.75f);
            float wu = r.range (0.10f, 0.40f);
            const float wt = wa + wn + wu;
            w.wAppeal = wa / wt; w.wNovelty = wn / wt; w.wUser = wu / wt;
            w.crowdingPenalty = r.range (0.3f, 0.9f);

            makeName (w, r);
            return w;
        }

        /** Scale degrees as a 12-bit pitch-class mask (bit 0 = root). */
        uint16_t scaleMask() const
        {
            switch (scale)
            {
                case ScaleKind::major:         return 0b101010110101;
                case ScaleKind::minorNatural:  return 0b010110101101;
                case ScaleKind::dorian:        return 0b011010101101;
                case ScaleKind::phrygian:      return 0b010110101011;
                case ScaleKind::lydian:        return 0b101011010101;
                case ScaleKind::pentatonic:    return 0b001010010101;
                case ScaleKind::wholeTone:     return 0b010101010101;
                case ScaleKind::octatonic:     return 0b011011011011;
                case ScaleKind::harmonicMinor: return 0b100110101101;
                default:                       return 0b111111111111;   // chromatic & microtonal
            }
        }

        /** Divisions per octave - lets a world be microtonal. */
        float edo() const
        {
            switch (scale)
            {
                case ScaleKind::quarterTone:  return 24.0f;
                case ScaleKind::bohlenPierce: return 13.0f;   // per tritave, close enough here
                case ScaleKind::justIntonation:
                default:                      return 12.0f;
            }
        }

        /** Snap a frequency to this world's tuning. */
        float quantise (float hz) const
        {
            if (! quantisePitch || hz <= 0.0f) return hz;

            const float n   = edo();
            const float rel = std::log2 (hz / rootHz) * n;      // steps from the root
            float step = std::round (rel);

            if (n == 12.0f)
            {
                const uint16_t mask = scaleMask();
                if (mask != 0b111111111111)
                {
                    // walk outward to the nearest permitted pitch class
                    for (int d = 0; d < 7; ++d)
                    {
                        const int up = (int) step + d, dn = (int) step - d;
                        const int upPc = ((up % 12) + 12) % 12;
                        const int dnPc = ((dn % 12) + 12) % 12;
                        if ((mask >> upPc) & 1u) { step = (float) up; break; }
                        if ((mask >> dnPc) & 1u) { step = (float) dn; break; }
                    }
                }
            }
            return rootHz * std::pow (2.0f, step / n);
        }

        /** Frequency ratio of partial k (0-based) under this world's palette. */
        float partialRatio (int k) const
        {
            const float n = (float) (k + 1);
            switch (partials)
            {
                case PartialSet::odd:         return 2.0f * n - 1.0f;
                case PartialSet::stretched:   return std::pow (n, stretch);
                case PartialSet::golden:      return std::pow (1.6180339887f, (float) k * 0.72f);
                case PartialSet::subharmonic: return 1.0f / n * 4.0f;
                case PartialSet::formantic:   return n * (1.0f + 0.06f * std::sin ((float) k * 2.4f));
                case PartialSet::harmonic:
                default:                      return n;
            }
        }

    private:
        static void makeName (WorldSeed& w, Rng& r)
        {
            static const char* a[] = { "PALE", "DEEP", "IRON", "GLASS", "SALT", "VELVET", "HOLLOW",
                                       "AMBER", "BITTER", "SLOW", "WILD", "QUIET", "BROKEN", "VAST",
                                       "COPPER", "FERAL" };
            static const char* b[] = { "BLOOM", "CHOIR", "ENGINE", "TIDE", "SWARM", "LUNG", "SPIRE",
                                       "DRIFT", "REEF", "ORCHARD", "FURNACE", "MARSH", "LATTICE",
                                       "HOLLOW", "CHORUS", "VEIN" };
            const char* x = a[r.intRange (0, 16)];
            const char* y = b[r.intRange (0, 16)];
            std::memset (w.name, 0, sizeof (w.name));
            std::snprintf (w.name, sizeof (w.name), "%s %s", x, y);
        }
    };
}
