// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The Resonance Acts: a second storyline that runs beside the Field Journal.
//
// Every line a character speaks carries one true fact about sound design,
// acoustics or music theory. The player is never told "this is a lesson" - the
// fact is simply what the character says while something audible happens in the
// colony (an interval sung by the cells, a delay opening, a tempo gate). The
// facts are counted as "lexicon" and that count is what unlocks the next act,
// so learning is the progression currency without ever being announced as one.
//
// No two runs are alike: each run draws a key and mode, a twist, an event
// order and event spacing from its own seed.
//
// Pure standard C++ so the test target can exercise it without JUCE.

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mutagen::story
{
    enum class Speaker : int
    {
        cadence, fourier, lyra, tuner, sub, echo, nyquist, moth, visitor, count
    };

    inline const char* speakerName (Speaker s)
    {
        constexpr const char* n[] = { "DR. CADENCE MIRELLE", "FOURIER", "LYRA KESTREL", "THE TUNER",
                                      "SUB", "ECHO", "NYQUIST", "MOTH", "THE VISITOR" };
        return n[std::clamp ((int) s, 0, (int) Speaker::count - 1)];
    }

    inline const char* speakerRole (Speaker s)
    {
        constexpr const char* r[] = { "Acoustician", "Spectrum Analyser", "Composer", "Antagonist",
                                      "Infrasonic Organism", "Delay Line", "Gatekeeper",
                                      "Lab Automation", "Unknown Observer" };
        return r[std::clamp ((int) s, 0, (int) Speaker::count - 1)];
    }

    /** What the colony does while a line is spoken. Notes are semitone offsets
        from the run's root; the editor turns them into note bursts. */
    enum class Effect : int
    {
        none, interval, chord, scale, mutate, radiate, gate, delay, haunted, collect
    };

    struct Event
    {
        Speaker speaker;
        const char* topic;
        const char* line;
        Effect effect;
        std::array<int, 4> notes;   // -99 = unused
        int param;                  // gate BPM, haunted recipe, etc.
        int minAct;                 // 1-based act that first allows it
    };

    constexpr int kNo = -99;   // unused note slot

    // Every line is one lexicon entry. Index = lexicon id (stable: append only).
    inline const std::array<Event, 64>& events()
    {
        static const std::array<Event, 64> e {{
            { Speaker::lyra, "Intervals", "An octave is a 2:1 ratio - 220 Hz to 440 Hz. The cells hear it as the same note, only higher.", Effect::interval, { 0, 12, kNo, kNo }, 0, 1 },
            { Speaker::lyra, "Intervals", "Seven semitones: a perfect fifth, close to a 3:2 ratio. A power chord is nothing but root and fifth.", Effect::interval, { 0, 7, kNo, kNo }, 0, 1 },
            { Speaker::lyra, "Intervals", "Four semitones, a major third. In equal temperament it sits about 14 cents sharp of the pure 5:4.", Effect::interval, { 0, 4, kNo, kNo }, 0, 3 },
            { Speaker::lyra, "Intervals", "Three semitones, a minor third. Swap it for the major third and the whole chord turns minor.", Effect::interval, { 0, 3, kNo, kNo }, 0, 3 },
            { Speaker::tuner, "Intervals", "Six semitones split the octave exactly in half. They called the tritone diabolus in musica. I call it a rounding error.", Effect::interval, { 0, 6, kNo, kNo }, 0, 3 },
            { Speaker::lyra, "Chords", "Root, major third, fifth: a major triad. Its frequencies sit close to 4:5:6, which is why it sounds settled.", Effect::chord, { 0, 4, 7, kNo }, 0, 3 },
            { Speaker::lyra, "Chords", "Root, minor third, fifth. Same fifth, one semitone lower in the middle, and the colony sounds like it is remembering something.", Effect::chord, { 0, 3, 7, kNo }, 0, 3 },
            { Speaker::lyra, "Chords", "Add a minor seventh to a major triad and it leans: the dominant seventh wants to fall to the chord a fifth below.", Effect::chord, { 0, 4, 7, 10 }, 0, 4 },
            { Speaker::lyra, "Scales", "Every major scale walks the same steps: whole, whole, half, whole, whole, whole, half.", Effect::scale, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::lyra, "Scales", "Five notes and no half steps: the pentatonic scale. With no semitone clashes almost any order sounds consonant.", Effect::scale, { kNo, kNo, kNo, kNo }, 1, 2 },
            { Speaker::lyra, "Chords", "Swap the third for a fourth and the chord hangs in the air: sus4. Without a third it is neither major nor minor.", Effect::chord, { 0, 5, 7, kNo }, 0, 4 },
            { Speaker::fourier, "Harmonics", "Every pitched sound is a stack of sines at whole-number multiples of its fundamental. Listen: harmonics one to four.", Effect::chord, { 0, 12, 19, 24 }, 0, 2 },
            { Speaker::fourier, "Waveforms", "A sawtooth carries every harmonic at 1/n strength. A square wave keeps only the odd ones. That is why it sounds hollow.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::fourier, "Timbre", "Same pitch, different instrument: that difference is timbre - the balance of harmonics and how it moves over time.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::fourier, "Analysis", "A Fourier transform turns a waveform into a spectrum. Bigger FFT windows resolve pitch finer but smear timing.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::nyquist, "Digital audio", "I stand at half the sample rate. At 48 kHz nothing above 24 kHz gets through. Whatever tries folds back as aliasing.", Effect::radiate, { kNo, kNo, kNo, kNo }, 0, 6 },
            { Speaker::nyquist, "Digital audio", "Each bit of depth buys about 6 dB of dynamic range. Sixteen bits give roughly 96 dB; twenty-four, about 144 on paper.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 6 },
            { Speaker::nyquist, "Digital audio", "CD audio samples 44,100 times a second - enough to cover hearing's roughly 20 Hz to 20 kHz.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 5 },
            { Speaker::sub, "Low end", "I live below 20 Hz. You do not hear me. You feel me. Most speakers cannot move enough air to say my name.", Effect::haunted, { kNo, kNo, kNo, kNo }, 2011, 4 },
            { Speaker::sub, "Low end", "At 20 Hz a wavelength is about 17 metres long. That is why bass bends round walls and climbs through floors.", Effect::interval, { -24, -12, kNo, kNo }, 0, 4 },
            { Speaker::sub, "Mixing", "Keep me in mono. Below about 120 Hz your ears can barely tell direction, and centred bass survives every speaker.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 4 },
            { Speaker::echo, "Delay", "A delay is a copy played back later. Feed it back into itself and you get repeats... you get repeats...", Effect::delay, { kNo, kNo, kNo, kNo }, 40, 5 },
            { Speaker::echo, "Psychoacoustics", "Under about 30 milliseconds the brain fuses an echo with the original and hears width instead. The Haas effect. The Haas effect.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 5 },
            { Speaker::echo, "Reverb", "Reverb is thousands of reflections smeared together. RT60 is the time they take to fall by 60 dB... 60 dB...", Effect::none, { kNo, kNo, kNo, kNo }, 0, 5 },
            { Speaker::echo, "Filters", "Mix a signal with a very short copy of itself and some frequencies cancel: a comb filter. Sweep the delay and it flanges.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 5 },
            { Speaker::cadence, "Level", "Decibels are logarithmic. Plus 6 dB doubles the amplitude; roughly plus 10 dB is heard as twice as loud.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::cadence, "Acoustics", "Double your distance from a source and the level drops about 6 dB. The inverse square law keeps the lab quiet.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::cadence, "Phase", "Two identical waves 180 degrees out of phase cancel to silence. Noise-cancelling headphones are built on that.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::cadence, "Envelopes", "Attack, decay, sustain, release: four numbers decide whether a sound plucks, swells or lingers.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::cadence, "Filters", "A low-pass filter lets the lows through and darkens the tone. Turn up resonance and the cutoff starts to sing.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::cadence, "Dynamics", "A compressor turns down whatever crosses its threshold. Ratio sets how hard; attack and release set how fast.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 3 },
            { Speaker::cadence, "Acoustics", "Sound covers about 343 metres a second in room-temperature air - roughly one foot every millisecond.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::cadence, "Hearing", "Ears are most sensitive between about 2 and 5 kHz. Turn the volume down and the bass seems to vanish: equal-loudness contours.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 3 },
            { Speaker::tuner, "Tuning", "Concert A is 440 Hz, agreed in 1939 and written into ISO 16. I intend to make it the only note left.", Effect::interval, { 0, 0, kNo, kNo }, 0, 1 },
            { Speaker::tuner, "Tuning", "Equal temperament splits the octave into twelve identical steps, each the twelfth root of two. Every key equally wrong. Fair.", Effect::scale, { kNo, kNo, kNo, kNo }, 2, 6 },
            { Speaker::tuner, "Tuning", "A semitone is 100 cents. Most listeners cannot hear a change smaller than about five. I can hear one.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 4 },
            { Speaker::tuner, "Tuning", "Two tones a few hertz apart pulse at their difference frequency. Tuners listen for the beats to slow, then stop.", Effect::haunted, { kNo, kNo, kNo, kNo }, 4401, 4 },
            { Speaker::lyra, "Rhythm", "Tempo is beats per minute. At 120 BPM a quarter note lasts exactly half a second. Feel the gate?", Effect::gate, { kNo, kNo, kNo, kNo }, 120, 2 },
            { Speaker::lyra, "Rhythm", "Syncopation puts the weight on the off-beats, where the ear does not expect it. Funk lives there.", Effect::gate, { kNo, kNo, kNo, kNo }, 96, 3 },
            { Speaker::lyra, "Rhythm", "A 3/4 bar holds three quarter-note beats: the waltz. 6/8 holds six eighths grouped in two.", Effect::gate, { kNo, kNo, kNo, kNo }, 90, 3 },
            { Speaker::echo, "Rhythm", "Three against two repeats every six subdivisions. That is a polyrhythm. That is a polyrhythm.", Effect::gate, { kNo, kNo, kNo, kNo }, 132, 5 },
            { Speaker::cadence, "Mixing", "Duck the pads every time the kick lands - sidechain compression - and the whole mix breathes in tempo.", Effect::gate, { kNo, kNo, kNo, kNo }, 128, 4 },
            { Speaker::fourier, "Noise", "White noise has equal energy per hertz. Pink noise has equal energy per octave, so it sounds more even to you.", Effect::radiate, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::fourier, "Synthesis", "Granular synthesis chops sound into grains of 1 to 100 ms and rebuilds it. Your orange grain species is doing exactly that.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::fourier, "Synthesis", "FM at audio rate makes sidebands. Simple carrier-to-modulator ratios stay harmonic; awkward ratios ring like bells.", Effect::mutate, { kNo, kNo, kNo, kNo }, 0, 3 },
            { Speaker::fourier, "Synthesis", "Ring modulation multiplies two signals and keeps only their sum and difference frequencies. The originals disappear.", Effect::haunted, { kNo, kNo, kNo, kNo }, 777, 4 },
            { Speaker::cadence, "Distortion", "Clipping flattens the peaks and adds harmonics. Symmetric clipping adds odd harmonics; asymmetric adds even ones too.", Effect::radiate, { kNo, kNo, kNo, kNo }, 0, 3 },
            { Speaker::nyquist, "Digital audio", "Dither adds a whisper of noise before you cut bit depth, so quantisation error becomes soft hiss instead of grit.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 6 },
            { Speaker::cadence, "Mixing", "Leave headroom. 0 dBFS is the ceiling where digital audio clips. Every sound you collect is saved peaking at -1 dBFS.", Effect::collect, { kNo, kNo, kNo, kNo }, 0, 2 },
            { Speaker::cadence, "Editing", "Cut at a zero crossing or add a few milliseconds of fade and an edit will not click. Your collected sounds get both.", Effect::collect, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::moth, "Foley", "FOLEY LOG: film bones are snapped celery. Footsteps in snow are cornstarch in a leather pouch. The colony finds this reassuring.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 3 },
            { Speaker::visitor, "Acoustics", "A SOURCE MOVING TOWARD YOU SQUEEZES ITS WAVELENGTHS AND RISES IN PITCH. I AM MOVING TOWARD YOU.", Effect::interval, { 0, 2, kNo, kNo }, 0, 6 },
            { Speaker::fourier, "Illusions", "Stack octave-spaced tones that fade in at the bottom and out at the top, and pitch seems to climb forever: the Shepard tone.", Effect::scale, { kNo, kNo, kNo, kNo }, 3, 7 },
            { Speaker::fourier, "Illusions", "Play harmonics two, three and four with no fundamental and your brain supplies it anyway. Small speakers fake bass this way.", Effect::chord, { 12, 19, 24, kNo }, 0, 4 },
            { Speaker::lyra, "Harmony", "Climb a fifth twelve times and you visit every key before coming home: the circle of fifths.", Effect::interval, { 0, 7, kNo, kNo }, 0, 6 },
            { Speaker::lyra, "Harmony", "Every major key shares its notes with a minor key three semitones down. C major and A minor are relatives.", Effect::chord, { -3, 0, 4, kNo }, 0, 5 },
            { Speaker::lyra, "Composition", "A leitmotif is a short theme tied to a character. Hear it again and you know who is coming. Listen for mine.", Effect::interval, { 0, 4, kNo, kNo }, 0, 5 },
            { Speaker::cadence, "Harmony", "Simple ratios like 2:1 and 3:2 sound stable; complex ones sound tense. Tension wants resolution.", Effect::chord, { 0, 6, 11, kNo }, 0, 7 },
            { Speaker::cadence, "Harmony", "A perfect cadence moves dominant to tonic, five to one: the musical full stop. My name is a promise of resolution.", Effect::chord, { 7, 11, 14, 17 }, 0, 7 },
            { Speaker::sub, "Hearing", "The hum under everything is mains: 50 Hz in Europe, 60 in America. Engineers notch it out. I keep it as a pet.", Effect::interval, { -24, kNo, kNo, kNo }, 0, 4 },
            { Speaker::sub, "Acoustics", "In a small room, a bass note whose half-wavelength fits between the walls piles up: a room mode. Walk one metre and the note disappears.", Effect::interval, { -24, -12, kNo, kNo }, 0, 4 },
            { Speaker::echo, "Delay", "Tempo-sync a delay: at 120 BPM a dotted eighth is 375 milliseconds. Set it by the clock and the repeats lock into the groove.", Effect::delay, { kNo, kNo, kNo, kNo }, 60, 5 },
            { Speaker::nyquist, "Digital audio", "Oversample before you distort. Clip at four times the sample rate and the new harmonics land under the gate instead of folding back as aliasing.", Effect::radiate, { kNo, kNo, kNo, kNo }, 0, 6 },
            { Speaker::lyra, "Harmony", "A deceptive cadence goes five to six instead of five to one. You were promised the full stop and got a question mark.", Effect::chord, { 9, 12, 16, kNo }, 0, 7 }
        }};
        return e;
    }

    constexpr int lexiconSize = 64;

    // ---- acts ------------------------------------------------------------

    struct Act
    {
        const char* title;
        Speaker speaker;
        const char* opening;
        std::array<int, 4> notes;   // the act's motif, played as a chord
        int lexiconNeeded;
        int collectedNeeded;
        int quizzesNeeded;
    };

    constexpr int actCount = 9;

    inline const std::array<Act, actCount>& acts()
    {
        static const std::array<Act, actCount> a {{
            { "ACT I  THE TUNING FORK", Speaker::cadence,
              "A tuning fork rings almost a pure sine: one frequency, no harmonics to argue with. That is why I trust it. Someone in this lab does not want you to hear anything else.",
              { 0, 12, kNo, kNo }, 0, 0, 0 },
            { "ACT II  HARMONICS IN THE WALLS", Speaker::fourier,
              "I see every sound as a stack of sines. The walls of this lab are full of them. Somebody has been filtering the upper ones out, one partial at a time.",
              { 0, 12, 19, 24 }, 4, 0, 0 },
            { "ACT III  THE INTERVAL GARDEN", Speaker::lyra,
              "I grow chords here. Thirds make them happy or sad, fifths hold them up. The Tuner pruned my garden down to unisons last night.",
              { 0, 4, 7, kNo }, 9, 2, 0 },
            { "ACT IV  BELOW HEARING", Speaker::sub,
              "...you found the floor. Down here the Tuner cannot reach. Frequencies this low take whole rooms to turn around.",
              { -24, -12, kNo, kNo }, 14, 3, 2 },
            { "ACT V  THE HALL OF ECHOES", Speaker::echo,
              "Every reflection is me arriving late. Every reflection is me arriving late. The Tuner wants the hall dry. Dead. Anechoic.",
              { 0, 7, 12, kNo }, 20, 6, 3 },
            { "ACT VI  THE FOLDING GATE", Speaker::nyquist,
              "Nothing above half the sample rate passes me. The Tuner asked me to lower the gate to 440 Hz. I said no. It is asking you next.",
              { 0, 5, 10, kNo }, 27, 9, 5 },
            { "ACT VII  EQUAL TEMPERAMENT", Speaker::tuner,
              "Twelve equal steps. Every interval slightly wrong, every key the same. Order is a kind of silence, and silence is my favourite note.",
              { 0, 6, kNo, kNo }, 35, 12, 7 },
            { "ACT VIII  PERFECT CADENCE", Speaker::cadence,
              "Dominant to tonic. Tension to rest. The colony has learned every interval you have. Play it home.",
              { 7, 11, 14, 17 }, 44, 16, 10 },
            { "ACT IX  CODA: THE LISTENER", Speaker::moth,
              "ALL INSTRUMENTS NOMINAL. The experiment had one unknown left, and it was never the colony. Everything in this lab was built to find out what you would do with a room full of sound. Keep listening.",
              { 0, 4, 7, 12 }, 54, 22, 13 }
        }};
        return a;
    }

    struct Progress
    {
        std::uint64_t lexicon = 0;  // bit per event heard
        int collected = 0;          // sounds collected, lifetime
        int quizzesCorrect = 0;
        int quizzesAsked = 0;
        int highestActSeen = 0;     // 0 = none yet
        int runsPlayed = 0;
        std::uint32_t twistsSeen = 0;
        std::uint64_t secrets = 0;  // bit per secret room found
        bool introSeen = false;
        std::string playerName;
        int plotFloor = 1;          // acts opened by time alone (one per 30 minutes of play)
        std::uint32_t chanceSeen = 0;   // bit per chance event witnessed
        std::uint32_t endingsSeen = 0;  // bit per ending reached

        int secretCount() const
        {
            int n = 0;
            for (auto v = secrets; v != 0; v &= v - 1) ++n;
            return n;
        }

        int lexiconCount() const
        {
            int n = 0;
            for (auto v = lexicon; v != 0; v &= v - 1) ++n;
            return n;
        }

        static int bits (std::uint32_t v) { int n = 0; for (; v != 0; v &= v - 1) ++n; return n; }
        int chanceCount() const { return bits (chanceSeen); }
        int endingCount() const { return bits (endingsSeen); }

        bool heard (int id) const { return id >= 0 && id < 64 && ((lexicon >> id) & 1u) != 0; }
        void markHeard (int id) { if (id >= 0 && id < 64) lexicon |= (std::uint64_t) 1 << id; }
    };

    /** 1-based act the player has earned. */
    inline int actFor (const Progress& p)
    {
        int act = std::clamp (p.plotFloor, 1, actCount);
        const int lex = p.lexiconCount();
        for (int i = 1; i < actCount; ++i)
        {
            const auto& a = acts()[(std::size_t) i];
            if (lex >= a.lexiconNeeded && p.collected >= a.collectedNeeded
                && p.quizzesCorrect >= a.quizzesNeeded)
                act = std::max (act, i + 1);
        }
        return act;
    }

    // ---- quizzes ("ear checks") -----------------------------------------

    struct Quiz
    {
        Speaker speaker;
        const char* question;
        std::array<const char*, 3> answers;
        int correct;
        const char* why;
        int minAct;
    };

    inline const std::array<Quiz, 16>& quizzes()
    {
        static const std::array<Quiz, 16> q {{
            { Speaker::lyra, "The cells are at 220 Hz. Where is the octave above?", { "330 Hz", "440 Hz", "880 Hz" }, 1, "Octaves double the frequency.", 1 },
            { Speaker::lyra, "How many semitones make a perfect fifth?", { "5", "7", "9" }, 1, "Seven semitones, near a 3:2 ratio.", 2 },
            { Speaker::nyquist, "The sample rate is 48 kHz. Where do I stand?", { "24 kHz", "48 kHz", "96 kHz" }, 0, "Nyquist is half the sample rate.", 4 },
            { Speaker::cadence, "Plus 6 dB does what to amplitude?", { "Doubles it", "Halves it", "Multiplies it by ten" }, 0, "+6 dB is about 2x amplitude.", 1 },
            { Speaker::fourier, "Which wave keeps only the odd harmonics?", { "Sawtooth", "Square", "Sine" }, 1, "Square: odd harmonics at 1/n.", 2 },
            { Speaker::cadence, "A low-pass filter removes...", { "the lows", "the highs", "only the mids" }, 1, "It passes lows and cuts highs.", 1 },
            { Speaker::echo, "An echo under 30 ms is heard as...", { "a separate repeat", "width", "a pitch shift" }, 1, "The Haas effect fuses it into width.", 3 },
            { Speaker::fourier, "Pink noise has equal energy per...", { "hertz", "octave", "second" }, 1, "Per octave. White is per hertz.", 2 },
            { Speaker::nyquist, "Each extra bit of depth adds about...", { "1 dB", "6 dB", "20 dB" }, 1, "About 6 dB per bit.", 4 },
            { Speaker::lyra, "A major triad stacks which semitones over the root?", { "3 and 7", "4 and 7", "5 and 7" }, 1, "Major third (4) and fifth (7).", 3 },
            { Speaker::lyra, "What is the relative minor of C major?", { "A minor", "E minor", "C minor" }, 0, "A minor shares C major's notes.", 4 },
            { Speaker::lyra, "At 120 BPM, one beat lasts...", { "0.25 s", "0.5 s", "1 s" }, 1, "60 / 120 = 0.5 seconds.", 2 },
            { Speaker::cadence, "In ADSR, the S is...", { "Sustain", "Swing", "Saturation" }, 0, "Sustain is a level, not a time.", 1 },
            { Speaker::cadence, "Two identical waves 180 degrees apart...", { "double", "cancel", "beat" }, 1, "Opposite phase cancels.", 2 },
            { Speaker::sub, "How fast does sound travel in room air?", { "34 m/s", "343 m/s", "3430 m/s" }, 1, "About 343 m/s.", 4 },
            { Speaker::tuner, "How many semitones in a tritone?", { "5", "6", "7" }, 1, "Six: half an octave.", 3 }
        }};
        return q;
    }

    // ---- per-run variety -------------------------------------------------

    struct Mode
    {
        const char* name;
        std::array<int, 8> steps;   // semitone offsets, -99 terminated
        const char* character;
    };

    inline const std::array<Mode, 10>& modes()
    {
        static const std::array<Mode, 10> m {{
            { "Ionian (major)",   { 0, 2, 4, 5, 7, 9, 11, 12 }, "Bright and resolved: the plain major scale." },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10, 12 }, "Minor with a raised sixth - melancholy that still dances." },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10, 12 }, "That flat second right above the root is the whole flamenco mood." },
            { "Lydian",           { 0, 2, 4, 6, 7, 9, 11, 12 }, "Major with a raised fourth: floating, dreamlike, film-score wonder." },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10, 12 }, "Major with a flat seventh: the blues-rock mode." },
            { "Aeolian (minor)",  { 0, 2, 3, 5, 7, 8, 10, 12 }, "The natural minor scale: the major scale's relative, started on its sixth." },
            { "Major pentatonic", { 0, 2, 4, 7, 9, 12, kNo, kNo },  "Five notes and no semitones: almost impossible to play wrong." },
            { "Minor pentatonic", { 0, 3, 5, 7, 10, 12, kNo, kNo }, "The backbone of rock and blues guitar solos." },
            { "Harmonic minor",   { 0, 2, 3, 5, 7, 8, 11, 12 }, "Natural minor with a raised seventh - that gap of three semitones sounds ancient." },
            { "Whole tone",       { 0, 2, 4, 6, 8, 10, 12, kNo }, "Six equal whole steps: no home note, pure dream sequence." }
        }};
        return m;
    }

    inline const char* noteName (int midi)
    {
        constexpr const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n[((midi % 12) + 12) % 12];
    }

    struct Twist
    {
        const char* name;
        Speaker speaker;
        const char* reveal;
        Effect effect;
        std::array<int, 4> notes;
        int param;
    };

    inline const std::array<Twist, 12>& twists()
    {
        static const std::array<Twist, 12> t {{
            { "THE TUNER WEARS A LAB COAT", Speaker::cadence,
              "...my tuning fork reads 440.000 Hz exactly. Real forks drift with temperature. Mine never has. Cadence and the Tuner have been the same voice all along.",
              Effect::interval, { 0, 0, kNo, kNo }, 0 },
            { "ECHO WAS ALWAYS ALONE", Speaker::echo,
              "There was never a twin. There was only me, delayed by 375 milliseconds - a dotted eighth at 120 BPM. You were listening to the repeat.",
              Effect::delay, { kNo, kNo, kNo, kNo }, 60 },
            { "FOURIER IS THE VISITOR", Speaker::visitor,
              "I WAS NEVER OUTSIDE. I AM THE SPECTRUM YOU HAVE BEEN LOOKING AT. EVERY SOUND YOU MADE WAS ADDED TO ME, ONE SINE AT A TIME.",
              Effect::chord, { 0, 12, 19, 24 }, 0 },
            { "SUB HOLDS UP THE LAB", Speaker::sub,
              "The hum you stopped hearing is me. The building resonates at my frequency. If the Tuner silences me, the room modes collapse.",
              Effect::haunted, { kNo, kNo, kNo, kNo }, 2011 },
            { "THE COLONY IS TRANSCRIBING YOU", Speaker::lyra,
              "I found a score in the colony's memory. Every bar is one of your clicks, quantised to sixteenths. It has been composing you.",
              Effect::scale, { kNo, kNo, kNo, kNo }, 0 },
            { "NYQUIST OPENS THE GATE", Speaker::nyquist,
              "I lowered the gate for one sample. Everything above folded back down as aliasing - mirrored frequencies that were never played.",
              Effect::radiate, { kNo, kNo, kNo, kNo }, 0 },
            { "THE 432 DECREE", Speaker::tuner,
              "New rule: A is 432 Hz. It is 31.77 cents flatter than 440, and no more magical. I simply wanted you to notice that I can.",
              Effect::interval, { 0, -1, kNo, kNo }, 0 },
            { "SILENCE WAS THE INSTRUMENT", Speaker::moth,
              "PERFORMANCE NOTE: In 1952 a pianist sat at a piano for 4'33\" and played nothing. The audience was the music. You are the audience.",
              Effect::none, { kNo, kNo, kNo, kNo }, 0 },
            { "MOTH WAS LISTENING BACK", Speaker::moth,
              "MIC CHECK: the colony's input has been open since boot. I was not recording you. I was measuring the room: RT60 of 0.4 seconds, about a carpeted bedroom.",
              Effect::delay, { kNo, kNo, kNo, kNo }, 30 },
            { "LYRA WROTE THE TUNER", Speaker::lyra,
              "Every note the Tuner removed, I hummed first. He is my rejected drafts, quantised until nothing was left. An editor is a composer with the courage to cut.",
              Effect::scale, { kNo, kNo, kNo, kNo }, 0 },
            { "THE LAB IS A RESONATOR", Speaker::cadence,
              "Walk the room and count: 41 Hz, 82, 123 - the lab's own modes, evenly spaced. We have been inside an instrument all along. You are the string.",
              Effect::interval, { -24, -12, kNo, kNo }, 0 },
            { "A CLAP CONTAINS EVERYTHING", Speaker::fourier,
              "An impulse holds every frequency at once. One click, one clap, one pop: the whole spectrum in a single instant. Nothing here was ever quiet.",
              Effect::radiate, { kNo, kNo, kNo, kNo }, 0 }
        }};
        return t;
    }

    // ---- banter: two voices, one exchange ------------------------------------

    struct Banter
    {
        Speaker a;
        const char* lineA;
        Speaker b;
        const char* lineB;
        Effect effect;              // performed with the second line
        std::array<int, 4> notes;
        int param;
        int minAct;
    };

    inline const std::array<Banter, 14>& banters()
    {
        static const std::array<Banter, 14> b {{
            { Speaker::lyra, "Cadence, why is the colony humming A?", Speaker::cadence, "Because 440 Hz is the reference. Orchestras tune to it before they play a note of music.", Effect::interval, { 0, 12, kNo, kNo }, 0, 1 },
            { Speaker::fourier, "Your tuning fork has no harmonics.", Speaker::cadence, "That is the point. A near-pure sine is the one voice in this room that never argues.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 1 },
            { Speaker::lyra, "Major or minor, Fourier?", Speaker::fourier, "I do not hear moods. I hear the third harmonic. Four semitones, or three. You decide what it means.", Effect::chord, { 0, 4, 7, kNo }, 0, 2 },
            { Speaker::cadence, "Moth, is the lab humming?", Speaker::moth, "AFFIRMATIVE. 50 OR 60 HZ DEPENDING ON THE GRID. SUB HAS NAMED IT. DO NOT ASK WHAT.", Effect::interval, { -24, kNo, kNo, kNo }, 0, 2 },
            { Speaker::lyra, "Echo, finish my sentence.", Speaker::echo, "Finish my sentence. Finish my sentence. Delay is a copy of you, a little late.", Effect::delay, { kNo, kNo, kNo, kNo }, 30, 3 },
            { Speaker::sub, "...", Speaker::fourier, "Sub is below my window. At 16 Hz one cycle takes 62.5 milliseconds. I need a long frame to see it.", Effect::haunted, { kNo, kNo, kNo, kNo }, 2011, 3 },
            { Speaker::tuner, "Your fifth is 2 cents flat, Lyra.", Speaker::lyra, "Equal temperament flattens it on purpose, Tuner. A pure 3:2 is 702 cents. Twelve equal steps give 700.", Effect::interval, { 0, 7, kNo, kNo }, 0, 3 },
            { Speaker::nyquist, "Fourier, show me a frequency above my gate.", Speaker::fourier, "Playing 30 kHz at 48 kHz sampling. It folds to 18 kHz. A mirror image, and I cannot tell it from the original.", Effect::radiate, { kNo, kNo, kNo, kNo }, 0, 4 },
            { Speaker::echo, "Sub, say something.", Speaker::sub, "...felt that? A 30 Hz wave is 11 metres long. It crossed the room before you heard it.", Effect::interval, { -24, -12, kNo, kNo }, 0, 4 },
            { Speaker::cadence, "Nyquist, you are supposed to be neutral.", Speaker::nyquist, "I am a threshold. Neutral is a position, and I stand in it at half the sample rate.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 5 },
            { Speaker::lyra, "Tuner, play me something that is not 440.", Speaker::tuner, "432. It is 31.77 cents flat. There. Do you feel healed? ... I thought not.", Effect::interval, { 0, -1, kNo, kNo }, 0, 5 },
            { Speaker::fourier, "Moth, what is the loudest thing you have logged?", Speaker::moth, "LOGGED: A PISTOL SHOT, ROUGHLY 160 DB SPL. LOGGED: A WHISPER, ROUGHLY 30. THE COLONY PREFERS THE WHISPER.", Effect::none, { kNo, kNo, kNo, kNo }, 0, 6 },
            { Speaker::cadence, "Tuner. Why are you still here?", Speaker::tuner, "Because tension needs somebody to want it gone. Every cadence needs a voice that fears the resolution.", Effect::chord, { 7, 11, 14, kNo }, 0, 7 },
            { Speaker::moth, "ALL VOICES PRESENT. NO NEW MEASUREMENTS REQUIRED.", Speaker::lyra, "Then we only listen. That was always the last movement.", Effect::chord, { 0, 4, 7, 12 }, 0, 8 }
        }};
        return b;
    }

    // ---- endings: what Act IX makes of how you played ----------------------

    struct Ending
    {
        const char* name;
        Speaker speaker;
        const char* text;
    };

    inline const std::array<Ending, 4>& endings()
    {
        static const std::array<Ending, 4> e {{
            { "RESOLVED", Speaker::cadence,
              "Dominant to tonic. You answered the lab's questions the way a good ear does, and every voice has somewhere to rest. The colony holds the last chord for as long as you let it." },
            { "THE HAUNTED SCORE", Speaker::moth,
              "SECRETS FOUND: ENOUGH. THE ROOMS BEHIND THE ROOMS WERE PART OF THE COMPOSITION. THE VISITOR SENDS ITS REGARDS, BACKWARDS." },
            { "THE ARCHIVE", Speaker::lyra,
              "You kept so many sounds that the jar has become a record of the whole season. Play them in any order. They will still sound like one piece." },
            { "OPEN CADENCE", Speaker::echo,
              "The ending is not an ending. It resolves to the dominant and waits. Come back. Come back. Come back." }
        }};
        return e;
    }

    inline int endingFor (const Progress& p)
    {
        const float accuracy = p.quizzesAsked > 0 ? (float) p.quizzesCorrect / (float) p.quizzesAsked : 0.0f;
        if (p.secretCount() >= 20) return 1;
        if (p.quizzesAsked >= 10 && accuracy >= 0.8f) return 0;
        if (p.collected >= 30) return 2;
        return 3;
    }

    /** SplitMix64 - small, seedable, identical everywhere. */
    struct Rng
    {
        std::uint64_t s;
        explicit Rng (std::uint64_t seed) : s (seed ^ 0x9e3779b97f4a7c15ull) {}
        std::uint64_t next()
        {
            std::uint64_t z = (s += 0x9e3779b97f4a7c15ull);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            return z ^ (z >> 31);
        }
        int range (int lo, int hi) { return lo + (int) (next() % (std::uint64_t) (hi - lo + 1)); }
        double unit() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    };

    /** Everything that makes this run's story unlike the last one. */
    class RunStory
    {
    public:
        explicit RunStory (std::uint64_t seed) : rng (seed)
        {
            root = rng.range (48, 59);
            mode = rng.range (0, (int) modes().size() - 1);
            twist = rng.range (0, (int) twists().size() - 1);
            twistAtSec = 150.0 + rng.unit() * 330.0;
            nextEventSec = 20.0 + rng.unit() * 25.0;
            nextQuizSec = 120.0 + rng.unit() * 120.0;
            nextBanterSec = 75.0 + rng.unit() * 90.0;

            for (int i = 0; i < lexiconSize; ++i) deck.push_back (i);
            for (int i = lexiconSize - 1; i > 0; --i)
                std::swap (deck[(std::size_t) i], deck[(std::size_t) rng.range (0, i)]);
        }

        int root = 60, mode = 0, twist = 0;
        double twistAtSec = 300.0, nextEventSec = 30.0, nextQuizSec = 180.0;
        double nextBanterSec = 120.0;
        bool twistRevealed = false;
        bool finaleDone = false;

        /** Next event for this act. Unheard lines first, so the lexicon grows,
            but heard ones still return so a run never falls silent. */
        int drawEvent (int act, const Progress& p)
        {
            int fallback = -1;
            for (std::size_t k = 0; k < deck.size(); ++k)
            {
                const std::size_t idx = (cursor + k) % deck.size();
                const int id = deck[idx];
                if (events()[(std::size_t) id].minAct > act) continue;
                if (! p.heard (id)) { cursor = idx + 1; return id; }
                if (fallback < 0) fallback = (int) idx;
            }
            if (fallback < 0) return -1;
            cursor = (std::size_t) fallback + 1;
            return deck[(std::size_t) fallback];
        }

        /** An exchange that suits the act; never the same one twice in a row. */
        int drawBanter (int act)
        {
            std::vector<int> pool;
            for (int i = 0; i < (int) banters().size(); ++i)
                if (banters()[(std::size_t) i].minAct <= act && i != lastBanter) pool.push_back (i);
            if (pool.empty()) return -1;
            lastBanter = pool[(std::size_t) rng.range (0, (int) pool.size() - 1)];
            return lastBanter;
        }

        double gapAfterBanter() { return 130.0 + rng.unit() * 150.0; }

        int drawQuiz (int act)
        {
            std::vector<int> pool;
            for (int i = 0; i < (int) quizzes().size(); ++i)
                if (quizzes()[(std::size_t) i].minAct <= act && i != lastQuiz) pool.push_back (i);
            if (pool.empty()) return -1;
            lastQuiz = pool[(std::size_t) rng.range (0, (int) pool.size() - 1)];
            return lastQuiz;
        }

        /** Seconds until the next event: random, a little quicker in later acts. */
        double gapAfterEvent (int act)
        {
            const double base = 55.0 + rng.unit() * 70.0;
            return base * (1.0 - 0.05 * std::clamp (act - 1, 0, 6));
        }

        double gapAfterQuiz() { return 170.0 + rng.unit() * 160.0; }

        /** Semitone offsets for the scale effect: the run's own mode by default,
            or a fixed one when an event names it. */
        std::vector<int> scaleNotes (int which) const
        {
            const int m = which == 1 ? 6 : which == 2 ? 9 : which == 3 ? 0 : mode;
            std::vector<int> out;
            for (auto s : modes()[(std::size_t) m].steps)
                if (s != kNo) out.push_back (s);
            return out;
        }

        std::string keyName() const
        {
            return std::string (noteName (root)) + " " + modes()[(std::size_t) mode].name;
        }

    private:
        Rng rng;
        std::vector<int> deck;
        std::size_t cursor = 0;
        int lastQuiz = -1;
        int lastBanter = -1;
    };
}
