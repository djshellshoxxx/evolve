#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/MutagenLookAndFeel.h"
#include "GUI/CultureChamber.h"
#include "GUI/GerminationPanel.h"
#include "GUI/EnvironmentPanel.h"
#include "GUI/GenomeInspector.h"
#include "GUI/EvolutionTimeline.h"
#include "GUI/BreedingLabView.h"
#include "GUI/PerformanceView.h"
#include "GUI/FxRackView.h"
#include "GUI/TopBar.h"
#include "Engine/RenderEngine.h"

namespace mutagen
{
    class MutagenEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
    {
    public:
        explicit MutagenEditor (MutagenProcessor&);
        ~MutagenEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void openBreedingLab (const OrganismState& parent);
        void doRender();
        void layoutMain();

        MutagenProcessor& processor;
        MutagenLookAndFeel lnf;

        TopBar            topBar;
        GerminationPanel  germination;
        EnvironmentPanel  environment;
        CultureChamber    chamber;
        GenomeInspector   inspector;
        EvolutionTimeline timeline;
        BreedingLabView   breedingLab;
        PerformanceView   performance;
        FxRackView        fxRack;

        juce::Label       renderStatus;
        RenderEngine      renderEngine;
        std::unique_ptr<juce::FileChooser> chooser;

        EngineSnapshot snapshot;
        double lastTimeSec = 0.0;
        double inspectorAccum = 0.0;
        bool   showInspector = true;
        bool   showPerformance = false;
        bool   showFx = false;
        int    labParentFilled = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenEditor)
    };
}
