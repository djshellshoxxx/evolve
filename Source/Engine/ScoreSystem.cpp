#include "ScoreSystem.h"

namespace mutagen
{
    namespace
    {
        // Points per second at full variety, before multipliers. Tuned so a
        // lively colony reads in the tens of thousands after a few minutes -
        // big enough to feel like a score, small enough to stay readable.
        constexpr float baseRate = 140.0f;

        const juce::Colour cGood   { 0xff7fe3a0 };
        const juce::Colour cGreat  { 0xffffd36e };
        const juce::Colour cBad    { 0xffff6b6b };
        const juce::Colour cInfo   { 0xff8fb8ff };
        const juce::Colour cReward { 0xffe6a0ff };
    }

    ScoreSystem::ScoreSystem()
    {
        feed.reserve (16);
        highScores.reserve (24);
        loadTable();
    }

    void ScoreSystem::reset()
    {
        total = 0.0;
        multiplierValue = 1.0f;
        comboCount = 0;
        comboTimer = 0.0f;
        lastRate = 0.0f;
        isFrozen = false;
        freezeReason = {};
        runSeconds = 0.0;
        peakVar = 0.0f;
        discoveryCount = 0;
        lastCoverage = 0.0f;
        lastGeneration = 0;
        lastPopulation = 0;
        wasNoiseLocked = wasStuck = false;
        goodSeconds = 0.0f;
        rewardCooldown = 0.0f;
        rewardPending = false;
        feed.clear();
    }

    // -----------------------------------------------------------------------

    void ScoreSystem::update (const EngineSnapshot& snap, double dt)
    {
        if (dt <= 0.0) return;
        runSeconds += dt;
        const float fdt = (float) dt;

        // ---- combo decay ---------------------------------------------------
        comboTimer -= fdt;
        if (comboTimer <= 0.0f && comboCount > 0)
        {
            comboCount = 0;
            comboTimer = 0.0f;
        }

        // The multiplier chases the combo rather than snapping to it, so a
        // burst of clicking ramps up and then eases back down.
        const float comboTarget = 1.0f + juce::jmin (4.0f, (float) comboCount * 0.18f);
        multiplierValue += (comboTarget - multiplierValue) * (1.0f - std::exp (-fdt / 0.8f));

        // ---- the freeze ------------------------------------------------------
        /*  "If the sound does reach a point where it is just noise the score
            should stop counting up." The engine's noiseLocked flag is already
            hysteretic, so a musical burst of noise will not trip this - it
            takes a couple of seconds of genuinely flat spectrum.             */
        isFrozen = snap.noiseLocked;
        if (isFrozen)
            freezeReason = "SATURATED - right-click to strip elements";
        else if (snap.population == 0)
        {
            isFrozen = true;
            freezeReason = "COLONY DEAD - click to seed";
        }
        else
            freezeReason = {};

        // ---- the rate ---------------------------------------------------------
        if (isFrozen)
        {
            lastRate = 0.0f;
        }
        else
        {
            /*  "The more the sound varies the quicker the counter should go up."

                variety is the long-window movement of the spectrum, so it is
                literally the requested quantity. Novelty and archive coverage
                ride on top as smaller bonuses - they reward the colony finding
                sounds it has not made before, not merely wobbling.           */
            const float variety  = juce::jlimit (0.0f, 1.0f, snap.variety);
            const float novelty  = juce::jlimit (0.0f, 1.0f, snap.novelty);
            const float coverage = juce::jlimit (0.0f, 1.0f, snap.coverage);

            // A little floor so a quiet, slow, deliberate colony still ticks.
            float r = baseRate * (0.08f + 0.92f * std::pow (variety, 1.25f));
            r *= 0.75f + 0.5f * novelty;
            r *= 0.85f + 0.45f * coverage;

            // Being pleasant is worth something, but only a little - otherwise
            // the optimal strategy would be one consonant drone, which is the
            // opposite of what the score is meant to encourage.
            r *= 0.9f + 0.25f * juce::jlimit (0.0f, 1.0f, snap.appeal);

            // Approaching the noise lock costs you before it actually bites,
            // so the number starts sagging while there is still time to act.
            r *= 1.0f - 0.7f * juce::jlimit (0.0f, 1.0f, snap.greyness);

            lastRate = r * multiplierValue;
            total += (double) lastRate * dt;
        }

        if (snap.variety > peakVar) peakVar = snap.variety;

        // ---- event feed ageing ------------------------------------------------
        for (auto& e : feed)
        {
            e.life -= fdt * 0.55f;
            e.y += fdt * 18.0f;
        }
        feed.erase (std::remove_if (feed.begin(), feed.end(),
                    [] (const ScoreEvent& e) { return e.life <= 0.0f; }), feed.end());

        detectMilestones (snap, dt);

        // ---- the fractal reward --------------------------------------------
        if (rewardCooldown > 0.0f) rewardCooldown -= fdt;

        /*  The gate has to mean "better than this colony's normal", and what
            normal *is* moved when the noisiness measurement was fixed.
            Appeal carries a tonalness term, and tonalness was pinned near zero
            for every colony while flatness was saturating at 1.000 - so the
            measured appeal of six healthy runs was 0.52-0.59 and this gate,
            set at 0.62, was one the instrument could essentially never pass.
            That is why the fractal reward had never been seen.

            The same six runs now measure 0.762-0.826, median about 0.78, so
            the gate sits at 0.76: inside the range the instrument actually
            occupies, and low enough that a colony having a good few seconds
            can clear it. 0.80 was tried first and is too high - it is above
            the mean of four of the six worlds, and a two-minute capture of
            the running app caught no reward at all.

            Scarcity is the cooldown's job, not this gate's. Seven seconds of
            *sustained* good play (the counter falls at twice the rate it
            rises, so a wobble resets it) and then 30-70 s before another one
            is possible, which puts it at a handful per long session. */
        const bool doingWell = ! isFrozen
                            && snap.appeal > 0.76f
                            && snap.variety > 0.42f
                            && lastRate > baseRate * 0.55f;

        if (doingWell) goodSeconds += fdt;
        else           goodSeconds = juce::jmax (0.0f, goodSeconds - fdt * 2.0f);

        if (goodSeconds > 7.0f && rewardCooldown <= 0.0f && ! rewardPending)
        {
            rewardPending = true;
            goodSeconds = 0.0f;
            // Long and slightly random, so it never becomes a metronome.
            rewardCooldown = 30.0f + juce::Random::getSystemRandom().nextFloat() * 40.0f;
        }
    }

