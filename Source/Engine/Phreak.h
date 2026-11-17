#pragma once

// The "phreak" cheat code: a tiny key-sequence matcher (docs/specs/phreak-spec.md).
// Pure standard C++ (no JUCE). Letters are case-insensitive; a gap longer than
// `gapSeconds` between keys starts the word again.

#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

namespace mutagen::phreak
{
    class SequenceMatcher
    {
    public:
        explicit SequenceMatcher (std::string word = "phreak", double gapSeconds = 2.5)
            : target (std::move (word)), gap (gapSeconds) {}

        /** Feed one typed character. Returns true on the key that completes the word. */
        bool push (char c, double nowSeconds)
        {
            if (target.empty()) return false;
            if (matched > 0 && nowSeconds - lastAt > gap) matched = 0;
            lastAt = nowSeconds;
            const char lower = (char) std::tolower ((unsigned char) c);
            if (lower == target[matched]) { ++matched; }
            else matched = lower == target[0] ? 1 : 0;
            if (matched == target.size()) { matched = 0; return true; }
            return false;
        }

        void reset() { matched = 0; }

    private:
        std::string target;
        double gap, lastAt = 0.0;
        std::size_t matched = 0;
    };
}
