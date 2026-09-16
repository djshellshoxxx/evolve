#pragma once

#include "Widgets.h"
#include "Selection.h"
#include "../Engine/OrganismState.h"
#include <array>

namespace mutagen
{
    class MutagenProcessor;

    /*  Genome Inspector: examine, mutate and lock the inherited traits of the
        current selection - whole colony, one species, or one family.          */
    class GenomeInspector : public PanelFrame
    {
    public:
        explicit GenomeInspector (MutagenProcessor&);
        ~GenomeInspector() override;

        void resized() override;
        void setSelection (const Selection&);
        void refresh (const OrganismState&);   // called ~2 Hz by the editor

    private:
        class TraitRow;

        void pushValue (int trait, float v);
        void pushDominance (int trait, int dom);
        void pushLock (int trait, bool locked);
        void mutateScope();

        MutagenProcessor& processor;
        Selection selection;

        juce::Label     scopeLabel;
        juce::TextButton mutateBtn { "Mutate" };
        juce::TextButton selectBtn { "Select" };
        juce::TextButton lockAllBtn { "Lock All" };
        juce::TextButton unlockAllBtn { "Unlock" };

        juce::Viewport  viewport;
        juce::Component  rowHost;
        std::array<std::unique_ptr<TraitRow>, numTraits> rows;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenomeInspector)
    };
}
