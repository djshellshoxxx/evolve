// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "MusicFacts.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <numeric>

namespace mutagen::facts
{
namespace
{
    // ---------------------------------------------------------------- helpers

    uint64_t mix64 (uint64_t z)
    {
        z += 0x9e3779b97f4a7c15ull;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    int mod12 (int x) { return ((x % 12) + 12) % 12; }
    int mod7 (int x)  { return ((x % 7) + 7) % 7; }

#if defined(__GNUC__) || defined(__clang__)
    std::string fmt (const char* pattern, ...) __attribute__ ((format (printf, 1, 2)));
#endif
    std::string fmt (const char* pattern, ...)
    {
        char buffer[512];
        va_list args;
        va_start (args, pattern);
        std::vsnprintf (buffer, sizeof buffer, pattern, args);
        va_end (args);
        return buffer;
    }

    /** Picks one of n phrasings from the ID, so neighbouring IDs read differently. */
    int pick (int id, int n) { return (int) (mix64 ((uint64_t) id * 2654435761ull + 17) % (uint64_t) n); }

    std::string join (const std::vector<std::string>& parts, const char* sep)
    {
        std::string out;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0) out += sep;
            out += parts[i];
        }
        return out;
    }

    std::string capitalise (std::string s)
    {
        if (! s.empty()) s[0] = (char) std::toupper ((unsigned char) s[0]);
        return s;
    }

    // ---------------------------------------------------------- note naming
    //
    // Spelling rule. A spelled note is a letter (C..B) plus an accidental. Its letter
    // is the root letter moved by a number of letter steps (a third is 2 steps, a
    // fifth 4 steps, and so on), and its accidental is whatever is needed to hit the
    // target pitch class. Chords and intervals use their own letter steps, so a
    // B-flat minor seventh is B-flat D-flat F A-flat: the letters are B D F A and the
    // accidentals fall out of the pitch classes.
    //
    // Scales use letter step = round(semitones * 7 / 12) (integer form (s*7+6)/12).
    // This gives the conventional spelling for every seven-note mode and for the
    // pentatonic scales; for the whole-tone and blues scales it is a valid spelling
    // that may use flats where a sharp-based reading would also be correct.
    //
    // Roots use one name per pitch class, chosen by the key convention: flat names
    // for Db, Eb, Ab and Bb, and sharp or natural names otherwise.

    const char kLetters[] = "CDEFGAB";
    const int kNatPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const char* kAbsNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const char* kRootNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const int kRootLetter[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };

    /** Spell the note `semis` above a root, reached by `steps` letter steps from it. */
    std::string spellAt (int rootPc, int steps, int semis)
    {
        const int rootLetter = kRootLetter[mod12 (rootPc)];
        const int target = mod12 (rootPc + semis);
        const int letter = mod7 (rootLetter + steps);
        int acc = mod12 (target - kNatPc[letter]);
        if (acc > 6) acc -= 12;

        std::string s (1, kLetters[letter]);
        switch (acc)
        {
            case  0: break;
            case  1: s += "#"; break;
            case -1: s += "b"; break;
            case  2: s += "##"; break;
            case -2: s += "bb"; break;
            default: s += "?"; break;   // never reached by the tables; the test rejects '?'
        }
        return s;
    }

    /** Accidental (-2..2) of the note `steps` letters and `semis` semitones above a root. */
    int accidentalOf (int rootPc, int steps, int semis)
    {
        const int letter = mod7 (kRootLetter[mod12 (rootPc)] + steps);
        int acc = mod12 (mod12 (rootPc + semis) - kNatPc[letter]);
        if (acc > 6) acc -= 12;
        return acc;
    }

    /** Spelled note with its scientific octave. The octave comes from the MIDI number
        of the note, so B#3 and C4 agree and Cb4 is the B below C4. */
    std::string spelledWithOctave (int rootPc, int steps, int semis, int midi)
    {
        const int acc = accidentalOf (rootPc, steps, semis);
        const int natural = midi - acc;
        const int octave = natural / 12 - 1;
        return spellAt (rootPc, steps, semis) + std::to_string (octave);
    }

    /** Absolute note name with scientific octave: MIDI 60 is C4. */
    std::string absName (int midi)
    {
        return std::string (kAbsNames[mod12 (midi)]) + std::to_string (midi / 12 - 1);
    }

    double midiFreq (double midi, double a4)
    {
        return a4 * std::pow (2.0, (midi - 69.0) / 12.0);
    }

    double pitchFreq (int midi, double a4) { return midiFreq ((double) midi, a4); }

    // ------------------------------------------------------------- tables

    struct IntervalDef { int semis; int steps; const char* name; };
    const IntervalDef kIntervals[25] = {
        {  0, 0, "unison" },            {  1, 1, "minor second" },      {  2, 1, "major second" },
        {  3, 2, "minor third" },       {  4, 2, "major third" },       {  5, 3, "perfect fourth" },
        {  6, 3, "augmented fourth" },  {  7, 4, "perfect fifth" },     {  8, 5, "minor sixth" },
        {  9, 5, "major sixth" },       { 10, 6, "minor seventh" },     { 11, 6, "major seventh" },
        { 12, 7, "octave" },            { 13, 8, "minor ninth" },       { 14, 8, "major ninth" },
        { 15, 9, "minor tenth" },       { 16, 9, "major tenth" },       { 17, 10, "perfect eleventh" },
        { 18, 10, "augmented eleventh" },{ 19, 11, "perfect twelfth" }, { 20, 12, "minor thirteenth" },
        { 21, 12, "major thirteenth" }, { 22, 13, "minor fourteenth" }, { 23, 13, "major fourteenth" },
        { 24, 14, "double octave" }
    };

