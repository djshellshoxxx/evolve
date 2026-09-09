#pragma once

#include "Widgets.h"
#include "../Engine/OrganismState.h"

namespace mutagen
{
    class MutagenProcessor;

    /*  Germination: choose what the colony grows from, then plant it. */
    class GerminationPanel : public PanelFrame
    {
    public:
        explicit GerminationPanel (MutagenProcessor&);
        ~GerminationPanel() override;

        void resized() override;
        void setSnapshot (const EngineSnapshot&);

    private:
        void germinate();
        void loadSampleFile();
        void loadOrganismFile();
        void captureLive();

        MutagenProcessor& processor;

        juce::ComboBox sourceBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttach;

        juce::TextButton loadSampleBtn { "Load Sample" };
        juce::TextButton loadOrgBtn    { "Load Organism" };
        juce::TextButton captureBtn    { "Capture Live" };
        juce::TextButton germinateBtn  { "GERMINATE" };
        juce::TextButton randomBtn     { "Random Seed" };

        LabeledKnob captureLenKnob, transientKnob, populationKnob;
        LabeledKnob distGrainKnob, distSpecKnob, distResKnob;

        SpeciesMixBar mixBar;
        juce::Label   sourceInfo;

        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GerminationPanel)
    };
}
