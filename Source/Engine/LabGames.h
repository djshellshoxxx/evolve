#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <optional>

namespace mutagen::labgames
{
    enum class Game { roulette = 0, monte = 1, slots = 2, dice = 3, twentyOne = 4, scratch = 5, skillRoulette = 6, mutationSlots = 7, sideMutationSlots = 8 };

    enum class SoundMoment
    {
        appear = 0,
        suspense,
        smallWin,
        bigWin,
        penalty,
        skillGain,
        orbCascade,
        jackpot
    };

    enum class RoulettePrize
    {
        skillA,
        skillB,
        skillC,
        skillD,
        skillE,
        sound,
        specialOrbs,
        minus1000,
        loseSkill
    };

    struct RouletteOutcome
    {
        RoulettePrize kind { RoulettePrize::skillA };
        int points = 0;
        int specialOrbs = 0;
        int skillDelta = 0;
        int soundUnlocks = 0;
    };

    struct MonteOutcome
    {
        int points = 0;
        int rainbowOrbs = 0;
        int skillDelta = 0;
        int soundUnlocks = 0;
    };

    struct SlotOutcome
    {
        int points = 0;
        int specialOrbs = 0;
        int glowingRainbowOrbs = 0;
        int skillDelta = 0;
        int soundUnlocks = 0;
        int rainbowMultiplier = 1;
        bool jackpot = false;
    };

    struct DiceOutcome
    {
        bool won = false;
        int monsterOrbs = 0;
        int miniOrbs = 0;
        bool orbInversionPenalty = false;
    };

    enum class TwentyOneResult { win, loss, push };

    struct TwentyOneOutcome
    {
        TwentyOneResult result { TwentyOneResult::push };
        int points = 0;
        int skillLoss = 0;
        int snareSounds = 0;
        int tomSounds = 0;
    };

    struct ScratchOutcome
    {
        int matchCount = 0;
        int matchedSymbol = -1;
        int points = 0;
        int bassDrumSounds = 0;
        int bassDrops = 0;
        int kazooSounds = 0;
        int stringSounds = 0;
        int tambourineSounds = 0;
    };

    struct BassDrumTrade
    {
        int bassDrumsSpent = 0;
        int pianoKeySounds = 0;
    };

    struct SkillRouletteOutcome
    {
        int pianoKeys = 0;
        int snareSounds = 0;
        int claps = 0;
        int pads = 0;
        int tomSounds = 0;
        int upSweeps = 0;
        int points = 0;
        int skillDelta = 0;
    };

    struct MutationSlotOutcome
    {
        int soundUnlocks = 0;
        int randomColourOrbs = 0;
        int giantRainbowOrbs = 0;
        int blackVirusOrbs = 0;
        int giantRainbowBreedRatio = 1;
        bool jackpot = false;
    };

    enum class ExchangeItem
    {
        tripEcho,
        spectralSmear,
        timeStretch,
        reverseBloom,
        specialOrbPack,
        rainbowOrbPack,
        monsterOrbPack,
        miniOrbPack
    };

    struct ExchangePurchase
    {
        bool allowed = false;
        int cost = 0;
        int specialOrbs = 0;
        int rainbowOrbs = 0;
        int monsterOrbs = 0;
        int miniOrbs = 0;
        bool effectUnlock = false;
        bool timeStretchUnlock = false;
    };

    struct GreenCollisionGateSpec
    {
        int bpm = 150;
        int durationSeconds = 120;
        int fadeSeconds = 18;
    };

    inline bool shouldOfferRoulette (int minuteBucket, int roll1to5)
    {
        return minuteBucket > 0 && minuteBucket % 5 == 0 && roll1to5 == 1;
    }

    inline bool shouldOfferMonte (int minuteBucket, int roll1to3)
    {
        return minuteBucket > 0 && minuteBucket % 10 == 0 && roll1to3 == 1;
    }

    inline bool shouldOfferSlots (int minuteBucket, int roll1to10)
    {
        return minuteBucket > 0 && minuteBucket % 3 == 0 && roll1to10 == 1;
    }

    inline bool shouldOfferDice (int minuteBucket, int roll1to30)
    {
        return minuteBucket > 0 && roll1to30 == 1;
    }

    inline bool shouldOfferTwentyOne (int minuteBucket, int roll1to11)
    {
        return minuteBucket > 0 && minuteBucket % 4 == 0 && roll1to11 == 1;
    }

