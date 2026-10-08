// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The Music Facts engine: 30,000 educational facts about sound, pitch and
// rhythm, addressed by a fixed ID space.
//
// Every fact is produced by a generator from its ID in O(1). Nothing is stored
// per fact: the generators compute their numbers from small theory tables
// (equal temperament, interval and chord spellings, scale patterns, tempo maths,
// the wave equation) and pick one of several phrasings by ID. A later step can
// embed curated facts through curatedText(); when it returns non-null that text
// wins over the computed filler for the same slot.
//
// Presentation picks demos, not IDs: Deck runs a 1-in-5 cadence (one
// demonstrable fact per five draws) over an affine permutation, so every ID is
// shown exactly once per pass.
//
// Pure standard C++17 so the test target can exercise it without JUCE.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mutagen::facts
{
    constexpr int kFactCount = 30000;
    constexpr int kDemoBonusScore = 9344;

    /** What the colony plays while a demonstrable fact is shown. MIDI numbers
        are 21..108; unused note slots hold -1. For Kind::scale, notes holds the
        first six scale tones and fxId is the scale index (see scaleSemitones). */
    struct Demo
    {
        enum class Kind { none, note, pianoKey, interval, chord, scale, octavePair, keySound,
                          circleOfFifths, ghostNote, instrumentColour, effect, songMotif,
                          dopplerPass, beats };

        Kind kind = Kind::none;
        std::array<int, 6> notes { { -1, -1, -1, -1, -1, -1 } };
        int noteCount = 0;
        int durationMs = 0;
        int fxId = 0;
    };

    struct Fact
    {
        std::string text;
        std::string category;
        Demo demo;
        bool demonstrable = false;
    };

    /** Full fact for an ID in [0, kFactCount). Out-of-range IDs return an empty fact. */
    Fact factAt (int id);

    /** Same answer as factAt(id).demonstrable, without building the text. */
    bool isDemonstrable (int id);

    /** Hook for curated facts. curatedIndex is 0..2999 (ID 27000 + index).
        Returns nullptr for slots that still use computed filler. */
    const char* curatedText (int curatedIndex);

    /** Number of scale definitions and their semitone offsets from the tonic. */
    int scaleCount();
    const std::vector<int>& scaleSemitones (int scaleIndex);

    /** Knowledge points for a fact draw: deterministic, always 1..4. */
    int knowledgePoints (uint64_t runSeed, uint32_t counter);

    /** Hands out fact IDs. Each pass over the ID space visits every ID exactly once.
        Cadence: slots come in blocks of five with exactly one demo slot per block,
        the position of that slot shuffled per block by the seed. A demo slot yields
        a demonstrable ID and a non-demo slot a non-demonstrable one, until one class
        is exhausted within the current pass. Coverage is kept with a per-pass
        30,000-bit visited set (3,750 bytes). The scan cursor is an affine
        permutation slot = (a * k + b) mod kFactCount with gcd(a, kFactCount) = 1. */
    class Deck
    {
    public:
        Deck (uint64_t runSeed, uint32_t counter);

        /** Consumes the next slot and returns its fact ID. */
        int next();

        /** True when the next slot (the one next() will consume) is a demo slot. */
        bool nextShouldBeDemo() const;

        /** Number of slots consumed so far; persist this to resume. */
        uint32_t counter() const { return counter_; }

    private:
        uint64_t seed_;
        uint64_t a_ = 1;
        uint64_t b_ = 0;
        uint32_t counter_ = 0;
        uint32_t cursor_ = 0;
        int visitedCount_ = 0;
        int demoVisited_ = 0;
        std::vector<uint64_t> visited_;

        bool demoSlotAt (uint32_t slot) const;
        void startPassIfDone();
    };

    /** Set of seen fact IDs, 30,000 bits, persisted as base64. */
    class SeenSet
    {
    public:
        static constexpr int kBytes = (kFactCount + 7) / 8;   // 3750

        void mark (int id);
        bool test (int id) const;
        int count() const;
        std::string toBase64() const;
        static SeenSet fromBase64 (const std::string& encoded);

    private:
        std::array<uint8_t, kBytes> bits_ {};
    };
}
