// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Secret events, secret rooms and the odd visitors.
//
// A secret fires when the player clicks the culture chamber at the right
// moment: at the top of a minute, on the golden ratio, during an echo, at
// 11:11, on a leap day. Some will happen by accident in the first few minutes;
// some need a calendar. Each one opens its own room, hands over its own skill,
// bursts its own orbs and plays its own sound.
//
// Pure standard C++ so the test target can exercise every rule without JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <cctype>
#include <string>

namespace mutagen::secrets
{
    enum class Tier : int { easy, medium, hard, legendary };

    /** What the chamber does when a secret opens. */
    enum class Burst : int
    {
        special, rainbow, glowing, monster, mini, snow, inversion, speed, rockets
    };

    /** The skill a secret hands over - an audible change to the colony. */
    enum class Skill : int
    {
        mutate, radiate, gate, delay, chord, scale, haunted, rockets
    };

    struct Secret
    {
        Tier tier;
        const char* name;
        const char* room;       // the secret room it opens
        const char* host;       // who is waiting in the room
        const char* line;       // what they say (one true audio fact where it fits)
        const char* skillName;
        Skill skill;
        int skillParam;         // gate BPM / delay seconds / chord root offset
        Burst burst;
        int burstCount;
        unsigned int hue;       // room colour, 0xRRGGBB
        int pattern;            // room wallpaper 0..5
    };

    constexpr int secretCount = 39;