    inline RouletteOutcome rouletteOutcome (std::int64_t score, int roll1to12)
    {
        RouletteOutcome out;
        roll1to12 = std::clamp (roll1to12, 1, 12);

        // When the live score ends in 7, both negative wedges are removed.
        const bool protectedSeven = score >= 0 && (score % 10) == 7;

        static constexpr RoulettePrize normal[12] = {
            RoulettePrize::skillA, RoulettePrize::skillB, RoulettePrize::skillC,
            RoulettePrize::skillD, RoulettePrize::skillE, RoulettePrize::sound,
            RoulettePrize::specialOrbs, RoulettePrize::skillA, RoulettePrize::sound,
            RoulettePrize::specialOrbs, RoulettePrize::minus1000, RoulettePrize::loseSkill
        };
        static constexpr RoulettePrize protectedTable[12] = {
            RoulettePrize::skillA, RoulettePrize::skillB, RoulettePrize::skillC,
            RoulettePrize::skillD, RoulettePrize::skillE, RoulettePrize::sound,
            RoulettePrize::specialOrbs, RoulettePrize::skillA, RoulettePrize::sound,
            RoulettePrize::specialOrbs, RoulettePrize::skillD, RoulettePrize::sound
        };

        out.kind = protectedSeven ? protectedTable[roll1to12 - 1]
                                  : normal[roll1to12 - 1];

        switch (out.kind)
        {
            case RoulettePrize::skillA:
            case RoulettePrize::skillB:
            case RoulettePrize::skillC:
            case RoulettePrize::skillD:
            case RoulettePrize::skillE:
                out.skillDelta = 1; break;
            case RoulettePrize::sound:
                out.soundUnlocks = 1; break;
            case RoulettePrize::specialOrbs:
                out.specialOrbs = 25 + (roll1to12 * 3); break;
            case RoulettePrize::minus1000:
                out.points = -1000; break;
            case RoulettePrize::loseSkill:
                out.skillDelta = -1; break;
        }
        return out;
    }

    inline MonteOutcome monteOutcome (bool won, int rewardRoll1to2)
    {
        MonteOutcome out;
        if (! won)
        {
            out.points = -20000;
            return out;
        }

        if (rewardRoll1to2 == 1)
        {
            out.rainbowOrbs = 100;
        }
        else
        {
            out.points = 10000;
            out.skillDelta = 3;
            out.soundUnlocks = 3;
        }
        return out;
    }

    inline SlotOutcome slotOutcome (int roll0to999)
    {
        SlotOutcome out;
        roll0to999 = std::clamp (roll0to999, 0, 999);

        // 777 is the single jackpot result.
        if (roll0to999 == 777)
        {
            out.glowingRainbowOrbs = 1000;
            out.rainbowMultiplier = 2;
            out.jackpot = true;
            return out;
        }

        if (roll0to999 % 111 == 0)
        {
            out.specialOrbs = 150;
            out.soundUnlocks = 1;
        }
        else if (roll0to999 % 37 == 0)
        {
            out.points = 7500;
            out.skillDelta = 1;
        }
        else if (roll0to999 % 17 == 0)
        {
            out.specialOrbs = 60;
        }
        else if (roll0to999 % 7 == 0)
        {
            out.points = 2500;
        }
        else if (roll0to999 % 5 == 0)
        {
            out.specialOrbs = 20;
        }
        else
        {
            out.points = 250;
        }
        return out;
    }

    inline DiceOutcome diceOutcome (int playerRoll, int labRoll)
    {
        DiceOutcome out;
        out.won = playerRoll > labRoll;
        if (out.won)
        {
            out.monsterOrbs = 100;
            out.miniOrbs = 10;
        }
        else
        {
            out.orbInversionPenalty = true;
        }
        return out;
    }

    inline TwentyOneOutcome twentyOneOutcome (int player, int house)
    {
        TwentyOneOutcome out;
        const bool playerBust = player > 21;
        const bool houseBust = house > 21;

        if (player == 21 && house == 21)
        {
            out.result = TwentyOneResult::push;
            out.tomSounds = 5;
            return out;
        }

        if (player == 21)
        {
            out.result = TwentyOneResult::win;
            out.snareSounds = 10;
            return out;
        }

        if (playerBust || (! houseBust && house > player))
        {
            out.result = TwentyOneResult::loss;
            out.points = -20;
            out.skillLoss = 5;
            return out;
        }

        if (houseBust || player > house)
        {
            out.result = TwentyOneResult::win;
            return out;
        }

        out.result = TwentyOneResult::push;
        return out;
    }

