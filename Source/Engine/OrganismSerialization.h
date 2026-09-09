#pragma once

#include <juce_core/juce_core.h>
#include "OrganismState.h"

namespace mutagen
{
    /*  OrganismState is trivially copyable (all POD, no pointers), so it round-
        trips through a size-checked, versioned binary blob. Used for plugin
        state, presets and history serialisation alike.                        */
    constexpr uint32_t organismMagic   = 0x4D544731; // 'MTG1'
    constexpr uint32_t organismVersion = 1;

    juce::MemoryBlock organismToBlock (const OrganismState& o);
    bool              organismFromBlock (const juce::MemoryBlock& mb, OrganismState& out);

    juce::String      organismToBase64 (const OrganismState& o);
    bool              organismFromBase64 (const juce::String& b64, OrganismState& out);
}
