#include "OrganismSerialization.h"
#include <type_traits>

namespace mutagen
{
    static_assert (std::is_trivially_copyable<OrganismState>::value,
                   "OrganismState must stay trivially copyable for blob serialisation");

    juce::MemoryBlock organismToBlock (const OrganismState& o)
    {
        juce::MemoryBlock mb;
        juce::MemoryOutputStream os (mb, false);
        os.writeInt ((int) organismMagic);
        os.writeInt ((int) organismVersion);
        os.writeInt ((int) sizeof (OrganismState));
        os.write (&o, sizeof (OrganismState));
        return mb;
    }

    bool organismFromBlock (const juce::MemoryBlock& mb, OrganismState& out)
    {
        if (mb.getSize() < 12 + sizeof (OrganismState))
            return false;

        juce::MemoryInputStream is (mb, false);
        const uint32_t magic = (uint32_t) is.readInt();
        const uint32_t ver   = (uint32_t) is.readInt();
        const int      sz    = is.readInt();
        if (magic != organismMagic || ver != organismVersion || sz != (int) sizeof (OrganismState))
            return false;

        is.read (&out, sizeof (OrganismState));
        // sanitise a couple of fields that would break the engine if corrupt
        out.cellCount = juce::jlimit (0, OrganismState::maxCells, out.cellCount);
        out.name[sizeof (out.name) - 1] = 0;
        return true;
    }

    juce::String organismToBase64 (const OrganismState& o)
    {
        const auto mb = organismToBlock (o);
        return mb.toBase64Encoding();
    }

    bool organismFromBase64 (const juce::String& b64, OrganismState& out)
    {
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (b64))
            return false;
        return organismFromBlock (mb, out);
    }
}