    inline int scratchTicketsCrossed (std::int64_t before, std::int64_t after)
    {
        if (after <= before || after < 10000) return 0;
        const auto a = before / 10000;
        const auto b = after / 10000;
        return (int) std::max<std::int64_t> (0, b - a);
    }

    inline ScratchOutcome scratchOutcome (const std::array<int, 5>& symbols)
    {
        ScratchOutcome out;
        std::array<int, 5> counts {};
        for (const int symbol : symbols)
            if (symbol >= 0 && symbol < 5)
                ++counts[(size_t) symbol];

        for (int i = 0; i < 5; ++i)
            if (counts[(size_t) i] > out.matchCount)
            {
                out.matchCount = counts[(size_t) i];
                out.matchedSymbol = i;
            }

        if (out.matchCount >= 5)
        {
            out.stringSounds = 10;
            out.tambourineSounds = 2;
        }
        else if (out.matchCount == 4)
        {
            out.bassDrops = 1;
            out.kazooSounds = 1;
        }
        else if (out.matchCount == 3)
        {
            out.bassDrumSounds = 5;
            out.points = 100;
        }
        return out;
    }

    inline BassDrumTrade bassDrumTrade (int ownedBassDrums)
    {
        BassDrumTrade out;
        if (ownedBassDrums >= 5)
        {
            out.bassDrumsSpent = 5;
            out.pianoKeySounds = 1;
        }
        return out;
    }

    inline bool skillRouletteOfferDue (int skillsBefore, int skillsAfter, int roll1to4)
    {
        if (skillsAfter <= skillsBefore || roll1to4 != 1)
            return false;
        return skillsAfter / 5 > skillsBefore / 5;
    }

    inline SkillRouletteOutcome skillRouletteOutcome (int roll1to8)
    {
        SkillRouletteOutcome out;
        switch (std::clamp (roll1to8, 1, 8))
        {
            case 1: out.pianoKeys = 2; break;
            case 2: out.snareSounds = 3; break;
            case 3: out.claps = 4; break;
            case 4: out.pads = 1; break;
            case 5: out.tomSounds = 5; break;
            case 6: out.upSweeps = 3; break;
            case 7: out.points = -1000; break;
            case 8: out.skillDelta = -1; break;
        }
        return out;
    }

    inline bool mutationSlotsOfferDue (int skillsBefore, int skillsAfter)
    {
        if (skillsAfter <= skillsBefore) return false;
        return skillsAfter / 20 > skillsBefore / 20;
    }

    inline MutationSlotOutcome mutationSlotOutcome (int roll0to999)
    {
        MutationSlotOutcome out;
        roll0to999 = std::clamp (roll0to999, 0, 999);

        if (roll0to999 == 777)
        {
            out.jackpot = true;
            out.randomColourOrbs = 50;
            out.giantRainbowOrbs = 10;
            out.blackVirusOrbs = 5;
            out.giantRainbowBreedRatio = 2;
            return out;
        }

        if (roll0to999 % 29 == 0) out.soundUnlocks = 8;
        else if (roll0to999 % 13 == 0) out.soundUnlocks = 5;
        else if (roll0to999 % 7 == 0) out.soundUnlocks = 3;
        else out.soundUnlocks = 1;
        return out;
    }

    inline bool sideMutationUnlockDue (int skillsBefore, int skillsAfter, int roll1to2)
    {
        if (skillsAfter <= skillsBefore || roll1to2 != 1)
            return false;
        return skillsAfter / 5 > skillsBefore / 5;
    }

    inline MutationSlotOutcome sideMutationSlotOutcome (int roll0to999)
    {
        // Same prize grammar as Mutation Slots, independently rolled. The
        // separate game ID guarantees a completely different sound family.
        return mutationSlotOutcome (roll0to999);
    }

    inline bool wolfermeanDue (int consecutiveWins, bool latestWasWin)
    {
        return latestWasWin && consecutiveWins > 0 && consecutiveWins % 3 == 0;
    }

    inline bool gatorUnlockDue (int consecutiveJackpots, bool latestWasJackpot)
    {
        return latestWasJackpot && consecutiveJackpots == 2;
    }

    inline bool gateShapesUnlockDue (bool gatorUnlocked, int laterJackpots)
    {
        return gatorUnlocked && laterJackpots >= 1;
    }

