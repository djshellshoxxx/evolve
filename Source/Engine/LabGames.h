#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace mutagen::labgames
{
    enum class Game { roulette = 0, monte = 1, slots = 2, dice = 3 };

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