    // -----------------------------------------------------------------------

    void ScoreSystem::detectMilestones (const EngineSnapshot& snap, double dt)
    {
        sinceMilestone += (float) dt;

        // --- a new corner of the behaviour space --------------------------
        if (snap.coverage > lastCoverage + 0.008f)
        {
            lastCoverage = snap.coverage;
            ++discoveryCount;
            const int pts = 450;
            total += pts;
            pushEvent ("NEW TIMBRE", pts, cGreat);
        }
        else if (snap.coverage < lastCoverage)
        {
            lastCoverage = snap.coverage;
        }

        // --- generations ----------------------------------------------------
        if (snap.generation > lastGeneration)
        {
            const int gained = snap.generation - lastGeneration;
            lastGeneration = snap.generation;
            if (gained > 0 && snap.generation % 10 == 0)
            {
                const int pts = 300;
                total += pts;
                pushEvent ("GENERATION " + juce::String (snap.generation), pts, cInfo);
            }
        }

        // --- recovering from a noise lock ------------------------------------
        if (wasNoiseLocked && ! snap.noiseLocked)
        {
            const int pts = 1200;
            total += pts;
            pushEvent ("COLONY RESCUED", pts, cGood);
            registerInteraction (1.0f);
        }
        if (! wasNoiseLocked && snap.noiseLocked)
            pushEvent ("SATURATED", 0, cBad);
        wasNoiseLocked = snap.noiseLocked;

        // --- unsticking a drone -----------------------------------------------
        if (wasStuck && ! snap.stuck)
            pushEvent ("MOVING AGAIN", 200, cGood), total += 200;
        wasStuck = snap.stuck;

        // --- speciation ---------------------------------------------------------
        if (snap.population > lastPopulation + 12 && sinceMilestone > 4.0f)
        {
            sinceMilestone = 0.0f;
            const int pts = 250;
            total += pts;
            pushEvent ("BLOOM", pts, cGood);
        }
        lastPopulation = snap.population;
    }

    // -----------------------------------------------------------------------

    void ScoreSystem::registerInteraction (float weight, const juce::String& label)
    {
        weight = juce::jlimit (0.0f, 1.0f, weight);

        // Acting while frozen is worth extra: it is exactly the move the game
        // wants the player to learn.
        const float bonus = isFrozen ? 2.0f : 1.0f;

        comboCount = juce::jmin (40, comboCount + (int) std::ceil (weight * 2.0f * bonus));
        comboTimer = 3.5f;

        if (label.isNotEmpty())
        {
            const int pts = (int) (60.0f * weight * bonus * multiplierValue);
            total += pts;
            pushEvent (label, pts, isFrozen ? cGood : cInfo);
        }
    }