    inline const std::array<Secret, secretCount>& all()
    {
        static const std::array<Secret, secretCount> s {{
            // ---- easy: most players stumble into these ------------------
            { Tier::easy, "TOP OF THE MINUTE", "The Metronome Closet", "Tik", "Every minute starts on a downbeat. Most music does too: beat one of the bar.", "DOWNBEAT", Skill::gate, 60, Burst::special, 40, 0xf2c14e, 0 },
            { Tier::easy, "DEAD CENTRE", "The Mono Room", "Phantom Centre", "A sound panned to the middle comes equally from both speakers. Your brain builds a phantom image between them.", "PHANTOM CENTRE", Skill::chord, 0, Burst::rainbow, 30, 0x4fb6c4, 1 },
            { Tier::easy, "FOUR CORNERS", "The Corner Shop of Room Modes", "Mode", "Bass piles up in room corners, where every reflecting surface meets. Studios put bass traps there.", "BASS TRAP", Skill::haunted, 3101, Burst::monster, 6, 0x7bc96f, 2 },
            { Tier::easy, "DRUM ROLL", "The Snare Cupboard", "Buzz", "A drum roll is fast enough that single hits blur into one sustained sound - above about 20 hits a second it starts to hum.", "DRUM ROLL", Skill::gate, 180, Burst::mini, 60, 0xe8532a, 3 },
            { Tier::easy, "RIGHT HOOK", "The Subtractive Kitchen", "Cutoff", "Subtractive synthesis starts bright and carves away. You just carved three times.", "CARVE", Skill::radiate, 0, Burst::inversion, 8, 0xe04b4b, 4 },
            { Tier::easy, "DOUBLE SEVEN", "The Lucky Interval", "Septima", "Seven semitones is a fifth. Seventy-seven of anything is just showing off.", "LUCKY FIFTH", Skill::chord, 7, Burst::glowing, 77, 0x9b7bd6, 5 },
            { Tier::easy, "THE EDGE", "The Stereo Field's Edge", "Hard Left", "Pan a sound hard left and it lives in one speaker only. Old Beatles mixes did that to whole drum kits.", "HARD PAN", Skill::delay, 20, Burst::speed, 4, 0x4fc4a8, 1 },
            { Tier::easy, "UNLUCKY THIRTEEN", "Room 13", "Nobody", "There is no room 13. There is no sound here either - only the noise floor of your speakers.", "NOISE FLOOR", Skill::radiate, 0, Burst::snow, 8, 0x8a929e, 4 },

            // ---- medium: need attention or a little luck ----------------
            { Tier::medium, "PALINDROME", "The Retrograde Gallery", "Anna", "Composers play melodies backwards on purpose: retrograde. Bach hid them in his canons.", "RETROGRADE", Skill::scale, 0, Burst::rainbow, 50, 0xe8556f, 3 },
            { Tier::medium, "CLICK IN THE ECHO", "The Echo Well", "Echo", "You clicked inside a repeat. Delay feedback above 100% would grow forever - that is why the knob stops.", "ECHO WELL", Skill::delay, 45, Burst::special, 60, 0xf2c14e, 1 },
            { Tier::medium, "IN THE POCKET", "The Groove Basement", "Pocket", "Playing slightly behind the beat is called playing in the pocket. Machines are perfectly on it; people are not.", "POCKET", Skill::gate, 92, Burst::mini, 80, 0x7bc96f, 3 },
            { Tier::medium, "WITNESS", "The Twist Archive", "The Visitor", "YOU WERE LOOKING WHEN IT CHANGED. MOST PEOPLE ARE NOT.", "WITNESS", Skill::haunted, 6666, Burst::glowing, 40, 0xe6e8ec, 5 },
            { Tier::medium, "INTERRUPT THE TUNER", "The Tuning Office", "The Tuner", "Rude. A pitch exactly 1200 cents up is an octave. You are exactly one interruption up.", "DETUNE", Skill::chord, 1, Burst::inversion, 12, 0xe04b4b, 0 },
            { Tier::medium, "PERFECT CADENCE", "The Cadence Stair", "Dr. Cadence Mirelle", "Fifth key, then first key: V to I. You just resolved a cadence with your mouse.", "RESOLVE", Skill::chord, 7, Burst::rainbow, 64, 0x4fb6c4, 2 },
            { Tier::medium, "OCTAVE JUMP", "The Octave Lift", "Lyra Kestrel", "Half the height, double the frequency. An octave is the same note on a different floor.", "OCTAVE", Skill::chord, 12, Burst::special, 48, 0x7bc96f, 2 },
            { Tier::medium, "CENTURY", "The Centenary Hall", "MOTH", "GENERATION COUNT DIVISIBLE BY ONE HUNDRED. CELEBRATION SUBROUTINE ENGAGED.", "CENTURY", Skill::mutate, 0, Burst::snow, 10, 0x9b7bd6, 5 },
            { Tier::medium, "QUIET ROOM", "The Anechoic Chamber", "Sub", "In an anechoic room nothing reflects. People hear their own heartbeat and blood within minutes.", "ANECHOIC", Skill::haunted, 2011, Burst::monster, 4, 0x12161c, 4 },
            { Tier::medium, "HALF PAST", "The Half-Time Lounge", "Halfie", "Half-time feel keeps the tempo but moves the snare to beat three. It sounds twice as slow without slowing down.", "HALF TIME", Skill::gate, 70, Burst::speed, 6, 0xf2c14e, 3 },
            { Tier::medium, "SCALE RUNNER", "The Staircase of Modes", "Lyra Kestrel", "Seven steps up and you have walked a scale. Which mode depends only on where the half steps fall.", "RUN", Skill::scale, 0, Burst::rainbow, 70, 0x4fc4a8, 2 },
            { Tier::medium, "TWENTY WORDS", "The Lexicon Library", "Fourier", "Twenty facts in, you know more acoustics than most people who own speakers.", "LIBRARIAN", Skill::mutate, 0, Burst::glowing, 20, 0x9b7bd6, 0 },

            // ---- hard: need a clock, a target or patience ---------------
            { Tier::hard, "ELEVEN ELEVEN", "The Wishing Oscillator", "Unison", "Four identical digits; four oscillators at one pitch. Detune them slightly and they chorus.", "UNISON", Skill::chord, 0, Burst::glowing, 111, 0xf2c14e, 5 },
            { Tier::hard, "THREE THIRTY-THREE", "The Triplet Attic", "Trio", "Triplets put three notes where two would go. Three-three-three is triplets all the way down.", "TRIPLETS", Skill::gate, 133, Burst::mini, 99, 0xe8556f, 3 },
            { Tier::hard, "MIDNIGHT LAB", "The Night Shift", "The Night Engineer", "Ears are freshest at the start of a session. At midnight, check your mix tomorrow.", "NIGHT SHIFT", Skill::haunted, 2400, Burst::snow, 12, 0x0e1116, 4 },
            { Tier::hard, "LEAP OF FAITH", "The Fire Escape", "Exit", "You clicked the last pixel. Limiters guard the last decibel the same way.", "LIMITER", Skill::radiate, 0, Burst::speed, 10, 0xe8532a, 1 },
            { Tier::hard, "GOLDEN RATIO", "The Fibonacci Garden", "Phi", "Some composers placed climaxes at 61.8% of a piece. You found the same spot on the screen.", "GOLDEN", Skill::chord, 5, Burst::rainbow, 89, 0xf2c14e, 2 },
            { Tier::hard, "PI O'CLOCK", "The Circular Room", "Radian", "A sine wave is a circle unrolled. 2 pi radians make one cycle.", "CIRCLE", Skill::haunted, 3141, Burst::special, 31, 0x4fb6c4, 1 },
            { Tier::hard, "THOUSANDTH GENERATION", "The Fossil Record", "The Archivist", "A thousand generations. Your colony is older than recorded music, in its own time.", "FOSSIL", Skill::mutate, 0, Burst::monster, 10, 0x8a929e, 0 },
            { Tier::hard, "SHEPARD STAIRCASE", "The Endless Stair", "Roger", "Twelve steps up and you are where you started. That is the Shepard tone illusion, and you just walked it.", "SHEPARD", Skill::scale, 3, Burst::glowing, 120, 0x9b7bd6, 2 },
            { Tier::hard, "FOUR THIRTY-THREE", "The Silent Auditorium", "Cage", "Four minutes and thirty-three seconds of silence from you. The room filled it with sound anyway.", "SILENCE", Skill::haunted, 433, Burst::snow, 20, 0xe6e8ec, 4 },
            { Tier::hard, "MIRROR", "The Hall of Mirrors", "Rorrim", "Mirror image clicks. Flip a waveform upside down and you invert its polarity; sum it with the original and it vanishes.", "POLARITY", Skill::rockets, 0, Burst::rockets, 6, 0x4fc4a8, 5 },

            // ---- legendary: need the calendar on your side --------------
            { Tier::legendary, "FRIDAY THE 13TH", "The Cursed Studio", "Thirteen", "Dissonance is not bad luck. It is tension, and tension is what makes resolution feel good.", "CURSE", Skill::chord, 6, Burst::inversion, 30, 0xe04b4b, 4 },
            { Tier::legendary, "NEW YEAR'S MINUTE", "The Countdown", "Auld", "Ten, nine, eight... a crowd counting down is a tempo everyone agrees on without a click track.", "COUNTDOWN", Skill::rockets, 0, Burst::rockets, 12, 0xf2c14e, 5 },
            { Tier::legendary, "LEAP DAY", "February's Extra Room", "Bissextus", "A leap day is a correction, like a sample-rate converter adding one sample to stay in sync.", "LEAP", Skill::delay, 29, Burst::glowing, 229, 0x7bc96f, 5 },
            { Tier::legendary, "A440", "The Standards Office", "The Tuner", "Score ending 440 at second 44. Even I am impressed. Here: a perfect A.", "CONCERT A", Skill::chord, 9, Burst::special, 440, 0xe04b4b, 0 },
            { Tier::legendary, "COMPLETE LEXICON", "The Last Library", "Everyone", "Sixty facts. Every interval, every filter, every illusion. The colony bows.", "SCHOLAR", Skill::scale, 0, Burst::rainbow, 200, 0xe6e8ec, 2 },
            { Tier::legendary, "EQUINOX", "The Balanced Room", "Equa", "Equal day, equal night. Equal temperament, equal steps. Balance is a choice.", "BALANCE", Skill::chord, 4, Burst::glowing, 60, 0x4fb6c4, 1 },
            { Tier::legendary, "SOLSTICE", "The Longest Note", "Sol", "The longest day: a sustain pedal held from sunrise to sunset.", "SUSTAIN", Skill::chord, 12, Burst::snow, 30, 0xf2c14e, 5 },
            { Tier::legendary, "DAWN CHORUS", "The Aviary", "Wren", "Birds sing hardest at dawn, when still air carries sound furthest. You clicked with them, in time.", "DAWN CHORUS", Skill::scale, 1, Burst::mini, 150, 0x7bc96f, 3 },
            { Tier::legendary, "THE ROOM BEHIND THE ROOMS", "The Room Behind the Rooms", "All Seven Visitors", "Thirty-eight rooms found. This one was always here. It was the dish.", "COMPLETION", Skill::rockets, 0, Burst::rockets, 30, 0xe8532a, 5 }
        }};
        return s;
    }

