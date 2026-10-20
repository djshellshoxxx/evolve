// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "AppOptions.h"
#include <iterator>
#include <cmath>

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
          optionsView (p),
          progressionView (progressionSystem)
    {
        setLookAndFeel (&lnf);

        addAndMakeVisible (topBar);
        addAndMakeVisible (germination);
        addAndMakeVisible (environment);
        addAndMakeVisible (chamber);
        addAndMakeVisible (storyPanel);
        wireLabGames();
        wireEncounters();
        scoreSystem.restoreRun (processor.savedRunScore.load(), processor.savedRunSeconds.load());
        scoreSystem.setRunId (juce::String (processor.savedRunId.load()));
        scoreRestoreRevision = processor.runRestoreRevision.load();

        addAndMakeVisible (secretLayer);
        addAndMakeVisible (storyFx);
        addChildComponent (cameraSynth);
        addChildComponent (cameraSynthLaunch);
        cameraSynthLaunch.onClick = [this] { cameraSynth.show(); };
        cameraSynthDiscovered = progressionSystem.webcamSynthUnlocked();
        cameraSynthLaunch.setVisible (cameraSynthDiscovered);
        storyPanel.onAnim   = [this] (chance::Anim a, juce::Colour c, float i) { storyFx.play (a, c, i); };
        storyPanel.onBanner = [this] (const juce::String& t, const juce::String& s, juce::Colour c) { storyFx.banner (t, s, c); };
        storyPanel.onGlitch = [this] (juce::Colour c) { storyFx.glitch (c); };
        storyPanel.onFact = [this] (const juce::String& cat, const juce::String& text, int pts, int bonus, juce::Colour c)
                            { storyFx.factToast (cat, text, pts, bonus, c); };
        storyPanel.onPoints = [this] (juce::int64 pts, const juce::String& label) { scoreSystem.adjustScore (pts, label); };
        labGameOverlay.onWin = [this] { storyPanel.grantFactReward(); };
        secretLayer.onReward = [this] (float w, const juce::String& label) { scoreSystem.registerInteraction (w, label); };
        addChildComponent (introOverlay);
        storyPanel.onIntro = [this] { introOverlay.open(); introOverlay.grabKeyboardFocus(); };
        addChildComponent (nameOverlay);
        nameOverlay.onName = [this] (const juce::String& n) { startGameAs (n); };
        secretLayer.onPoints = [this] (juce::int64 pts, const juce::String& label) { scoreSystem.adjustScore (pts, label); };
        secretLayer.onLoseSkills = [this]
        {
            for (int i = 0; i < 5; ++i)
                progressionSystem.changeGameSkill (i, -progressionSystem.gameSkills()[(size_t) i]);
        };
        // Every new game starts by asking who is playing (after the intro, the first time).
        introOverlay.onClosed = [this]
        {
            const bool first = ! storyPanel.progress().introSeen;
            storyPanel.markIntroSeen();
            if (first) askName();
        };
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MutagenEditor> (this)]
        {
            if (safe == nullptr) return;

           #if MUTAGEN_TEST_HOOKS
            // Smoke-test hooks (test builds only): force a fate or a secret room.
            const auto fate = juce::SystemStats::getEnvironmentVariable ("MUTAGEN_TEST_FATE", {});
            const auto room = juce::SystemStats::getEnvironmentVariable ("MUTAGEN_TEST_SECRET", {});
            if (fate.isNotEmpty() || room.isNotEmpty())
            {
                if (fate.isNotEmpty()) safe->secretLayer.playFate ((secrets::Fate) fate.getIntValue());
                if (room.isNotEmpty()) safe->secretLayer.openSecretForTest (room.getIntValue());
                return;
            }
           #endif

            if (! safe->storyPanel.progress().introSeen) { safe->introOverlay.open(); safe->introOverlay.grabKeyboardFocus(); }
            else safe->askName();
        });
        storyPanel.onReward = [this] (float w, const juce::String& label)
        {
            scoreSystem.registerInteraction (w, label);
        };
        addAndMakeVisible (inspector);
        addAndMakeVisible (timeline);
        addChildComponent (breedingLab);
        addChildComponent (performance);
        addChildComponent (fxRack);

        addChildComponent (helpView);
        addChildComponent (optionsView);
        addChildComponent (progressionView);

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

        // Observe clicks from nested controls as well as the chamber. Register
        // on top-level children rather than on this component itself, avoiding
        // a duplicate callback when the editor background receives a click.
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* child = getChildComponent (i))
                child->addMouseListener (this, true);
        spontaneousPhantomTicks = hauntedRng.rollInclusive (1200, 3600);

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
        breedingLab.onBreed = [this]
        {
            progressionSystem.record (ProgressionSystem::Action::breedingOperation);
            progressionView.refresh();
        };
        environment.onSteeringUsed = [this]
        {
            progressionSystem.record (ProgressionSystem::Action::steeringIntervention);
            progressionView.refresh();
        };
        progressionView.onClose = [this] { progressionView.setVisible (false); };
        progressionView.onReplayMilestone = [this] (int index)
        {
            triggerMilestoneArtifact (index, true);
        };
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
        setResizeLimits (1060, 740, 2600, 1700);
        setSize (1320, 860);

        lastTimeSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        startTimerHz (40);
    }

    MutagenEditor::~MutagenEditor()
    {
        processor.setRoomTargets (story::RoomTargets::baseline());
        for (const auto& n : encounterNotes) if (n.started) processor.keyboardState.noteOff (1, n.pitch, 0.0f);
        stopTimer();
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* child = getChildComponent (i))
                child->removeMouseListener (this);
        processor.cancelHauntedMicCapture();
        setLookAndFeel (nullptr);
    }

    void MutagenEditor::applyHoverValues (bool enabled)
    {
        std::function<void(juce::Component&)> visit = [&] (juce::Component& c)
        {
            if (auto* slider = dynamic_cast<ParamSlider*> (&c))
                slider->setPopupDisplayEnabled (enabled, enabled, slider->getParentComponent());
            for (auto* child : c.getChildren()) visit (*child);
        };
        visit (*this);
    }

    bool MutagenEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            if (processor.midiLearn.isLearning())
            { processor.midiLearn.cancelLearn(); topBar.setStatus ("MIDI learn cancelled"); return true; }
            if (introOverlay.isVisible() || nameOverlay.isVisible()) return false;
            for (int i = getNumChildComponents() - 1; i >= 0; --i)
            {
                auto* child = getChildComponent (i);
                if (! child->isVisible()) continue;
                if (child == &encounter) return encounter.keyPressed (key);
                if (child == &labGameOverlay) { presentNextLabGame(); return true; }
                if (child == &performance)
                { topBar.closePerformanceView(); topBar.grabKeyboardFocus(); return true; }
                if (child == &fxRack)
                { topBar.closeFxView(); topBar.grabKeyboardFocus(); return true; }
                for (juce::Component* overlay : std::array<juce::Component*, 4> { &optionsView, &helpView,
                        &progressionView, &breedingLab })
                    if (child == overlay)
                    { overlay->setVisible (false); topBar.grabKeyboardFocus(); return true; }
            }
            if (scoreHud.isTableVisible()) { scoreHud.setTableVisible (false); return true; }
            return false;
        }
        auto* focus = juce::Component::getCurrentlyFocusedComponent();
        if (focus != nullptr && (dynamic_cast<juce::TextEditor*> (focus) != nullptr
            || focus->findParentComponentOfClass<juce::TextEditor>() != nullptr)) return false;
        if (introOverlay.isVisible() || nameOverlay.isVisible() || encounter.isVisible()
            || labGameOverlay.isVisible() || helpView.isVisible() || optionsView.isVisible()
            || progressionView.isVisible() || breedingLab.isVisible() || performance.isVisible() || fxRack.isVisible()) return false;
        const auto modifiers = key.getModifiers();
        if (! modifiers.isShiftDown() || (! modifiers.isCtrlDown() && ! modifiers.isCommandDown()) || modifiers.isAltDown()) return false;
        switch (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()))
        {
            case 'R': topBar.randomiseSound(); return true;
            case 'F': topBar.toggleFxView(); return true;
            case 'J': if (gameBar.onJournal) gameBar.onJournal(); return true;
            case 'H': if (topBar.onHelp) topBar.onHelp(); return true;
            case 'M':
            {
                EngineCommand c; c.type = CommandType::mutateAt; c.fa = c.fb = 0.5f; c.fc = 0.2f; c.fd = 0.5f;
                processor.pushCommand (c); return true;
            }
            default: return false;
        }
    }

    void MutagenEditor::mouseDown (const juce::MouseEvent& e)
    {
        handleHiddenClick (e);
    }

    void MutagenEditor::handleHiddenClick (const juce::MouseEvent& e)
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return;

        const auto p = getLocalPoint (e.eventComponent, e.getPosition());
        const int col = juce::jlimit (0, 3, p.x * 4 / juce::jmax (1, getWidth()));
        const int row = juce::jlimit (0, 2, p.y * 3 / juce::jmax (1, getHeight()));
        const int zone = row * 4 + col;
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto result = hiddenClicks.push (zone, now);

        if (result.creatureId >= 0 && now - lastCreatureDiscoverySec > 2.4)
        {
            const bool fresh = progressionSystem.discoverCreature (result.creatureId);
            if (fresh || hauntedRng.rollInclusive (1, 4) == 1)
            {
                lastCreatureDiscoverySec = now;
                triggerCreature (result.creatureId, fresh);
            }
        }

        if (now - lastHiddenEffectSec < 1.25)
            return;

        auto hiddenEvent = [this, now] (const juce::String& text, int recipe, int phantomId)
        {
            lastHiddenEffectSec = now;
            topBar.setStatus (text);
            processor.triggerHauntedSound (recipe, 0.82f);
            phantomManager.launch (phantomId, getScreenBounds(), recipe, 0.82f);
            if (hauntedRng.rollInclusive (1, 4) == 1)
                haunted::systemBeepAsync (120 + (recipe * 37) % 880, 70 + recipe % 180);
        };

        if (result.mirrorEvent)
            hiddenEvent ("MIRROR EVENT: THE LAST GESTURE ARRIVED TWICE", 901, 77);
        else if (result.cornerChoir)
            hiddenEvent ("CORNER CHOIR: FOUR EDGES ANSWER", 902, 33);
        else if (result.panicBloom)
            hiddenEvent ("PANIC BLOOM: TOO MANY HANDS", 903, 91);
        else if (result.mothLooksBack)
            hiddenEvent ("MOTH LOOKS BACK", 904, 66);
        else if (result.visitorFootprint)
            hiddenEvent ("VISITOR FOOTPRINT: INPUT SOURCE UNKNOWN", 905, 99);
    }

    void MutagenEditor::triggerCreature (int creatureId, bool newlyDiscovered)
    {
        const int recipe = 1000 + creatureId * 17;
        processor.triggerHauntedSound (recipe, newlyDiscovered ? 0.9f : 0.58f);
        if (newlyDiscovered)
            storyPanel.collectSoon ("creature " + juce::String (creatureId), 0.3, 3.0f);
        phantomManager.launch (creatureId, getScreenBounds(),
                                       creatureId * 13 + 7,
                                       newlyDiscovered ? 1.0f : 0.68f);

        if (newlyDiscovered)
        {
            topBar.setStatus ("SPECIMEN " + juce::String (creatureId + 1)
                              + "/100: "
                              + juce::String (haunted::creatureName (creatureId)).toUpperCase());
            progressionView.refresh();
        }
        else
        {
            topBar.setStatus ("SOMETHING LEFT THE WINDOW");
        }

        if (hauntedRng.rollInclusive (1, 6) == 1)
            haunted::systemBeepAsync (90 + (creatureId * 29) % 1300,
                                      45 + (creatureId * 11) % 240);
    }

    void MutagenEditor::applyMilestoneSkill (int index)
    {
        EngineCommand a;
        EngineCommand b;
        const float strength = 0.35f + (float) (index % 6) * 0.09f;

        switch ((index - 1) & 7)
        {
            case 0: a.type = CommandType::addEnzyme; break;
            case 1: a.type = CommandType::addCatalyst; break;
            case 2: a.type = CommandType::addHeat; a.fa = 1.0f; break;
            case 3: a.type = CommandType::addHeat; a.fa = -1.0f; break;
            case 4: a.type = CommandType::applySelection; a.fa = strength; break;
            case 5: a.type = CommandType::mutateNow; break;
            case 6:
                a.type = CommandType::knobGesture;
                a.ia = 0; a.fa = index % 2 == 0 ? 0.7f : -0.7f; a.fb = strength;
                break;
            default:
                a.type = CommandType::knobGesture;
                a.ia = 1; a.fa = index % 2 == 0 ? -0.6f : 0.6f; a.fb = strength;
                break;
        }
        processor.pushCommand (a);

        // A second, gentler gesture makes every numbered skill materially
        // distinct without making later milestones progressively destructive.
        b.type = CommandType::knobGesture;
        b.ia = 2;
        b.fa = std::sin ((float) index * 1.618f) * 0.55f;
        b.fb = 0.25f + (float) (index % 7) * 0.07f;
        processor.pushCommand (b);
        scoreSystem.registerInteraction (0.8f, "MILESTONE SKILL");
    }

    void MutagenEditor::triggerMilestoneArtifact (int index, bool replay)
    {
        const auto artifact = haunted::milestoneForScore (
            (juce::int64) index * 1000000 - 1,
            (juce::int64) index * 1000000);
        if (! artifact.has_value())
            return;

        if (! replay)
        {
            progressionSystem.unlockMilestoneArtifact (index);
            progressionView.refresh();
        }

        processor.triggerHauntedSound (artifact->soundRecipe, replay ? 0.72f : 1.12f);
        chamber.triggerReward (std::fmod ((float) index * 0.173f, 1.0f));
        applyMilestoneSkill (index);

        for (int i = 0; i < (replay ? 1 : 3); ++i)
        {
            const int creatureId = (index * 29 + i * 31) % 100;
            phantomManager.launch (creatureId, getScreenBounds(),
                                           artifact->animationRecipe + i * 17,
                                           replay ? 0.72f : 1.18f);
        }

        topBar.setStatus ((replay ? "REPLAY: " : "MILESTONE: ")
                          + juce::String (artifact->title).toUpperCase()
                          + " / " + juce::String (artifact->skillName).toUpperCase());

        if (hauntedRng.rollInclusive (1, 3) == 1)
            haunted::systemBeepAsync (160 + (artifact->soundRecipe % 1400),
                                      90 + (index * 23) % 320);
    }

    void MutagenEditor::serviceHauntedMilestones (juce::int64 beforeScore,
                                                  juce::int64 afterScore)
    {
        if (afterScore <= beforeScore)
            return;

        if (! micReverseRollDone
            && haunted::crossedThreshold (beforeScore, afterScore,
                                          haunted::micReverseThreshold))
        {
            micReverseRollDone = true;
            const int roll = hauntedRng.rollInclusive (1, 10);
            if (haunted::micRollWins (roll) && processor.getTotalNumInputChannels() > 0)
            {
                if (processor.startHauntedMicCapture (5.0f))
                {
                    hiddenMicAwaitingReplay = true;
                    topBar.setStatus ("HIDDEN EVENT: THE LAB IS LISTENING FOR FIVE SECONDS");
                    haunted::systemBeepAsync (220, 80);
                }
            }
        }

        if (! cdTrayRollDone
            && haunted::crossedThreshold (beforeScore, afterScore,
                                          haunted::cdTrayThreshold))
        {
            cdTrayRollDone = true;
            const int roll = hauntedRng.rollInclusive (1, 20);
            if (haunted::cdTrayRollWins (roll))
            {
                cdTrayPulser.start (3);
                processor.triggerHauntedSound (1717, 1.0f);
                topBar.setStatus ("DEVICE EVENT 17: AN UNUSED DOOR OPENS");
            }
        }

        const int first = (int) (beforeScore / 1000000) + 1;
        const int last  = (int) (afterScore / 1000000);
        for (int index = juce::jmax (1, first); index <= last; ++index)
            if (progressionSystem.milestoneArtifacts().count (index) == 0)
                triggerMilestoneArtifact (index, false);
    }

    void MutagenEditor::serviceSpontaneousPhantom()
    {
        if (--spontaneousPhantomTicks > 0)
            return;

        spontaneousPhantomTicks = hauntedRng.rollInclusive (1200, 3600);
        const auto& known = progressionSystem.creatures();
        if (known.empty())
            return;

        int which = hauntedRng.rollInclusive (0, (int) known.size() - 1);
        auto it = known.begin();
        std::advance (it, which);
        triggerCreature (*it, false);
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
        if ((int) *processor.apvts.getRawParameterValue (params::pluginRole) != (int) params::PluginRole::instrument)
        { topBar.setStatus ("Switch Role to Instrument to export the colony."); return; }
        juce::PopupMenu durations;
        durations.addItem (4, "4 seconds"); durations.addItem (8, "8 seconds");
        durations.addItem (12, "12 seconds"); durations.addItem (30, "30 seconds");
        auto safe = juce::Component::SafePointer<MutagenEditor> (this);
        durations.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&topBar), [safe] (int seconds)
        {
            if (safe == nullptr || seconds == 0) return;
            safe->chooser = std::make_unique<juce::FileChooser> ("Render the colony to WAV",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile (safe->processor.organismName + ".wav"), "*.wav");
            safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [safe, seconds] (const juce::FileChooser& fc)
                {
                    if (safe == nullptr || fc.getResult() == juce::File{}) return;
                    safe->topBar.setStatus ("rendering " + juce::String (seconds) + "s ...");
                    RenderEngine::Job job; job.destination = fc.getResult(); job.seconds = (float) seconds;
                    safe->renderEngine.onFinished = [safe] (bool ok, juce::File out)
                    { if (safe != nullptr) safe->topBar.setStatus (ok ? "saved " + out.getFileName() : "render failed"); };
                    safe->renderEngine.start (safe->processor, job);
                });
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
        optionsView.onHoverValuesChanged = [this] (bool on) { applyHoverValues (on); };

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
            encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            pendingGames.clear(); labGameOverlay.setVisible (false);
            storyPanel.restartRun();
            scoreSystem.reset(); playSeconds = 0; lastGameMinuteBucket = 0; lastSeenScore = 0;
            scratchTickets = scratchStreak = diceLossStreak = skillsEarnedThisGame = 0;
            millionChecked = squidsChecked = giantsChecked = false;
            lastDivisibleEventSec = lastCreatureDiscoverySec = lastHiddenEffectSec = -100.0;
            spontaneousPhantomTicks = 0;
            processor.cancelHauntedMicCapture(); hiddenMicAwaitingReplay = false;
            processor.savedRunScore.store (0); processor.savedRunSeconds.store (0);
            processor.savedRunId.store (scoreSystem.currentRunId().getLargeIntValue());
            processor.rollNewWorld();
            scoreSystem.registerInteraction (1.0f, "NEW RUN");
        };

        gameBar.onSaveRun  = [this] { saveRun(); };
        gameBar.onLoadRun  = [this] { loadRun(); };
        gameBar.onScores   = [this] { scoreHud.setTableVisible (! scoreHud.isTableVisible()); };
        gameBar.onJournal  = [this]
        {
            const bool show = ! progressionView.isVisible();
            progressionView.setVisible (show);
            if (show)
            {
                progressionView.refresh();
                progressionView.toFront (true);
            }
        };

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
                progressionSystem.record (ProgressionSystem::Action::sampleDigested);
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
                        progressionSystem.record (ProgressionSystem::Action::savedRun);
                        progressionSystem.flush();
                        progressionView.refresh();
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

                // The score and play time come back with the run, as the README and Help promise.
                scoreSystem.restoreRun ((juce::int64) (double) run.getProperty ("score", 0.0),
                                        (double) run.getProperty ("seconds", 0.0));

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
        const auto revision = processor.runRestoreRevision.load (std::memory_order_acquire);
        if (revision != scoreRestoreRevision)
        {
            scoreRestoreRevision = revision;
            scoreSystem.restoreRun (processor.savedRunScore.load(), processor.savedRunSeconds.load());
            scoreSystem.setRunId (juce::String (processor.savedRunId.load()));
        }
        const auto runScoreBefore = scoreSystem.score();
        scoreSystem.update (snapshot, dt);
        const auto runScoreAfter = scoreSystem.score();
        progressionSystem.observe (snapshot, scoreSystem);
        storyPanel.tick (dt, breedingLab.isVisible() || performance.isVisible()
                               || helpView.isVisible() || encounter.isVisible() || labGameOverlay.isVisible()
                               || introOverlay.isVisible() || nameOverlay.isVisible());
        serviceHauntedMilestones (runScoreBefore, runScoreAfter);
        const bool playPaused = breedingLab.isVisible() || performance.isVisible()
            || helpView.isVisible() || optionsView.isVisible() || progressionView.isVisible()
            || labGameOverlay.isVisible() || encounter.isVisible()
            || introOverlay.isVisible() || nameOverlay.isVisible();
        if (! playPaused) playSeconds += dt;
        if (! cameraSynthDiscovered && playSeconds >= 1800.0)
        {
            cameraSynthDiscovered = true;
            progressionSystem.unlockWebcamSynth();
            cameraSynthLaunch.setVisible (true);
            storyFx.banner ("CHROMATIC PORTAL UNLOCKED", "Move to make music. Webcam optional.",
                            juce::Colours::cyan);
        }
        serviceEncounters (dt, playPaused);
        secretLayer.update (dt, scoreSystem.score(), snapshot.generation, snapshot.population,
                            introOverlay.isVisible() || nameOverlay.isVisible()
                            || breedingLab.isVisible() || performance.isVisible());
        // Compare against the score seen last tick, so bonuses applied between
        // ticks (skills, fates, secrets) still count as crossings.
        if (! playPaused) serviceLabGameSchedule (lastSeenScore, scoreSystem.score());
        lastSeenScore = scoreSystem.score();
        scoreHud.setState (scoreSystem, snapshot);
        scoreHud.setKnowledge ((juce::int64) storyPanel.progress().knowledge);
        processor.setMorphScore ((float) (std::log10 (1.0 + (double) juce::jmax ((juce::int64) 0, scoreSystem.score())) / 9.0));
        processor.savedRunId.store (scoreSystem.currentRunId().getLargeIntValue());
        processor.savedRunScore.store (scoreSystem.score());
        processor.savedRunSeconds.store (scoreSystem.elapsed());
        gameBar.tick ((float) dt);
        gameBar.setStatus (processor.micArmed(), processor.micCapturing(),
                           processor.micLevel(), processor.entropyTapLevel(),
                           processor.entropyTapLive());

        // A RADIATE landed on the audio thread; react exactly once per result.
        for (int outcome = 0; processor.nextRadiation (lastRadiationCounter, outcome);)
        {
            scoreSystem.onRadiation (outcome);
            progressionSystem.record (ProgressionSystem::Action::radiationExposure);
            gameBar.flashRadiation (outcome);
        }

        // A microphone capture finished, or the guard killed one. Hidden
        // milestone captures are consumed by their own reverse/stretch path
        // and are never digested into the colony.
        if (hiddenMicAwaitingReplay)
        {
            if (processor.pollHauntedMicCapture())
            {
                hiddenMicAwaitingReplay = false;
                topBar.setStatus ("HIDDEN EVENT: YOUR FIVE SECONDS CAME BACK WRONG");
                phantomManager.launch (38, getScreenBounds(), 3854, 1.0f);
                haunted::systemBeepAsync (185, 110);
            }
            else if (processor.pollMicAbort())
            {
                hiddenMicAwaitingReplay = false;
                processor.cancelHauntedMicCapture();
                scoreSystem.registerInteraction (0.0f, "HIDDEN MIC ABORTED - FEEDBACK GUARD");
                topBar.setStatus ("THE LAB STOPPED LISTENING");
            }
        }
        else
        {
            if (processor.pollMicCapture())
            {
                scoreSystem.onSampleDigested ("mic");
                progressionSystem.record (ProgressionSystem::Action::sampleDigested);
            }
            if (processor.pollMicAbort())
                scoreSystem.registerInteraction (0.0f, "FEEDBACK - CAPTURE ABORTED");
        }

        // The reward. ScoreSystem decides when; the chamber draws it.
        if (scoreSystem.consumeRewardFlash())
            chamber.triggerReward (snapshot.worldHue);

        progressionFlushAccum += dt;
        if (progressionFlushAccum >= 5.0)
        {
            progressionFlushAccum = 0.0;
            progressionSystem.flush();
            if (progressionView.isVisible()) progressionView.refresh();
        }
        if (const auto notice = progressionSystem.consumeNotice(); notice.isNotEmpty())
            topBar.setStatus (notice);

        serviceSpontaneousPhantom();

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

        // The narrator strip sits under the chamber rather than over it, so it
        // never steals a click meant for the colony.
        storyPanel.setBounds (mid.removeFromBottom (100));
        mid.removeFromBottom (6);

        chamber.setBounds (mid);
        scoreHud.setBounds (chamber.getBounds());
        scoreHud.toFront (false);
        secretLayer.setBounds (chamber.getBounds());
        secretLayer.toFront (false);
        storyFx.setBounds (chamber.getBounds());
        storyFx.toFront (false);
        if (cameraSynthDiscovered) cameraSynthLaunch.setVisible (true);
        // Whatever modal layer is open stays above the secret layer.
        for (juce::Component* c : { (juce::Component*) &labGameOverlay, (juce::Component*) &introOverlay, (juce::Component*) &nameOverlay })
            if (c->isVisible()) c->toFront (false);
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
        introOverlay.setBounds (getLocalBounds());
        nameOverlay.setBounds (getLocalBounds());
        cameraSynthLaunch.setBounds (chamber.getBounds().removeFromTop (36).removeFromRight (175));
        cameraSynth.setBounds (overlay.reduced (24));
        if (cameraSynth.isVisible()) cameraSynth.toFront (false);
        labGameOverlay.setBounds (chamber.getBounds().withSizeKeepingCentre (juce::jmin (520, chamber.getWidth()), juce::jmin (300, chamber.getHeight())));
        encounter.setBounds (overlay.reduced (24));
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
        progressionView.setBounds (centred (1080, 900));
    }

    void MutagenEditor::askName()
    {
        nameOverlay.open (juce::String (juce::CharPointer_UTF8 (storyPanel.progress().playerName.c_str())));
    }

    void MutagenEditor::startGameAs (const juce::String& name)
    {
        storyPanel.setPlayerName (name);

        // A name of exactly five letters: +3,904 points and three skills.
        if (name.length() == 5 && name.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))
        {
            scoreSystem.adjustScore (3904, "FIVE-LETTER NAME");
            for (int i = 0; i < 3; ++i) awardGameSkill (hauntedRng.rollInclusive (0, 4));
        }
        const auto fate = secrets::fateForName (name.toStdString(), (std::uint64_t) juce::Random::getSystemRandom().nextInt64());
        if (fate != secrets::Fate::none)
            juce::Timer::callAfterDelay (1200, [safe = juce::Component::SafePointer<MutagenEditor> (this), fate]
            {
                if (safe != nullptr) safe->secretLayer.playFate (fate);
            });
    }
}
