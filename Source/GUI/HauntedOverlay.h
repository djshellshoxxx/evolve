#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace mutagen::haunted
{
    struct PhantomRegistry;

    /** Owns any desktop phantom windows created by this editor. */
    class DesktopPhantomManager
    {
    public:
        DesktopPhantomManager();
        ~DesktopPhantomManager();

        DesktopPhantomManager (const DesktopPhantomManager&) = delete;
        DesktopPhantomManager& operator= (const DesktopPhantomManager&) = delete;

        void launch (int creatureId, juce::Rectangle<int> sourceGlobal,
                     int animationRecipe, float intensity = 1.0f);
        void clear();

    private:
        std::shared_ptr<PhantomRegistry> registry;
    };
}