    inline const char* tierName (Tier t)
    {
        constexpr const char* n[] = { "EASY", "UNCOMMON", "RARE", "LEGENDARY" };
        return n[std::clamp ((int) t, 0, 3)];
    }

    /** Everything a click needs to be judged by. */
    struct Context
    {
        double nx = 0.5, ny = 0.5;      // click position in the chamber, 0..1
        bool right = false;
        double now = 0.0;               // seconds since the session started
        int hour = 12, minute = 0, second = 30;
        int weekday = 3;                // 0 = Sunday
        int day = 15, month = 6;        // month 1..12
        std::int64_t score = 0;
        int generation = 0, population = 1;
        int lexicon = 0, actsSeen = 1, secretsFound = 0;
        bool gateActive = false, delayActive = false;
        double sinceTwist = 1.0e9;      // seconds since the twist was revealed
        bool tunerSpeaking = false;
    };

    /** Remembers recent clicks for the pattern secrets. */
    class Tracker
    {
    public:
        struct Click { double t, x, y; bool right; };

        /** Records the click and returns the first secret it satisfies that
            has not been found yet (-1 for none). */
        int click (const Context& c, std::uint64_t foundMask)
        {
            history.push_back ({ c.now, c.nx, c.ny, c.right });
            while (history.size() > 24) history.pop_front();

            const double quiet = c.now - lastClick;
            lastClick = c.now;

            if (const int k = cornerIndex (c); k >= 0) cornerTimes[(std::size_t) k] = c.now;

            for (int id = 0; id < secretCount; ++id)
            {
                if ((foundMask >> id) & 1u) continue;
                if (matches (id, c, quiet)) return id;
            }
            return -1;
        }

