#pragma once

#include "Widgets.h"
#include "../Engine/ProgressionSystem.h"
#include <functional>

namespace mutagen
{
    class ProgressionView : public PanelFrame
    {
    public:
        explicit ProgressionView (ProgressionSystem&);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        std::function<void()> onClose;
        std::function<void(int milestoneIndex)> onReplayMilestone;

    private:
        ProgressionSystem& progression;
        void rebuildArtifactSelector();

        juce::TextButton closeButton { "Close" };
        juce::ComboBox artifactSelector;
        juce::TextButton replayArtifact { "REPLAY SKILL" };
        juce::Viewport viewport;
        juce::Component content;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressionView)
    };
}
