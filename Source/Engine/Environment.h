#pragma once

namespace mutagen
{
    /*  The knobs the musician actually turns. Every field is 0..1 unless noted.
        The Colony reads this struct once per ecological tick; nothing here maps
        to a filter or an oscillator, only to the odds of birth, death, change
        and cooperation inside the population.                                  */
    struct Environment
    {
        // the five large controls
        float nutrients   = 0.55f;   // available energy -> population it can sustain
        float mutation    = 0.30f;   // master mutation rate
        float selection   = 0.35f;   // how hard fitness is enforced
        float metabolism  = 0.50f;   // energy burn -> tempo of the whole lifecycle
        float stability   = 0.60f;   // resists drift; damps temperature & radiation

        // deeper ecology
        float fertility     = 0.50f;
        float mutationDepth = 0.35f;
        float radiation     = 0.05f;
        float temperature   = 0.25f;  // micro-instability injected into genomes
        float competition   = 0.45f;
        float symbiosis     = 0.35f;
        float lifespan      = 0.55f;
        float apoptosis     = 0.20f;
        float diversity     = 0.50f;
        float migration     = 0.30f;

        // selection targets: what "fit" means right now
        float selBrightness  = 0.0f;  // -1 dark .. +1 bright
        float selDensity     = 0.0f;  // -1 sparse .. +1 dense
        float selHarmonicity = 0.0f;  // -1 noisy .. +1 harmonic
        float selAggression  = 0.0f;  // -1 calm .. +1 aggressive
        float selDivergence  = 0.0f;  // 0 familiar .. 1 far from the seed

        bool  explore = true;         // false = Preserve: freeze evolution
        float memory  = 0.5f;         // inheritance of new note-organisms from colony

        /** Aggregate "stress" the colony is under. Dormant genes wake above ~0.5. */
        float stress() const
        {
            const float s = 0.45f * temperature
                          + 0.30f * competition
                          + 0.20f * radiation
                          + 0.25f * apoptosis
                          - 0.35f * stability
                          + 0.15f;
            return s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        }
    };
}
