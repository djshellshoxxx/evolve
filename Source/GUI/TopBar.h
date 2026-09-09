#pragma once

#include "Widgets.h"
#include "../Engine/OrganismState.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  The top bar: the organism's identity and the transport-level verbs -
        clone, freeze, reanimate, render, and a full reset to defaults.        */
    class TopBar : public juce::Component
    {
    public:
        explicit TopBar (MutagenProcessor&);

        void resized() override;
        void paint (juce::Graphics&) override;
        void setStats (const EngineSnapshot&);

        std::function<void()> onClone;
        std::function<void()> onRender;
        std::function<void()> onReset;
        std::function<void (bool)> onPerformanceToggled;
        std::function<void (bool)> onInspectorToggled;

    private:
        MutagenProcessor& processor;

        juce::Label      nameLabel;
        juce::Label      statsLabel;

        juce::ComboBox   cpuBox, roleBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cpuAttach, roleAttach;

        juce::TextButton exploreToggle { "EXPLORING" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> exploreAttach;

        juce::TextButton cloneBtn   { "Clone" };
        juce::TextButton freezeBtn  { "Freeze" };
        juce::TextButton reanimateBtn { "Reanimate" };
        juce::TextButton renderBtn  { "Render" };
        juce::TextButton resetBtn   { "RESET" };

        juce::TextButton perfToggle { "Perform" };
        juce::TextButton inspToggle { "Inspector" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
