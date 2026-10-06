#include "../Source/Engine/Progression.h"
#include "../Source/Engine/HauntedEvents.h"
#include "../Source/Engine/HiddenDiscoveries.h"
#include "../Source/Engine/LabGames.h"
#include <cassert>
#include <iostream>
#include <set>

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

    assert (! mutagen::haunted::micRollWins (3));
    assert (mutagen::haunted::micRollWins (4));
    assert (! mutagen::haunted::micRollWins (5));
    assert (crossedThreshold (100384, 100385, mutagen::haunted::micReverseThreshold));
    assert (! crossedThreshold (100385, 100386, mutagen::haunted::micReverseThreshold));

    mutagen::haunted::ClickSequence seq;
    // Five exploratory zones in time should discover a stable creature.
    seq.push (0, 0.00);
    seq.push (5, 0.40);
    seq.push (9, 0.90);
    seq.push (2, 1.20);
    const auto discovery = seq.push (7, 1.70);
    assert (discovery.creatureId >= 0 && discovery.creatureId < 100);
    assert (! mutagen::haunted::creatureName (discovery.creatureId).empty());

    // The same sequence is deterministic.
    mutagen::haunted::ClickSequence seq2;
    seq2.push (0, 0.00); seq2.push (5, 0.40); seq2.push (9, 0.90); seq2.push (2, 1.20);
    assert (seq2.push (7, 1.70).creatureId == discovery.creatureId);

    // Easy discovery still rejects five clicks on one control.
    mutagen::haunted::ClickSequence dull;
    dull.push (1, 0.0); dull.push (1, 0.2); dull.push (1, 0.4); dull.push (1, 0.6);
    assert (dull.push (1, 0.8).creatureId == -1);

    const auto m1 = mutagen::haunted::milestoneForScore (999999, 1000000);
    assert (m1.has_value());
    assert (m1->index == 1);
    assert (! m1->title.empty());
    assert (! m1->skillName.empty());
    assert (! mutagen::haunted::milestoneForScore (1000000, 1999999).has_value());

    const auto m2 = mutagen::haunted::milestoneForScore (1999999, 2000000);
    assert (m2.has_value());
    assert (m2->index == 2);
    assert (m2->title != m1->title);
    assert (m2->skillName != m1->skillName);
    assert (m2->animationRecipe != m1->animationRecipe);
    assert (m2->soundRecipe != m1->soundRecipe);

    std::set<std::string> creatureNames;
    for (int id = 0; id < 100; ++id)
    {
        creatureNames.insert (mutagen::haunted::creatureName (id));
        assert (! mutagen::haunted::creatureLore (id).empty());
    }
    assert (creatureNames.size() == 100);

    mutagen::haunted::ClickSequence mirror;
    mirror.push (2, 0.0); mirror.push (7, 0.2); mirror.push (2, 0.4);
    mirror.push (7, 0.6); mirror.push (2, 0.8);
    assert (mirror.push (7, 1.0).mirrorEvent);

    mutagen::haunted::SessionRng sessionRng;
    assert (sessionRng.seed() != 0);
    for (int i = 0; i < 200; ++i)
    {
        const int roll = sessionRng.rollInclusive (1, 20);
        assert (roll >= 1 && roll <= 20);
    }

    using namespace mutagen::labgames;

    assert (shouldOfferRoulette (5, 1));
    assert (! shouldOfferRoulette (5, 2));
    assert (shouldOfferMonte (10, 1));
    assert (! shouldOfferMonte (10, 2));
    assert (shouldOfferSlots (3, 1));
    assert (! shouldOfferSlots (3, 2));

    const auto safeRoulette = rouletteOutcome (127, 6);
    assert (safeRoulette.kind != RoulettePrize::minus1000);
    assert (safeRoulette.kind != RoulettePrize::loseSkill);

    bool sawPenalty = false;
    for (int roll = 1; roll <= 12; ++roll)
    {
        const auto o = rouletteOutcome (126, roll);
        if (o.kind == RoulettePrize::minus1000 || o.kind == RoulettePrize::loseSkill)
            sawPenalty = true;
    }
    assert (sawPenalty);

    const auto monteWin = monteOutcome (true, 1);
    assert (monteWin.points >= 0);
    assert (monteWin.rainbowOrbs == 100 || monteWin.points == 10000);
    const auto monteLoss = monteOutcome (false, 1);
    assert (monteLoss.points == -20000);

    const auto jackpot = slotOutcome (777);
    assert (jackpot.jackpot);
    assert (jackpot.glowingRainbowOrbs == 1000);
    assert (jackpot.rainbowMultiplier == 2);

    std::set<int> soundRecipes;
    for (int game = 0; game < 3; ++game)
        for (int result = 0; result < 8; ++result)
            soundRecipes.insert (soundRecipe ((Game) game, (SoundMoment) result, 11));
    assert (soundRecipes.size() >= 20);

    std::cout << "Progression rules: PASS\n";
    return 0;
}
