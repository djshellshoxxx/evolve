#pragma once

#include <algorithm>
#include <cstdint>

namespace mutagen::steering
{
    struct Profile
    {
        float brightness  = 0.0f; // -1 dark .. +1 bright
        float density     = 0.0f; // -1 sparse .. +1 dense
        float harmonicity = 0.0f; // -1 noisy .. +1 tonal
        float aggression  = 0.0f; // -1 calm .. +1 fierce
        float divergence  = 0.0f; //  0 familiar .. 1 divergent
    };

    enum class Direction
    {
        dark,
        bright,
        sparse,
        dense,
        noise,
        tone,
        calm,
        fierce
    };

    inline Profile directionProfile (Direction d)
    {
        Profile p;
        switch (d)
        {
            case Direction::dark:   p.brightness  = -0.9f; break;
            case Direction::bright: p.brightness  =  0.9f; break;
            case Direction::sparse: p.density     = -0.9f; break;
            case Direction::dense:  p.density     =  0.9f; break;
            case Direction::noise:  p.harmonicity = -0.9f; break;
            case Direction::tone:   p.harmonicity =  0.9f; break;
            case Direction::calm:   p.aggression  = -0.9f; break;
            case Direction::fierce: p.aggression  =  0.9f; break;
        }
        return p;
    }

    inline float clamp01 (float v) { return std::clamp (v, 0.0f, 1.0f); }

    /** Build a target that deliberately moves away from the measured sound.
        Descriptor inputs are normalized 0..1 values. Density is left neutral
        because the snapshot has no perceptual density descriptor. */
    inline Profile counterProfile (float brightness, float tonalness, float roughness)
    {
        Profile p;
        p.brightness  = -0.85f * (clamp01 (brightness) * 2.0f - 1.0f);
        p.harmonicity = -0.85f * (clamp01 (tonalness)  * 2.0f - 1.0f);
        p.aggression  = -0.85f * (clamp01 (roughness)  * 2.0f - 1.0f);
        p.divergence  = 0.75f;
        return p;
    }

    inline uint32_t mixBits (uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    inline float bipolarFrom (uint32_t x)
    {
        return ((float) (mixBits (x) & 0xffffu) / 32767.5f) - 1.0f;
    }

    /** A deterministic mapping from a random 32-bit throw to a musically broad
        steering target. Keeping the transform deterministic makes the feature
        testable while the UI is free to supply genuinely different throws. */
    inline Profile diceProfile (uint32_t roll)
    {
        Profile p;
        p.brightness  = 0.95f * bipolarFrom (roll ^ 0xA341316Cu);
        p.density     = 0.95f * bipolarFrom (roll ^ 0xC8013EA4u);
        p.harmonicity = 0.95f * bipolarFrom (roll ^ 0xAD90777Du);
        p.aggression  = 0.95f * bipolarFrom (roll ^ 0x7E95761Eu);
        p.divergence  = 0.35f + 0.65f * ((float) (mixBits (roll ^ 0x9E3779B9u) & 0xffffu) / 65535.0f);
        return p;
    }
}
