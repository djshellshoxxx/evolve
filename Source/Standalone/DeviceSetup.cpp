/*  Compiled into the standalone application only - see the comment in
    GUI/DeviceSetupHook.h for why this lives outside the shared code.        */

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "../GUI/DeviceSetupHook.h"

namespace
{
    struct Installer
    {
        Installer()
        {
            mutagen::deviceSetup::factory() = [] () -> std::unique_ptr<juce::Component>
            {
                auto* holder = juce::StandalonePluginHolder::getInstance();
                if (holder == nullptr)
                    return nullptr;

                // Two inputs so the microphone and live-input modes have
                // something to listen to; stereo out.
                return std::make_unique<juce::AudioDeviceSelectorComponent> (
                    holder->deviceManager,
                    0, 2,       // min/max inputs
                    2, 2,       // min/max outputs
                    true,       // show MIDI inputs
                    false,      // no MIDI output
                    true,       // treat channels as stereo pairs
                    false);     // hide the advanced options
            };
        }
    };

    const Installer installer;
}
