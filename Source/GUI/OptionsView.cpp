#include "OptionsView.h"
#include "../PluginProcessor.h"
#include "../AppOptions.h"
#include "DeviceSetupHook.h"

namespace mutagen
{
    using namespace theme;

    OptionsView::OptionsView (MutagenProcessor& p) : processor (p)
    {
        auto& opts = AppOptions::get();

        tooltipsToggle.setToggleState (opts.tooltipsEnabled(), juce::dontSendNotification);
        tooltipsToggle.setTooltip ("Turn the hover explanations on or off");
        tooltipsToggle.onClick = [this]
        {
            const bool on = tooltipsToggle.getToggleState();
            AppOptions::get().setTooltipsEnabled (on);
            if (onTooltipsChanged) onTooltipsChanged (on);
        };
        addAndMakeVisible (tooltipsToggle);

        hoverValueToggle.setToggleState (opts.showValueOnHover(), juce::dontSendNotification);
        hoverValueToggle.setTooltip ("Show a knob's value while you hover or drag it");
        hoverValueToggle.onClick = [this]
        {
            AppOptions::get().setShowValueOnHover (hoverValueToggle.getToggleState());
        };
        addAndMakeVisible (hoverValueToggle);

        midiMapLabel.setColour (juce::Label::textColourId, textDim);
        midiMapLabel.setFont (uiFont (12.0f));
        addAndMakeVisible (midiMapLabel);

        clearMidiBtn.setTooltip ("Forget every MIDI CC mapping in this instance");
        clearMidiBtn.onClick = [this]
        {
            processor.midiLearn.clearAll();
            refresh();
        };
        addAndMakeVisible (clearMidiBtn);

        presetFolderLabel.setColour (juce::Label::textColourId, textDim);
        presetFolderLabel.setFont (monoFont (11.0f));
        presetFolderLabel.setText (PresetManager::userPresetDirectory().getFullPathName(),
                                   juce::dontSendNotification);
        addAndMakeVisible (presetFolderLabel);

        revealPresetsBtn.setTooltip ("Open the folder your saved presets live in");
        revealPresetsBtn.onClick = []
        {
            auto dir = PresetManager::userPresetDirectory();
            dir.createDirectory();
            dir.revealToUser();
        };
        addAndMakeVisible (revealPresetsBtn);

        deviceHeading.setColour (juce::Label::textColourId, text);
        deviceHeading.setFont (uiFont (11.0f, true));
        deviceHeading.setText ("AUDIO & MIDI", juce::dontSendNotification);
        addAndMakeVisible (deviceHeading);

        deviceNote.setColour (juce::Label::textColourId, textDim);
        deviceNote.setFont (uiFont (12.0f));
        deviceNote.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (deviceNote);

        deviceSelector = deviceSetup::create();

        if (deviceSelector != nullptr)
        {
            addAndMakeVisible (*deviceSelector);
            deviceNote.setText ({}, juce::dontSendNotification);
        }
        else
        {
            deviceNote.setText (
                "MUTAGEN is running inside a host, which owns the soundcard and the "
                "MIDI routing. Change the audio device, sample rate, buffer size and "
                "MIDI inputs in your host's own audio settings.\n\n"
                "These controls appear here when you run the MUTAGEN standalone app.",
                juce::dontSendNotification);
        }

        closeBtn.setTooltip ("Close the options page");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeBtn);

        refresh();
    }

    OptionsView::~OptionsView() = default;

    void OptionsView::refresh()
    {
        const int n = processor.midiLearn.mappingCount();
        midiMapLabel.setText (n == 0 ? "No MIDI mappings. Right-click any control to make one."
                                     : juce::String (n) + (n == 1 ? " parameter is mapped to a MIDI CC."
                                                                  : " parameters are mapped to MIDI CCs."),
                              juce::dontSendNotification);
        clearMidiBtn.setEnabled (n > 0);

        tooltipsToggle.setToggleState (AppOptions::get().tooltipsEnabled(),
                                       juce::dontSendNotification);
        hoverValueToggle.setToggleState (AppOptions::get().showValueOnHover(),
                                         juce::dontSendNotification);
    }

    void OptionsView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();

        juce::DropShadow (shadow, 16, { 0, 4 }).drawForRectangle (g, getLocalBounds());

        g.setColour (bg0);
        g.fillRoundedRectangle (r, radiusWindow);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radiusWindow, 1.0f);

        auto header = getLocalBounds().removeFromTop (headerHeight);
        g.setColour (panel);
        g.fillRect (header);
        g.setColour (stroke);
        g.drawHorizontalLine (header.getBottom() - 1, r.getX(), r.getRight());

        g.setColour (text);
        g.setFont (uiFont (14.0f, true));
        g.drawText ("Options", header.withTrimmedLeft (edgePadding),
                    juce::Justification::centredLeft);

        // Section rules, drawn where resized() put the gaps.
        auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (edgePadding, gridUnit);
        g.setColour (stroke);
        for (int y : sectionRules)
            g.drawHorizontalLine (y, (float) body.getX(), (float) body.getRight());

        auto footer = getLocalBounds().removeFromBottom (24).reduced (edgePadding, 0);
        g.setColour (textDim);
        g.setFont (monoFont (9.0f));
        g.drawText ("v" + juce::String (JucePlugin_VersionString), footer,
                    juce::Justification::centredRight);

        drawSignatureNotch (g, getLocalBounds());
    }

    void OptionsView::resized()
    {
        sectionRules.clear();

        auto r = getLocalBounds();
        auto header = r.removeFromTop (headerHeight);
        closeBtn.setBounds (header.removeFromRight (72).reduced (gridUnit, 3));

        r.removeFromBottom (24);
        r = r.reduced (edgePadding, gridUnit * 2);

        // ---- interface -------------------------------------------------
        tooltipsToggle.setBounds (r.removeFromTop (buttonHeight));
        r.removeFromTop (gridUnit / 2);
        hoverValueToggle.setBounds (r.removeFromTop (buttonHeight));

        r.removeFromTop (gridUnit * 2);
        sectionRules.add (r.getY());
        r.removeFromTop (gridUnit * 2);

        // ---- MIDI ------------------------------------------------------
        midiMapLabel.setBounds (r.removeFromTop (20));
        r.removeFromTop (gridUnit);
        clearMidiBtn.setBounds (r.removeFromTop (buttonHeight).removeFromLeft (220));

        r.removeFromTop (gridUnit * 2);
        sectionRules.add (r.getY());
        r.removeFromTop (gridUnit * 2);

        // ---- presets ---------------------------------------------------
        presetFolderLabel.setBounds (r.removeFromTop (18));
        r.removeFromTop (gridUnit);
        revealPresetsBtn.setBounds (r.removeFromTop (buttonHeight).removeFromLeft (220));

        r.removeFromTop (gridUnit * 2);
        sectionRules.add (r.getY());
        r.removeFromTop (gridUnit * 2);

        // ---- audio & MIDI devices --------------------------------------
        deviceHeading.setBounds (r.removeFromTop (18));
        r.removeFromTop (gridUnit);

        if (deviceSelector != nullptr)
            deviceSelector->setBounds (r);
        else
            deviceNote.setBounds (r.removeFromTop (juce::jmin (r.getHeight(), 120)));
    }
}
