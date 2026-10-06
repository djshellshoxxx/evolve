#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mutagen::haunted
{
    /** Fire-and-forget transparent desktop animation. The object owns itself
        and disappears after the animation. Best-effort: hosts/OSes may choose
        to constrain plugin-created desktop windows. */
    void launchDesktopPhantom (int creatureId,
                               juce::Rectangle<int> sourceGlobal,
                               int animationRecipe,
                               float intensity = 1.0f);
}
