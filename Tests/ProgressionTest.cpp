#include "../Source/Engine/Progression.h"
#include "../Source/Engine/HauntedEvents.h"
#include "../Source/Engine/HiddenDiscoveries.h"
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace mutagen::progression;

#define CHECK(condition) do { if (! (condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " << #condition << '\n'; \
    return 1; \
} } while (false)

int main()
{
    LifetimeStats empty;
    const auto start = evaluate (empty);
    CHECK (start.level == 1);
    CHECK (start.features[(size_t) Feature::fieldJournal]);
    CHECK (! start.features[(size_t) Feature::storyBranches]);
    CHECK (start.storyChapter == 0);

    LifetimeStats field;
    field.lifetimeDiscoveries = 3;
    field.generationsObserved = 30;
    field.samplesDigested = 1;
    field.steeringInterventions = 12;
    field.worldsVisited = 4;
    field.breedingOperations = 3;
    field.noiseRescues = 2;
    field.radiationExposures = 2;
    field.anomaliesFound = 2;
    field.lifetimeScore = 75000;

    const auto mid = evaluate (field);
    CHECK (mid.level >= 4);
    CHECK (mid.characters[(size_t) Character::maraVoss]);
    CHECK (mid.characters[(size_t) Character::ivoChen]);
    CHECK (mid.characters[(size_t) Character::nadiOkafor]);
    CHECK (mid.characters[(size_t) Character::moth]);
    CHECK (mid.features[(size_t) Feature::challengeDeck]);
    CHECK (mid.features[(size_t) Feature::lineageCodex]);
    CHECK (mid.storyChapter >= 4);

    LifetimeStats late = field;
    late.lifetimeDiscoveries = 18;
    late.generationsObserved = 160;
    late.samplesDigested = 8;
    late.steeringInterventions = 45;
    late.worldsVisited = 14;
    late.breedingOperations = 12;
    late.noiseRescues = 8;
    late.radiationExposures = 9;
    late.anomaliesFound = 8;
    late.relicsFound = 7;
    late.lifetimeScore = 420000;

    const auto end = evaluate (late);
    CHECK (end.level == maxLevel);
    CHECK (end.storyChapter == finalStoryChapter);
    for (bool unlocked : end.features) CHECK (unlocked);
    for (bool unlocked : end.characters) CHECK (unlocked);
    CHECK (end.directives[(size_t) Directive::preserve]);
    CHECK (end.directives[(size_t) Directive::accelerate]);
    CHECK (end.directives[(size_t) Directive::release]);

    const auto preserve = recommendedDirective (late);
    CHECK (preserve == Directive::preserve);

    late.radiationExposures = 40;
    late.steeringInterventions = 120;
    late.noiseRescues = 2;
    CHECK (recommendedDirective (late) == Directive::accelerate);

    late.worldsVisited = 40;
    late.anomaliesFound = 20;
    late.breedingOperations = 2;
    CHECK (recommendedDirective (late) == Directive::release);

    const auto challenges = challengeDeck (late);
    CHECK (challenges.size() == 3);
    CHECK (! challenges[0].title.empty());
    CHECK (! challenges[1].objective.empty());

    using mutagen::haunted::crossedThreshold;
    using mutagen::haunted::cdTrayRollWins;

    CHECK (! crossedThreshold (1000034, 1000034, 1000035));
    CHECK (crossedThreshold (1000034, 1000035, 1000035));
    CHECK (crossedThreshold (999999, 1000040, 1000035));
    CHECK (! crossedThreshold (1000035, 1000040, 1000035));
    CHECK (! cdTrayRollWins (16));
    CHECK (cdTrayRollWins (17));
    CHECK (! cdTrayRollWins (20));

    CHECK (! mutagen::haunted::micRollWins (3));
    CHECK (mutagen::haunted::micRollWins (4));
    CHECK (! mutagen::haunted::micRollWins (5));
    CHECK (crossedThreshold (100384, 100385, mutagen::haunted::micReverseThreshold));
    CHECK (! crossedThreshold (100385, 100386, mutagen::haunted::micReverseThreshold));

    mutagen::haunted::ClickSequence seq;
    // Five exploratory zones in time should discover a stable creature.
    seq.push (0, 0.00);
    seq.push (5, 0.40);
    seq.push (9, 0.90);
    seq.push (2, 1.20);
    const auto discovery = seq.push (7, 1.70);
    CHECK (discovery.creatureId >= 0 && discovery.creatureId < 100);
    CHECK (! mutagen::haunted::creatureName (discovery.creatureId).empty());

    // The same sequence is deterministic.
    mutagen::haunted::ClickSequence seq2;
    seq2.push (0, 0.00); seq2.push (5, 0.40); seq2.push (9, 0.90); seq2.push (2, 1.20);
    CHECK (seq2.push (7, 1.70).creatureId == discovery.creatureId);

    // Easy discovery still rejects five clicks on one control.
    mutagen::haunted::ClickSequence dull;
    dull.push (1, 0.0); dull.push (1, 0.2); dull.push (1, 0.4); dull.push (1, 0.6);
    CHECK (dull.push (1, 0.8).creatureId == -1);

    const auto m1 = mutagen::haunted::milestoneForScore (999999, 1000000);
    CHECK (m1.has_value());
    CHECK (m1->index == 1);
    CHECK (! m1->title.empty());
    CHECK (! m1->skillName.empty());
    CHECK (! mutagen::haunted::milestoneForScore (1000000, 1999999).has_value());

    const auto m2 = mutagen::haunted::milestoneForScore (1999999, 2000000);
    CHECK (m2.has_value());
    CHECK (m2->index == 2);
    CHECK (m2->title != m1->title);
    CHECK (m2->skillName != m1->skillName);
    CHECK (m2->animationRecipe != m1->animationRecipe);
    CHECK (m2->soundRecipe != m1->soundRecipe);

    std::set<std::string> creatureNames;
    for (int id = 0; id < 100; ++id)
    {
        creatureNames.insert (mutagen::haunted::creatureName (id));
        CHECK (! mutagen::haunted::creatureLore (id).empty());
    }
    CHECK (creatureNames.size() == 100);

    mutagen::haunted::ClickSequence mirror;
    mirror.push (2, 0.0); mirror.push (7, 0.2); mirror.push (2, 0.4);
    mirror.push (7, 0.6); mirror.push (2, 0.8);
    CHECK (mirror.push (7, 1.0).mirrorEvent);

    mutagen::haunted::SessionRng sessionRng;
    CHECK (sessionRng.seed() != 0);
    for (int i = 0; i < 200; ++i)
    {
        const int roll = sessionRng.rollInclusive (1, 20);
        CHECK (roll >= 1 && roll <= 20);
    }

    std::cout << "Progression rules: PASS\n";
    return 0;
}
