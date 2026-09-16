#include "DeviceSetupHook.h"

namespace mutagen::deviceSetup
{
    Factory& factory()
    {
        // Function-local static: the standalone's installer runs during static
        // initialisation, and this guarantees the slot exists before it does.
        static Factory f;
        return f;
    }
}
