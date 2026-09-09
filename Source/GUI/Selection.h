#pragma once

#include "../Engine/OrganismState.h"

namespace mutagen
{
    /*  What the user currently has selected in the Culture Chamber. Shared by
        the chamber, the Genome Inspector and the Breeding Lab.                 */
    struct Selection
    {
        bool       active = false;
        ScopeLevel level  = ScopeLevel::colony;
        int        id     = 0;      // species id / family id / cell slot
        int        cellSlot = -1;   // last clicked cell, for "cell" inspection

        bool operator== (const Selection& o) const
        {
            return active == o.active && level == o.level && id == o.id;
        }
        bool operator!= (const Selection& o) const { return ! (*this == o); }

        juce::String describe() const
        {
            if (! active) return "Whole colony";
            switch (level)
            {
                case ScopeLevel::colony:  return "Whole colony";
                case ScopeLevel::species:
                    return juce::String (id == 0 ? "Grain species"
                                       : id == 1 ? "Spectral species" : "Resonator species");
                case ScopeLevel::family:  return "Family #" + juce::String (id);
                case ScopeLevel::cell:    return "Cell #" + juce::String (id);
            }
            return "Whole colony";
        }
    };
}
