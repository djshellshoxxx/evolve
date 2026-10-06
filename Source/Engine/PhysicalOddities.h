#pragma once

namespace mutagen::haunted
{
    /** Opens and closes the default optical drive three times on Windows.
        The work is dispatched away from the message/audio threads.
        Returns false on unsupported platforms. */
    bool pulseCdTrayThreeTimesAsync();
}