        static int column (double x) { return std::clamp ((int) (x * 12.0), 0, 11); }

    private:
        static int cornerIndex (const Context& c)
        {
            if (c.nx < 0.2 && c.ny < 0.2) return 0;
            if (c.nx > 0.8 && c.ny < 0.2) return 1;
            if (c.nx < 0.2 && c.ny > 0.8) return 2;
            if (c.nx > 0.8 && c.ny > 0.8) return 3;
            return -1;
        }

        bool allCornersWithin (double now, double window) const
        {
            for (auto t : cornerTimes) if (now - t > window) return false;
            return true;
        }

        int countSince (double t, bool rightOnly) const
        {
            int n = 0;
            for (const auto& h : history)
                if (h.t >= t && (! rightOnly || h.right)) ++n;
            return n;
        }

        /** Last `n` clicks walk strictly up the 12 columns, within `window` seconds. */
        bool ascendingRun (int n, double window) const
        {
            if ((int) history.size() < n) return false;
            const auto first = history.end() - n;
            if (history.back().t - first->t > window) return false;
            for (auto it = first + 1; it != history.end(); ++it)
                if (column (it->x) <= column ((it - 1)->x)) return false;
            return true;
        }

        const Click* previous() const
        {
            return history.size() >= 2 ? &history[history.size() - 2] : nullptr;
        }

