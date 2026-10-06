#include "../Source/Engine/Progression.h"
#include "../Source/Engine/HauntedEvents.h"
#include <cassert>
#include <iostream>

using namespace mutagen::progression;

int main()
{
    LifetimeStats empty;
    const auto start = evaluate (empty);
    assert (start.level == 1);
    assert (start.features[(size_t) Feature::fieldJournal]);
    assert (! start.features[(size_t) Feature::storyBranches]);
    assert (start.storyChapter == 0);

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
    assert (mid.level >= 4);
    assert (mid.characters[(size_t) Character::maraVoss]);
    assert (mid.characters[(size_t) Character::ivoChen]);
    assert (mid.characters[(size_t) Character::nadiOkafor]);
    assert (mid.characters[(size_t) Character::moth]);
    assert (mid.features[(size_t) Feature::challengeDeck]);
    assert (mid.features[(size_t) Feature::lineageCodex]);
    assert (mid.storyChapter >= 4);

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
    assert (end.level == maxLevel);
    assert (end.storyChapter == finalStoryChapter);
    for (bool unlocked : end.features) assert (unlocked);
    for (bool unlocked : end.characters) assert (unlocked);
    assert (end.directives[(size_t) Directive::preserve]);
    assert (end.directives[(size_t) Directive::accelerate]);
    assert (end.directives[(size_t) Directive::release]);

    const auto preserve = recommendedDirective (late);
    assert (preserve == Directive::preserve);

    late.radiationExposures = 40;
    late.steeringInterventions = 120;
    late.noiseRescues = 2;
    assert (recommendedDirective (late) == Directive::accelerate);

    late.worldsVisited = 40;
    late.anomaliesFound = 20;
    late.breedingOperations = 2;
    assert (recommendedDirective (late) == Directive::release);

    const auto challenges = challengeDeck (late);
    assert (challenges.size() == 3);
    assert (! challenges[0].title.empty());
    assert (! challenges[1].objective.empty());

    using mutagen::haunted::crossedThreshold;
    using mutagen::haunted::cdTrayRollWins;

    assert (! crossedThreshold (1000034, 1000034, 1000035));
    assert (crossedThreshold (1000034, 1000035, 1000035));
    assert (crossedThreshold (999999, 1000040, 1000035));
    assert (! crossedThreshold (1000035, 1000040, 1000035));
    assert (! cdTrayRollWins (16));
    assert (cdTrayRollWins (17));
    assert (! cdTrayRollWins (20));

    std::cout << "Progression rules: PASS\n";
    return 0;
}
