#include "../Source/Engine/Progression.h"
#include "../Source/Engine/HauntedEvents.h"
#include "../Source/Engine/HiddenDiscoveries.h"
#include "../Source/Engine/LabGames.h"
#include "../Source/Engine/Storyline.h"
#include "../Source/Engine/Secrets.h"
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

    using namespace mutagen::labgames;

    CHECK (shouldOfferRoulette (5, 1));
    CHECK (! shouldOfferRoulette (5, 2));
    CHECK (shouldOfferMonte (10, 1));
    CHECK (! shouldOfferMonte (10, 2));
    CHECK (shouldOfferSlots (3, 1));
    CHECK (! shouldOfferSlots (3, 2));
    CHECK (shouldOfferDice (1, 1));
    CHECK (! shouldOfferDice (1, 2));
    CHECK (shouldOfferTwentyOne (4, 1));
    CHECK (! shouldOfferTwentyOne (4, 2));

    const auto diceWin = diceOutcome (6, 2);
    CHECK (diceWin.won);
    CHECK (diceWin.monsterOrbs == 100);
    CHECK (diceWin.miniOrbs == 10);
    CHECK (! diceWin.orbInversionPenalty);

    const auto diceLoss = diceOutcome (2, 6);
    CHECK (! diceLoss.won);
    CHECK (diceLoss.orbInversionPenalty);

    const auto twentyOneWin = twentyOneOutcome (20, 18);
    CHECK (twentyOneWin.result == TwentyOneResult::win);
    CHECK (twentyOneWin.skillLoss == 0);

    const auto twentyOneLoss = twentyOneOutcome (17, 20);
    CHECK (twentyOneLoss.result == TwentyOneResult::loss);
    CHECK (twentyOneLoss.points == -20);
    CHECK (twentyOneLoss.skillLoss == 5);

    const auto natural = twentyOneOutcome (21, 19);
    CHECK (natural.snareSounds == 10);
    CHECK (natural.tomSounds == 0);

    const auto shared21 = twentyOneOutcome (21, 21);
    CHECK (shared21.result == TwentyOneResult::push);
    CHECK (shared21.snareSounds == 0);
    CHECK (shared21.tomSounds == 5);

    CHECK (scratchTicketsCrossed (9999, 10000) == 1);
    CHECK (scratchTicketsCrossed (9999, 30001) == 3);
    CHECK (scratchTicketsCrossed (10000, 19999) == 0);

    const std::array<int, 5> threeMatch { 2, 2, 2, 1, 4 };
    const auto scratch3 = scratchOutcome (threeMatch);
    CHECK (scratch3.matchCount == 3);
    CHECK (scratch3.bassDrumSounds == 5);
    CHECK (scratch3.points == 100);

    const std::array<int, 5> fourMatch { 3, 3, 3, 3, 1 };
    const auto scratch4 = scratchOutcome (fourMatch);
    CHECK (scratch4.matchCount == 4);
    CHECK (scratch4.bassDrops == 1);
    CHECK (scratch4.kazooSounds == 1);

    const std::array<int, 5> fiveMatch { 4, 4, 4, 4, 4 };
    const auto scratch5 = scratchOutcome (fiveMatch);
    CHECK (scratch5.matchCount == 5);
    CHECK (scratch5.stringSounds == 10);
    CHECK (scratch5.tambourineSounds == 2);

    const auto trade = bassDrumTrade (10);
    CHECK (trade.bassDrumsSpent == 5);
    CHECK (trade.pianoKeySounds == 1);

    CHECK (skillRouletteOfferDue (4, 5, 1));
    CHECK (! skillRouletteOfferDue (4, 5, 2));
    CHECK (! skillRouletteOfferDue (5, 9, 1));
    CHECK (skillRouletteOfferDue (9, 10, 1));

    const auto alt0 = skillRouletteOutcome (1);
    CHECK (alt0.pianoKeys == 2);
    const auto alt5 = skillRouletteOutcome (6);
    CHECK (alt5.upSweeps == 3);
    const auto alt6 = skillRouletteOutcome (7);
    CHECK (alt6.points == -1000);
    const auto alt7 = skillRouletteOutcome (8);
    CHECK (alt7.skillDelta == -1);

    CHECK (mutationSlotsOfferDue (19, 20));
    CHECK (! mutationSlotsOfferDue (20, 39));
    CHECK (mutationSlotsOfferDue (39, 40));

    const auto mutationJackpot = mutationSlotOutcome (777);
    CHECK (mutationJackpot.jackpot);
    CHECK (mutationJackpot.randomColourOrbs == 50);
    CHECK (mutationJackpot.giantRainbowOrbs == 10);
    CHECK (mutationJackpot.blackVirusOrbs == 5);
    CHECK (mutationJackpot.giantRainbowBreedRatio == 2);

    CHECK (sideMutationUnlockDue (4, 5, 1));
    CHECK (! sideMutationUnlockDue (4, 5, 2));
    CHECK (! sideMutationUnlockDue (5, 9, 1));
    CHECK (sideMutationUnlockDue (9, 10, 1));

    const auto sideJackpot = sideMutationSlotOutcome (777);
    CHECK (sideJackpot.jackpot);
    CHECK (sideJackpot.randomColourOrbs == 50);
    CHECK (sideJackpot.giantRainbowOrbs == 10);
    CHECK (sideJackpot.blackVirusOrbs == 5);

    CHECK (! wolfermeanDue (2, true));
    CHECK (wolfermeanDue (3, true));
    CHECK (! wolfermeanDue (4, false));

    CHECK (! gatorUnlockDue (1, true));
    CHECK (gatorUnlockDue (2, true));
    CHECK (! gatorUnlockDue (2, false));
    CHECK (! gateShapesUnlockDue (false, 1));
    CHECK (gateShapesUnlockDue (true, 1));

    CHECK (! tripDelayUnlockDue (2, true));
    CHECK (tripDelayUnlockDue (3, true));
    CHECK (! tripDelayUnlockDue (3, false));

    CHECK (! reverseSkillUnlockDue (1, true));
    CHECK (reverseSkillUnlockDue (2, true));
    CHECK (! reverseSkillUnlockDue (2, false));

    CHECK (! labExchangeUnlocked (9));
    CHECK (labExchangeUnlocked (10));
    CHECK (labExchangeUnlocked (25));

    CHECK (miniGameParticipationPoints() == 3);
    CHECK (orbGenerationPoints (0) == 0);
    CHECK (orbGenerationPoints (37) == 37);
    CHECK (skillDiscoveryPoints (1) == 4);
    CHECK (skillDiscoveryPoints (5) == 20);
    CHECK (skillUsePoints() == -1);
    CHECK (skillUseBonusForScore (1099) == SkillUseBonus::horn);
    CHECK (skillUseBonusForScore (129) == SkillUseBonus::risingSweep);
    CHECK (skillUseBonusForScore (123) == SkillUseBonus::fallingSweep);
    CHECK (skillUseBonusForScore (121) == SkillUseBonus::burp);
    CHECK (skillUseBonusForScore (128) == SkillUseBonus::none);
    CHECK (skillUseBonusForScore (1010) == SkillUseBonus::timeStretchCelebration);
    CHECK (skillUseBonusForScore (1201) == SkillUseBonus::timeStretchCelebration);

    const auto greenGate = greenCollisionGateSpec();
    CHECK (greenGate.bpm == 150);
    CHECK (greenGate.durationSeconds == 120);
    CHECK (greenGate.fadeSeconds > 0);

    const auto yellowGreenGate = yellowGreenCollisionGateSpec();
    CHECK (yellowGreenGate.bpm == 110);
    CHECK (yellowGreenGate.durationSeconds == 120);
    CHECK (yellowGreenGate.fadeSeconds == greenGate.fadeSeconds);

    const auto blackWhiteGate = blackWhiteCollisionGateSpec();
    CHECK (blackWhiteGate.bpm == 90);
    CHECK (blackWhiteGate.durationSeconds == 120);
    CHECK (blackWhiteGate.fadeSeconds == greenGate.fadeSeconds);

    const auto pinkCornerGate = pinkCornerGateSpec();
    CHECK (pinkCornerGate.bpm == 140);
    CHECK (pinkCornerGate.durationSeconds == 120);
    CHECK (pinkCornerGate.fadeSeconds == greenGate.fadeSeconds);

    const auto thirdGameGate = thirdMiniGameGateSpec (3, true, 1000007);
    CHECK (thirdGameGate.has_value());
    CHECK (thirdGameGate->bpm == 180);
    CHECK (thirdGameGate->durationSeconds == 300);
    CHECK (! thirdMiniGameGateSpec (3, false, 1000007).has_value());
    CHECK (! thirdMiniGameGateSpec (4, true, 1000007).has_value());
    CHECK (! thirdMiniGameGateSpec (3, true, 1000008).has_value());

    CHECK (fourthMiniGameCelebrationDue (4, true, 10001));
    CHECK (! fourthMiniGameCelebrationDue (4, false, 10001));
    CHECK (! fourthMiniGameCelebrationDue (3, true, 10001));
    CHECK (! fourthMiniGameCelebrationDue (4, true, 10000));

    const auto fxBuy = exchangePurchase (ExchangeItem::tripEcho, 5000);
    CHECK (fxBuy.allowed);
    CHECK (fxBuy.cost == 2500);
    const auto stretchBuy = exchangePurchase (ExchangeItem::timeStretch, 1500);
    CHECK (! stretchBuy.allowed);
    const auto orbBuy = exchangePurchase (ExchangeItem::rainbowOrbPack, 5000);
    CHECK (orbBuy.allowed);
    CHECK (orbBuy.rainbowOrbs == 25);

    const auto safeRoulette = rouletteOutcome (127, 6);
    CHECK (safeRoulette.kind != RoulettePrize::minus1000);
    CHECK (safeRoulette.kind != RoulettePrize::loseSkill);

    bool sawPenalty = false;
    for (int roll = 1; roll <= 12; ++roll)
    {
        const auto o = rouletteOutcome (126, roll);
        if (o.kind == RoulettePrize::minus1000 || o.kind == RoulettePrize::loseSkill)
            sawPenalty = true;
    }
    CHECK (sawPenalty);

    const auto monteWin = monteOutcome (true, 1);
    CHECK (monteWin.points >= 0);
    CHECK (monteWin.rainbowOrbs == 100 || monteWin.points == 10000);
    const auto monteLoss = monteOutcome (false, 1);
    CHECK (monteLoss.points == -20000);

    const auto jackpot = slotOutcome (777);
    CHECK (jackpot.jackpot);
    CHECK (jackpot.glowingRainbowOrbs == 1000);
    CHECK (jackpot.rainbowMultiplier == 2);

    std::set<int> soundRecipes;
    for (int game = 0; game < 9; ++game)
        for (int result = 0; result < 8; ++result)
            soundRecipes.insert (soundRecipe ((Game) game, (SoundMoment) result, 11));
    CHECK (soundRecipes.size() >= 20);

    // ---- Resonance Acts -------------------------------------------------
    {
        using namespace mutagen::story;
        Progress p;
        CHECK (actFor (p) == 1);
        CHECK (p.lexiconCount() == 0);
        for (int i = 0; i < lexiconSize; ++i) p.markHeard (i);
        CHECK (p.lexiconCount() == lexiconSize);
        p.collected = 100; p.quizzesCorrect = 100;
        CHECK (actFor (p) == actCount);

        // Act thresholds must be reachable and monotonic.
        for (int i = 1; i < actCount; ++i)
        {
            CHECK (acts()[(std::size_t) i].lexiconNeeded >= acts()[(std::size_t) i - 1].lexiconNeeded);
            CHECK (acts()[(std::size_t) i].lexiconNeeded <= lexiconSize);
        }

        // Every act has at least three events of its own, and every quiz has a valid answer.
        for (int a = 1; a <= actCount; ++a)
        {
            int n = 0;
            for (const auto& e : events()) n += e.minAct <= a ? 1 : 0;
            CHECK (n >= 3);
        }
        for (const auto& q : quizzes()) CHECK (q.correct >= 0 && q.correct < 3 && q.minAct <= actCount);
        for (const auto& e : events()) for (auto n : e.notes) CHECK (n == kNo || (n >= -36 && n <= 36));

        // No two runs alike: different seeds give different keys/twists/orders.
        std::set<std::string> fingerprints;
        for (std::uint64_t seed = 1; seed <= 40; ++seed)
        {
            RunStory r (seed * 7919);
            CHECK (r.root >= 48 && r.root <= 59);
            Progress fresh;
            std::string fp = r.keyName() + "/" + std::to_string (r.twist);
            for (int k = 0; k < 4; ++k) fp += "/" + std::to_string (r.drawEvent (1, fresh));
            fingerprints.insert (fp);
            CHECK (r.scaleNotes (-1).size() >= 6);
        }
        CHECK (fingerprints.size() >= 38);

        // Unheard events are preferred, and a run never runs dry.
        RunStory r (99);
        Progress fresh;
        std::set<int> seen;
        for (int k = 0; k < 200; ++k)
        {
            const int id = r.drawEvent (actCount, fresh);
            CHECK (id >= 0 && id < lexiconSize);
            if (seen.size() < (std::size_t) lexiconSize) CHECK (! fresh.heard (id));
            fresh.markHeard (id);
            seen.insert (id);
        }
        CHECK ((int) seen.size() == lexiconSize);
        CHECK (r.drawQuiz (1) >= 0);
    }

    // ---- secrets, visitors, polyglots, name fates ------------------------
    {
        using namespace mutagen::secrets;
        CHECK (secretCount == 39 && (int) all().size() == secretCount);
        int tiers[4] {};
        for (const auto& s : all()) ++tiers[(int) s.tier];
        CHECK (tiers[0] >= 6 && tiers[1] >= 6 && tiers[2] >= 6 && tiers[3] >= 6);

        int mirrored = 0, upside = 0;
        for (const auto& v : visitors()) (v.glyphs == Glyphs::mirrored ? mirrored : upside)++;
        CHECK (mirrored == 3 && upside == 4);
        CHECK (polyglots().size() == 10);
        CHECK (gibberish (42, 4).size() > 6 && gibberish (42, 4) != gibberish (43, 4));

        // Easy ones really are easy: the dead centre and the top of a minute.
        Tracker t;
        Context c;
        c.now = 1.0; c.nx = 0.5; c.ny = 0.5; c.second = 30;
        CHECK (t.click (c, 0) == 1);
        c.now = 2.0; c.nx = 0.3; c.second = 0;
        CHECK (t.click (c, 0) == 0);
        c.second = 30;
        CHECK (t.click (c, 1) == -1 || t.click (c, 1) != 1);   // found secrets never re-fire

        // Perfect cadence: column 7 then column 0 within two seconds.
        Tracker cad;
        Context k; k.second = 30; k.ny = 0.3;
        k.now = 10.0; k.nx = 7.5 / 12.0; cad.click (k, 0);
        k.now = 10.8; k.nx = 0.5 / 12.0;
        CHECK (cad.click (k, 0) == 13);

        // Calendar secrets.
        Tracker cal;
        Context d; d.weekday = 5; d.day = 13; d.nx = 0.3; d.ny = 0.3; d.second = 30;
        CHECK (cal.click (d, 0) == 30);

        // Name fates.
        CHECK (fateForName ("Jo", 0) == Fate::wallRain);        // 1/2: roll 0 hits
        CHECK (fateForName ("jo", 1) == Fate::none);            // 1/2: roll 1 misses
        CHECK (fateForName ("Quinn", 12345) == Fate::flowers);  // always
        CHECK (fateForName ("7even", 99) == Fate::numberCurse); // digits always
        CHECK (fateForName ("Dee", 5) == Fate::hugeMites);      // always
        CHECK (fateForName ("  Kai", 3) == Fate::moonCrash);    // 4/5, leading spaces ignored
        CHECK (fateForName ("Kai", 4) == Fate::none);
        CHECK (fateForName ("Zed", 0) == Fate::none);
        CHECK (fateForName ("", 0) == Fate::none);
        CHECK (ruleForName ("Tess") != nullptr && ruleForName ("Tess")->fate == Fate::blackSwarm);
    }

    std::cout << "Progression rules: PASS\n";
    return 0;
}