        static bool palindrome (std::int64_t v)
        {
            if (v < 1000) return false;
            const auto s = std::to_string (v);
            return std::equal (s.begin(), s.begin() + (long) s.size() / 2, s.rbegin());
        }

        bool matches (int id, const Context& c, double quiet) const
        {
            const auto* prev = previous();
            const double cx = std::abs (c.nx - 0.5), cy = std::abs (c.ny - 0.5);
            switch (id)
            {
                case 0:  return c.second == 0;
                case 1:  return cx < 0.03 && cy < 0.03;
                case 2:  return allCornersWithin (c.now, 20.0);
                case 3:  return countSince (c.now - 2.0, false) >= 8;
                case 4:  return c.right && countSince (c.now - 1.5, true) >= 3;
                case 5:  return c.score % 100 == 77;
                case 6:  return c.nx < 0.01;
                case 7:  return c.right && c.second == 13;

                case 8:  return palindrome (c.score);
                case 9:  return c.delayActive;
                case 10: return c.gateActive;
                case 11: return c.sinceTwist >= 0.0 && c.sinceTwist < 4.0;
                case 12: return c.tunerSpeaking;
                case 13: return prev != nullptr && c.now - prev->t < 2.0
                             && column (prev->x) == 7 && column (c.nx) == 0;
                case 14: return prev != nullptr && c.now - prev->t < 1.5
                             && std::abs (std::abs (prev->y - c.ny) - 0.5) < 0.04;
                case 15: return c.generation >= 100 && c.generation % 100 <= 1;
                case 16: return c.population == 0;
                case 17: return c.minute == 30 && c.second < 10;
                case 18: return ascendingRun (7, 6.0);
                case 19: return c.lexicon == 20;

                case 20: return (c.hour == 11 || c.hour == 23) && c.minute == 11;
                case 21: return (c.hour == 3 || c.hour == 15) && c.minute == 33;
                case 22: return c.hour == 0 && c.minute < 5;
                case 23: return c.nx > 0.98 && c.ny > 0.98 && c.score > 100000;
                case 24: return std::abs (c.nx - 0.618) < 0.015 && std::abs (c.ny - 0.382) < 0.015;
                case 25: return (c.hour == 3 || c.hour == 15) && c.minute == 14;
                case 26: return c.generation >= 1000 && c.generation < 1003;
                case 27: return ascendingRun (12, 8.0);
                case 28: return quiet >= 273.0 && quiet < 1.0e8;
                case 29: return prev != nullptr && c.now - prev->t < 1.0
                             && std::abs ((1.0 - prev->x) - c.nx) < 0.03 && std::abs (prev->y - c.ny) < 0.03
                             && std::abs (c.nx - 0.5) > 0.1;

                case 30: return c.weekday == 5 && c.day == 13;
                case 31: return c.month == 1 && c.day == 1 && c.hour == 0;
                case 32: return c.month == 2 && c.day == 29;
                case 33: return c.score % 1000 == 440 && c.second == 44;
                case 34: return c.lexicon >= 60;
                case 35: return (c.month == 3 && c.day == 20) || (c.month == 9 && (c.day == 22 || c.day == 23));
                case 36: return (c.month == 6 && c.day == 21) || (c.month == 12 && c.day == 21);
                case 37: return c.hour == 5 && c.minute < 10 && c.gateActive;
                case 38: return c.secretsFound >= secretCount - 1;
                default: return false;
            }
        }

        std::deque<Click> history;
        double lastClick = 1.0e9;   // the first click of a session never counts as a long silence
        std::array<double, 4> cornerTimes { -1.0e9, -1.0e9, -1.0e9, -1.0e9 };
    };

