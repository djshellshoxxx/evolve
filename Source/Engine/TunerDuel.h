#pragma once

// The Tuner Duels: tension and resolution bosses (docs/specs/tuner-duels-spec.md).
//
// Pure standard C++ (no JUCE). The GUI drives a `duel::Duel` with update(dt) and
// grab/drop input, and reads state, normalised cell/target positions and the
// notes to sound. All enums are append-only (values may be persisted).

#include "Storyline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mutagen::story::duel
{
    // ---- chords -----------------------------------------------------------

    /** Why a chord tone is unstable. Append-only. */
    enum class Tension : int { stable, leadingTone, seventh, tritone, suspension, augmented, count };

    /** Append-only. */
    enum class ChordType : int { dom7, sus4, dimTriad, dim7, halfDim7, aug, count };

    constexpr int chordTypeCount = (int) ChordType::count;

    /** Target chord quality. 0 = follow the mode. */
    enum class Quality : int { fromMode, major, minor };

    struct UnstableDef { int tone; Tension kind; int delta; };

    struct TypeDef
    {
        const char* name;
        int count;                          // chord tones
        std::array<int, 4> tones;           // semitones above the chord root
        int unstableCount;                  // entries used in `unstable`
        std::array<UnstableDef, 3> unstable;
        int targetOffset;                   // target chord root = chord root + this
        int tier;                           // first difficulty tier that may use it
    };

    inline const TypeDef& typeDef (ChordType t)
    {
        static const std::array<TypeDef, chordTypeCount> d {{
            { "dominant 7th",   4, { 0, 4, 7, 10 }, 2, {{ { 1, Tension::leadingTone, +1 }, { 3, Tension::seventh, -1 }, { 0, Tension::stable, 0 } }}, 5, 1 },
            { "suspended 4th",  3, { 0, 5, 7, 0 },  1, {{ { 1, Tension::suspension, -1 }, { 0, Tension::stable, 0 }, { 0, Tension::stable, 0 } }}, 0, 1 },
            { "diminished",     3, { 0, 3, 6, 0 },  2, {{ { 0, Tension::leadingTone, +1 }, { 2, Tension::tritone, -1 }, { 0, Tension::stable, 0 } }}, 1, 1 },
            { "diminished 7th", 4, { 0, 3, 6, 9 },  3, {{ { 0, Tension::leadingTone, +1 }, { 2, Tension::tritone, -1 }, { 3, Tension::seventh, -1 } }}, 1, 2 },
            { "half-dim 7th",   4, { 0, 3, 6, 10 }, 3, {{ { 0, Tension::leadingTone, +1 }, { 2, Tension::tritone, -1 }, { 3, Tension::seventh, -2 } }}, 1, 2 },
            { "augmented",      3, { 0, 4, 8, 0 },  2, {{ { 2, Tension::augmented, +1 }, { 1, Tension::leadingTone, +1 }, { 0, Tension::stable, 0 } }}, 5, 2 }
        }};
        return d[(std::size_t) std::clamp ((int) t, 0, chordTypeCount - 1)];
    }

    inline const char* tensionName (Tension t)
    {
        constexpr const char* n[] = { "stable", "leading tone", "seventh", "tritone", "suspension", "augmented" };
        return n[std::clamp ((int) t, 0, (int) Tension::count - 1)];
    }

    /** A live chord: absolute MIDI pitches plus which tones are still unstable. */
    struct Chord
    {
        ChordType type = ChordType::dom7;
        int root = 48;
        int count = 0;
        std::array<int, 4> pitch { 0, 0, 0, 0 };
        std::array<Tension, 4> kind { Tension::stable, Tension::stable, Tension::stable, Tension::stable };
        int targetRoot = 53;                         // absolute MIDI root of the chord it resolves to
        Quality targetQuality = Quality::fromMode;

        int unstableCount() const
        {
            int n = 0;
            for (int i = 0; i < count; ++i) if (kind[(std::size_t) i] != Tension::stable) ++n;
            return n;
        }
    };

    /** Build a chord with its first `unstable` unstable tones marked. */
    inline Chord makeChord (ChordType type, int root, int unstable, int targetRoot, Quality q)
    {
        const auto& d = typeDef (type);
        Chord c;
        c.type = type;
        c.root = root;
        c.count = d.count;
        c.targetRoot = targetRoot;
        c.targetQuality = q;
        for (int i = 0; i < d.count; ++i)
            c.pitch[(std::size_t) i] = root + d.tones[(std::size_t) i];
        for (int i = 0; i < std::min (unstable, d.unstableCount); ++i)
            c.kind[(std::size_t) d.unstable[(std::size_t) i].tone] = d.unstable[(std::size_t) i].kind;
        return c;
    }

    /** True when the mode's third is minor (so a tonic resolution is minor). */
    inline bool modeIsMinor (int mode)
    {
        const auto& m = modes()[(std::size_t) std::clamp (mode, 0, (int) modes().size() - 1)];
        for (int i = 0; i < 4; ++i)
        {
            const int s = m.steps[(std::size_t) i];
            if (s == 4) return false;
            if (s == 3) return true;
        }
        return false;
    }

    inline bool targetIsMinor (const Chord& c, int mode)
    {
        if (c.targetQuality == Quality::major) return false;
        if (c.targetQuality == Quality::minor) return true;
        return modeIsMinor (mode);
    }

    /** Semitone offsets of the target chord's tones above its root. */
    inline std::array<int, 3> targetTriad (bool minor) { return { 0, minor ? 3 : 4, 7 }; }

    struct Resolution
    {
        int tone = 0;                       // index into Chord::pitch
        Tension kind = Tension::stable;
        int from = 0;
        int preferred = 0;                  // the textbook resolution pitch
        int count = 0;                      // accepted pitches
        std::array<int, 8> accepted {};     // every pitch that lands on a tone of the target chord
    };

    inline bool isTargetTone (const Chord& c, bool minor, int pitch)
    {
        for (int off : targetTriad (minor))
            if ((((pitch - c.targetRoot - off) % 12) + 12) % 12 == 0)
                return true;
        return false;
    }

    /** All still-unstable tones with the pitches that resolve them. Any pitch
        within a whole tone (widening if needed) that is a tone of the target
        chord is accepted; `preferred` follows the textbook direction. */
    inline std::vector<Resolution> resolutionsFor (const Chord& c, int mode)
    {
        const auto& d = typeDef (c.type);
        const bool minor = targetIsMinor (c, mode);
        std::vector<Resolution> out;
        for (int i = 0; i < c.count; ++i)
        {
            const auto k = c.kind[(std::size_t) i];
            if (k == Tension::stable) continue;
            int delta = 0;
            for (int u = 0; u < d.unstableCount; ++u)
                if (d.unstable[(std::size_t) u].tone == i) delta = d.unstable[(std::size_t) u].delta;
            if (delta == 0) delta = (k == Tension::seventh || k == Tension::tritone || k == Tension::suspension) ? -1 : 1;

            Resolution r;
            r.tone = i;
            r.kind = k;
            r.from = c.pitch[(std::size_t) i];
            for (int radius = 2; radius <= 4 && r.count == 0; ++radius)
            {
                int best = 0, bestScore = 1 << 20;
                for (int dist = 1; dist <= radius; ++dist)
                    for (int sgn = -1; sgn <= 1; sgn += 2)
                    {
                        const int p = r.from + sgn * dist;
                        if (! isTargetTone (c, minor, p) || r.count >= 8) continue;
                        r.accepted[(std::size_t) r.count++] = p;
                        const int score = std::abs (p - (r.from + delta)) * 4 + dist;
                        if (score < bestScore) { bestScore = score; best = p; }
                    }
                r.preferred = best;
            }
            out.push_back (r);
        }
        return out;
    }

    inline bool isResolved (const Chord& c) { return c.unstableCount() == 0; }

    // ---- relics -----------------------------------------------------------

    constexpr int relicCount = 8;

    struct Relic { const char* name; int degree; float cents; const char* blurb; };

    /** Each detunes one scale degree (0-based, 0 = root) by 10 to 40 cents. */
    inline const std::array<Relic, relicCount>& relics()
    {
        static const std::array<Relic, relicCount> r {{
            { "Pythagorean comma", 6, +24.0f, "Twelve pure fifths overshoot seven octaves by 23.5 cents." },
            { "Syntonic comma",    2, -22.0f, "The 81:80 gap between a stacked-fifths third and a pure one." },
            { "Werckmeister",      5, -12.0f, "A Baroque well-temperament: every key usable, each with its colour." },
            { "Kirnberger",        1, +14.0f, "Pure thirds on the commonest keys, wolves tucked away." },
            { "Meantone",          4, -17.0f, "Fifths tempered narrow so the thirds ring sweet." },
            { "Just 5th",          3, +20.0f, "A pure 3:2 fifth, and the fourth beneath it sits a shade high." },
            { "Septimal",          6, -31.0f, "The 7:4 seventh, 31 cents flatter than equal temperament." },
            { "Quarter-tone",      1, +40.0f, "Half of a semitone, the Arabic and Turkish maqam colour." }
        }};
        return r;
    }

    constexpr float maxRelicCents = 40.0f;

    inline bool hasRelic (std::uint32_t mask, int id)
    {
        return id >= 0 && id < relicCount && ((mask >> id) & 1u) != 0;
    }

    inline std::uint32_t withRelic (std::uint32_t mask, int id)
    {
        return (id >= 0 && id < relicCount) ? (mask | (1u << id)) : mask;
    }

    inline int relicsOwned (std::uint32_t mask) { return Progress::bits (mask & 0xFFu); }

    /** Cents offset for a scale degree (0-based) from every relic owned. Bounded to +-40. */
    inline float centsForDegree (std::uint32_t mask, int degree)
    {
        float sum = 0.0f;
        for (int i = 0; i < relicCount; ++i)
            if (hasRelic (mask, i) && relics()[(std::size_t) i].degree == degree)
                sum += relics()[(std::size_t) i].cents;
        return std::clamp (sum, -maxRelicCents, maxRelicCents);
    }

    /** Cents offset for a pitch given the key's tonic and mode (0 when not in the scale). */
    inline float centsForPitch (std::uint32_t mask, int tonic, int mode, int midi)
    {
        if (mask == 0) return 0.0f;
        const auto& steps = modes()[(std::size_t) std::clamp (mode, 0, (int) modes().size() - 1)].steps;
        const int rel = (((midi - tonic) % 12) + 12) % 12;
        int degree = 0;
        for (int s : steps)
        {
            if (s == kNo || s >= 12) break;
            if (s == rel) return centsForDegree (mask, degree);
            ++degree;
        }
        return 0.0f;
    }

    /** Pick an unowned relic; -1 if all are owned. */
    inline int pickRelic (std::uint32_t owned, Rng& rng)
    {
        std::array<int, relicCount> free {};
        int n = 0;
        for (int i = 0; i < relicCount; ++i)
            if (! hasRelic (owned, i)) free[(std::size_t) n++] = i;
        return n == 0 ? -1 : free[(std::size_t) rng.range (0, n - 1)];
    }

    /** Duel-aware ending: three or more duels won favours RESOLVED (0) unless the secret ending triggers. */
    inline int endingFor (const Progress& p, std::uint32_t duelsWon)
    {
        const int base = story::endingFor (p);
        if (base == 1) return base;
        if (duelsWon >= 3 && (base == 3 || base == 2)) return 0;
        if (duelsWon >= 2 && base == 3 && p.quizzesAsked >= 5) return 0;
        return base;
    }

    // ---- difficulty and rounds ---------------------------------------------

    struct Difficulty
    {
        int tier = 1;               // 1: acts 2-3, 2: acts 4-6, 3: acts 7-9
        int rounds = 1;
        int minUnstable = 1;
        int maxUnstable = 2;
        bool deceptive = false;
        bool borrowed = false;
        float resolveSeconds = 45.0f;
        float wrongStep = 0.22f;    // tension added per wrong drop
        float passiveRate = 0.004f; // tension per second while resolving
    };

    inline Difficulty difficultyFor (int act)
    {
        Difficulty d;
        if (act >= 7)      d = { 3, 3, 2, 3, true, true, 30.0f, 0.30f, 0.008f };
        else if (act >= 4) d = { 2, 2, 1, 3, false, false, 38.0f, 0.26f, 0.006f };
        else               d = { 1, 1, 1, 2, false, false, 45.0f, 0.22f, 0.004f };
        return d;
    }

    constexpr int triesPerRound = 3;

    struct Round
    {
        Chord chord;
        int tonic = 48;
        int mode = 0;
        bool deceptive = false;     // V-vi instead of V-I
        bool borrowed = false;      // target quality borrowed from the parallel mode
        int unstable = 1;
    };

    /** Deterministic for a given Rng state. `tonic` is the run's root (MIDI), `mode` its mode index. */
    inline Round makeRound (Rng& rng, int act, int mode = 0, int tonic = 48)
    {
        const auto diff = difficultyFor (act);
        Round r;
        r.tonic = tonic;
        r.mode = mode;

        std::array<ChordType, chordTypeCount> avail {};
        int n = 0;
        for (int i = 0; i < chordTypeCount; ++i)
            if (typeDef ((ChordType) i).tier <= diff.tier) avail[(std::size_t) n++] = (ChordType) i;
        const auto type = avail[(std::size_t) rng.range (0, n - 1)];

        r.deceptive = diff.deceptive && rng.unit() < 0.4;
        r.borrowed = diff.borrowed && rng.unit() < 0.4;

        bool minor = modeIsMinor (mode);
        if (r.borrowed) minor = ! minor;
        int degree = 0;
        if (r.deceptive) { degree = modeIsMinor (mode) ? 8 : 9; minor = ! modeIsMinor (mode); }
        const auto q = minor ? Quality::minor : Quality::major;

        const int targetPc = (((tonic + degree) % 12) + 12) % 12;
        const int chordRootPc = (((targetPc - typeDef (type).targetOffset) % 12) + 12) % 12;
        const int root = 48 + chordRootPc;

        const int maxU = std::min (diff.maxUnstable, typeDef (type).unstableCount);
        const int minU = std::min (diff.minUnstable, maxU);
        r.unstable = rng.range (minU, maxU);
        r.chord = makeChord (type, root, r.unstable, root + typeDef (type).targetOffset, q);
        return r;
    }

    // ---- trigger ------------------------------------------------------------

    struct TriggerContext
    {
        int act = 1;
        bool twistSeenThisAct = false;      // the act's first twist has fired
        double unpausedSecondsThisAct = 0.0;
        bool duelDoneThisAct = false;
        bool phantomCall = false, dreamRoom = false, quiz = false;
    };

    constexpr double triggerMinSeconds = 360.0;

    /** From Act 2, once per act, after the first twist and 6 minutes (whichever is later). */
    inline bool shouldTrigger (const TriggerContext& c)
    {
        if (c.act < 2 || c.duelDoneThisAct) return false;
        if (c.phantomCall || c.dreamRoom || c.quiz) return false;
        return c.twistSeenThisAct && c.unpausedSecondsThisAct >= triggerMinSeconds;
    }

    // ---- the duel -------------------------------------------------------------

    /** Append-only. */
    enum class State : int { idle, quantise, tension, resolve, cadence, won, lost, skipped, count };

    enum class DropResult : int { ignored, correct, wrong };

    struct Cell
    {
        int pitch = 0;
        float x = 0.5f, y = 0.3f;       // normalised [0,1]
        Tension kind = Tension::stable;
        bool unstable = false;          // highlight red and draggable
        bool resolved = false;          // was unstable, now settled
    };

    struct Target
    {
        int cell = 0;
        int pitch = 0;
        float x = 0.5f, y = 0.75f;
        bool preferred = false;
        bool hint = false;              // pulse this ring (after the second failure)
    };

    class Duel
    {
    public:
        static constexpr double quantiseSeconds = 2.0;
        static constexpr double tensionSeconds = 2.0;
        static constexpr double cadenceSeconds = 2.2;
        static constexpr double shatterSeconds = 2.5;
        static constexpr double sterileSeconds = 45.0;
        static constexpr float correctRelief = 0.15f;
        static constexpr int pitchSpan = 24;

        /** Begin a duel. `ownedRelics` is Progress.relics, used for the win reward. */
        void start (std::uint64_t seed, int act, int mode, int tonic, std::uint32_t ownedRelics = 0)
        {
            *this = Duel();
            rng = Rng (seed);
            actNum = std::clamp (act, 1, actCount);
            modeIdx = mode;
            tonicNote = tonic;
            diff = difficultyFor (actNum);
            owned = ownedRelics;
            rounds.clear();
            for (int i = 0; i < diff.rounds; ++i)
                rounds.push_back (makeRound (rng, actNum, mode, tonic));
            setState (State::quantise);
        }

        /** "Ignore" the Tuner: once per run. `ignoreUsedThisRun` is the caller's flag. */
        bool ignore (bool& ignoreUsedThisRun)
        {
            if (state_ != State::quantise || ignoreUsedThisRun) return false;
            ignoreUsedThisRun = true;
            setState (State::skipped);
            return true;
        }

        void update (double dt)
        {
            dt = std::max (0.0, dt);
            for (int guard = 0; guard < 64; ++guard)
            {
                const double rem = remaining();
                if (rem >= kInf || dt < rem) { advance (dt); break; }
                advance (rem);
                dt -= rem;
                transition();
            }
        }

        // -- input --
        bool grab (int cell)
        {
            if (state_ != State::resolve || cell < 0 || cell >= cellCount()) return false;
            return round().chord.kind[(std::size_t) cell] != Tension::stable;
        }

        DropResult dropPitch (int cell, int pitch)
        {
            if (state_ != State::resolve || ! grab (cell) || pitch == round().chord.pitch[(std::size_t) cell])
                return DropResult::ignored;
            for (const auto& r : resolutionsFor (round().chord, modeIdx))
                if (r.tone == cell)
                    for (int i = 0; i < r.count; ++i)
                        if (r.accepted[(std::size_t) i] == pitch)
                        {
                            auto& ch = rounds[(std::size_t) roundIdx].chord;
                            ch.pitch[(std::size_t) cell] = pitch;
                            ch.kind[(std::size_t) cell] = Tension::stable;
                            settled[(std::size_t) cell] = true;
                            tensionMeter = std::max (0.0f, tensionMeter - correctRelief);
                            ++correctDrops;
                            if (isResolved (ch)) setState (State::cadence);
                            return DropResult::correct;
                        }
            // wrong
            tensionMeter = std::min (1.0f, tensionMeter + diff.wrongStep);
            rub = 1.0f;
            ++wrongDrops;
            ++wrongThisRound;
            if (wrongThisRound >= triesPerRound || tensionMeter >= 1.0f)
            {
                tensionMeter = 1.0f;
                setState (State::lost);
            }
            return DropResult::wrong;
        }

        /** Drop at a normalised x on the pitch axis. */
        DropResult drop (int cell, float x) { return dropPitch (cell, xToPitch (x)); }

        // -- queries --
        State state() const { return state_; }
        bool finished() const { return state_ == State::won || state_ == State::lost || state_ == State::skipped; }
        bool active() const { return state_ != State::idle && ! finished(); }
        int act() const { return actNum; }
        int roundIndex() const { return roundIdx; }
        int roundCount() const { return (int) rounds.size(); }
        const Round& round() const { return rounds[(std::size_t) std::clamp (roundIdx, 0, std::max (0, (int) rounds.size() - 1))]; }
        const Difficulty& difficulty() const { return diff; }
        float tension() const { return tensionMeter; }
        int triesLeft() const { return std::max (0, triesPerRound - wrongThisRound); }
        bool hintActive() const { return state_ == State::resolve && wrongThisRound >= 2; }
        float rubAmount() const { return rub; }                     // 0..1, decays; wrong drops spike it
        int wrongCount() const { return wrongDrops; }
        int correctCount() const { return correctDrops; }
        float gridAmount() const                                    // 0..1 overlay strength
        {
            if (state_ == State::idle || state_ == State::skipped) return 0.0f;
            if (state_ == State::quantise) return (float) std::clamp (timer / quantiseSeconds, 0.0, 1.0);
            if (state_ == State::won) return 1.0f - shatterProgress();
            if (state_ == State::lost) return 0.0f;
            return 1.0f;
        }
        int gridLayers() const { return (int) rounds.size(); }
        int gridLayersBroken() const { return layersBroken; }
        float shatterProgress() const { return state_ == State::won ? (float) std::clamp (timer / shatterSeconds, 0.0, 1.0) : 0.0f; }
        /** 1 while the dish should be steered toward low divergence (dull, quantised). */
        float timbreDull() const { return (state_ == State::lost || state_ == State::skipped || state_ == State::idle) ? 0.0f : gridAmount(); }
        double sterileRemaining() const { return state_ == State::lost ? std::max (0.0, sterileSeconds - timer) : 0.0; }
        int rewardRelic() const { return relicWon; }                // -1 if none
        std::uint32_t relicMask() const { return owned; }           // include reward once won
        double phaseTime() const { return timer; }
        double resolveTimeLeft() const { return state_ == State::resolve ? std::max (0.0, (double) diff.resolveSeconds - timer) : 0.0; }

        int cellCount() const { return rounds.empty() ? 0 : round().chord.count; }

        int pitchLow() const { return rounds.empty() ? 0 : round().chord.root - 7; }
        float pitchToX (int pitch) const { return std::clamp ((float) (pitch - pitchLow()) / (float) pitchSpan, 0.0f, 1.0f); }
        int xToPitch (float x) const { return pitchLow() + (int) std::lround (std::clamp (x, 0.0f, 1.0f) * (float) pitchSpan); }

        std::vector<Cell> cells() const
        {
            std::vector<Cell> out;
            if (rounds.empty()) return out;
            const auto& ch = round().chord;
            for (int i = 0; i < ch.count; ++i)
            {
                Cell c;
                c.pitch = ch.pitch[(std::size_t) i];
                c.kind = ch.kind[(std::size_t) i];
                c.unstable = c.kind != Tension::stable;
                c.resolved = settled[(std::size_t) i];
                c.x = pitchToX (c.pitch);
                c.y = 0.3f;
                out.push_back (c);
            }
            return out;
        }

        /** Target rings for every unresolved unstable cell (empty outside the resolve phase). */
        std::vector<Target> targets() const
        {
            std::vector<Target> out;
            if (state_ != State::resolve) return out;
            for (const auto& r : resolutionsFor (round().chord, modeIdx))
                for (int i = 0; i < r.count; ++i)
                {
                    Target t;
                    t.cell = r.tone;
                    t.pitch = r.accepted[(std::size_t) i];
                    t.preferred = t.pitch == r.preferred;
                    t.hint = hintActive() && t.preferred;
                    t.x = pitchToX (t.pitch);
                    t.y = 0.75f;
                    out.push_back (t);
                }
            return out;
        }

        /** MIDI notes the engine should sound right now. */
        std::vector<int> notesToPlay() const
        {
            std::vector<int> n;
            if (rounds.empty()) return n;
            const auto& r = round();
            const bool minor = targetIsMinor (r.chord, modeIdx);
            auto targetChord = [&] {
                for (int off : targetTriad (minor)) n.push_back (r.chord.targetRoot + off);
                n.push_back (r.chord.targetRoot - 12);
            };
            switch (state_)
            {
                case State::tension:
                case State::resolve:
                    for (int i = 0; i < r.chord.count; ++i) n.push_back (r.chord.pitch[(std::size_t) i]);
                    break;
                case State::cadence:
                case State::won:
                    targetChord();
                    break;
                default: break;
            }
            return n;
        }

        /** The Tuner's current line (for the panel's say/queue). */
        const char* line() const
        {
            switch (state_)
            {
                case State::quantise: return "Hold still. Twelve equal steps. No more, no fewer.";
                case State::tension:  return "Hear that? Tension. I keep it, and you cannot have it.";
                case State::resolve:
                    return hintActive() ? "Lean toward the nearest chord tone... a semitone is enough."
                                        : (wrongThisRound > 0 ? "Wrong. It still aches." : "Resolve them, if you can.");
                case State::cadence:  return "Dominant to tonic. Fine. The grid cracks.";
                case State::won:      return "Resolved. Take it, a little out of tune. Imperfection has character.";
                case State::lost:     return "Sterile. Perfect. Silent. You may have your dish back later.";
                case State::skipped:  return "You ignore me. How very flat of you.";
                default:              return "";
            }
        }

    private:
        static constexpr double kInf = 1.0e18;

        double remaining() const
        {
            switch (state_)
            {
                case State::quantise: return std::max (0.0, quantiseSeconds - timer);
                case State::tension:  return std::max (0.0, tensionSeconds - timer);
                case State::resolve:  return std::max (0.0, (double) diff.resolveSeconds - timer);
                case State::cadence:  return std::max (0.0, cadenceSeconds - timer);
                case State::won:      return timer < shatterSeconds ? shatterSeconds - timer : kInf;
                case State::lost:     return timer < sterileSeconds ? sterileSeconds - timer : kInf;
                default:              return kInf;
            }
        }

        void advance (double step)
        {
            if (state_ == State::idle || state_ == State::skipped) return;
            timer += step;
            rub = std::max (0.0f, rub - (float) (step / 1.5));
            if (state_ == State::tension)
            {
                const float floorT = 0.08f * (float) round().unstable;
                tensionMeter = std::max (tensionMeter, floorT * (float) std::clamp (timer / tensionSeconds, 0.0, 1.0));
            }
            else if (state_ == State::resolve)
            {
                tensionMeter = std::min (1.0f, tensionMeter + diff.passiveRate * (float) step);
                if (tensionMeter >= 1.0f) setState (State::lost);
            }
        }

        void transition()
        {
            switch (state_)
            {
                case State::quantise: setState (State::tension); break;
                case State::tension:  setState (State::resolve); break;
                case State::resolve:  tensionMeter = 1.0f; setState (State::lost); break;   // timeout
                case State::cadence:
                    ++layersBroken;
                    if (roundIdx + 1 < (int) rounds.size()) { ++roundIdx; wrongThisRound = 0; settled = {}; setState (State::tension); }
                    else
                    {
                        relicWon = pickRelic (owned, rng);
                        owned = withRelic (owned, relicWon);
                        setState (State::won);
                    }
                    break;
                default: break;
            }
        }

        void setState (State s) { state_ = s; timer = 0.0; }

        State state_ = State::idle;
        Rng rng { 1 };
        std::vector<Round> rounds;
        Difficulty diff;
        int actNum = 1, modeIdx = 0, tonicNote = 48;
        int roundIdx = 0, wrongThisRound = 0, layersBroken = 0, wrongDrops = 0, correctDrops = 0;
        std::array<bool, 4> settled { false, false, false, false };
        float tensionMeter = 0.0f, rub = 0.0f;
        double timer = 0.0;
        std::uint32_t owned = 0;
        int relicWon = -1;
    };
}
