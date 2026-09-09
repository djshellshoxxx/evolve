#include "PluginEditor.h"

namespace mutagen
{
    using namespace theme;

    static OrganismState filterOrganism (const OrganismState& o, const Selection& sel)
    {
        if (! sel.active || sel.level == ScopeLevel::colony) return o;

        OrganismState out;
        out.seed = o.seed;
        out.generation = o.generation;
        out.env = o.env;
        out.baseline = o.baseline;

        int idx = 0;
        for (int i = 0; i < o.cellCount && i < OrganismState::maxCells; ++i)
        {
            const auto& c = o.cells[i];
            if (! c.alive) continue;
            bool match = false;
            switch (sel.level)
            {
                case ScopeLevel::species: match = (c.species == sel.id); break;
                case ScopeLevel::family:  match = (c.familyId == sel.id); break;
                case ScopeLevel::cell:    match = (i == sel.cellSlot); break;
                default: break;
            }
            if (! match) continue;
            out.cells[idx++] = c;
            if (c.species < numSpecies) out.popBySpecies[c.species]++;
        }
        out.cellCount = idx;
        if (idx == 0) return o;   // nothing matched: fall back to the whole colony
        return out;
    }

    // =====================================================================

    MutagenEditor::MutagenEditor (MutagenProcessor& p)
        : juce::AudioProcessorEditor (p),
          processor (p),
          topBar (p),
          germination (p),
          environment (p),
          chamber (p),
          inspector (p),
          timeline (p),
          breedingLab (p),
          performance (p)
    {
        setLookAndFeel (&lnf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (germination);
        addAndMakeVisible (environment);
        addAndMakeVisible (chamber);
        addAndMakeVisible (inspector);
        addAndMakeVisible (timeline);
        addChildComponent (breedingLab);
        addChildComponent (performance);

        renderStatus.setColour (juce::Label::textColourId, theme::spectral);
        renderStatus.setJustificationType (juce::Justification::centredRight);
        renderStatus.setFont (11.0f);
        addAndMakeVisible (renderStatus);

        // ---- wiring ----
        chamber.onSelectionChanged = [this] (const Selection& s)
        {
            inspector.setSelection (s);
        };
        chamber.onInspect = [this] (const Selection& s)
        {
            showInspector = true;
            inspector.setVisible (true);
            inspector.setSelection (s);
            layoutMain();
        };
        chamber.onSendToBreedingLab = [this] (const Selection& s)
        {
            openBreedingLab (filterOrganism (processor.latestOrganism(), s));
        };

        timeline.onSendOrganismToLab = [this] (const OrganismState& o) { openBreedingLab (o); };
        timeline.onColonyChanged = [this] { chamber.repaint(); };

        breedingLab.onClose = [this] { breedingLab.setVisible (false); };
        performance.onClose = [this]
        {
            showPerformance = false;
            performance.setVisible (false);
        };

        topBar.onClone = [this]
        {
            processor.history.beginBranchFrom (processor.history.currentId());
            processor.requestGenerationCapture();
        };
        topBar.onRender = [this] { doRender(); };
        topBar.onReset  = [this]
        {
            processor.resetEverything();
            timeline.refresh();
            inspector.setSelection ({});
            chamber.setSelection ({});
        };
        topBar.onPerformanceToggled = [this] (bool on)
        {
            showPerformance = on;
            performance.setVisible (on);
            if (on) performance.toFront (false);
        };
        topBar.onInspectorToggled = [this] (bool on)
        {
            showInspector = on;
            inspector.setVisible (on);
            layoutMain();
        };

        setResizable (true, true);
        setResizeLimits (1060, 700, 2600, 1700);
        setSize (1320, 860);

        lastTimeSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        startTimerHz (40);
    }

    MutagenEditor::~MutagenEditor()
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    // =====================================================================

    void MutagenEditor::openBreedingLab (const OrganismState& parent)
    {
        const int which = (labParentFilled == 0) ? 0 : 1;
        breedingLab.setParent (which,
                               parent,
                               juce::String (which == 0 ? "Parent A" : "Parent B")
                                   + "  gen " + juce::String (parent.generation));
        labParentFilled = juce::jmin (2, labParentFilled + 1);
        breedingLab.setVisible (true);
        breedingLab.toFront (false);
        breedingLab.resized();
    }

    void MutagenEditor::doRender()
    {
        if (renderEngine.isBusy()) return;

        chooser = std::make_unique<juce::FileChooser> (
            "Render the colony to WAV",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (processor.organismName + ".wav"),
            "*.wav");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File{}) return;

                renderStatus.setText ("rendering 12s ...", juce::dontSendNotification);
                RenderEngine::Job job;
                job.destination = f;
                job.seconds = 12.0f;
                renderEngine.onFinished = [this] (bool ok, juce::File out)
                {
                    renderStatus.setText (ok ? "saved " + out.getFileName()
                                             : "render failed",
                                          juce::dontSendNotification);
                };
                renderEngine.start (processor, job);
            });
    }

    // =====================================================================

    void MutagenEditor::timerCallback()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        double dt = now - lastTimeSec;
        lastTimeSec = now;
        dt = juce::jlimit (0.0, 0.1, dt);

        processor.copyLatestSnapshot (snapshot);

        chamber.update (snapshot, dt);
        topBar.setStats (snapshot);
        germination.setSnapshot (snapshot);

        inspectorAccum += dt;
        if (inspectorAccum > 0.35)
        {
            inspectorAccum = 0.0;
            if (showInspector && ! showPerformance)
                inspector.refresh (processor.latestOrganism());
        }

        if (processor.pumpHistory() > 0)
            timeline.refresh();
        else
            timeline.repaint();
    }

    // =====================================================================

    void MutagenEditor::paint (juce::Graphics& g)
    {
        g.fillAll (bg0);
    }

    void MutagenEditor::layoutMain()
    {
        auto r = getLocalBounds();
        r.removeFromTop (48);                     // topbar
        r.removeFromBottom (134);                 // timeline

        auto mid = r.reduced (8);
        germination.setBounds (mid.removeFromLeft (240));
        mid.removeFromLeft (8);
        environment.setBounds (mid.removeFromRight (306));
        mid.removeFromRight (8);

        if (showInspector)
        {
            inspector.setBounds (mid.removeFromRight (298));
            mid.removeFromRight (8);
        }
        chamber.setBounds (mid);
    }

    void MutagenEditor::resized()
    {
        auto r = getLocalBounds();

        topBar.setBounds (r.removeFromTop (48));
        renderStatus.setBounds (getWidth() - 260, 2, 250, 14);

        timeline.setBounds (r.removeFromBottom (134).reduced (8, 6));

        layoutMain();

        const auto overlay = juce::Rectangle<int> (0, 48, getWidth(), getHeight() - 48);
        breedingLab.setBounds (overlay.reduced (24));
        performance.setBounds (overlay.reduced (24));
    }
}
