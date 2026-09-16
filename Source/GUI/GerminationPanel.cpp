#include "GerminationPanel.h"
#include "../PluginProcessor.h"
#include "../Engine/OrganismSerialization.h"

namespace mutagen
{
    using namespace theme;

    GerminationPanel::GerminationPanel (MutagenProcessor& p)
        : PanelFrame ("Germination"),
          processor (p),
          captureLenKnob (p.apvts, params::captureLength, "Capture", grain),
          transientKnob  (p.apvts, params::transientSens, "Transient", grain),
          populationKnob (p.apvts, params::initialPopulation, "Population", grain),
          distGrainKnob  (p.apvts, params::distGrain,  "Grain",  grain),
          distSpecKnob   (p.apvts, params::distSpectral, "Spectral", spectral),
          distResKnob    (p.apvts, params::distResonator, "Resonator", resonator)
    {
        accentColour = grain;

        sourceBox.addItemList (params::sourceModeChoices(), 1);
        paramMenu::tag (sourceBox, params::sourceMode);
        sourceBox.setTooltip (params::describe (params::sourceMode));
        addAndMakeVisible (sourceBox);
        sourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                        (p.apvts, params::sourceMode, sourceBox);

        for (auto* b : { &loadSampleBtn, &loadOrgBtn, &captureBtn, &germinateBtn, &randomBtn })
            addAndMakeVisible (b);

        loadSampleBtn.setTooltip ("Read a WAV or AIFF from disk as the colony's food. "
                                  "You can also drop files straight onto the dish.");
        loadOrgBtn.setTooltip    ("Load a preserved organism and germinate from it "
                                  "instead of from audio.");
        captureBtn.setTooltip    ("Take the plugin's incoming audio as the source.");
        germinateBtn.setTooltip  ("Kill the current colony and grow a new one from the "
                                  "source above. This is the destructive one.");
        randomBtn.setTooltip     ("A new random seed: the same source, a different colony.");

        germinateBtn.getProperties().set ("tint", (int) grain.getARGB());
        germinateBtn.onClick = [this] { germinate(); };
        randomBtn.onClick     = [this]
        {
            EngineCommand c; c.type = CommandType::reseedRandom;
            processor.pushCommand (c);
        };
        loadSampleBtn.onClick = [this] { loadSampleFile(); };
        loadOrgBtn.onClick    = [this] { loadOrganismFile(); };
        captureBtn.onClick    = [this] { captureLive(); };

        for (auto* k : { &captureLenKnob, &transientKnob, &populationKnob,
                         &distGrainKnob, &distSpecKnob, &distResKnob })
            addAndMakeVisible (k);

        addAndMakeVisible (mixBar);

        sourceInfo.setText ("Colony divided into grain / spectral / resonator material",
                            juce::dontSendNotification);
        sourceInfo.setColour (juce::Label::textColourId, textDim);
        sourceInfo.setJustificationType (juce::Justification::topLeft);
        sourceInfo.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (sourceInfo);
    }

    GerminationPanel::~GerminationPanel() = default;

    void GerminationPanel::setSnapshot (const EngineSnapshot& s)
    {
        mixBar.setCounts (s.popBySpecies[0], s.popBySpecies[1], s.popBySpecies[2]);
    }

    void GerminationPanel::germinate()
    {
        EngineCommand c;
        c.type = CommandType::germinate;
        processor.pushCommand (c);
    }

    void GerminationPanel::loadSampleFile()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Select audio to germinate from", juce::File{},
            "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile() && processor.loadSourceFromFile (f))
                {
                    sourceInfo.setText ("Seed: " + f.getFileName(), juce::dontSendNotification);
                    germinate();
                }
            });
    }

    void GerminationPanel::loadOrganismFile()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load a preserved organism", juce::File{}, "*.mutagen");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                OrganismState o;
                if (organismFromBase64 (f.loadFileAsString().trim(), o) && o.cellCount > 0)
                {
                    EngineCommand c;
                    c.type = CommandType::restoreOrganism;
                    c.payloadIndex = processor.stageOrganismPayload (o);
                    processor.pushCommand (c);
                    sourceInfo.setText ("Organism: " + f.getFileNameWithoutExtension(),
                                        juce::dontSendNotification);
                }
            });
    }

    void GerminationPanel::captureLive()
    {
        const float len = processor.apvts.getRawParameterValue (params::captureLength)->load();
        const float sens = processor.apvts.getRawParameterValue (params::transientSens)->load();
        processor.captureLiveToSource (len, sens);
        sourceInfo.setText ("Seed: captured " + juce::String (len, 1) + "s of live audio",
                            juce::dontSendNotification);
        germinate();
    }

    void GerminationPanel::resized()
    {
        auto r = contentArea();

        sourceBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);

        auto row = r.removeFromTop (24);
        loadSampleBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        loadOrgBtn.setBounds (row);
        r.removeFromTop (5);
        captureBtn.setBounds (r.removeFromTop (24));
        r.removeFromTop (8);

        auto knobs = r.removeFromTop (74);
        const int kw = knobs.getWidth() / 3;
        captureLenKnob.setBounds (knobs.removeFromLeft (kw));
        transientKnob.setBounds  (knobs.removeFromLeft (kw));
        populationKnob.setBounds (knobs);
        r.removeFromTop (10);

        auto dist = r.removeFromTop (74);
        const int dw = dist.getWidth() / 3;
        distGrainKnob.setBounds (dist.removeFromLeft (dw));
        distSpecKnob.setBounds  (dist.removeFromLeft (dw));
        distResKnob.setBounds   (dist);
        r.removeFromTop (8);

        mixBar.setBounds (r.removeFromTop (20));
        r.removeFromTop (10);

        germinateBtn.setBounds (r.removeFromTop (30));
        r.removeFromTop (5);
        randomBtn.setBounds (r.removeFromTop (24));
        r.removeFromTop (8);

        sourceInfo.setBounds (r);
    }
}
