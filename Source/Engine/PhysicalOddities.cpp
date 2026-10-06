#include "PhysicalOddities.h"

#include <chrono>
#include <thread>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <mmsystem.h>
#endif

namespace mutagen::haunted
{
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
