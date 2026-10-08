// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Chance events: small things that simply happen to the dish.
//
// They are not tied to anything the player did. A director rolls one every
// minute or two; most are common and gentle, a few are rare and a couple are
// mythic. Each one carries a visual (Anim), something the colony does (Kind) and
// a short line from the lab. A pity counter makes rare ones slowly more likely
// the longer none has landed, so a long session always gets its moment.
//
// Pure standard C++ so the test target can exercise it without JUCE.

#include "Storyline.h"
#include <array>
#include <cstdint>
#include <vector>

namespace mutagen::chance
{
    enum class Rarity : int { common, uncommon, rare, mythic };

    /** What the colony does. */
    enum class Kind : int
    {
        none,       // flavour and a little score only
        mutate,     // one mutation pass
        meteor,     // `param` local mutations at random points
        sweep,      // a mutation front crossing the dish
        prune,      // param/100 of the colony removed, roughest first
        enzyme,
        catalyst,
        warm,       // heat up
        cool,       // cool down
        cure,
        notes,      // sing `notes`
        scale,      // sing the run's own mode
        haunted,    // `param` is a haunted-sound recipe
        gate,       // `param` BPM tempo gate
        delay,      // `param` seconds of trip delay
        collect     // record the moment into the jar
    };

    /** Stateless particle animations drawn by the story FX layer. */
    enum class Anim : int
    {
        ripples, sporeRain, comet, eclipse, goldDust, staticBurst,
        lightning, bubbles, aurora, fireflies, snowSines, heartbeat
    };

    struct Chance
    {
        const char* name;
        const char* text;
        Rarity rarity;
        Kind kind;
        Anim anim;
        std::array<int, 4> notes;
        int param;
        float reward;       // score weight, same scale as other interactions
        int minAct;
    };

    constexpr int count = 24;
    constexpr int kNo = story::kNo;

    inline const std::array<Chance, count>& all()
    {
        using R = Rarity; using K = Kind; using A = Anim;
        static const std::array<Chance, count> e {{
            // ---- common -----------------------------------------------------
            { "DEW ON THE DISH", "A drop condenses on the lid and rolls through the dish. The colony hums at the ripple.", R::common, K::mutate, A::ripples, { kNo, kNo, kNo, kNo }, 0, 0.20f, 1 },
            { "WARM DRAUGHT", "The vent breathes out. The dish warms by a degree and everything in it speeds up a little.", R::common, K::warm, A::aurora, { kNo, kNo, kNo, kNo }, 0, 0.20f, 1 },
            { "COLD SNAP", "The lab door was left open overnight. The dish cools and the colony slows to a thoughtful drift.", R::common, K::cool, A::snowSines, { kNo, kNo, kNo, kNo }, 0, 0.20f, 1 },
            { "FIREFLY CELLS", "A few cells flash in sequence and the colony answers with a bright arpeggio. Nobody scheduled this.", R::common, K::notes, A::fireflies, { 0, 4, 7, 12 }, 0, 0.25f, 1 },
            { "STRAY SPORES", "Spores drift in from the vent. The colony digests the roughest ones and keeps the sweetest.", R::common, K::enzyme, A::sporeRain, { kNo, kNo, kNo, kNo }, 0, 0.25f, 1 },
            { "FALSE ALARM", "The lab alarm sounds for four seconds and stops. Moth logs it as 'weather'.", R::common, K::haunted, A::staticBurst, { kNo, kNo, kNo, kNo }, 4401, 0.10f, 2 },
            { "CONTACT HUM", "Something under the floor hums back at the dish for a moment. Sub declines to comment.", R::common, K::haunted, A::heartbeat, { kNo, kNo, kNo, kNo }, 2011, 0.15f, 3 },
            { "BUBBLE TRAIN", "A line of bubbles climbs the dish wall and pops in time. The colony takes the rhythm for itself.", R::common, K::gate, A::bubbles, { kNo, kNo, kNo, kNo }, 108, 0.20f, 2 },
            // ---- uncommon ---------------------------------------------------
            { "METEOR SHOWER", "A burst of cosmic rays crosses the dish. Five small mutations bloom where they land.", R::uncommon, K::meteor, A::comet, { kNo, kNo, kNo, kNo }, 5, 0.45f, 1 },
            { "SECOND WIND", "A catalyst nobody added starts working. The pitch wobbles, then quietly lets something go.", R::uncommon, K::catalyst, A::aurora, { kNo, kNo, kNo, kNo }, 0, 0.40f, 1 },
            { "SLOW TIDE", "A long soft wave crosses the dish from one wall to the other and changes everything it touches.", R::uncommon, K::sweep, A::ripples, { kNo, kNo, kNo, kNo }, 0, 0.40f, 2 },
            { "CROSSED WIRES", "Two cables touch. For six seconds the dish is gated at 140 beats a minute.", R::uncommon, K::gate, A::lightning, { kNo, kNo, kNo, kNo }, 140, 0.35f, 2 },
            { "ECHO LEAK", "Echo is leaking into the dish again. Everything repeats, a little later each time.", R::uncommon, K::delay, A::ripples, { kNo, kNo, kNo, kNo }, 30, 0.35f, 4 },
            { "TUNING FORK DROPPED", "Somewhere a tuning fork hits the floor. Two cells sing the same A and then, politely, a different one.", R::uncommon, K::notes, A::ripples, { 0, 0, 12, kNo }, 0, 0.35f, 1 },
            { "SPRING CLEAN", "Moth runs the cleaning cycle early. Infections are flushed and a few dead branches are removed.", R::uncommon, K::prune, A::bubbles, { kNo, kNo, kNo, kNo }, 8, 0.40f, 2 },
            { "STORM IN THE DISH", "A tiny thunderstorm builds over the dish. The colony mutates twice before it passes.", R::uncommon, K::meteor, A::lightning, { kNo, kNo, kNo, kNo }, 3, 0.45f, 3 },
            // ---- rare -------------------------------------------------------
            { "AURORA IN THE PETRI DISH", "Charged particles curtain across the glass. The colony sings a Lydian chord, the raised fourth floating above everything.", R::rare, K::notes, A::aurora, { 0, 4, 7, 11 }, 0, 0.80f, 2 },
            { "GOLDEN SPORE", "One spore falls gold. The enzymes find it first and the colony comes out brighter than it went in.", R::rare, K::enzyme, A::goldDust, { kNo, kNo, kNo, kNo }, 0, 1.20f, 1 },
            { "TOTAL ECLIPSE", "The lights fail. In the dark the weakest cells let go, and what is left sings lower and clearer.", R::rare, K::prune, A::eclipse, { kNo, kNo, kNo, kNo }, 15, 0.70f, 3 },
            { "THE HEARTBEAT", "A slow pulse at sixty beats a minute, from nowhere, from everything. The whole dish gates to it.", R::rare, K::gate, A::heartbeat, { kNo, kNo, kNo, kNo }, 60, 0.60f, 4 },
            { "UNSCHEDULED ENCORE", "The colony plays the whole of tonight's scale, unprompted, then bows. The jar records the applause.", R::rare, K::scale, A::fireflies, { kNo, kNo, kNo, kNo }, 0, 0.60f, 2 },
            // ---- mythic -----------------------------------------------------
            { "THE COLONY SINGS BACK", "Every cell finds the same chord at once, and for a moment the dish is one instrument. You were heard.", R::mythic, K::notes, A::goldDust, { 0, 7, 12, 16 }, 0, 2.00f, 1 },
            { "A CLAP FOR NO ONE", "One click, perfectly flat, containing every frequency at once. The colony rearranges itself around it.", R::mythic, K::collect, A::staticBurst, { kNo, kNo, kNo, kNo }, 0, 1.50f, 3 },
            { "THE MOTH'S GIFT", "MOTH: UNAUTHORISED GENEROSITY. I HAVE CURED YOUR INFECTIONS AND TIDIED YOUR WORST NOTES. DO NOT MENTION THIS TO THE TUNER.", R::mythic, K::cure, A::fireflies, { kNo, kNo, kNo, kNo }, 0, 1.80f, 5 }
        }};
        return e;
    }

