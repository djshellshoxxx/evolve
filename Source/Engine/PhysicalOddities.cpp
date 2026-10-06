#include "PhysicalOddities.h"

#include <chrono>
#include <thread>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <mmsystem.h>
#endif

namespace mutagen::haunted
{
    bool systemBeepAsync (int frequencyHz, int durationMs)
    {
       #if JUCE_WINDOWS
        frequencyHz = frequencyHz < 80 ? 80 : frequencyHz > 2400 ? 2400 : frequencyHz;
        durationMs = durationMs < 20 ? 20 : durationMs > 900 ? 900 : durationMs;
        std::thread ([frequencyHz, durationMs]
        {
            ::Beep ((DWORD) frequencyHz, (DWORD) durationMs);
        }).detach();
        return true;
       #else
        (void) frequencyHz;
        (void) durationMs;
        return false;
       #endif
    }

    bool pulseCdTrayThreeTimesAsync()
    {
       #if JUCE_WINDOWS
        std::thread ([]
        {
            // MCI talks to the operating system's CD-audio device. Failure is
            // intentionally silent: many current computers have no optical drive.
            for (int i = 0; i < 3; ++i)
            {
                mciSendStringW (L"set cdaudio door open", nullptr, 0, nullptr);
                std::this_thread::sleep_for (std::chrono::milliseconds (650));
                mciSendStringW (L"set cdaudio door closed", nullptr, 0, nullptr);
                std::this_thread::sleep_for (std::chrono::milliseconds (500));
            }
        }).detach();
        return true;
       #else
        return false;
       #endif
    }
}
