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

    private:
        ProgressionSystem& progression;
        juce::TextButton closeButton { "Close" };
        juce::Viewport viewport;
        juce::Component content;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressionView)
    };
}