    void ScoreSystem::adjustScore (juce::int64 points, const juce::String& label)
    {
        const auto before = (juce::int64) total;
        total = juce::jmax (0.0, total + (double) points);
        const auto applied = (juce::int64) total - before;
        pushEvent (label, (int) juce::jlimit<juce::int64> (-2000000000LL, 2000000000LL, applied),
                   applied < 0 ? cBad : applied > 0 ? cReward : cInfo);
    }

    void ScoreSystem::onRadiation (int outcome)
    {
        if (outcome < 0)
        {
            // The stated 5%. Everything goes.
            total = 0.0;
            comboCount = 0;
            multiplierValue = 1.0f;
            pushEvent ("FATAL DOSE - SCORE LOST", 0, cBad);
        }
        else if (outcome > 0)
        {
            const int pts = 2500;
            total += pts;
            pushEvent ("BENEFICIAL MUTATION", pts, cGreat);
            registerInteraction (1.0f);
        }
        else
        {
            pushEvent ("IRRADIATED", 0, cInfo);
            registerInteraction (0.5f);
        }
    }

    void ScoreSystem::onSampleDigested (const juce::String& name)
    {
        const int pts = 1000;
        total += pts;
        pushEvent ("DIGESTED " + name.toUpperCase(), pts, cGreat);
        registerInteraction (1.0f);
    }

    bool ScoreSystem::consumeRewardFlash()
    {
        if (! rewardPending) return false;
        rewardPending = false;
        return true;
    }

    void ScoreSystem::pushEvent (const juce::String& text, int points, juce::Colour c)
    {
        if (feed.size() > 7) feed.erase (feed.begin());
        ScoreEvent e;
        e.text = text;
        e.points = points;
        e.colour = c;
        e.life = 1.0f;
        e.y = 0.0f;
        feed.push_back (e);
    }

    // =======================================================================
    //  Persistence
    // =======================================================================

    juce::File ScoreSystem::scoresFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MUTAGEN")
                   .getChildFile ("scores.json");
    }

    void ScoreSystem::loadTable()
    {
        highScores.clear();

        const auto f = scoresFile();
        if (! f.existsAsFile()) return;

        const auto parsed = juce::JSON::parse (f.loadFileAsString());
        if (auto* arr = parsed.getArray())
        {
            for (const auto& item : *arr)
            {
                if (! item.isObject()) continue;
                HighScore h;
                h.name        = item.getProperty ("name", "colony").toString();
                h.world       = item.getProperty ("world", "WORLD").toString();
                h.score       = (juce::int64) (double) item.getProperty ("score", 0);
                h.seconds     = (double) item.getProperty ("seconds", 0);
                h.peakVariety = (float) (double) item.getProperty ("peakVariety", 0);
                h.generations = (int) item.getProperty ("generations", 0);
                h.discoveries = (int) item.getProperty ("discoveries", 0);
                h.when        = (juce::int64) (double) item.getProperty ("when", 0);
                highScores.push_back (h);
            }
        }

        std::sort (highScores.begin(), highScores.end(),
                   [] (const HighScore& a, const HighScore& b) { return a.score > b.score; });
    }

    void ScoreSystem::saveTable() const
    {
        juce::Array<juce::var> arr;
        for (const auto& h : highScores)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", h.name);
            o->setProperty ("world", h.world);
            o->setProperty ("score", (double) h.score);
            o->setProperty ("seconds", h.seconds);
            o->setProperty ("peakVariety", (double) h.peakVariety);
            o->setProperty ("generations", h.generations);
            o->setProperty ("discoveries", h.discoveries);
            o->setProperty ("when", (double) h.when);
            arr.add (juce::var (o));
        }

        const auto f = scoresFile();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::JSON::toString (juce::var (arr), false));
    }

    void ScoreSystem::submit (const juce::String& playerName, const EngineSnapshot& snap)
    {
        HighScore h;
        h.name        = playerName.isNotEmpty() ? playerName : juce::String ("colony");
        h.world       = juce::String (snap.worldName);
        h.score       = (juce::int64) total;
        h.seconds     = runSeconds;
        h.peakVariety = peakVar;
        h.generations = snap.generation;
        h.discoveries = discoveryCount;
        h.when        = juce::Time::getCurrentTime().toMilliseconds();

        highScores.push_back (h);
        std::sort (highScores.begin(), highScores.end(),
                   [] (const HighScore& a, const HighScore& b) { return a.score > b.score; });
        if (highScores.size() > 20) highScores.resize (20);

        saveTable();
    }

    int ScoreSystem::projectedRank() const
    {
        const auto s = (juce::int64) total;
        int rank = 1;
        for (const auto& h : highScores)
        {
            if (h.score > s) ++rank;
            else break;
        }
        return rank <= 20 ? rank : 0;
    }
}
