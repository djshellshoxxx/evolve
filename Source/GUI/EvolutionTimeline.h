#pragma once

#include "Widgets.h"
#include "../Engine/OrganismState.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  The branching Evolution Timeline. Every filed generation is a node;
        forks are alternate evolutionary paths. Click to audition an ancestor,
        double-click (or Restore) to return the live colony to it, Branch to
        fork a new path, or send it to the Breeding Lab.                        */
    class EvolutionTimeline : public PanelFrame
    {
    public:
        explicit EvolutionTimeline (MutagenProcessor&);

        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        void refresh();   // editor timer -> repaint / relayout

        std::function<void (const OrganismState&)> onSendOrganismToLab;
        std::function<void()> onColonyChanged;

    private:
        void restoreSelected();
        int  nodeAt (juce::Point<float>) const;
        juce::Point<float> nodePos (int index) const;

        MutagenProcessor& processor;

        juce::TextButton restoreBtn { "Restore" };
        juce::TextButton branchBtn  { "Branch" };
        juce::TextButton preserveBtn { "Preserve" };
        juce::TextButton toLabBtn   { "To Lab" };

        juce::Rectangle<int> graphArea;
        float scrollX = 0.0f;
        float dragStartScrollX = 0.0f;
        int   selectedNode = -1;
        int   hoverNode = -1;

        static constexpr float dx = 62.0f;
        static constexpr float dy = 30.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvolutionTimeline)
    };
}