    inline const char* rarityName (Rarity r)
    {
        constexpr const char* n[] = { "COMMON", "UNCOMMON", "RARE", "MYTHIC" };
        return n[std::clamp ((int) r, 0, 3)];
    }

    /** Rolls chance events for one run. */
    class Director
    {
    public:
        explicit Director (std::uint64_t seed) : rng (seed ^ 0xc4a9ce5eb1d0f00dull)
        {
            nextAtSec = 45.0 + rng.unit() * 45.0;
        }

        double nextAtSec = 60.0;

        /** Seconds until the next roll. */
        double gap() { return 55.0 + rng.unit() * 80.0; }

        /** Draws an event that is allowed in `act`, or -1. Rarer tiers get a
            pity bonus for every roll that has not produced one. */
        int draw (int act)
        {
            const double w[4] = { 60.0, 28.0, 10.0 + pity * 1.6, 2.0 + pity * 0.25 };
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                double total = 0.0;
                for (auto v : w) total += v;
                double pick = rng.unit() * total;
                int tier = 0;
                while (tier < 3 && pick >= w[tier]) pick -= w[tier++];

                std::vector<int> pool;
                for (int i = 0; i < count; ++i)
                {
                    const auto& c = all()[(std::size_t) i];
                    if ((int) c.rarity == tier && c.minAct <= act && ! recently (i)) pool.push_back (i);
                }
                if (pool.empty()) continue;

                const int id = pool[(std::size_t) rng.range (0, (int) pool.size() - 1)];
                pity = tier >= 2 ? 0 : std::min (pity + 1, 8);
                history[historyPos++ % history.size()] = id;
                return id;
            }
            return -1;
        }

    private:
        bool recently (int id) const
        {
            for (auto h : history) if (h == id) return true;
            return false;
        }

        story::Rng rng;
        int pity = 0;
        std::array<int, 3> history { -1, -1, -1 };
        std::size_t historyPos = 0;
    };
}
