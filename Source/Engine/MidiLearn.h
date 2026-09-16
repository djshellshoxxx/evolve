#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace mutagen
{
    /*  ==================================================================
        MidiLearn

        Maps MIDI CC numbers onto plugin parameters for the right-click
        "Map to MIDI" gesture.

        The audio thread reads this on every controller message and the
        message thread writes it whenever the user maps or clears one, so
        the table is a fixed array of atomics rather than anything that
        allocates: 128 slots, each holding an index into the processor's
        parameter list, or -1 for unmapped. No locks, no allocation, and
        a torn read is impossible because each slot is a single atomic int.
        ================================================================== */
    class MidiLearn
    {
    public:
        static constexpr int numCCs = 128;
        static constexpr int unmapped = -1;

        void prepare (juce::AudioProcessor& p)
        {
            params.clear();
            for (auto* param : p.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    params.add (rp);
        }

        // ---- message thread ------------------------------------------
        /** Arm learning: the next CC to arrive claims this parameter. */
        void beginLearn (const juce::String& paramID)
        {
            learnTarget.store (indexOf (paramID));
        }

        void cancelLearn() { learnTarget.store (unmapped); }

        bool isLearning() const { return learnTarget.load() != unmapped; }

        juce::String learningParamID() const
        {
            const int i = learnTarget.load();
            return juce::isPositiveAndBelow (i, params.size()) ? params[i]->paramID : juce::String();
        }

        /** Assign a CC to a parameter directly. */
        void map (int cc, const juce::String& paramID)
        {
            if (! juce::isPositiveAndBelow (cc, numCCs)) return;
            const int idx = indexOf (paramID);
            if (idx == unmapped) return;

            // One parameter, one CC: drop any previous binding for it, or the
            // parameter ends up answering to two controllers and fighting
            // itself when both are moved.
            for (int c = 0; c < numCCs; ++c)
                if (table[(size_t) c].load() == idx)
                    table[(size_t) c].store (unmapped);

            table[(size_t) cc].store (idx);
        }

        void clearMapping (const juce::String& paramID)
        {
            const int idx = indexOf (paramID);
            for (int c = 0; c < numCCs; ++c)
                if (table[(size_t) c].load() == idx)
                    table[(size_t) c].store (unmapped);
        }

        void clearAll()
        {
            for (auto& slot : table) slot.store (unmapped);
            learnTarget.store (unmapped);
        }

        /** The CC currently driving this parameter, or -1. */
        int ccFor (const juce::String& paramID) const
        {
            const int idx = indexOf (paramID);
            if (idx == unmapped) return unmapped;
            for (int c = 0; c < numCCs; ++c)
                if (table[(size_t) c].load() == idx)
                    return c;
            return unmapped;
        }

        int mappingCount() const
        {
            int n = 0;
            for (const auto& slot : table)
                if (slot.load() != unmapped) ++n;
            return n;
        }

        // ---- audio thread ---------------------------------------------
        /** Handle one controller message. Returns true if it was consumed by
            a mapping or by an armed learn. */
        bool handleController (int cc, int value)
        {
            if (! juce::isPositiveAndBelow (cc, numCCs)) return false;

            // An armed learn claims the first controller that moves.
            if (const int target = learnTarget.load(); target != unmapped)
            {
                for (int c = 0; c < numCCs; ++c)
                    if (table[(size_t) c].load() == target)
                        table[(size_t) c].store (unmapped);

                table[(size_t) cc].store (target);
                learnTarget.store (unmapped);
                learnedStamp.fetch_add (1);
                // Fall through: the message that taught the mapping also sets
                // the value, which is what makes the gesture feel immediate.
            }

            const int idx = table[(size_t) cc].load();
            if (! juce::isPositiveAndBelow (idx, params.size())) return false;

            // setValueNotifyingHost from the audio thread is the documented
            // route for hardware-driven parameter changes.
            params[idx]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value / 127.0f));
            return true;
        }

        /** Increments whenever a learn completes, so the editor can refresh. */
        juce::uint64 learnCounter() const { return learnedStamp.load(); }

        // ---- persistence ------------------------------------------------
        juce::ValueTree toValueTree() const
        {
            juce::ValueTree t ("MIDIMAP");
            for (int c = 0; c < numCCs; ++c)
            {
                const int idx = table[(size_t) c].load();
                if (! juce::isPositiveAndBelow (idx, params.size())) continue;

                juce::ValueTree e ("MAP");
                e.setProperty ("cc", c, nullptr);
                e.setProperty ("param", params[idx]->paramID, nullptr);
                t.appendChild (e, nullptr);
            }
            return t;
        }

        void fromValueTree (const juce::ValueTree& t)
        {
            clearAll();
            if (! t.hasType ("MIDIMAP")) return;
            for (int i = 0; i < t.getNumChildren(); ++i)
            {
                const auto e = t.getChild (i);
                map ((int) e.getProperty ("cc", -1), e.getProperty ("param").toString());
            }
        }

    private:
        int indexOf (const juce::String& paramID) const
        {
            for (int i = 0; i < params.size(); ++i)
                if (params[i]->paramID == paramID)
                    return i;
            return unmapped;
        }

        juce::Array<juce::RangedAudioParameter*> params;
        std::array<std::atomic<int>, numCCs> table { };
        std::atomic<int> learnTarget { unmapped };
        std::atomic<juce::uint64> learnedStamp { 0 };

    public:
        MidiLearn() { for (auto& slot : table) slot.store (unmapped); }
    };
}