    inline bool tripDelayUnlockDue (int consecutiveMonteWins, bool latestWasWin)
    {
        return latestWasWin && consecutiveMonteWins > 0
            && consecutiveMonteWins % 3 == 0;
    }

    inline bool reverseSkillUnlockDue (int consecutiveMonteLosses, bool latestWasLoss)
    {
        return latestWasLoss && consecutiveMonteLosses > 0
            && consecutiveMonteLosses % 2 == 0;
    }

    inline bool labExchangeUnlocked (int gamesCompleted)
    {
        return gamesCompleted >= 10;
    }

    inline ExchangePurchase exchangePurchase (ExchangeItem item, std::int64_t currentScore)
    {
        ExchangePurchase out;
        switch (item)
        {
            case ExchangeItem::tripEcho:
                out.cost = 2500; out.effectUnlock = true; break;
            case ExchangeItem::spectralSmear:
                out.cost = 3500; out.effectUnlock = true; break;
            case ExchangeItem::timeStretch:
                out.cost = 3000; out.timeStretchUnlock = true; break;
            case ExchangeItem::reverseBloom:
                out.cost = 4200; out.effectUnlock = true; break;
            case ExchangeItem::specialOrbPack:
                out.cost = 1200; out.specialOrbs = 40; break;
            case ExchangeItem::rainbowOrbPack:
                out.cost = 1800; out.rainbowOrbs = 25; break;
            case ExchangeItem::monsterOrbPack:
                out.cost = 2100; out.monsterOrbs = 15; break;
            case ExchangeItem::miniOrbPack:
                out.cost = 900; out.miniOrbs = 12; break;
        }
        out.allowed = currentScore >= out.cost;
        return out;
    }

    inline GreenCollisionGateSpec greenCollisionGateSpec()
    {
        return {};
    }

    inline GreenCollisionGateSpec yellowGreenCollisionGateSpec()
    {
        auto s = GreenCollisionGateSpec{};
        s.bpm = 110;
        return s;
    }

    inline GreenCollisionGateSpec blackWhiteCollisionGateSpec()
    {
        auto s = GreenCollisionGateSpec{};
        s.bpm = 90;
        return s;
    }

    inline GreenCollisionGateSpec pinkCornerGateSpec()
    {
        auto s = GreenCollisionGateSpec{};
        s.bpm = 140;
        return s;
    }

    inline std::optional<GreenCollisionGateSpec> thirdMiniGameGateSpec (
        int gamesCompleted, bool latestWasWin, std::int64_t score)
    {
        if (gamesCompleted != 3 || ! latestWasWin || score >= 1000008)
            return std::nullopt;
        auto s = GreenCollisionGateSpec{};
        s.bpm = 180;
        s.durationSeconds = 300;
        return s;
    }

    inline bool fourthMiniGameCelebrationDue (int gamesCompleted, bool latestWasWin,
                                               std::int64_t score)
    {
        return gamesCompleted == 4 && latestWasWin && score > 10000;
    }

    inline int miniGameParticipationPoints() { return 3; }
    inline int orbGenerationPoints (int orbsGenerated) { return std::max (0, orbsGenerated); }
    inline int skillDiscoveryPoints (int skillsEarned) { return std::max (0, skillsEarned) * 4; }

    inline int soundRecipe (Game game, SoundMoment moment, int variation)
    {
        // Large separated bases plus co-prime offsets prevent result families
        // from collapsing onto the same procedural voice.
        return 20000
             + (int) game * 4000
             + (int) moment * 431
             + (variation * 197)
             + ((int) game + 1) * ((int) moment + 7) * 53;
    }

    inline const char* roulettePrizeName (RoulettePrize p)
    {
        switch (p)
        {
            case RoulettePrize::skillA: return "PHASE GRAFT";
            case RoulettePrize::skillB: return "MIRROR STEP";
            case RoulettePrize::skillC: return "GHOST BREED";
            case RoulettePrize::skillD: return "VOID NUDGE";
            case RoulettePrize::skillE: return "CHROMA BITE";
            case RoulettePrize::sound: return "SURREAL SOUND";
            case RoulettePrize::specialOrbs: return "SPECIAL ORBS";
            case RoulettePrize::minus1000: return "-1000 POINTS";
            case RoulettePrize::loseSkill: return "-1 RANDOM SKILL";
        }
        return "UNKNOWN";
    }
}