    struct ChordDef
    {
        const char* name;
        int n;
        int semis[5];
        int steps[5];
    };
    const ChordDef kChords[24] = {
        { "major",               3, { 0, 4, 7, 0, 0 },        { 0, 2, 4, 0, 0 } },
        { "minor",               3, { 0, 3, 7, 0, 0 },        { 0, 2, 4, 0, 0 } },
        { "diminished",          3, { 0, 3, 6, 0, 0 },        { 0, 2, 4, 0, 0 } },
        { "augmented",           3, { 0, 4, 8, 0, 0 },        { 0, 2, 4, 0, 0 } },
        { "suspended two",       3, { 0, 2, 7, 0, 0 },        { 0, 1, 4, 0, 0 } },
        { "suspended four",      3, { 0, 5, 7, 0, 0 },        { 0, 3, 4, 0, 0 } },
        { "major sixth",         4, { 0, 4, 7, 9, 0 },        { 0, 2, 4, 5, 0 } },
        { "minor sixth",         4, { 0, 3, 7, 9, 0 },        { 0, 2, 4, 5, 0 } },
        { "dominant seventh",    4, { 0, 4, 7, 10, 0 },       { 0, 2, 4, 6, 0 } },
        { "major seventh",       4, { 0, 4, 7, 11, 0 },       { 0, 2, 4, 6, 0 } },
        { "minor seventh",       4, { 0, 3, 7, 10, 0 },       { 0, 2, 4, 6, 0 } },
        { "half-diminished seventh", 4, { 0, 3, 6, 10, 0 },   { 0, 2, 4, 6, 0 } },
        { "diminished seventh",  4, { 0, 3, 6, 9, 0 },        { 0, 2, 4, 6, 0 } },
        { "dominant ninth",      5, { 0, 4, 7, 10, 14 },      { 0, 2, 4, 6, 1 } },
        { "major ninth",         5, { 0, 4, 7, 11, 14 },      { 0, 2, 4, 6, 1 } },
        { "minor ninth",         5, { 0, 3, 7, 10, 14 },      { 0, 2, 4, 6, 1 } },
        { "dominant eleventh",   5, { 0, 4, 7, 10, 17 },      { 0, 2, 4, 6, 3 } },
        { "dominant thirteenth", 5, { 0, 4, 7, 10, 21 },      { 0, 2, 4, 6, 5 } },
        { "add nine",            4, { 0, 4, 7, 14, 0 },       { 0, 2, 4, 1, 0 } },
        { "seven suspended four",4, { 0, 5, 7, 10, 0 },       { 0, 3, 4, 6, 0 } },
        { "dominant seven sharp nine", 5, { 0, 4, 7, 10, 15 }, { 0, 2, 4, 6, 1 } },
        { "dominant seven flat nine",  5, { 0, 4, 7, 10, 13 }, { 0, 2, 4, 6, 1 } },
        { "augmented seventh",   4, { 0, 4, 8, 10, 0 },       { 0, 2, 4, 6, 0 } },
        { "minor major seventh", 4, { 0, 3, 7, 11, 0 },       { 0, 2, 4, 6, 0 } }
    };

