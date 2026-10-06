#pragma once

namespace mutagen::haunted
{
    /** Opens and closes the default optical drive three times on Windows.
        The work is dispatched away from the message/audio threads.
        Returns false on unsupported platforms. */
    bool pulseCdTrayThreeTimesAsync();

    /** Best-effort short system/PC-speaker beep. Non-blocking; unsupported
        platforms simply return false. */
    bool systemBeepAsync (int frequencyHz, int durationMs);
}
