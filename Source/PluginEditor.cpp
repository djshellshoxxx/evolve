// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "AppOptions.h"

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
          performance (p),
          fxRack (p),
          optionsView (p)
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
        addChildComponent (fxRack);

        addChildComponent (helpView);
        addChildComponent (optionsView);

        addAndMakeVisible (gameBar);
        addAndMakeVisible (scoreHud);
        scoreHud.setInterceptsMouseClicks (true, false);
        chamber.setHudProbe ([this] (juce::Point<int> p)
        {
            // The HUD floats over the chamber, so the chamber has to know not
            // to treat a click on the score panel as a mutation.
            return scoreHud.hitsInteractive (scoreHud.getLocalPoint (this, p));
        });

        wireGameLayer();
        wireChrome();

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
        fxRack.onClose = [this]
        {
            showFx = false;
            fxRack.setVisible (false);
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
            if (on) { showFx = false; fxRack.setVisible (false); performance.toFront (false); }
        };
        topBar.onFxToggled = [this] (bool on)
        {
            showFx = on;
            fxRack.setVisible (on);
            if (on) { showPerformance = false; performance.setVisible (false); fxRack.toFront (false); }
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

                topBar.setStatus ("rendering 12s ...");
                RenderEngine::Job job;
                job.destination = f;
                job.seconds = 12.0f;
                renderEngine.onFinished = [this] (bool ok, juce::File out)
                {
                    topBar.setStatus (ok ? "saved " + out.getFileName()
                                         : "render failed");
                };
                renderEngine.start (processor, job);
            });
    }


    // =====================================================================
    //  The chrome: help, options, tooltips
    // =====================================================================

    void MutagenEditor::applyTooltipSetting (bool enabled)
    {
        // The window is created and destroyed rather than hidden, because a
        // live TooltipWindow keeps polling the mouse whether or not anything
        // is shown, and "off" should cost nothing.
        if (enabled && tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, theme::tooltipDelayMs);
        else if (! enabled)
            tooltips.reset();
    }

    void MutagenEditor::wireChrome()
    {
        applyTooltipSetting (AppOptions::get().tooltipsEnabled());

        topBar.onHelp = [this]
        {
            const bool show = ! helpView.isVisible();
            helpView.setVisible (show);
            if (show) { optionsView.setVisible (false); helpView.toFront (true); }
        };

        topBar.onOptions = [this]
        {
            const bool show = ! optionsView.isVisible();
            optionsView.setVisible (show);
            if (show) { helpView.setVisible (false); optionsView.refresh(); optionsView.toFront (true); }
        };

        helpView.onClose    = [this] { helpView.setVisible (false); };
        optionsView.onClose = [this] { optionsView.setVisible (false); };
        optionsView.onTooltipsChanged = [this] (bool on) { applyTooltipSetting (on); };

        topBar.onSaveRun = [this] { saveRun(); };
        topBar.onLoadRun = [this] { loadRun(); };
    }

    // =====================================================================
    //  The game layer
    // =====================================================================

    void MutagenEditor::sendSimple (CommandType t)
    {
        EngineCommand c;
        c.type = t;
        processor.pushCommand (c);
    }

    void MutagenEditor::wireGameLayer()
    {
        // ---- the chamber is the play surface ---------------------------
        chamber.onInteraction = [this] (float weight, juce::String label)
        {
            scoreSystem.registerInteraction (weight, label);
        };

        chamber.onFilesDropped = [this] (const juce::StringArray& files)
        {
            handleDroppedFiles (files);
        };

        // ---- the one-shot gestures --------------------------------------
        gameBar.onEnzyme = [this]
        {
            sendSimple (CommandType::addEnzyme);
            scoreSystem.registerInteraction (0.6f, "ENZYME");
        };

        gameBar.onCatalyst = [this]
        {
            sendSimple (CommandType::addCatalyst);
            scoreSystem.registerInteraction (0.6f, "CATALYST");
        };

        gameBar.onHeat = [this]
        {
            EngineCommand c;
            c.type = CommandType::addHeat;
            c.fa = 1.0f;
            processor.pushCommand (c);
            scoreSystem.registerInteraction (0.4f, "HEAT");
        };

        gameBar.onWater = [this]
        {
            EngineCommand c;
            c.type = CommandType::addHeat;
            c.fa = -1.0f;
            processor.pushCommand (c);
            scoreSystem.registerInteraction (0.4f, "WATER");
        };

        gameBar.onRadiate = [this]
        {
            // The score reacts when the outcome comes back from the audio
            // thread, not here - we do not know yet which way it fell.
            sendSimple (CommandType::radiate);
        };

        // ---- the three gesture knobs ------------------------------------
        gameBar.onKnob = [this] (int which, float amount, float speed)
        {
            EngineCommand c;
            c.type = CommandType::knobGesture;
            c.ia = which;
            c.fa = amount;
            c.fb = speed;
            processor.pushCommand (c);

            // Turning a knob is an entropy observation with unusually good
            // timing resolution, so feed it in.
            processor.noteUserGesture (0.5f + amount * 0.5f, speed);
            scoreSystem.registerInteraction (0.25f * std::abs (amount));
        };

        // ---- run controls -------------------------------------------------
        gameBar.onNewWorld = [this]
        {
            processor.rollNewWorld();
            scoreSystem.registerInteraction (1.0f, "NEW WORLD");
        };

        gameBar.onSaveRun  = [this] { saveRun(); };
        gameBar.onLoadRun  = [this] { loadRun(); };
        gameBar.onScores   = [this] { scoreHud.setTableVisible (! scoreHud.isTableVisible()); };

        // ---- microphone ---------------------------------------------------
        gameBar.onMicArm = [this]
        {
            const bool wasArmed = processor.micArmed();
            processor.armMic (! wasArmed);
        };

        gameBar.onMicCapture = [this]
        {
            processor.startMicCapture (4.0f);
            scoreSystem.registerInteraction (0.5f, "LISTENING");
        };
    }

    void MutagenEditor::handleDroppedFiles (const juce::StringArray& files)
    {
        int eaten = 0;
        for (const auto& f : files)
        {
            const juce::File file (f);
            if (! file.existsAsFile()) continue;
            if (processor.digestFile (file))
            {
                scoreSystem.onSampleDigested (file.getFileNameWithoutExtension());
                ++eaten;
            }
        }

        if (eaten == 0)
            scoreSystem.registerInteraction (0.0f, "COULD NOT READ THAT FILE");
    }

    // =====================================================================
    //  Saving and loading a run
    // =====================================================================

    void MutagenEditor::saveRun()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Save this run",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (juce::String (snapshot.worldName).replace (" ", "_") + ".mutagen"),
            "*.mutagen");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File{}) return;

                // The plugin's own state carries the colony; the score and the
                // run statistics ride alongside it.
                juce::MemoryBlock state;
                processor.getStateInformation (state);

                juce::ValueTree run ("MUTAGEN_RUN");
                run.setProperty ("version", 1, nullptr);
                run.setProperty ("world", juce::String (snapshot.worldName), nullptr);
                run.setProperty ("score", (double) scoreSystem.score(), nullptr);
                run.setProperty ("seconds", scoreSystem.elapsed(), nullptr);
                run.setProperty ("peakVariety", (double) scoreSystem.peakVariety(), nullptr);
                run.setProperty ("discoveries", scoreSystem.discoveries(), nullptr);
                run.setProperty ("generation", snapshot.generation, nullptr);
                run.setProperty ("state", state.toBase64Encoding(), nullptr);

                if (auto xml = run.createXml())
                {
                    if (xml->writeTo (f))
                    {
                        // Filing the run is also what puts it on the board.
                        scoreSystem.submit (juce::String (snapshot.worldName), snapshot);
                        topBar.setStatus ("saved " + f.getFileName());
                        scoreHud.setTableVisible (true);
                        return;
                    }
                }
                topBar.setStatus ("could not save");
            });
    }

    void MutagenEditor::loadRun()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load a run",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.mutagen");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File{} || ! f.existsAsFile()) return;

                auto xml = juce::XmlDocument::parse (f);
                if (xml == nullptr) { topBar.setStatus ("bad file"); return; }

                auto run = juce::ValueTree::fromXml (*xml);
                if (! run.hasType ("MUTAGEN_RUN"))
                {
                    topBar.setStatus ("not a MUTAGEN run");
                    return;
                }

                juce::MemoryBlock state;
                if (state.fromBase64Encoding (run.getProperty ("state").toString())
                    && state.getSize() > 0)
                {
                    processor.setStateInformation (state.getData(), (int) state.getSize());
                }

                timeline.refresh();
                topBar.setStatus ("loaded " + f.getFileName());
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

        // ---- the game layer -------------------------------------------
        scoreSystem.update (snapshot, dt);
        scoreHud.setState (scoreSystem, snapshot);
        gameBar.tick ((float) dt);
        gameBar.setStatus (processor.micArmed(), processor.micCapturing(),
                           processor.micLevel(), processor.entropyTapLevel(),
                           processor.entropyTapLive());

        // A RADIATE landed on the audio thread; react exactly once per result.
        if (const auto counter = processor.radiationCounter(); counter != lastRadiationCounter)
        {
            lastRadiationCounter = counter;
            const int outcome = processor.lastRadiationOutcome();
            scoreSystem.onRadiation (outcome);
            gameBar.flashRadiation (outcome);
        }

        // A microphone capture finished, or the guard killed one.
        if (processor.pollMicCapture())
            scoreSystem.onSampleDigested ("mic");
        if (processor.pollMicAbort())
            scoreSystem.registerInteraction (0.0f, "FEEDBACK - CAPTURE ABORTED");

        // The reward. ScoreSystem decides when; the chamber draws it.
        if (scoreSystem.consumeRewardFlash())
            chamber.triggerReward (snapshot.worldHue);

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
        r.removeFromTop (TopBar::totalHeight);
        r.removeFromBottom (134);                 // timeline

        auto mid = r.reduced (8);

        // The action bar runs the full width above the timeline rather than
        // sitting under the chamber alone: eleven verbs and three knobs do not
        // fit in a column, and at the minimum window size they were clipping
        // off the right-hand end.
        gameBar.setBounds (mid.removeFromBottom (94));
        mid.removeFromBottom (8);

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
        scoreHud.setBounds (chamber.getBounds());
        scoreHud.toFront (false);
    }

    void MutagenEditor::resized()
    {
        auto r = getLocalBounds();

        topBar.setBounds (r.removeFromTop (TopBar::totalHeight));

        timeline.setBounds (r.removeFromBottom (134).reduced (8, 6));

        layoutMain();

        const auto overlay = juce::Rectangle<int> (0, TopBar::totalHeight, getWidth(),
                                                   getHeight() - TopBar::totalHeight);
        breedingLab.setBounds (overlay.reduced (24));
        performance.setBounds (overlay.reduced (24));
        fxRack.setBounds (overlay.reduced (16));

        // Help and options are read, not played with, so they are narrower
        // than the working views and centred rather than filling the window.
        // Options is given the taller box: in the standalone it carries the
        // whole audio/MIDI device selector, and cutting the MIDI input list
        // in half is the one thing that page must not do.
        auto centred = [overlay] (int wantW, int wantH)
        {
            return overlay.reduced (juce::jmax (24, (overlay.getWidth()  - wantW) / 2),
                                    juce::jmax (12, (overlay.getHeight() - wantH) / 2));
        };

        helpView.setBounds (centred (980, 720));
        optionsView.setBounds (centred (980, 860));
    }
}
