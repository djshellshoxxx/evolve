#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <set>
#include <string>

namespace mutagen::haunted
{
    struct DiscoveryResult
    {
        int creatureId = -1;
        bool mirrorEvent = false;
        bool cornerChoir = false;
        bool panicBloom = false;
        bool mothLooksBack = false;
        bool visitorFootprint = false;
    };

    class ClickSequence
    {
    public:
        DiscoveryResult push (int zone, double seconds)
        {
            zone = zone < 0 ? 0 : zone > 11 ? 11 : zone;

            if (count > 0 && seconds - times[(std::size_t) (count - 1)] > 4.5)
                clear();

            if (count == capacity)
            {
                for (int i = 1; i < capacity; ++i)
                {
                    zones[(std::size_t) (i - 1)] = zones[(std::size_t) i];
                    times[(std::size_t) (i - 1)] = times[(std::size_t) i];
                }
                --count;
            }

            zones[(std::size_t) count] = zone;
            times[(std::size_t) count] = seconds;
            ++count;

            DiscoveryResult r;
            r.panicBloom = count >= 7 && (seconds - times[(std::size_t) (count - 7)]) <= 1.4;

            if (count >= 6)
            {
                const int b = count - 6;
                r.mirrorEvent =
                    zones[(std::size_t) b] == zones[(std::size_t) (b + 2)]
                    && zones[(std::size_t) b] == zones[(std::size_t) (b + 4)]
                    && zones[(std::size_t) (b + 1)] == zones[(std::size_t) (b + 3)]
                    && zones[(std::size_t) (b + 1)] == zones[(std::size_t) (b + 5)]
                    && zones[(std::size_t) b] != zones[(std::size_t) (b + 1)];
            }

            if (count >= 5)
            {
                const int b = count - 5;
                std::set<int> distinct;
                std::uint64_t hash = 0xcbf29ce484222325ULL;
                for (int i = b; i < count; ++i)
                {
                    distinct.insert (zones[(std::size_t) i]);
                    hash ^= (std::uint64_t) (zones[(std::size_t) i] + 17);
                    hash *= 0x100000001b3ULL;
                }

                const bool inWindow = seconds - times[(std::size_t) b] <= 4.5;
                if (inWindow && distinct.size() >= 3)
                    r.creatureId = (int) (hash % 100ULL);

                r.mothLooksBack =
                    zones[(std::size_t) b] == zones[(std::size_t) (b + 4)]
                    && zones[(std::size_t) (b + 1)] == zones[(std::size_t) (b + 3)]
                    && zones[(std::size_t) b] != zones[(std::size_t) (b + 2)];
            }

            if (count >= 4)
            {
                const int b = count - 4;
                bool c0=false,c1=false,c2=false,c3=false;
                for (int i=b;i<count;++i)
                {
                    const int z=zones[(std::size_t)i];
                    c0 |= z==0; c1 |= z==3; c2 |= z==8; c3 |= z==11;
                }
                r.cornerChoir = c0&&c1&&c2&&c3;
            }

            if (count >= 10)
            {
                std::set<int> recent;
                for (int i=count-10;i<count;++i) recent.insert(zones[(std::size_t)i]);
                r.visitorFootprint = recent.size() >= 7;
            }
            return r;
        }

        void clear() { count = 0; }

    private:
        static constexpr int capacity = 16;
        std::array<int, capacity> zones {};
        std::array<double, capacity> times {};
        int count = 0;
    };

    inline std::string creatureName (int id)
    {
        static constexpr const char* bodies[10] = {
            "Glass Mite", "Wire Hound", "Velvet Larva", "Static Heron", "Clock Wasp",
            "Ash Snail", "Signal Eel", "Mirror Tick", "Hollow Finch", "Tape Spider"
        };
        static constexpr const char* epithets[10] = {
            "of the Fifth Click", "that Hums Back", "under the Cursor", "with No Shadow",
            "from Channel Zero", "behind the Meter", "that Remembers", "in the Dead Air",
            "under Glass", "from the Other Window"
        };
        if (id < 0 || id >= 100) return {};
        return std::string (bodies[id / 10]) + " " + epithets[id % 10];
    }

    inline std::string creatureLore (int id)
    {
        static constexpr const char* verbs[10] = {
            "counts clicks you do not remember making",
            "leans toward silent channels",
            "copies the last motion one frame late",
            "appears in worlds before they are visited",
            "hides in the decay of bright sounds",
            "follows preserved lineages",
            "moves only while the meter is falling",
            "stares at the edge of the plugin",
            "feeds on repeated gestures",
            "leaves when you try to inspect it"
        };
        if (id < 0 || id >= 100) return {};
        return creatureName(id) + " " + verbs[id % 10] + ".";
    }

    struct MilestoneArtifact
    {
        int index = 0;
        std::string title;
        std::string skillName;
        int animationRecipe = 0;
        int soundRecipe = 0;
    };

    inline std::optional<MilestoneArtifact> milestoneForScore (std::int64_t before,
                                                               std::int64_t after)
    {
        if (after <= before) return std::nullopt;
        const auto previous = before / 1000000;
        const auto current  = after / 1000000;
        if (current <= previous || current <= 0)
            return std::nullopt;

        const int index = (int) current;
        static constexpr const char* titles[8] = {
            "THE FIRST OPENING", "SECOND MEMORY", "FALSE SKY", "ROOM BELOW ROOM",
            "UNLISTED SPECIMEN", "THE QUIET OPERATOR", "MOTH DREAMS", "VISITOR HANDSHAKE"
        };
        static constexpr const char* skills[8] = {
            "Echo Bloom", "Reverse Selection", "World Fold", "Ghost Germination",
            "Archive Bite", "Quiet Mutation", "MOTH Override", "Visitor Drift"
        };
        MilestoneArtifact out;
        out.index = index;
        out.title = std::string (titles[(index - 1) % 8]) + " #" + std::to_string (index);
        out.skillName = std::string (skills[(index - 1) % 8]) + " " + std::to_string (index);
        out.animationRecipe = index * 37 + (index * index % 31);
        out.soundRecipe = index * 53 + (index * index % 43);
        return out;
    }
}