    struct ScaleDef
    {
        const char* name;
        int n;
        int semis[8];   // semitones above the tonic
        int deg[8];     // scale-degree number of each tone (1..7), spelled by letter
    };
    const ScaleDef kScales[36] = {
        { "major", 7, { 0, 2, 4, 5, 7, 9, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "dorian", 7, { 0, 2, 3, 5, 7, 9, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "phrygian", 7, { 0, 1, 3, 5, 7, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "lydian", 7, { 0, 2, 4, 6, 7, 9, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "aeolian", 7, { 0, 2, 3, 5, 7, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "locrian", 7, { 0, 1, 3, 5, 6, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "harmonic minor", 7, { 0, 2, 3, 5, 7, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "melodic minor", 7, { 0, 2, 3, 5, 7, 9, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "harmonic major", 7, { 0, 2, 4, 5, 7, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "major pentatonic", 5, { 0, 2, 4, 7, 9, 0, 0, 0 }, { 1, 2, 3, 5, 6, 0, 0, 0 } },
        { "minor pentatonic", 5, { 0, 3, 5, 7, 10, 0, 0, 0 }, { 1, 3, 4, 5, 7, 0, 0, 0 } },
        { "blues", 6, { 0, 3, 5, 6, 7, 10, 0, 0 }, { 1, 3, 4, 5, 5, 7, 0, 0 } },
        { "whole tone", 6, { 0, 2, 4, 6, 8, 10, 0, 0 }, { 1, 2, 3, 4, 5, 6, 0, 0 } },
        { "dorian flat two", 7, { 0, 1, 3, 5, 7, 9, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "lydian dominant", 7, { 0, 2, 4, 6, 7, 9, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "mixolydian flat six", 7, { 0, 2, 4, 5, 7, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "locrian sharp two", 7, { 0, 2, 3, 5, 6, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "altered", 7, { 0, 1, 3, 4, 6, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "phrygian dominant", 7, { 0, 1, 4, 5, 7, 8, 10, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "double harmonic", 7, { 0, 1, 4, 5, 7, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "hungarian minor", 7, { 0, 2, 3, 6, 7, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "neapolitan minor", 7, { 0, 1, 3, 5, 7, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "neapolitan major", 7, { 0, 1, 3, 5, 7, 9, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "enigmatic", 7, { 0, 1, 4, 6, 8, 10, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "hirajoshi", 5, { 0, 2, 3, 7, 8, 0, 0, 0 }, { 1, 2, 3, 5, 6, 0, 0, 0 } },
        { "in sen", 5, { 0, 1, 5, 7, 10, 0, 0, 0 }, { 1, 2, 4, 5, 7, 0, 0, 0 } },
        { "iwato", 5, { 0, 1, 5, 6, 10, 0, 0, 0 }, { 1, 2, 4, 5, 7, 0, 0, 0 } },
        { "yo", 5, { 0, 2, 5, 7, 9, 0, 0, 0 }, { 1, 2, 4, 5, 6, 0, 0, 0 } },
        { "egyptian", 5, { 0, 2, 5, 7, 10, 0, 0, 0 }, { 1, 2, 4, 5, 7, 0, 0, 0 } },
        { "bebop dominant", 8, { 0, 2, 4, 5, 7, 9, 10, 11 }, { 1, 2, 3, 4, 5, 6, 7, 7 } },
        { "bebop major", 8, { 0, 2, 4, 5, 7, 8, 9, 11 }, { 1, 2, 3, 4, 5, 5, 6, 7 } },
        { "major blues", 6, { 0, 2, 3, 4, 7, 9, 0, 0 }, { 1, 2, 3, 3, 5, 6, 0, 0 } },
        { "persian", 7, { 0, 1, 4, 5, 6, 8, 11, 0 }, { 1, 2, 3, 4, 5, 6, 7, 0 } },
        { "prometheus", 6, { 0, 2, 4, 6, 9, 10, 0, 0 }, { 1, 2, 3, 4, 6, 7, 0, 0 } },
        { "tritone", 6, { 0, 1, 4, 6, 7, 10, 0, 0 }, { 1, 2, 3, 5, 5, 7, 0, 0 } }
    };


    struct InstrumentDef { const char* name; int concertOffset; };
    // Concert pitch = written pitch + offset (semitones, mod 12).
    const InstrumentDef kInstruments[30] = {
        { "B-flat clarinet", -2 },        { "A clarinet", -3 },              { "E-flat clarinet", 3 },
        { "B-flat bass clarinet", -2 },   { "B-flat trumpet", -2 },          { "A trumpet", -3 },
        { "D trumpet", 2 },              { "E-flat trumpet", -9 },          { "F trumpet", -7 },
        { "B-flat cornet", -2 },          { "B-flat flugelhorn", -2 },       { "E-flat alto saxophone", -9 },
        { "B-flat tenor saxophone", -2 }, { "E-flat baritone saxophone", -9 },{ "B-flat soprano saxophone", -2 },
        { "E-flat sopranino saxophone", -9 }, { "B-flat bass saxophone", -2 }, { "F horn", -7 },
        { "E-flat horn", -9 },            { "B-flat tenor horn", -2 },       { "English horn", -7 },
        { "oboe d'amore", -3 },           { "G clarinet", -5 },              { "F clarinet", -7 },
        { "alto flute in G", -5 },        { "E-flat alto clarinet", -9 },    { "B-flat contrabass clarinet", -2 },
        { "E-flat soprano clarinet", 3 }, { "A piccolo trumpet", -3 },       { "B-flat bass trumpet", -2 }
    };

    const char* kFlatNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    constexpr int kTuningRefs[3] = { 440, 432, 415 };
    constexpr int kBpmFirst = 60;
    constexpr int kBpmCount = 180;
    constexpr double kBaseBeats[4] = { 2.0, 1.0, 0.5, 0.25 };
    const char* kBaseNames[4] = { "half note", "quarter note", "eighth note", "sixteenth note" };
    const char* kModNames[3] = { "", "dotted ", "triplet " };
    constexpr double kModFactor[3] = { 1.0, 1.5, 2.0 / 3.0 };
    constexpr double kSpeeds[20] = { 1, 2, 3, 5, 10, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80, 90, 100, 120, 150, 200 };
    constexpr int kSampleRates[11] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 };

    // ------------------------------------------------------------ families
    //
    // The ID space is split into families in this fixed order. Each family has a
    // fixed size; the last family (ear-training pairs) takes whatever remains, so
    // the sizes always add up to kFactCount. IDs 27000..29999 are the curated
    // slots: a non-null curatedText() replaces the computed text there.

    enum Family
    {
        famTuning, famCents, famPiano, famInterval, famChord, famChordInv, famScale,
        famKeySig, famCircle, famBpm, famHarmonic, famFreqWl, famDb, famNyquist,
        famTranspose, famDoppler, famIntervalCents, famOctInterval, famOctChord,
        famOctScale, famDrillInterval, famDrillDesc, famDrillChord, famDrillDegree,
        famPairs, famCount
    };

    constexpr int kCuratedStart = 27000;
    constexpr int kCuratedCount = 3000;

    int degreeSlots()
    {
        int n = 0;
        for (const auto& s : kScales) n += s.n;
        return n;
    }

    int familySize (int f)
    {
        switch (f)
        {
            case famTuning:        return 88 * 3;
            case famCents:         return 88 * 2;
            case famPiano:         return 88;
            case famInterval:      return 2 * 25 * 12;
            case famChord:         return 24 * 12;
            case famChordInv:      return 2 * 24 * 12;
            case famScale:         return 36 * 12;
            case famKeySig:        return 24;
            case famCircle:        return 12;
            case famBpm:           return kBpmCount * 12;
            case famHarmonic:      return 60 * 16;
            case famFreqWl:        return 600;
            case famDb:            return 3 * 20;
            case famNyquist:       return 11 + 32 + 20;
            case famTranspose:     return 30 * 12;
            case famDoppler:       return 20 * 2;
            case famIntervalCents: return 25 * 2;
            case famOctInterval:   return 25 * 12 * 7 * 2;
            case famOctChord:      return 24 * 12 * 7;
            case famOctScale:      return 36 * 12 * 7;
            case famDrillInterval: return 1900;   // sum over s = 0..24 of (88 - s)
            case famDrillDesc:     return 1900;
            case famDrillChord:    return 24 * 12;
            case famDrillDegree:   return 12 * degreeSlots();
            default:               return 0;      // famPairs: remainder
        }
    }

    void locate (int id, int& family, int& local)
    {
        for (int f = 0; f < famPairs; ++f)
        {
            const int size = familySize (f);
            if (id < size) { family = f; local = id; return; }
            id -= size;
        }
        family = famPairs;
        local = id;
    }

    // ---------------------------------------------------------------- demos

    Demo demoOf (Demo::Kind kind, std::initializer_list<int> notes, int durationMs, int fxId = 0)
    {
        Demo d;
        d.kind = kind;
        d.durationMs = durationMs;
        d.fxId = fxId;
        int i = 0;
        for (int n : notes)
        {
            if (i < 6) d.notes[(size_t) i] = n;
            ++i;
        }
        d.noteCount = std::min (i, 6);
        return d;
    }

    Fact make (std::string text, const char* category, Demo demo = Demo(), bool demonstrable = false)
    {
        Fact f;
        f.text = std::move (text);
        f.category = category;
        f.demo = demo;
        f.demonstrable = demonstrable;
        return f;
    }

    /** "a major third", "an augmented fourth", "a unison". */
    std::string articled (const std::string& name)
    {
        const char c = name.empty() ? 'x' : name[0];
        const bool vowel = (c == 'a' || c == 'e' || c == 'i' || c == 'o');
        return std::string (vowel ? "an " : "a ") + name;
    }

    std::string joinNames (const std::vector<std::string>& names) { return join (names, " "); }

    std::string signatureText (int sharps)
    {
        if (sharps == 0) return "no sharps or flats";
        const int n = std::abs (sharps);
        const std::string word = (sharps > 0) ? (n == 1 ? "sharp" : "sharps") : (n == 1 ? "flat" : "flats");
        return std::to_string (n) + " " + word;
    }

    /** Key signature of a major tonic (pitch class): positive = sharps, negative = flats. */
    int fifthsOfMajor (int pc)
    {
        int s = (pc * 7) % 12;
        if (s > 6) s -= 12;
        return s;
    }

    // ---------------------------------------------------------- Tier A

    Fact genTuning (int local)
    {
        const int midi = 21 + local % 88;
        const int tuning = local / 88;
        const int ref = kTuningRefs[tuning];
        const double f = pitchFreq (midi, ref);
        const double wl = 343.0 / f;
        std::string text;
        switch (pick (local, 3))
        {
            case 0:  text = fmt ("MIDI %d (%s) is %.2f Hz when A=%d. Its wavelength in air at 343 m/s is %.2f m.", midi, absName (midi).c_str(), f, ref, wl); break;
            case 1:  text = fmt ("At A=%d, %s (MIDI %d) vibrates at %.2f Hz, a wavelength of %.2f m in air.", ref, absName (midi).c_str(), midi, f, wl); break;
            default: text = fmt ("Tuned to A=%d, note %s (MIDI %d) runs at %.2f Hz; in air at 343 m/s that is %.2f m long.", ref, absName (midi).c_str(), midi, f, wl); break;
        }
        Demo demo;
        bool demonstrable = (tuning == 0);
        if (demonstrable) demo = demoOf (Demo::Kind::note, { midi }, 900);
        return make (text, "Pitch", demo, demonstrable);
    }

    Fact genCents (int local)
    {
        const int midi = 21 + local % 88;
        const int ref = local / 88 == 0 ? 432 : 415;
        const double lower = pitchFreq (midi, 440) - pitchFreq (midi, ref);
        const std::string name = absName (midi);
        std::string text = pick (local, 2) == 0
            ? fmt ("Moving from A=440 down to A=%d lowers %s (MIDI %d) by %.2f Hz.", ref, name.c_str(), midi, lower)
            : fmt ("%s (MIDI %d) sits %.2f Hz below its A=440 pitch when A is set to %d.", name.c_str(), midi, lower, ref);
        return make (text, "Pitch");
    }

    Fact genPiano (int local)
    {
        const int midi = 21 + local;
        const std::string name = absName (midi);
        std::string text = pick (local, 2) == 0
            ? fmt ("Piano key %d of 88 is %s, sounding at %.2f Hz when A=440.", local + 1, name.c_str(), pitchFreq (midi, 440))
            : fmt ("Key %d on a standard piano is %s, which vibrates at %.2f Hz.", local + 1, name.c_str(), pitchFreq (midi, 440));
        return make (text, "Keys", demoOf (Demo::Kind::pianoKey, { midi }, 900), true);
    }

    Fact genInterval (int local)
    {
        const int dir = local / 300;          // 0 ascending, 1 descending
        const int r = local % 300;
        const int s = r / 12;
        const int root = r % 12;
        const IntervalDef& d = kIntervals[s];
        const int steps = dir == 0 ? d.steps : -d.steps;
        const int semis = dir == 0 ? s : -s;
        const std::string rootName = kRootNames[root];
        const std::string target = spellAt (root, steps, semis);
        const std::string name = capitalise (d.name);
        std::string text;
        if (dir == 0)
            text = pick (local, 2) == 0 ? fmt ("%s above %s is %s.", name.c_str(), rootName.c_str(), target.c_str())
                                        : fmt ("Up from %s by %s: %s.", rootName.c_str(), d.name, target.c_str());
        else
            text = pick (local, 2) == 0 ? fmt ("%s below %s is %s.", name.c_str(), rootName.c_str(), target.c_str())
                                        : fmt ("Down from %s by %s: %s.", rootName.c_str(), d.name, target.c_str());
        const int lo = 60 + root;
        const int hi = lo + semis;
        return make (text, "Intervals", demoOf (Demo::Kind::interval, { lo, hi }, 1200), true);
    }

    Fact genChord (int local)
    {
        const int q = local / 12;
        const int root = local % 12;
        const ChordDef& c = kChords[q];
        std::vector<std::string> tones;
        for (int i = 0; i < c.n; ++i) tones.push_back (spellAt (root, c.steps[i], c.semis[i]));
        const std::string rootName = kRootNames[root];
        const std::string list = joinNames (tones);
        std::string text = pick (local, 2) == 0
            ? fmt ("%s %s chord: %s.", rootName.c_str(), c.name, list.c_str())
            : fmt ("The %s %s is built from %s.", rootName.c_str(), c.name, list.c_str());
        Demo demo;
        demo.kind = Demo::Kind::chord;
        demo.durationMs = 1800;
        demo.noteCount = c.n;
        for (int i = 0; i < c.n; ++i) demo.notes[(size_t) i] = 60 + root + c.semis[i];
        return make (text, "Chords", demo, true);
    }

    Fact genChordInv (int local)
    {
        const int inv = 1 + local / 288;      // first or second inversion
        const int rest = local % 288;
        const int q = rest / 12;
        const int root = rest % 12;
        const ChordDef& c = kChords[q];
        std::vector<std::string> tones;
        for (int i = 0; i < c.n; ++i) tones.push_back (spellAt (root, c.steps[i], c.semis[i]));
        std::vector<std::string> ordered;
        for (int i = 0; i < c.n; ++i) ordered.push_back (tones[(size_t) ((i + inv) % c.n)]);
        const std::string bass = ordered[0];
        std::vector<std::string> rest_ (ordered.begin() + 1, ordered.end());
        const char* word = inv == 1 ? "first" : "second";
        std::string text = fmt ("%s %s, %s inversion: bass %s, then %s.", kRootNames[root], c.name, word,
                                bass.c_str(), joinNames (rest_).c_str());
        return make (text, "Chords");
    }

    Fact genScale (int local)
    {
        const int sc = local / 12;
        const int root = local % 12;
        const ScaleDef& s = kScales[sc];
        std::vector<std::string> notes;
        for (int i = 0; i < s.n; ++i)
            notes.push_back (spellAt (root, s.deg[i] - 1, s.semis[i]));
        const std::string rootName = kRootNames[root];
        std::string text = pick (local, 2) == 0
            ? fmt ("%s %s scale: %s.", rootName.c_str(), s.name, joinNames (notes).c_str())
            : fmt ("The %s %s scale runs %s.", rootName.c_str(), s.name, joinNames (notes).c_str());
        Demo demo;
        demo.kind = Demo::Kind::scale;
        demo.durationMs = 2400;
        demo.fxId = sc;
        demo.noteCount = std::min (s.n, 6);
        for (int i = 0; i < demo.noteCount; ++i) demo.notes[(size_t) i] = 60 + root + s.semis[i];
        return make (text, "Scales", demo, true);
    }

    Fact genKeySig (int local)
    {
        const bool minor = local >= 12;
        const int pc = local % 12;
        if (! minor)
        {
            const int s = fifthsOfMajor (pc);
            std::string text = pick (local, 2) == 0
                ? fmt ("%s major key signature: %s.", kRootNames[pc], signatureText (s).c_str())
                : fmt ("The key of %s major uses %s.", kRootNames[pc], signatureText (s).c_str());
            Demo demo = demoOf (Demo::Kind::keySound, { 60 + pc }, 1200);
            return make (text, "Keys", demo, true);
        }
        const int relative = (pc + 3) % 12;
        const int s = fifthsOfMajor (relative);
        std::string text = fmt ("%s minor shares the signature of %s major: %s.", kRootNames[pc],
                                kRootNames[relative], signatureText (s).c_str());
        Demo demo = demoOf (Demo::Kind::keySound, { 57 + pc }, 1200);
        return make (text, "Keys", demo, true);
    }

    Fact genCircle (int local)
    {
        const int pc = (7 * local) % 12;
        const int next = (pc + 7) % 12;
        std::string text = fmt ("Circle of fifths, step %d of 12: %s up a perfect fifth is %s.", local + 1,
                                kRootNames[pc], kRootNames[next]);
        Demo demo = demoOf (Demo::Kind::circleOfFifths, { 60 + pc, 60 + next }, 1600);
        return make (text, "Keys", demo, true);
    }

    Fact genBpm (int local)
    {
        const int tempo = kBpmFirst + local / 12;
        const int v = local % 12;
        const int base = v / 3;
        const int mod = v % 3;
        const double beats = kBaseBeats[base] * kModFactor[mod];
        const double ms = 60000.0 / tempo * beats;
        const std::string name = std::string (kModNames[mod]) + kBaseNames[base];
        std::string text = pick (local, 2) == 0
            ? fmt ("At %d BPM, a %s lasts %.1f ms.", tempo, name.c_str(), ms)
            : fmt ("A %s at %d BPM is a delay of %.1f ms.", name.c_str(), tempo, ms);
        return make (text, "Rhythm");
    }

    Fact genHarmonic (int local)
    {
        const int fund = local / 16;
        const int n = local % 16 + 1;
        const int midi = 36 + fund;
        const double f0 = pitchFreq (midi, 440);
        const double fn = n * f0;
        const int nearest = (int) std::lround (69.0 + 12.0 * std::log2 (fn / 440.0));
        const double cents = 1200.0 * std::log2 (fn / pitchFreq (nearest, 440));
        const std::string fundName = absName (midi);
        const std::string nearName = absName (nearest);
        const long roundedCents = std::lround (cents);
        std::string text = pick (local, 2) == 0
            ? fmt ("Harmonic %d of %s (%.2f Hz) is %.2f Hz, nearest %s at %+ld cents.", n, fundName.c_str(), f0, fn, nearName.c_str(), roundedCents)
            : fmt ("The harmonic %d of %s sounds at %.2f Hz, closest to %s (%+ld cents).", n, fundName.c_str(), fn, nearName.c_str(), roundedCents);
        return make (text, "Harmonics");
    }

    Fact genFreqWl (int local)
    {
        const double f = 20.0 * std::pow (1000.0, local / 599.0);
        std::string text = pick (local, 2) == 0
            ? fmt ("A %.1f Hz tone has a wavelength of %.3f m in air and a period of %.3f ms.", f, 343.0 / f, 1000.0 / f)
            : fmt ("Sound at %.1f Hz: wavelength %.3f m at 343 m/s, period %.3f ms.", f, 343.0 / f, 1000.0 / f);
        return make (text, "Acoustics");
    }

    Fact genDb (int local)
    {
        const int sub = local / 20;
        const int k = local % 20 + 2;   // 2..21
        std::string text;
        if (sub == 0)
            text = fmt ("Amplitude times %d changes the level by %+.2f dB.", k, 20.0 * std::log10 ((double) k));
        else if (sub == 1)
            text = fmt ("Moving %d times farther from a point source changes its level by %+.2f dB (inverse square).", k, -20.0 * std::log10 ((double) k));
        else
            text = fmt ("Power times %d changes the level by %+.2f dB.", k, 10.0 * std::log10 ((double) k));
        return make (text, "Acoustics");
    }

    Fact genNyquist (int local)
    {
        if (local < 11)
        {
            const int rate = kSampleRates[local];
            std::string text = fmt ("A %d Hz sample rate has a Nyquist limit of %.1f Hz; anything above it folds back as aliasing.", rate, rate / 2.0);
            return make (text, "Digital audio");
        }
        if (local < 43)
        {
            const int bits = local - 11 + 1;
            std::string text = fmt ("%d-bit audio: about %.2f dB signal-to-quantisation-noise for a full-scale sine, 6.02 dB per bit.", bits, 6.02 * bits + 1.76);
            return make (text, "Digital audio");
        }
        const int i = local - 43;
        const double f = 23000.0 + 1000.0 * i;
        std::string text = fmt ("A %.0f Hz tone sampled at 44100 Hz aliases to %.0f Hz.", f, 44100.0 - f);
        return make (text, "Digital audio");
    }

    Fact genTranspose (int local)
    {
        const InstrumentDef& inst = kInstruments[local / 12];
        const int written = local % 12;
        const int concert = mod12 (written + inst.concertOffset);
        std::string text = pick (local, 2) == 0
            ? fmt ("On the %s, written %s sounds as %s.", inst.name, kFlatNames[written], kFlatNames[concert])
            : fmt ("A %s part written %s is heard as %s.", inst.name, kFlatNames[written], kFlatNames[concert]);
        return make (text, "Instruments");
    }

    Fact genDoppler (int local)
    {
        const int idx = local / 2;
        const int dir = local % 2;
        const double v = kSpeeds[idx];
        const double c = 343.0;
        const double f = 440.0;
        Demo demo = demoOf (Demo::Kind::dopplerPass, { 69 }, 2400, idx);
        if (dir == 0)
            return make (fmt ("A 440 Hz source moving toward a listener at %.0f m/s is heard at %.2f Hz.", v, f * c / (c - v)), "Doppler", demo, true);
        return make (fmt ("A 440 Hz source moving away at %.0f m/s is heard at %.2f Hz.", v, f * c / (c + v)), "Doppler", demo, true);
    }

    Fact genIntervalCents (int local)
    {
        const int s = local / 2;
        const IntervalDef& d = kIntervals[s];
        std::string text = local % 2 == 0
            ? fmt ("Equal temperament: %s spans %d cents.", d.name, 100 * s)
            : fmt ("Equal temperament: %s is a frequency ratio of %.4f to 1.", d.name, std::pow (2.0, s / 12.0));
        return make (text, "Intervals");
    }

    // Octave-placed variants (computed on the same tables, placed in a register).

    Fact genOctInterval (int local)
    {
        const int dir = local / 2100;
        const int r = local % 2100;
        const int octave = r / 300 + 1;
        const int rest = r % 300;
        const int s = rest / 12;
        const int root = rest % 12;
        const IntervalDef& d = kIntervals[s];
        const int steps = dir == 0 ? d.steps : -d.steps;
        const int semis = dir == 0 ? s : -s;
        const std::string target = spellAt (root, steps, semis);
        std::string text = dir == 0
            ? fmt ("In octave %d, %s above %s is %s.", octave, articled (d.name).c_str(), kRootNames[root], target.c_str())
            : fmt ("In octave %d, %s below %s is %s.", octave, articled (d.name).c_str(), kRootNames[root], target.c_str());
        return make (text, "Intervals");
    }

    Fact genOctChord (int local)
    {
        const int octave = local / 288 + 1;
        const int rest = local % 288;
        const ChordDef& c = kChords[rest / 12];
        const int root = rest % 12;
        std::vector<std::string> tones;
        for (int i = 0; i < c.n; ++i) tones.push_back (spellAt (root, c.steps[i], c.semis[i]));
        std::string text = fmt ("In octave %d, the %s %s chord is %s.", octave, kRootNames[root], c.name, joinNames (tones).c_str());
        return make (text, "Chords");
    }

    Fact genOctScale (int local)
    {
        const int octave = local / 432 + 1;
        const int rest = local % 432;
        const ScaleDef& s = kScales[rest / 12];
        const int root = rest % 12;
        std::vector<std::string> notes;
        for (int i = 0; i < s.n; ++i) notes.push_back (spellAt (root, s.deg[i] - 1, s.semis[i]));
        std::string text = fmt ("In octave %d, %s %s runs %s.", octave, kRootNames[root], s.name, joinNames (notes).c_str());
        return make (text, "Scales");
    }

    // ---------------------------------------------------------- Tier C drills

    Fact genDrillInterval (int local, bool descending)
    {
        int s = 0;
        for (; s <= 24; ++s)
        {
            const int count = 88 - s;
            if (local < count) break;
            local -= count;
        }
        const IntervalDef& d = kIntervals[s];
        int lo, hi;
        if (! descending)
        {
            lo = 21 + local;
            hi = lo + s;
        }
        else
        {
            hi = 21 + s + local;
            lo = hi - s;
        }
        const int loPc = mod12 (lo);
        const int hiPc = mod12 (hi);
        const std::string bottom = descending ? spelledWithOctave (hiPc, -d.steps, -s, lo)
                                              : spelledWithOctave (loPc, 0, 0, lo);
        const std::string top = descending ? spelledWithOctave (hiPc, 0, 0, hi)
                                           : spelledWithOctave (loPc, d.steps, s, hi);
        std::string text;
        if (! descending)
        {
            switch (pick (21 + local + 97 * s, 3))
            {
                case 0:  text = fmt ("This is %s above %s: %s. Listen.", articled (d.name).c_str(), bottom.c_str(), top.c_str()); break;
                case 1:  text = fmt ("Listen: %s above %s is %s.", articled (d.name).c_str(), bottom.c_str(), top.c_str()); break;
                default: text = fmt ("Ear check: %s above %s is %s.", articled (d.name).c_str(), bottom.c_str(), top.c_str()); break;
            }
        }
        else
        {
            switch (pick (21 + local + 97 * s, 3))
            {
                case 0:  text = fmt ("This is %s below %s: %s. Listen.", articled (d.name).c_str(), top.c_str(), bottom.c_str()); break;
                case 1:  text = fmt ("Listen: %s below %s is %s.", articled (d.name).c_str(), top.c_str(), bottom.c_str()); break;
                default: text = fmt ("Ear check: %s below %s is %s.", articled (d.name).c_str(), top.c_str(), bottom.c_str()); break;
            }
        }
        return make (text, "Drills", demoOf (Demo::Kind::interval, { lo, hi }, 1200), true);
    }

    Fact genDrillChord (int local)
    {
        const ChordDef& c = kChords[local / 12];
        const int root = local % 12;
        std::vector<std::string> tones;
        for (int i = 0; i < c.n; ++i) tones.push_back (spellAt (root, c.steps[i], c.semis[i]));
        const std::string list = joinNames (tones);
        std::string text = pick (local, 2) == 0
            ? fmt ("This is %s chord on %s: %s. Listen.", articled (c.name).c_str(), kRootNames[root], list.c_str())
            : fmt ("Listen for the %s chord rooted on %s: %s.", c.name, kRootNames[root], list.c_str());
        Demo demo;
        demo.kind = Demo::Kind::chord;
        demo.durationMs = 1800;
        demo.noteCount = c.n;
        for (int i = 0; i < c.n; ++i) demo.notes[(size_t) i] = 60 + root + c.semis[i];
        return make (text, "Drills", demo, true);
    }

    Fact genDrillDegree (int local)
    {
        const int slots = degreeSlots();
        const int root = local / slots;
        int rem = local % slots;
        int sc = 0;
        for (; sc < scaleCount(); ++sc)
        {
            if (rem < kScales[sc].n) break;
            rem -= kScales[sc].n;
        }
        const ScaleDef& s = kScales[sc];
        const int semis = s.semis[rem];
        const std::string note = spellAt (root, s.deg[rem] - 1, semis);
        std::string text = pick (local, 2) == 0
            ? fmt ("Degree %d of %s %s is %s. Listen.", rem + 1, kRootNames[root], s.name, note.c_str())
            : fmt ("Listen: note %d of %s %s is %s.", rem + 1, kRootNames[root], s.name, note.c_str());
        return make (text, "Drills", demoOf (Demo::Kind::note, { 60 + root + semis }, 900), true);
    }

    // ------------------------------------------------ ear-training pairs (filler)

    Fact genPairs (int local)
    {
        constexpr int kPairsPerDir = 88 * 87 / 2;   // 3828 pairs of keys, lower then higher
        const int base = local % (2 * kPairsPerDir);
        const int variant = local / (2 * kPairsPerDir);
        const int dir = base / kPairsPerDir;        // 0 ascending, 1 descending
        int p = base % kPairsPerDir;
        int lo = 0;
        for (; lo < 87; ++lo)
        {
            const int count = 87 - lo;
            if (p < count) break;
            p -= count;
        }
        const int hi = lo + 1 + p;
        const int semis = hi - lo;
        const std::string a = absName (21 + lo);
        const std::string b = absName (21 + hi);
        std::string text;
        if (variant == 0)
            text = dir == 0 ? fmt ("From %s up to %s is %d semitones.", a.c_str(), b.c_str(), semis)
                            : fmt ("From %s down to %s is %d semitones.", b.c_str(), a.c_str(), semis);
        else
            text = dir == 0 ? fmt ("%s to %s, going up, spans %d semitones.", a.c_str(), b.c_str(), semis)
                            : fmt ("%s to %s, going down, spans %d semitones.", b.c_str(), a.c_str(), semis);
        return make (text, "Pitch");
    }
}

    // ================================================================ public API

    Fact factAt (int id)
    {
        if (id < 0 || id >= kFactCount) return Fact();

        if (id >= kCuratedStart && id < kCuratedStart + kCuratedCount)
        {
            if (const char* curated = curatedText (id - kCuratedStart))
                return make (curated, "Curated");
        }

        int family = 0;
        int local = 0;
        locate (id, family, local);
        switch (family)
        {
            case famTuning:        return genTuning (local);
            case famCents:         return genCents (local);
            case famPiano:         return genPiano (local);
            case famInterval:      return genInterval (local);
            case famChord:         return genChord (local);
            case famChordInv:      return genChordInv (local);
            case famScale:         return genScale (local);
            case famKeySig:        return genKeySig (local);
            case famCircle:        return genCircle (local);
            case famBpm:           return genBpm (local);
            case famHarmonic:      return genHarmonic (local);
            case famFreqWl:        return genFreqWl (local);
            case famDb:            return genDb (local);
            case famNyquist:       return genNyquist (local);
            case famTranspose:     return genTranspose (local);
            case famDoppler:       return genDoppler (local);
            case famIntervalCents: return genIntervalCents (local);
            case famOctInterval:   return genOctInterval (local);
            case famOctChord:      return genOctChord (local);
            case famOctScale:      return genOctScale (local);
            case famDrillInterval: return genDrillInterval (local, false);
            case famDrillDesc:     return genDrillInterval (local, true);
            case famDrillChord:    return genDrillChord (local);
            case famDrillDegree:   return genDrillDegree (local);
            default:               return genPairs (local);
        }
    }

    bool isDemonstrable (int id)
    {
        return factAt (id).demonstrable;
    }

    const char* curatedText (int curatedIndex)
    {
        // Hook for curated facts (docs/data/curated_facts.json via tools/gen_facts.py).
        // Until that step lands, every slot falls back to computed filler.
        (void) curatedIndex;
        return nullptr;
    }

    int scaleCount()
    {
        return (int) (sizeof kScales / sizeof kScales[0]);
    }

    const std::vector<int>& scaleSemitones (int scaleIndex)
    {
        static const std::vector<std::vector<int>> table = [] {
            std::vector<std::vector<int>> t;
            for (const auto& s : kScales) t.emplace_back (s.semis, s.semis + s.n);
            return t;
        }();
        return table[(size_t) std::max (0, std::min (scaleIndex, (int) table.size() - 1))];
    }

    int knowledgePoints (uint64_t runSeed, uint32_t counter)
    {
        const uint64_t h = mix64 (runSeed ^ mix64 (0xc0ffee00ull + counter));
        return 1 + (int) (h % 4);
    }

    // ------------------------------------------------------------------ Deck

    namespace
    {
        constexpr uint64_t kN = kFactCount;

        uint64_t gcd64 (uint64_t a, uint64_t b)
        {
            while (b != 0) { const uint64_t t = a % b; a = b; b = t; }
            return a;
        }

        const std::vector<bool>& demoMask()
        {
            static const std::vector<bool> mask = [] {
                std::vector<bool> m ((size_t) kFactCount);
                for (int id = 0; id < kFactCount; ++id) m[(size_t) id] = isDemonstrable (id);
                return m;
            }();
            return mask;
        }

        int demoTotal()
        {
            static const int total = [] {
                int n = 0;
                for (bool b : demoMask()) n += b ? 1 : 0;
                return n;
            }();
            return total;
        }
    }

    Deck::Deck (uint64_t runSeed, uint32_t counter)
        : seed_ (mix64 (runSeed ^ 0x5eedf00dull))
    {
        const uint64_t r = mix64 (seed_ + 1);
        a_ = 1 + r % (kN - 1);
        while (gcd64 (a_, kN) != 1)
            a_ = (a_ + 1 >= kN) ? 1 : a_ + 1;
        b_ = mix64 (seed_ + 2) % kN;
        visited_.assign ((size_t) (kN + 63) / 64, 0);

        // Replay so that a persisted counter resumes exactly where it stopped.
        for (uint32_t i = 0; i < counter; ++i)
            next();
    }

    bool Deck::demoSlotAt (uint32_t slot) const
    {
        const uint64_t block = slot / 5;
        const uint32_t position = slot % 5;
        const uint32_t demoPosition = (uint32_t) (mix64 (seed_ ^ mix64 (block + 77)) % 5);
        return position == demoPosition;
    }

    bool Deck::nextShouldBeDemo() const
    {
        return demoSlotAt (counter_);
    }

    void Deck::startPassIfDone()
    {
        if (visitedCount_ < kFactCount) return;
        std::fill (visited_.begin(), visited_.end(), 0ull);
        visitedCount_ = 0;
        demoVisited_ = 0;
    }

    int Deck::next()
    {
        startPassIfDone();

        const auto& mask = demoMask();
        const int demos = demoTotal();
        const bool wantDemo = demoSlotAt (counter_);

        // The 1-in-5 cadence always holds. A class that has been fully shown
        // starts its own new pass (it repeats) instead of borrowing the other class.
        const auto restartClass = [&] (bool demoClass)
        {
            for (int id = 0; id < kFactCount; ++id)
                if (mask[(size_t) id] == demoClass)
                    visited_[(size_t) (id >> 6)] &= ~(1ull << (id & 63));
            if (demoClass) { visitedCount_ -= demoVisited_; demoVisited_ = 0; }
            else           { visitedCount_ = demoVisited_; }
        };
        if (wantDemo && demoVisited_ >= demos) restartClass (true);
        if (! wantDemo && visitedCount_ - demoVisited_ >= kFactCount - demos) restartClass (false);

        for (uint32_t j = 0; j < (uint32_t) kFactCount; ++j)
        {
            const uint32_t position = (cursor_ + j) % (uint32_t) kFactCount;
            const int id = (int) ((a_ * position + b_) % kN);
            const uint64_t bit = 1ull << (id & 63);
            if ((visited_[(size_t) (id >> 6)] & bit) != 0) continue;
            if (mask[(size_t) id] != wantDemo) continue;

            visited_[(size_t) (id >> 6)] |= bit;
            ++visitedCount_;
            if (mask[(size_t) id]) ++demoVisited_;
            cursor_ = (position + 1) % (uint32_t) kFactCount;
            ++counter_;
            return id;
        }

        // Unreachable: every pass has an unvisited ID of each class until it is exhausted.
        ++counter_;
        return 0;
    }

    // -------------------------------------------------------------- SeenSet

    void SeenSet::mark (int id)
    {
        if (id < 0 || id >= kFactCount) return;
        bits_[(size_t) (id >> 3)] |= (uint8_t) (1u << (id & 7));
    }

    bool SeenSet::test (int id) const
    {
        if (id < 0 || id >= kFactCount) return false;
        return (bits_[(size_t) (id >> 3)] >> (id & 7)) & 1u;
    }

    int SeenSet::count() const
    {
        int n = 0;
        for (uint8_t byte : bits_)
            for (int b = 0; b < 8; ++b)
                n += (byte >> b) & 1;
        return n;
    }

    std::string SeenSet::toBase64() const
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve ((kBytes + 2) / 3 * 4);
        for (size_t i = 0; i < bits_.size(); i += 3)
        {
            const uint32_t b0 = bits_[i];
            const uint32_t b1 = i + 1 < bits_.size() ? bits_[i + 1] : 0;
            const uint32_t b2 = i + 2 < bits_.size() ? bits_[i + 2] : 0;
            const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
            out += alphabet[(v >> 18) & 63];
            out += alphabet[(v >> 12) & 63];
            out += (i + 1 < bits_.size()) ? alphabet[(v >> 6) & 63] : '=';
            out += (i + 2 < bits_.size()) ? alphabet[v & 63] : '=';
        }
        return out;
    }

    SeenSet SeenSet::fromBase64 (const std::string& encoded)
    {
        auto value = [] (char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };

        SeenSet set;
        if (encoded.size() % 4 != 0) return SeenSet();

        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < encoded.size(); i += 4)
        {
            int v[4];
            int pad = 0;
            for (int k = 0; k < 4; ++k)
            {
                const char c = encoded[i + (size_t) k];
                if (c == '=' && i + 4 == encoded.size() && k >= 2) { v[k] = 0; ++pad; continue; }
                v[k] = value (c);
                if (v[k] < 0) return SeenSet();
            }
            const uint32_t word = ((uint32_t) v[0] << 18) | ((uint32_t) v[1] << 12) | ((uint32_t) v[2] << 6) | (uint32_t) v[3];
            bytes.push_back ((uint8_t) (word >> 16));
            if (pad < 2) bytes.push_back ((uint8_t) (word >> 8));
            if (pad < 1) bytes.push_back ((uint8_t) word);
        }

        if (bytes.size() != (size_t) kBytes) return SeenSet();
        std::copy (bytes.begin(), bytes.end(), set.bits_.begin());
        return set;
    }
}
