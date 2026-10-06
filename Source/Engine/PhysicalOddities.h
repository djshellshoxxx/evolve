#pragma once

#include <juce_events/juce_events.h>

namespace mutagen::haunted
{
    /** Editor-owned optical drive pulse sequencer. All MCI commands run on the
        message thread and the timer stops automatically with the owning editor,
        avoiding detached code that could outlive a plugin DLL. */
    class CdTrayPulser final : private juce::Timer
    {
    public:
        CdTrayPulser() = default;
        ~CdTrayPulser() override;

        void start (int pulses = 3);
        void stop();

    private:
        void timerCallback() override;
        void setDoor (bool open);

        int pulsesRemaining = 0;
        bool doorOpen = false;
    };

    /** Best-effort non-blocking system/PC-speaker notification. On modern
        Windows machines this is commonly routed through the system audio path.
        Unsupported platforms return false. */
    bool systemBeepAsync (int frequencyHintHz, int durationHintMs);
}
