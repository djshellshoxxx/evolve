#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <bit>
#include <cstdint>
namespace mutagen
{
// Parameter comparisons belong to a processor instance, not an editor lifetime.
// All methods are host/message-thread operations; never call from processBlock.
class ParameterSlots
{
public:
    explicit ParameterSlots (juce::AudioProcessorValueTreeState& s) : state (s) {}
    bool isB() const { const juce::ScopedLock guard (lock); return activeB; }
    void switchTo (bool b)
    {
        const juce::ScopedLock guard (lock);
        if (b == activeB) return;
        slots[activeB ? 1 : 0] = capture();
        auto& target = slots[b ? 1 : 0];
        if (! target.isValid()) target = capture(); else apply (target);
        activeB = b;
    }
    void copyAcross()
    {
        const juce::ScopedLock guard (lock);
        slots[0] = capture(); slots[1] = slots[0].createCopy();
    }
    juce::ValueTree toValueTree()
    {
        const juce::ScopedLock guard (lock);
        slots[activeB ? 1 : 0] = capture();
        juce::ValueTree root ("PARAMETER_SLOTS"); root.setProperty ("version", 1, nullptr);
        root.setProperty ("activeB", activeB, nullptr);
        for (int i = 0; i < 2; ++i) if (slots[i].isValid())
        { auto child = slots[i].createCopy(); child.setProperty ("index", i, nullptr); root.appendChild (child, nullptr); }
        return root;
    }
    void fromValueTree (const juce::ValueTree& root)
    {
        const juce::ScopedLock guard (lock); slots = {}; activeB = false;
        if (! root.hasType ("PARAMETER_SLOTS") || (int) root.getProperty ("version", 0) != 1) return;
        activeB = (bool) root.getProperty ("activeB", false);
        for (auto child : root)
        {
            const int i = (int) child.getProperty ("index", -1);
            if (! child.hasType ("SLOT") || i < 0 || i > 1) continue;
            juce::ValueTree clean ("SLOT");
            for (auto* parameter : state.processor.getParameters())
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    if (child.hasProperty (p->paramID))
                    {
                        const double value = (double) child.getProperty (p->paramID);
                        if ((std::bit_cast<std::uint64_t> (value) & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL) clean.setProperty (p->paramID, juce::jlimit (0.0, 1.0, value), nullptr);
                    }
            slots[i] = clean;
        }
    }
private:
    juce::ValueTree capture() const
    {
        juce::ValueTree v ("SLOT");
        for (auto* parameter : state.processor.getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameter)) v.setProperty (p->paramID, p->getValue(), nullptr);
        return v;
    }
    void apply (const juce::ValueTree& v)
    {
        for (auto* parameter : state.processor.getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (v.hasProperty (p->paramID))
                { p->beginChangeGesture(); p->setValueNotifyingHost ((float) v.getProperty (p->paramID)); p->endChangeGesture(); }
    }
    juce::AudioProcessorValueTreeState& state;
    mutable juce::CriticalSection lock;
    std::array<juce::ValueTree, 2> slots;
    bool activeB = false;
};
}
