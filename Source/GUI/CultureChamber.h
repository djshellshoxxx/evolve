#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include "../Engine/OrganismState.h"
#include "Selection.h"

namespace mutagen
{
    class MutagenProcessor;

    /*  The Culture Chamber: a functional read-out of the live engine. Every
        blob, ring, arc and haze corresponds to something the colony is doing
        right now. Click to select a cell / family / species; right-click for
        the isolate / mute / preserve / eliminate / inspect / breed menu.      */
    class CultureChamber : public juce::Component
    {
    public:
        explicit CultureChamber (MutagenProcessor&);

        void update (const EngineSnapshot& snap, double dtSeconds);

        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        std::function<void (const Selection&)> onSelectionChanged;
        std::function<void (const Selection&)> onInspect;
        std::function<void (const Selection&)> onSendToBreedingLab;

        void setSelection (const Selection& s) { selection = s; repaint(); }
        const Selection& getSelection() const { return selection; }

    private:
        struct Ripple { float x, y, r, life, maxLife; juce::Colour c; };

        juce::Point<float> toPixels (float nx, float ny) const;
        int   hitTestCell (juce::Point<float> p) const;
        void  showContextMenu (int cellIndex);
        void  emitSelection();

        MutagenProcessor& processor;
        EngineSnapshot snap;
        double phase = 0.0;
        double lastWidth = 0.0, lastHeight = 0.0;

        Selection selection;
        int hoverCell = -1;
        juce::Point<float> mousePos;
        std::vector<Ripple> ripples;

        juce::Rectangle<float> field; // drawable inner area

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CultureChamber)
    };
}
