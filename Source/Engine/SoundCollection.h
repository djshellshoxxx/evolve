// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Storyline.h"

namespace mutagen
{
    /** The specimen jar: short recordings of the colony, kept on disk as WAV.

        Sounds are collected when a story event, lab game or haunted moment
        rewards one, or when the player presses COLLECT. Each one is prepared
        the way an engineer would prepare a one-shot for a sample library:
        DC removed, peak-normalised to -1 dBFS for headroom, and given short
        fades so it starts and ends at zero without a click. They are written
        straight away as 24-bit WAV, so nothing is lost if the host crashes,
        and EXPORT copies the whole collection to any folder.

        Message thread only. */
    class SoundCollection
    {
    public:
        struct Entry
        {
            juce::File file;
            juce::String label;
        };

        SoundCollection();

        /** Prepares and writes one sound. Returns false on silence or a disk error. */
        bool collect (const juce::AudioBuffer<float>& source, double sampleRate,
                      const juce::String& label);

        /** Copies every collected WAV into the folder. Returns how many were copied. */
        int exportTo (const juce::File& folder) const;

        const juce::Array<Entry>& entries() const { return items; }
        int size() const { return items.size(); }

        static juce::File collectionFolder();

        /** Pure processing step, public so it can be tested in isolation:
            DC removal, -1 dBFS peak normalise, 10 ms fades. Returns the peak
            before normalising (0 for silence, in which case nothing changed). */
        static float prepare (juce::AudioBuffer<float>& buffer, double sampleRate);

        static bool writeWav (const juce::AudioBuffer<float>&, double sampleRate,
                              const juce::File& destination);

    private:
        void rescan();
        juce::Array<Entry> items;
        int nextIndex = 1;
    };

    /** Persists the Resonance Acts progress next to the field journal. */
    namespace storyio
    {
        story::Progress load();
        void save (const story::Progress&);
    }
}
