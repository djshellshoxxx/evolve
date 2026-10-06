#pragma once

#include <chrono>
#include <cstdint>
#include <random>

namespace mutagen::haunted
{
    constexpr std::int64_t cdTrayThreshold = 1000035;

    inline bool crossedThreshold (std::int64_t before, std::int64_t after,
                                  std::int64_t threshold)
    {
        return before < threshold && after >= threshold;
    }

    inline bool cdTrayRollWins (int roll)
    {
        return roll == 17;
    }

    inline std::uint64_t makeSessionSeed()
    {
        std::random_device rd;
        const auto now = (std::uint64_t)
            std::chrono::high_resolution_clock::now().time_since_epoch().count();

        std::uint64_t seed = now;
        seed ^= (std::uint64_t) rd() << 32;
        seed ^= (std::uint64_t) rd();
        seed ^= seed >> 29;
        seed *= 0x9E3779B97F4A7C15ULL;
        seed ^= seed >> 31;
        return seed;
    }

    class SessionRng
    {
    public:
        SessionRng() : seedValue (makeSessionSeed()), engine (seedValue) {}

        std::uint64_t seed() const { return seedValue; }

        int rollInclusive (int low, int high)
        {
            std::uniform_int_distribution<int> distribution (low, high);
            return distribution (engine);
        }

    private:
        std::uint64_t seedValue;
        std::mt19937_64 engine;
    };
}