    // ---- the odd visitors -------------------------------------------------

    enum class Glyphs : int { mirrored, upsideDown };

    struct Visitor
    {
        const char* name;
        Glyphs glyphs;
        unsigned int colour;
        int rocketRecipe;
    };

    inline const std::array<Visitor, 7>& visitors()
    {
        static const std::array<Visitor, 7> v {{
            { "LUMEN",   Glyphs::mirrored,   0xf2c14e, 61001 },
            { "VESPER",  Glyphs::mirrored,   0x9b7bd6, 61037 },
            { "QUILL",   Glyphs::mirrored,   0x4fc4a8, 61073 },
            { "BRAMBLE", Glyphs::upsideDown, 0x7bc96f, 61109 },
            { "OZZIE",   Glyphs::upsideDown, 0xe8556f, 61151 },
            { "TILDA",   Glyphs::upsideDown, 0x4fb6c4, 61187 },
            { "GORP",    Glyphs::upsideDown, 0xe8532a, 61223 }
        }};
        return v;
    }

    /** Gibberish: pronounceable, never a real sentence, different every time. */
    inline std::string gibberish (std::uint64_t seed, int words)
    {
        static const char* syl[] = { "bo", "zin", "ka", "plu", "mee", "rr", "ft", "oog", "wib", "zz",
                                     "tra", "lop", "snee", "gub", "yip", "ork", "fla", "dib", "nu", "kree" };
        std::string out;
        for (int w = 0; w < words; ++w)
        {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            const int n = 1 + (int) ((seed >> 33) % 3);
            for (int k = 0; k < n; ++k)
            {
                seed = seed * 6364136223846793005ull + 1442695040888963407ull;
                out += syl[(seed >> 35) % 20];
            }
            out += (w + 1 < words) ? ((seed >> 40) % 5 == 0 ? "! " : " ") : "!";
        }
        if (! out.empty()) out[0] = (char) std::toupper ((unsigned char) out[0]);
        return out;
    }

    // ---- polyglot events (for no reason) -----------------------------------

    struct Polyglot
    {
        const char* language;
        const char* phrase;     // UTF-8
        const char* meaning;
        Skill skill;
        int param;
        Burst burst;
        int count;
    };

    inline const std::array<Polyglot, 10>& polyglots()
    {
        static const std::array<Polyglot, 10> p {{
            { "Japanese",   "\xe9\x9f\xb3\xe3\x81\xae\xe8\x8a\xb1\xe7\x81\xab\xef\xbc\x81", "sound fireworks!", Skill::rockets, 0, Burst::snow, 12 },
            { "French",     "Le Grand \xc3\x89" "cho", "the great echo", Skill::delay, 30, Burst::special, 40 },
            { "Swahili",    "Ngoma ya Upinde wa Mvua", "rainbow drum", Skill::gate, 110, Burst::rainbow, 60 },
            { "Icelandic",  "Nor\xc3\xb0urlj\xc3\xb3sahlj\xc3\xb3\xc3\xb0", "northern-lights sound", Skill::haunted, 4404, Burst::glowing, 50 },
            { "Portuguese", "Saudade Sonora", "a sound you miss", Skill::chord, 3, Burst::mini, 40 },
            { "Finnish",    "Kaikukaivo", "echo well", Skill::delay, 55, Burst::special, 30 },
            { "Hindi",      "\xe0\xa4\xb0\xe0\xa4\xbe\xe0\xa4\x97 \xe0\xa4\xb5\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xb7\xe0\xa4\xbe", "raga of rain", Skill::scale, 0, Burst::rainbow, 45 },
            { "Korean",     "\xec\x86\x8c\xeb\xa6\xac \xed\x8f\xad\xec\xa3\xbd", "sound firecracker", Skill::rockets, 0, Burst::monster, 5 },
            { "Welsh",      "C\xc3\xa2n y Morfil", "whale song", Skill::chord, -24, Burst::speed, 6 },
            { "Esperanto",  "\xc4\x88ielarka Salto", "rainbow jump", Skill::mutate, 0, Burst::glowing, 70 }
        }};
        return p;
    }
    // ---- name fates ----------------------------------------------------------
    // The first character of the player's name can tilt the start of a game.

