#include "TopBar.h"
#include "../PluginProcessor.h"
#include "../AppOptions.h"

namespace mutagen
{
    using namespace theme;

    // =====================================================================
    //  IconButton
    // =====================================================================

    void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        if (over || down)
        {
            g.setColour (down ? panelHi : panel);
            g.fillRoundedRectangle (r, radiusControl);
        }

        // The house hover rule: brighten, do not recolour.
        auto c = over ? tint.brighter (0.25f) : tint;
        if (! isEnabled()) c = c.withAlpha (0.4f);
        g.setColour (c);

        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;

        switch (icon)
        {
            case Icon::gear:
            {
                juce::Path p;
                const float rOuter = s * 0.82f, rInner = s * 0.56f;
                constexpr int teeth = 8;
                for (int i = 0; i < teeth * 2; ++i)
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) i / (teeth * 2.0f);
                    const float rr = (i % 2 == 0) ? rOuter : rInner;
                    const auto pt = juce::Point<float> (cx + std::cos (a) * rr,
                                                        cy + std::sin (a) * rr);
                    if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                }
                p.closeSubPath();
                g.fillPath (p);

                // hub punched back out to the background, so the gear reads as
                // a ring rather than as a blob at 16px
                g.setColour (over ? panel : bg1);
                g.fillEllipse (cx - s * 0.26f, cy - s * 0.26f, s * 0.52f, s * 0.52f);
                break;
            }

            case Icon::question:
            {
                g.setFont (uiFont (s * 1.5f, true));
                g.drawText ("?", getLocalBounds(), juce::Justification::centred);
                break;
            }

            case Icon::chevronLeft:
            case Icon::chevronRight:
            {
                const float dir = (icon == Icon::chevronLeft) ? -1.0f : 1.0f;
                juce::Path p;
                p.startNewSubPath (cx - dir * s * 0.28f, cy - s * 0.42f);
                p.lineTo          (cx + dir * s * 0.28f, cy);
                p.lineTo          (cx - dir * s * 0.28f, cy + s * 0.42f);
                g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
                break;
            }

            case Icon::dice:
            {
                g.drawRoundedRectangle (cx - s * 0.6f, cy - s * 0.6f, s * 1.2f, s * 1.2f,
                                        2.0f, 1.4f);
                for (auto o : { juce::Point<float> (-0.28f, -0.28f),
                                juce::Point<float> ( 0.28f,  0.28f),
                                juce::Point<float> ( 0.0f,   0.0f) })
                    g.fillEllipse (cx + o.x * s - 1.3f, cy + o.y * s - 1.3f, 2.6f, 2.6f);
                break;
            }

            case Icon::disk:
            {
                g.drawRoundedRectangle (cx - s * 0.62f, cy - s * 0.62f, s * 1.24f, s * 1.24f,
                                        2.0f, 1.4f);
                g.fillRect (cx - s * 0.3f, cy + s * 0.05f, s * 0.6f, s * 0.45f);
                break;
            }
        }
    }

    // =====================================================================
    //  TopBar
    // =====================================================================

    namespace
    {
        constexpr int firstFactoryId = 1;
        constexpr int firstUserId    = 1000;

        enum FileMenuIds
        {
            mSavePreset = 1, mSavePresetAs, mOpenPreset, mRevealPresets,
            mSaveRun, mLoadRun,
            mExportAudio,
            mRandomise, mReset,
            mOptions, mHelp
        };
    }

    TopBar::TopBar (MutagenProcessor& p) : processor (p)
    {
        // ---- identity ---------------------------------------------------
        wordmark.setText ("MUTAGEN", juce::dontSendNotification);
        wordmark.setFont (uiFont (14.0f, true));
        wordmark.setColour (juce::Label::textColourId, text);
        wordmark.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (wordmark);

        nameLabel.setText (processor.organismName, juce::dontSendNotification);
        nameLabel.setEditable (true);
        nameLabel.setFont (uiFont (13.0f));
        nameLabel.setColour (juce::Label::textColourId, accent);
        nameLabel.setTooltip ("The name of this organism. Click to rename it; the name "
                              "travels with the run when you save it.");
        nameLabel.onTextChange = [this]
        {
            processor.organismName = nameLabel.getText().isNotEmpty() ? nameLabel.getText() : "MUTAGEN";
        };
        addAndMakeVisible (nameLabel);

        statusLabel.setFont (monoFont (11.0f));
        statusLabel.setColour (juce::Label::textColourId, spectral);
        statusLabel.setJustificationType (juce::Justification::centredRight);
        statusLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (statusLabel);

        statsLabel.setColour (juce::Label::textColourId, textDim);
        statsLabel.setFont (monoFont (11.0f));
        statsLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statsLabel);

        // ---- header strip ------------------------------------------------
        fileBtn.setTooltip ("Presets, runs, audio export, options and help");
        fileBtn.onClick = [this] { showFileMenu(); };
        addAndMakeVisible (fileBtn);

        presetBox.setTooltip ("The preset bank. A preset is the environment the colony "
                              "lives in - it does not replace the colony itself.");
        presetBox.setTextWhenNothingSelected ("Init");
        presetBox.onChange = [this] { presetSelected(); };
        addAndMakeVisible (presetBox);

        prevPreset.setTooltip ("Previous preset");
        nextPreset.setTooltip ("Next preset");
        prevPreset.onClick = [this] { stepPreset (-1); };
        nextPreset.onClick = [this] { stepPreset (+1); };
        addAndMakeVisible (prevPreset);
        addAndMakeVisible (nextPreset);

        aBtn.setClickingTogglesState (true);
        bBtn.setClickingTogglesState (true);
        aBtn.setToggleState (true, juce::dontSendNotification);
        aBtn.getProperties().set ("tint", (int) accent2.getARGB());
        bBtn.getProperties().set ("tint", (int) accent2.getARGB());
        aBtn.setTooltip ("Compare slot A. Switching slots keeps the colony and swaps "
                         "every parameter around it.");
        bBtn.setTooltip ("Compare slot B. The first switch copies A into B, so you are "
                         "always comparing against something.");
        copyBtn.setTooltip ("Copy the slot you are in over the other one");
        aBtn.onClick    = [this] { toggleAB (false); };
        bBtn.onClick    = [this] { toggleAB (true); };
        copyBtn.onClick = [this] { copyAcross(); };
        addAndMakeVisible (aBtn);
        addAndMakeVisible (bBtn);
        addAndMakeVisible (copyBtn);

        optionsBtn.setTooltip ("Options: tooltips, MIDI mappings, audio and MIDI devices");
        optionsBtn.onClick = [this] { if (onOptions) onOptions(); };
        addAndMakeVisible (optionsBtn);

        helpBtn.tint = accent2;
        helpBtn.setTooltip ("The manual: every control, the workflow, and the version number");
        helpBtn.onClick = [this] { if (onHelp) onHelp(); };
        addAndMakeVisible (helpBtn);

        // ---- verb strip ----------------------------------------------------
        cpuBox.addItemList (params::cpuQualityChoices(), 1);
        roleBox.addItemList (params::pluginRoleChoices(), 1);
        cpuBox.setTooltip ("How much CPU the colony may spend. Higher settings allow a "
                           "larger population and more partials per cell.");
        roleBox.setTooltip ("Instrument plays from MIDI, Effect feeds on the incoming "
                            "audio, Hybrid does both.");
        paramMenu::tag (cpuBox,  params::cpuQuality);
        paramMenu::tag (roleBox, params::pluginRole);
        addAndMakeVisible (cpuBox);
        addAndMakeVisible (roleBox);
        cpuAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, params::cpuQuality, cpuBox);
        roleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, params::pluginRole, roleBox);

        exploreToggle.setClickingTogglesState (true);
        exploreToggle.getProperties().set ("tint", (int) resonator.getARGB());
        exploreToggle.setTooltip ("EXPLORING lets the colony keep evolving. PRESERVED "
                                  "freezes evolution and holds the sound where it is.");
        paramMenu::tag (exploreToggle, params::exploreMode);
        exploreToggle.onStateChange = [this]
        {
            exploreToggle.setButtonText (exploreToggle.getToggleState() ? "EXPLORING" : "PRESERVED");
        };
        addAndMakeVisible (exploreToggle);
        exploreAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::exploreMode, exploreToggle);

        auto setParam = [this] (const char* id, float v)
        {
            if (auto* param = processor.apvts.getParameter (id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (v);
                param->endChangeGesture();
            }
        };

        for (auto* b : { &cloneBtn, &freezeBtn, &reanimateBtn, &exportBtn, &randomBtn, &resetBtn,
                         &perfToggle, &fxToggle, &inspToggle })
            addAndMakeVisible (b);

        cloneBtn.setTooltip ("Branch the evolution history here, so this colony becomes "
                             "the parent of a new line you can return to.");
        freezeBtn.setTooltip ("Stop evolving and hold the current sound (same as PRESERVED)");
        reanimateBtn.setTooltip ("Start evolving again (same as EXPLORING)");
        exportBtn.setTooltip ("Record what the colony plays next into a WAV file");
        randomBtn.setTooltip ("A completely new set of settings. Every press after the "
                              "first resets first, so you get a new world rather than a "
                              "drift away from the last random one.");
        resetBtn.setTooltip ("Every parameter, the colony, the source and the MIDI "
                             "mappings back to their defaults.");

        cloneBtn.onClick     = [this] { if (onClone) onClone(); };
        freezeBtn.onClick    = [setParam] { setParam (params::exploreMode, 0.0f); };
        reanimateBtn.onClick = [setParam] { setParam (params::exploreMode, 1.0f); };
        exportBtn.onClick    = [this] { if (onRender) onRender(); };

        randomBtn.getProperties().set ("tint", (int) spectralV.getARGB());
        randomBtn.onClick = [this] { doRandomise(); };

        resetBtn.getProperties().set ("tint", (int) infection.getARGB());
        resetBtn.onClick = [this]
        {
            if (onReset) onReset();
            refreshPresets();
            setStatus ("reset to defaults");
        };

        perfToggle.setClickingTogglesState (true);
        perfToggle.getProperties().set ("tint", (int) spectralV.getARGB());
        perfToggle.setTooltip ("The performance page: macros, the XY pad and the keyboard");
        perfToggle.onClick = [this]
        {
            if (perfToggle.getToggleState()) { fxToggle.setToggleState (false, juce::dontSendNotification); if (onFxToggled) onFxToggled (false); }
            if (onPerformanceToggled) onPerformanceToggled (perfToggle.getToggleState());
        };

        fxToggle.setClickingTogglesState (true);
        fxToggle.getProperties().set ("tint", (int) infection.getARGB());
        fxToggle.setTooltip ("The post-colony rack: oscillators, filter, LFOs, EQ, gator, glitch");
        fxToggle.onClick = [this]
        {
            if (fxToggle.getToggleState()) { perfToggle.setToggleState (false, juce::dontSendNotification); if (onPerformanceToggled) onPerformanceToggled (false); }
            if (onFxToggled) onFxToggled (fxToggle.getToggleState());
        };

        inspToggle.setClickingTogglesState (true);
        inspToggle.setToggleState (true, juce::dontSendNotification);
        inspToggle.getProperties().set ("tint", (int) spectral.getARGB());
        inspToggle.setTooltip ("Show or hide the genome inspector");
        inspToggle.onClick = [this] { if (onInspectorToggled) onInspectorToggled (inspToggle.getToggleState()); };

        refreshPresets();
        startTimerHz (4);
    }

    TopBar::~TopBar() { stopTimer(); }

    // =====================================================================
    //  Presets
    // =====================================================================

    void TopBar::refreshPresets()
    {
        presetBox.onChange = nullptr;          // rebuilding is not a user choice
        presetBox.clear (juce::dontSendNotification);

        const auto factory = processor.presets.factoryNames();
        juce::String lastCategory;
        for (int i = 0; i < factory.size(); ++i)
        {
            const auto category = factory[i].upToFirstOccurrenceOf (":", false, false);
            if (category != lastCategory)
            {
                presetBox.addSectionHeading (category.toUpperCase());
                lastCategory = category;
            }
            presetBox.addItem (factory[i].fromFirstOccurrenceOf (": ", false, false),
                               firstFactoryId + i);
        }

        userNames = processor.presets.userPresetNames();
        if (! userNames.isEmpty())
        {
            presetBox.addSectionHeading ("MY PRESETS");
            for (int i = 0; i < userNames.size(); ++i)
                presetBox.addItem (userNames[i], firstUserId + i);
        }

        // Show what is actually loaded. A user preset and a factory preset can
        // share a name, so the file is checked first.
        const auto currentName = processor.presets.currentName();
        int id = 0;
        if (processor.presets.currentFile() != juce::File())
            id = firstUserId + userNames.indexOf (currentName);
        else
            for (int i = 0; i < factory.size(); ++i)
                if (factory[i].fromFirstOccurrenceOf (": ", false, false) == currentName)
                    id = firstFactoryId + i;

        if (id >= firstFactoryId)
            presetBox.setSelectedId (id, juce::dontSendNotification);
        else
            presetBox.setText (currentName, juce::dontSendNotification);

        presetBox.onChange = [this] { presetSelected(); };
    }

    void TopBar::presetSelected()
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;

        if (id >= firstUserId)
        {
            const int index = id - firstUserId;
            if (! juce::isPositiveAndBelow (index, userNames.size())) return;

            const auto f = PresetManager::userPresetDirectory()
                               .getChildFile (userNames[index] + PresetManager::fileExtension);
            if (processor.presets.loadFromFile (f))
                setStatus ("loaded " + userNames[index]);
            else
                setStatus ("could not read that preset");
        }
        else
        {
            processor.presets.loadFactory (id - firstFactoryId);
            setStatus ("loaded " + processor.presets.currentName());
        }
    }

    void TopBar::stepPreset (int delta)
    {
        // Walk the combo's own item list so the section headings, which have
        // no ID, are stepped over rather than landed on.
        const int n = presetBox.getNumItems();
        if (n == 0) return;

        int index = presetBox.indexOfItemId (presetBox.getSelectedId());
        if (index < 0) index = (delta > 0) ? -1 : n;

        index = juce::jlimit (0, n - 1, index + delta);
        presetBox.setSelectedItemIndex (index, juce::sendNotificationSync);
    }

    void TopBar::savePreset()
    {
        if (processor.presets.saveCurrent())
        {
            setStatus ("saved " + processor.presets.currentName());
            refreshPresets();
        }
        else
        {
            savePresetAs();          // nothing to save over yet
        }
    }

    void TopBar::savePresetAs()
    {
        auto dir = PresetManager::userPresetDirectory();
        dir.createDirectory();

        chooser = std::make_unique<juce::FileChooser> (
            "Save this environment as a preset",
            dir.getChildFile (processor.presets.currentName() + PresetManager::fileExtension),
            PresetManager::wildcard);

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File()) return;

                const auto name = f.getFileNameWithoutExtension();
                if (processor.presets.saveToFile (f.withFileExtension (PresetManager::fileExtension),
                                                  name))
                {
                    refreshPresets();
                    setStatus ("saved " + name);
                }
                else
                {
                    setStatus ("could not save that preset");
                }
            });
    }

    void TopBar::openPreset()
    {
        auto dir = PresetManager::userPresetDirectory();
        dir.createDirectory();

        chooser = std::make_unique<juce::FileChooser> ("Open a preset", dir,
                                                       PresetManager::wildcard);

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File() || ! f.existsAsFile()) return;

                if (processor.presets.loadFromFile (f))
                {
                    refreshPresets();
                    setStatus ("loaded " + f.getFileNameWithoutExtension());
                }
                else
                {
                    setStatus ("that is not a MUTAGEN preset");
                }
            });
    }

    void TopBar::doRandomise()
    {
        processor.presets.randomise();
        presetBox.setText ("Random", juce::dontSendNotification);
        setStatus ("randomised");
    }

    // =====================================================================
    //  A / B
    // =====================================================================

    void TopBar::toggleAB (bool wantB)
    {
        if (wantB == onSlotB)
        {
            // Clicking the slot you are already in should not look like it did
            // nothing, so put the toggle back rather than leaving both lit.
            aBtn.setToggleState (! wantB, juce::dontSendNotification);
            bBtn.setToggleState (wantB, juce::dontSendNotification);
            return;
        }

        (onSlotB ? slotB : slotA) = processor.apvts.copyState();

        auto& target = wantB ? slotB : slotA;
        if (! target.isValid())
            target = processor.apvts.copyState();   // first visit: seed from here
        else
            processor.apvts.replaceState (target.createCopy());

        onSlotB = wantB;
        aBtn.setToggleState (! wantB, juce::dontSendNotification);
        bBtn.setToggleState (wantB, juce::dontSendNotification);
        setStatus (wantB ? "slot B" : "slot A");
    }

    void TopBar::copyAcross()
    {
        const auto here = processor.apvts.copyState();
        (onSlotB ? slotA : slotB) = here;
        (onSlotB ? slotB : slotA) = here;
        setStatus (onSlotB ? "B copied to A" : "A copied to B");
    }

    // =====================================================================
    //  The file menu
    // =====================================================================

    void TopBar::showFileMenu()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());

        m.addSectionHeader ("PRESET");
        m.addItem (mSavePreset, "Save" + (processor.presets.currentFile() != juce::File()
                                              ? " \"" + processor.presets.currentName() + "\""
                                              : juce::String()));
        m.addItem (mSavePresetAs, "Save As...");
        m.addItem (mOpenPreset,   "Open...");
        m.addItem (mRevealPresets, "Show Preset Folder");

        m.addSeparator();
        m.addSectionHeader ("RUN");
        m.addItem (mSaveRun, "Save Run (colony + score)...");
        m.addItem (mLoadRun, "Open Run...");

        m.addSeparator();
        m.addItem (mExportAudio, "Export Audio to WAV...");

        m.addSeparator();
        m.addItem (mRandomise, "Randomise Everything");
        m.addItem (mReset,     "Reset to Defaults");

        m.addSeparator();
        m.addItem (mOptions, "Options...");
        m.addItem (mHelp,    "Help / Manual");

        juce::Component::SafePointer<TopBar> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn),
            [safe] (int result)
            {
                auto* self = safe.getComponent();
                if (self == nullptr || result == 0) return;

                switch (result)
                {
                    case mSavePreset:    self->savePreset(); break;
                    case mSavePresetAs:  self->savePresetAs(); break;
                    case mOpenPreset:    self->openPreset(); break;
                    case mRevealPresets:
                    {
                        auto dir = PresetManager::userPresetDirectory();
                        dir.createDirectory();
                        dir.revealToUser();
                        break;
                    }
                    case mSaveRun:     if (self->onSaveRun) self->onSaveRun(); break;
                    case mLoadRun:     if (self->onLoadRun) self->onLoadRun(); break;
                    case mExportAudio: if (self->onRender)  self->onRender();  break;
                    case mRandomise:   self->doRandomise(); break;
                    case mReset:
                        if (self->onReset) self->onReset();
                        self->refreshPresets();
                        self->setStatus ("reset to defaults");
                        break;
                    case mOptions:     if (self->onOptions) self->onOptions(); break;
                    case mHelp:        if (self->onHelp)    self->onHelp();    break;
                    default: break;
                }
            });
    }

    // =====================================================================

    void TopBar::setStatus (const juce::String& s)
    {
        statusLabel.setText (s, juce::dontSendNotification);
        statusClearAt = juce::Time::getMillisecondCounterHiRes() * 0.001 + 4.0;
    }

    void TopBar::timerCallback()
    {
        if (statusClearAt > 0.0
            && juce::Time::getMillisecondCounterHiRes() * 0.001 > statusClearAt)
        {
            statusClearAt = 0.0;
            statusLabel.setText ({}, juce::dontSendNotification);
        }

        // The MIDI learn armed by a right-click is invisible otherwise: the
        // menu has closed and the plugin is silently waiting for a controller.
        if (processor.midiLearn.isLearning())
        {
            statusLabel.setColour (juce::Label::textColourId, warning);
            statusLabel.setText ("move a MIDI controller to map it...", juce::dontSendNotification);
            statusClearAt = 0.0;
        }
        else if (statusLabel.findColour (juce::Label::textColourId) == warning)
        {
            statusLabel.setColour (juce::Label::textColourId, spectral);
            setStatus ("mapped");
        }
    }

    void TopBar::setStats (const EngineSnapshot& s)
    {
        juce::String seedHex (juce::String::toHexString ((juce::int64) s.seed).toUpperCase());
        statsLabel.setText (
            "GEN " + juce::String (s.generation)
            + "   POP " + juce::String (s.population)
            + " (" + juce::String (s.popBySpecies[0]) + "/" + juce::String (s.popBySpecies[1])
            + "/" + juce::String (s.popBySpecies[2]) + ")"
            + "   SEED 0x" + seedHex
            + "   DIV " + juce::String (s.diversity, 2),
            juce::dontSendNotification);
    }

    // =====================================================================

    void TopBar::paint (juce::Graphics& g)
    {
        auto header = getLocalBounds().removeFromTop (headerRow);

        g.setColour (panel);
        g.fillRect (header);

        auto verbs = getLocalBounds().withTrimmedTop (headerRow);
        juce::ColourGradient grad (bg1, verbs.getTopLeft().toFloat(),
                                   bg0, verbs.getBottomLeft().toFloat(), false);
        g.setGradientFill (grad);
        g.fillRect (verbs);

        g.setColour (stroke);
        g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());
        g.drawHorizontalLine (getHeight() - 1,        0.0f, (float) getWidth());

        // The living dot: green while the colony is exploring, dim when it is
        // being preserved. It sits inside the header's left padding.
        const bool alive = exploreToggle.getToggleState();
        g.setColour (alive ? success : textDim.withAlpha (0.5f));
        g.fillEllipse (edgePadding - 3.0f, headerRow * 0.5f - 3.0f, 6.0f, 6.0f);

        drawSignatureNotch (g, getLocalBounds());
    }

    void TopBar::resized()
    {
        // ---- header strip ------------------------------------------------
        auto header = getLocalBounds().removeFromTop (headerRow);
        header.reduce (edgePadding, 3);
        header.removeFromLeft (10);                     // clear of the living dot

        wordmark.setBounds (header.removeFromLeft (84));
        header.removeFromLeft (gridUnit);
        nameLabel.setBounds (header.removeFromLeft (160));

        helpBtn.setBounds    (header.removeFromRight (26));
        header.removeFromRight (2);
        optionsBtn.setBounds (header.removeFromRight (26));
        header.removeFromRight (gridUnit);

        copyBtn.setBounds (header.removeFromRight (42));
        bBtn.setBounds    (header.removeFromRight (26));
        aBtn.setBounds    (header.removeFromRight (26));
        header.removeFromRight (gridUnit);

        nextPreset.setBounds (header.removeFromRight (20));
        prevPreset.setBounds (header.removeFromRight (20));
        presetBox.setBounds  (header.removeFromRight (196));
        header.removeFromRight (gridUnit / 2);
        fileBtn.setBounds    (header.removeFromRight (56));
        header.removeFromRight (gridUnit);

        statusLabel.setBounds (header);

        // ---- verb strip ---------------------------------------------------
        auto r = getLocalBounds().withTrimmedTop (headerRow).reduced (edgePadding, 4);

        auto row = r.removeFromRight (566);
        resetBtn.setBounds     (row.removeFromRight (56));
        row.removeFromRight (4);
        randomBtn.setBounds    (row.removeFromRight (66));
        row.removeFromRight (gridUnit);
        exportBtn.setBounds    (row.removeFromRight (56));
        row.removeFromRight (4);
        reanimateBtn.setBounds (row.removeFromRight (74));
        row.removeFromRight (4);
        freezeBtn.setBounds    (row.removeFromRight (54));
        row.removeFromRight (4);
        cloneBtn.setBounds     (row.removeFromRight (48));
        row.removeFromRight (gridUnit);
        perfToggle.setBounds   (row.removeFromRight (62));
        row.removeFromRight (4);
        fxToggle.setBounds     (row.removeFromRight (36));
        row.removeFromRight (4);
        inspToggle.setBounds   (row.removeFromRight (70));

        roleBox.setBounds (r.removeFromLeft (92).withSizeKeepingCentre (92, 22));
        r.removeFromLeft (4);
        cpuBox.setBounds  (r.removeFromLeft (92).withSizeKeepingCentre (92, 22));
        r.removeFromLeft (gridUnit);
        exploreToggle.setBounds (r.removeFromLeft (88).withSizeKeepingCentre (88, 22));
        r.removeFromLeft (gridUnit);
        statsLabel.setBounds (r);
    }
}
