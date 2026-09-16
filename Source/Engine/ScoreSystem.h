#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "OrganismState.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        The score.

        It is deliberately meaningless - nothing in the engine reads it back -
        but it is the thing that turns a sound toy into a game, so the rules
        have to be legible from the number alone:

          * it climbs faster the more the sound varies
          * it stops dead while the sound is stuck in noise
          * clicking to remove elements clears the noise and starts it again
          * interacting at all builds a combo multiplier that decays if you
            stop, so playing is worth more than watching

        Everything here runs on the message thread from the published
        snapshot. Nothing on the audio thread knows the score exists.
        ------------------------------------------------------------------ */

    struct ScoreEvent
    {
        juce::String  text;
        int           points = 0;
        juce::Colour  colour { 0xffffffff };
        float         life = 1.0f;       // 1 -> 0
        float         y = 0.0f;          // drift for the toast
    };

    struct HighScore
    {
        juce::String name  { "colony" };
        juce::String world { "WORLD" };
        juce::int64  score = 0;
        double       seconds = 0.0;
        float        peakVariety = 0.0f;
        int          generations = 0;
        int          discoveries = 0;
        juce::int64  when = 0;           // ms since epoch
    };

    class ScoreSystem
    {
    public:
        ScoreSystem();

        /** Feed a fresh snapshot. dt in seconds. */
        void update (const EngineSnapshot& snap, double dt);

        /** The player did something. `weight` 0..1 scales how much combo it
            builds; removing elements while noise-locked counts for more. */
        void registerInteraction (float weight, const juce::String& label = {});

        /** A RADIATE landed. -1 wipes the score, +1 is a windfall. */
        void onRadiation (int outcome);

        /** A sample was dropped in and eaten. */
        void onSampleDigested (const juce::String& name);

        void reset();

        // ---- read-out --------------------------------------------------
        juce::int64 score()      const { return (juce::int64) total; }
        float  multiplier()      const { return multiplierValue; }
        int    combo()           const { return comboCount; }
        bool   frozen()          const { return isFrozen; }
        float  rate()            const { return lastRate; }        // points/sec
        double elapsed()         const { return runSeconds; }
        float  peakVariety()     const { return peakVar; }
        int    discoveries()     const { return discoveryCount; }
        const juce::String& frozenReason() const { return freezeReason; }

        const std::vector<ScoreEvent>& events() const { return feed; }

        /*  The fractal reward.

            True at most once per earning, and only when the sound is genuinely
            appealing *and* the score is climbing well. It has a long cooldown
            on purpose: a reward that shows up whenever you are doing fine
            stops reading as a reward within about a minute.                 */
        bool consumeRewardFlash();

        // ---- persistence -------------------------------------------------
        static juce::File scoresFile();
        void loadTable();
        void saveTable() const;

        /** File the current run into the table. Keeps the top 20. */
        void submit (const juce::String& playerName, const EngineSnapshot& snap);

        const std::vector<HighScore>& table() const { return highScores; }

        /** Rank the current score would take, 1-based; 0 if it would not make
            the table. Shown live so the player knows what is at stake. */
        int projectedRank() const;

    private:
        void pushEvent (const juce::String& text, int points, juce::Colour c);
        void detectMilestones (const EngineSnapshot& snap, double dt);

        double total = 0.0;
        float  multiplierValue = 1.0f;
        int    comboCount = 0;
        float  comboTimer = 0.0f;
        float  lastRate = 0.0f;
        bool   isFrozen = false;
        juce::String freezeReason;

        double runSeconds = 0.0;
        float  peakVar = 0.0f;
        int    discoveryCount = 0;

        // milestone tracking
        float lastCoverage = 0.0f;
        int   lastGeneration = 0;
        int   lastPopulation = 0;
        bool  wasNoiseLocked = false;
        bool  wasStuck = false;
        float sinceMilestone = 0.0f;

        // reward gating
        float goodSeconds = 0.0f;
        float rewardCooldown = 0.0f;
        bool  rewardPending = false;

        std::vector<ScoreEvent> feed;
        std::vector<HighScore>  highScores;
    };
}
