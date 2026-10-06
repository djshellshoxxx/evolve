#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mutagen::progression
{
    constexpr int maxLevel = 8;
    constexpr int finalStoryChapter = 7;

    enum class Feature : std::size_t
    {
        fieldJournal,
        researcherCharacters,
        storyChapters,
        storyBranches,
        researchProtocols,
        challengeDeck,
        anomalyCatalogue,
        relicCabinet,
        lineageCodex,
        worldAtlas,
        count
    };

    enum class Character : std::size_t
    {
        maraVoss,
        ivoChen,
        nadiOkafor,
        moth,
        junVale,
        visitor,
        count
    };

    enum class Directive : std::size_t
    {
        preserve,
        accelerate,
        release,
        count
    };

    struct LifetimeStats
    {
        std::int64_t lifetimeScore = 0;
        int generationsObserved = 0;
        int lifetimeDiscoveries = 0;
        int worldsVisited = 0;
        int noiseRescues = 0;
        int samplesDigested = 0;
        int radiationExposures = 0;
        int breedingOperations = 0;
        int steeringInterventions = 0;
        int anomaliesFound = 0;
        int relicsFound = 0;
        int savedRuns = 0;
        int peakCombo = 0;
    };

    struct ResearchState
    {
        int level = 1;
        int insight = 0;
        int storyChapter = 0;
        std::array<bool, (std::size_t) Feature::count> features {};
        std::array<bool, (std::size_t) Character::count> characters {};
        std::array<bool, (std::size_t) Directive::count> directives {};
    };

    struct Challenge
    {
        std::string title;
        std::string objective;
        std::string reward;
    };

    inline int researchInsight (const LifetimeStats& s)
    {
        return (int) std::min<std::int64_t> (1000000,
            s.lifetimeScore / 5000
            + (std::int64_t) s.lifetimeDiscoveries * 6
            + (std::int64_t) s.generationsObserved / 5
            + (std::int64_t) s.worldsVisited * 5
            + (std::int64_t) s.noiseRescues * 8
            + (std::int64_t) s.samplesDigested * 7
            + (std::int64_t) s.radiationExposures * 3
            + (std::int64_t) s.breedingOperations * 7
            + (std::int64_t) s.steeringInterventions / 2
            + (std::int64_t) s.anomaliesFound * 10
            + (std::int64_t) s.relicsFound * 12
            + (std::int64_t) s.savedRuns * 4);
    }

    inline int levelForInsight (int insight)
    {
        constexpr std::array<int, maxLevel> thresholds { 0, 18, 42, 75, 120, 180, 260, 360 };
        int level = 1;
        for (std::size_t i = 1; i < thresholds.size(); ++i)
            if (insight >= thresholds[i])
                level = (int) i + 1;
        return level;
    }

    inline int storyChapterFor (const LifetimeStats& s, int level)
    {
        int chapter = 0;
        if (s.lifetimeDiscoveries >= 1) chapter = 1;
        if (s.generationsObserved >= 15 && s.lifetimeDiscoveries >= 2) chapter = 2;
        if (s.steeringInterventions >= 8 && s.generationsObserved >= 25) chapter = 3;
        if (s.samplesDigested >= 1 && level >= 4) chapter = 4;
        if (s.anomaliesFound >= 3 && s.steeringInterventions >= 18) chapter = 5;
        if (s.anomaliesFound >= 6 && s.lifetimeDiscoveries >= 12) chapter = 6;
        if (level >= maxLevel && s.worldsVisited >= 10 && s.breedingOperations >= 8) chapter = 7;
        return chapter;
    }

    inline ResearchState evaluate (const LifetimeStats& s)
    {
        ResearchState out;
        out.insight = researchInsight (s);
        out.level = levelForInsight (out.insight);
        out.storyChapter = storyChapterFor (s, out.level);

        auto& f = out.features;
        f[(std::size_t) Feature::fieldJournal] = true;
        f[(std::size_t) Feature::researcherCharacters] = out.level >= 2;
        f[(std::size_t) Feature::storyChapters] = out.storyChapter >= 1;
        f[(std::size_t) Feature::challengeDeck] = out.level >= 3;
        f[(std::size_t) Feature::anomalyCatalogue] = out.level >= 3 || s.anomaliesFound > 0;
        f[(std::size_t) Feature::lineageCodex] = out.level >= 4 || s.breedingOperations >= 2;
        f[(std::size_t) Feature::researchProtocols] = out.level >= 2;
        f[(std::size_t) Feature::relicCabinet] = out.level >= 5 || s.relicsFound > 0;
        f[(std::size_t) Feature::worldAtlas] = out.level >= 5 || s.worldsVisited >= 6;
        f[(std::size_t) Feature::storyBranches] = out.storyChapter >= finalStoryChapter;

        auto& c = out.characters;
        c[(std::size_t) Character::maraVoss] = s.lifetimeDiscoveries >= 1;
        c[(std::size_t) Character::ivoChen] = s.generationsObserved >= 20;
        c[(std::size_t) Character::nadiOkafor] = s.samplesDigested >= 1;
        c[(std::size_t) Character::moth] = s.steeringInterventions >= 10;
        c[(std::size_t) Character::junVale] = s.worldsVisited >= 4 && s.breedingOperations >= 2;
        c[(std::size_t) Character::visitor] =
            s.anomaliesFound >= 6 && s.lifetimeDiscoveries >= 12 && out.level >= 7;

        const bool endingsVisible = out.storyChapter >= finalStoryChapter;
        for (auto& d : out.directives) d = endingsVisible;
        return out;
    }

    inline Directive recommendedDirective (const LifetimeStats& s)
    {
        const int preserve = s.noiseRescues * 5
                           + s.breedingOperations * 2
                           + s.generationsObserved / 40
                           + s.savedRuns;
        const int accelerate = s.radiationExposures * 3
                             + s.steeringInterventions / 8
                             + s.peakCombo / 4;
        const int release = s.worldsVisited * 2
                          + s.anomaliesFound * 3
                          + s.lifetimeDiscoveries / 3;

        if (accelerate > preserve && accelerate >= release)
            return Directive::accelerate;
        if (release > preserve && release > accelerate)
            return Directive::release;
        return Directive::preserve;
    }

    inline std::vector<Challenge> challengeDeck (const LifetimeStats& s)
    {
        const auto state = evaluate (s);
        std::vector<Challenge> out;
        out.reserve (3);

        if (s.noiseRescues < 3)
            out.push_back ({ "Rescue Operation", "Recover a colony from a sustained noise lock.", "+1 rescue, relic progress" });
        else
            out.push_back ({ "Controlled Collapse", "Push toward noise, then recover without starting a new world.", "advanced ecology record" });

        if (s.worldsVisited < 8)
            out.push_back ({ "Field Survey", "Visit three different worlds and preserve one useful organism.", "atlas progress" });
        else
            out.push_back ({ "Distant Ecology", "Find an anomaly in a world you have not used recently.", "anomaly + atlas progress" });

        if (state.level < 6)
            out.push_back ({ "Long Lineage", "Carry a colony past generation 40 and breed from it.", "lineage progress" });
        else
            out.push_back ({ "Soft Hand", "Use low-strength steering while keeping variety high.", "MOTH briefing + protocol progress" });

        return out;
    }

    inline const char* featureName (Feature f)
    {
        constexpr const char* names[] = {
            "Field Journal", "Researchers", "Story Chapters", "Three Directives",
            "Research Protocols", "Challenge Deck", "Anomaly Catalogue",
            "Relic Cabinet", "Lineage Codex", "World Atlas"
        };
        return names[(std::size_t) f];
    }

    inline const char* characterName (Character c)
    {
        constexpr const char* names[] = {
            "Dr. Mara Voss", "Ivo Chen", "Nadi Okafor", "MOTH", "Jun Vale", "The Visitor"
        };
        return names[(std::size_t) c];
    }

    inline const char* directiveName (Directive d)
    {
        constexpr const char* names[] = { "PRESERVE", "ACCELERATE", "RELEASE" };
        return names[(std::size_t) d];
    }

    inline const char* storyTitle (int chapter)
    {
        constexpr const char* titles[] = {
            "Petri Dish / First Discrepancy", "Variation / The Extra Specimen",
            "Selection Leaves a Trace", "The Colony Remembers Us",
            "Signal From Outside", "MOTH's Hypothesis", "The Visitor Was Early",
            "Three Directives / No Exit"
        };
        return titles[(std::size_t) std::clamp (chapter, 0, finalStoryChapter)];
    }

    inline const char* characterRole (Character c)
    {
        constexpr const char* roles[] = {
            "Ecologist", "Signal Engineer", "Field Recordist",
            "Lab Automation", "Archivist", "Unknown Observer"
        };
        return roles[(std::size_t) c];
    }

    inline const char* characterBriefing (Character c)
    {
        constexpr const char* text[] = {
            "Diversity is not a defect to average away. Protect the strange branches.",
            "If a sound cannot be described twice, the measurement is not finished.",
            "Every recording brought into the dish is environmental DNA.",
            "Your interventions are becoming statistically predictable. I am watching the pattern.",
            "A lineage is a decision that survived long enough to become history.",
            "YOU KEEP CALLING THEM WORLDS. THEY MAY BE CALLING YOU THE ENVIRONMENT."
        };
        return text[(std::size_t) c];
    }

    inline int protocolUnlockLevel (int index)
    {
        constexpr int levels[10] = { 2, 2, 3, 3, 4, 4, 5, 6, 7, 8 };
        return levels[std::clamp (index, 0, 9)];
    }

    inline const char* protocolName (int index)
    {
        constexpr const char* names[10] = {
            "Observation Log", "Directed Selection", "Counter-Evolution",
            "Source DNA", "Lineage Breeding", "Genome Surgery",
            "MAP Archive", "Anomaly Scan", "World Memory", "Directive Analysis"
        };
        return names[std::clamp (index, 0, 9)];
    }

    inline const char* protocolDescription (int index)
    {
        constexpr const char* text[10] = {
            "Turns run telemetry into a persistent field journal.",
            "Uses selection targets deliberately instead of waiting for drift.",
            "Pushes away from the sound the colony is currently producing.",
            "Treats imported audio and microphone capture as environmental material.",
            "Carries selected traits through explicit parent and offspring lineages.",
            "Uses the inspector, locks and targeted mutation as deliberate genome editing.",
            "Reads coverage and novelty as a map of explored behavior space.",
            "Recognizes rare descriptor combinations and records them permanently.",
            "Compares worlds as persistent regions rather than disposable random seeds.",
            "Interprets long-term play as Preserve, Accelerate or Release without locking content."
        };
        return text[std::clamp (index, 0, 9)];
    }

    inline const char* storyText (int chapter)
    {
        constexpr const char* text[] = {
            "LOG 00. The colony begins under observation. MOTH counts three observers in the room. The lab inventory lists one operator.",
            "The same run produces incompatible histories. A specimen appears in the frame before its first recorded discovery. Mara calls it a rendering fault and then deletes that sentence.",
            "Selection works. More troubling: the colony begins anticipating the directions the operator usually chooses. MOTH labels this USER PREDICTION, then changes the label to WEATHER.",
            "Lineages preserve decisions that were never saved. Jun finds a child organism whose parent checksum belongs to tomorrow's archive.",
            "Outside recordings alter the colony like environmental DNA. Nadi hears a room tone inside one capture that does not match the room where it was made.",
            "MOTH reports that steering habits predict future colony behavior. In the next line MOTH denies writing the report. Both lines have the same timestamp.",
            "Anomalies recur in unrelated worlds. The Visitor leaves notes naming worlds before the operator visits them. One note contains the operator's next click sequence.",
            "The lab stops asking what the colony should become. PRESERVE, ACCELERATE and RELEASE remain. A fourth directive flashes for one frame: OBSERVE THE OPERATOR."
        };
        return text[(std::size_t) std::clamp (chapter, 0, finalStoryChapter)];
    }
}
