#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

namespace mutagen::deviceSetup
{
    /*  ==================================================================
        Reaching the standalone app's soundcard from shared code.

        The options page wants to show the audio/MIDI device selector, but
        the device manager belongs to JUCE's StandaloneFilterWindow, which
        only exists in the standalone wrapper. Every source file in
        `target_sources(MUTAGEN ...)` is compiled once into the shared code
        that all three formats link, with JucePlugin_Build_Standalone = 0,
        so a #if in OptionsView.cpp can never be true there - which is why
        the page used to tell standalone users they were inside a host.

        So the direction is inverted: the standalone target compiles one
        extra file, Standalone/DeviceSetup.cpp, which installs a factory
        here at static-initialisation time. The plugin builds never compile
        that file, the factory stays null, and the page says the host owns
        the soundcard - which there is true.
        ================================================================== */
    using Factory = std::function<std::unique_ptr<juce::Component>()>;

    /** The one global slot. Empty in the VST3/AU builds. */
    Factory& factory();

    inline bool available() { return (bool) factory(); }

    /** The device selector, or null in a plugin build. */
    inline std::unique_ptr<juce::Component> create()
    {
        return available() ? factory()() : nullptr;
    }
}
