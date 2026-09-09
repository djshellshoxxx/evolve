#include "TopBar.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    TopBar::TopBar (MutagenProcessor& p) : processor (p)
    {
        nameLabel.setText (processor.organismName, juce::dontSendNotification);
        nameLabel.setEditable (true);
        nameLabel.setFont (juce::Font (18.0f, juce::Font::bold));
        nameLabel.setColour (juce::Label::textColourId, accent);
        nameLabel.onTextChange = [this]
        {
            processor.organismName = nameLabel.getText().isNotEmpty() ? nameLabel.getText() : "MUTAGEN";
        };
        addAndMakeVisible (nameLabel);

        statsLabel.setColour (juce::Label::textColourId, textDim);
        statsLabel.setFont (juce::Font (12.0f));
        statsLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statsLabel);

        cpuBox.addItemList (params::cpuQualityChoices(), 1);
        roleBox.addItemList (params::pluginRoleChoices(), 1);
        addAndMakeVisible (cpuBox);
        addAndMakeVisible (roleBox);
        cpuAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, params::cpuQuality, cpuBox);
        roleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, params::pluginRole, roleBox);

        exploreToggle.setClickingTogglesState (true);
        exploreToggle.getProperties().set ("tint", (int) resonator.getARGB());
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

        for (auto* b : { &cloneBtn, &freezeBtn, &reanimateBtn, &renderBtn, &resetBtn,
                         &perfToggle, &inspToggle })
            addAndMakeVisible (b);

        cloneBtn.onClick     = [this] { if (onClone) onClone(); };
        freezeBtn.onClick    = [setParam] { setParam (params::exploreMode, 0.0f); };
        reanimateBtn.onClick = [setParam] { setParam (params::exploreMode, 1.0f); };
        renderBtn.onClick    = [this] { if (onRender) onRender(); };

        resetBtn.getProperties().set ("tint", (int) infection.getARGB());
        resetBtn.onClick = [this]
        {
            if (onReset) onReset();
        };

        perfToggle.setClickingTogglesState (true);
        perfToggle.getProperties().set ("tint", (int) spectralV.getARGB());
        perfToggle.onClick = [this] { if (onPerformanceToggled) onPerformanceToggled (perfToggle.getToggleState()); };

        inspToggle.setClickingTogglesState (true);
        inspToggle.setToggleState (true, juce::dontSendNotification);
        inspToggle.getProperties().set ("tint", (int) spectral.getARGB());
        inspToggle.onClick = [this] { if (onInspectorToggled) onInspectorToggled (inspToggle.getToggleState()); };
    }

    void TopBar::setStats (const EngineSnapshot& s)
    {
        juce::String seedHex (juce::String::toHexString ((juce::int64) s.seed).toUpperCase());
        statsLabel.setText (
            "GEN " + juce::String (s.generation)
            + "     POP " + juce::String (s.population)
            + "  (" + juce::String (s.popBySpecies[0]) + "/" + juce::String (s.popBySpecies[1])
            + "/" + juce::String (s.popBySpecies[2]) + ")"
            + "     SEED 0x" + seedHex
            + "     DIV " + juce::String (s.diversity, 2),
            juce::dontSendNotification);
    }

    void TopBar::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient grad (panelHi, r.getTopLeft(), bg1, r.getBottomLeft(), false);
        g.setGradientFill (grad);
        g.fillRect (r);
        g.setColour (stroke);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // little living dot
        g.setColour (resonator);
        g.fillEllipse (10.0f, getHeight() * 0.5f - 3.0f, 6.0f, 6.0f);
    }

    void TopBar::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        r.removeFromLeft (14);

        nameLabel.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (8);

        // right-hand cluster
        auto right = r.removeFromRight (560);

        auto row = right;
        resetBtn.setBounds     (row.removeFromRight (60));
        row.removeFromRight (6);
        renderBtn.setBounds    (row.removeFromRight (64));
        row.removeFromRight (6);
        reanimateBtn.setBounds (row.removeFromRight (78));
        row.removeFromRight (4);
        freezeBtn.setBounds    (row.removeFromRight (60));
        row.removeFromRight (4);
        cloneBtn.setBounds     (row.removeFromRight (54));
        row.removeFromRight (10);
        exploreToggle.setBounds (row.removeFromRight (96));
        row.removeFromRight (10);
        perfToggle.setBounds   (row.removeFromRight (72));
        row.removeFromRight (4);
        inspToggle.setBounds   (row.removeFromRight (76));

        // middle: role + cpu + stats
        roleBox.setBounds (r.removeFromLeft (96).withSizeKeepingCentre (96, 24));
        r.removeFromLeft (6);
        cpuBox.setBounds (r.removeFromLeft (96).withSizeKeepingCentre (96, 24));
        r.removeFromLeft (10);
        statsLabel.setBounds (r);
    }
}