    enum class Fate : int
    {
        none,
        wallRain,       // J 1/2: 100,000 orbs that die on the walls, +23 each
        yellowEscape,   // P 1/2: 40 yellow orbs escape across the screen
        blueRun,        // A 1/3: 40 blue orbs race to the right edge, fireworks, +88,787
        flowers,        // Q always: flowers grow out of the orbs
        seizure,        // N 1/4: every orb convulses, multiplies, then goes 3x speed
        hail,           // B 2/3: red and white hail, +3,048
        numberCurse,    // digit always: -390,943 and every skill lost
        squidDrop,      // G 3/5: bass drop, 499 pink orbs turn into squids and mites
        whitePop,       // H 5/6: popping, 9 huge white orbs zoom then pop
        moonCrash,      // K 4/5: the moon crashes into the samples, burping, +9,094
        turbo,          // C 3/4: every orb multiplies 3x and runs at 5x speed
        hugeMites,      // D always ("4/3"): 32 huge orbs multiply into mites, +8,897
        yellow,         // Y 4/5: 34 yellow orbs
        blackSwarm      // T 3/6: 9,448 black orbs zoom around
    };

    struct FateRule { char first; int num, den; Fate fate; std::int64_t points; const char* title; };

    inline const std::array<FateRule, 14>& fateRules()
    {
        static const std::array<FateRule, 14> r {{
            { 'j', 1, 2, Fate::wallRain,     0,       "A HUNDRED THOUSAND ORBS" },
            { 'p', 1, 2, Fate::yellowEscape, 0,       "THE YELLOW ESCAPE" },
            { 'a', 1, 3, Fate::blueRun,      88787,   "THE BLUE RUN" },
            { 'q', 1, 1, Fate::flowers,      0,       "QUIET BLOOM" },
            { 'n', 1, 4, Fate::seizure,      0,       "THE CONVULSION" },
            { 'b', 2, 3, Fate::hail,         3048,    "RED AND WHITE HAIL" },
            { '#', 1, 1, Fate::numberCurse,  -390943, "THE NUMBER CURSE" },
            { 'g', 3, 5, Fate::squidDrop,    0,       "SQUID DROP" },
            { 'h', 5, 6, Fate::whitePop,     0,       "NINE WHITE MOONS" },
            { 'k', 4, 5, Fate::moonCrash,    9094,    "THE MOON FELL IN" },
            { 'c', 3, 4, Fate::turbo,        0,       "TURBO COLONY" },
            { 'd', 1, 1, Fate::hugeMites,    8897,    "THIRTY-TWO GIANTS" },
            { 'y', 4, 5, Fate::yellow,       0,       "YELLOW VISITORS" },
            { 't', 3, 6, Fate::blackSwarm,   0,       "THE BLACK SWARM" }
        }};
        return r;
    }

    /** Which rule a name falls under (or nullptr). Digits share the '#' rule. */
    inline const FateRule* ruleForName (const std::string& name)
    {
        std::size_t i = 0;
        while (i < name.size() && std::isspace ((unsigned char) name[i])) ++i;
        if (i >= name.size()) return nullptr;
        const char c = std::isdigit ((unsigned char) name[i]) ? '#' : (char) std::tolower ((unsigned char) name[i]);
        for (const auto& r : fateRules()) if (r.first == c) return &r;
        return nullptr;
    }

    /** Rolls the fate. `roll` is any uniform random integer. */
    inline Fate fateForName (const std::string& name, std::uint64_t roll)
    {
        const auto* r = ruleForName (name);
        if (r == nullptr) return Fate::none;
        return (int) (roll % (std::uint64_t) r->den) < r->num ? r->fate : Fate::none;
    }
}
