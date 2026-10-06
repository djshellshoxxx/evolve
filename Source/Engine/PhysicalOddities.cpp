#include "PhysicalOddities.h"
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <mmsystem.h>
#endif

namespace mutagen::haunted
{
    bool systemBeepAsync (int frequencyHintHz, int durationHintMs)
    {
       #if JUCE_WINDOWS
        // MessageBeep returns immediately and cannot outlive the plugin in a
        // detached worker. Use the hints to vary the OS sound class.
        const unsigned int selector =
            ((frequencyHintHz / 100) + (durationHintMs / 50)) & 3;
        const UINT type = selector == 0 ? MB_OK
                        : selector == 1 ? MB_ICONASTERISK
                        : selector == 2 ? MB_ICONEXCLAMATION
                                        : MB_ICONQUESTION;
        return ::MessageBeep (type) != FALSE;
       #else
        (void) frequencyHintHz;
        (void) durationHintMs;
        return false;
       #endif
    }

    CdTrayPulser::~CdTrayPulser()
    {
        stop();
    }

    void CdTrayPulser::setDoor (bool open)
    {
       #if JUCE_WINDOWS
        mciSendStringW (open ? L"set cdaudio door open"
                             : L"set cdaudio door closed",
                        nullptr, 0, nullptr);
       #else
        (void) open;
       #endif
    }

    void CdTrayPulser::start (int pulses)
    {
        stop();
        pulsesRemaining = juce::jlimit (1, 8, pulses);
        doorOpen = true;
        setDoor (true);
        startTimer (650);
    }

    void CdTrayPulser::stop()
    {
        stopTimer();
        if (doorOpen)
            setDoor (false);
        doorOpen = false;
        pulsesRemaining = 0;
    }

    void CdTrayPulser::timerCallback()
    {
        if (doorOpen)
        {
            setDoor (false);
            doorOpen = false;
            --pulsesRemaining;
            if (pulsesRemaining <= 0)
            {
                stopTimer();
                return;
            }
            startTimer (500);
        }
        else
        {
            setDoor (true);
            doorOpen = true;
            startTimer (650);
        }
    }
}
