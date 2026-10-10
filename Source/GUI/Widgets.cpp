#include "Widgets.h"
#include "../Parameters.h"
#include "../AppOptions.h"

namespace mutagen
{
    using namespace theme;

    LabeledKnob::LabeledKnob (juce::AudioProcessorValueTreeState& state,
                              const juce::String& paramID,
                              const juce::String& caption,
                              juce::Colour tint, bool big)
        : isBig (big)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 10, 10);
        slider.setColour (juce::Slider::rotarySliderFillColourId, tint);
        slider.getProperties().set ("tint", (int) tint.getARGB());
        slider.setDoubleClickReturnValue (true, 0.0);
        if (auto* rp = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, (double) rp->convertFrom0to1 (rp->getDefaultValue()));
        slider.setVelocityBasedMode (false);
        // The house rule: the value appears over the knob while you hover or
        // drag it, and the caption is what you see the rest of the time.
        slider.setPopupDisplayEnabled (AppOptions::get().showValueOnHover(), true, this);

        // Right-click gets Set Value / Reset / Map to MIDI, which needs the
        // control to know which parameter it drives.
        paramMenu::tag (slider, paramID);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, big ? text : textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (state, paramID, slider);

        // Tooltip: the explanation if there is one, otherwise the host-facing
        // parameter name, which at least says what it is.
        auto help = params::describe (paramID);
        if (help.isEmpty())
            if (auto* rp = state.getParameter (paramID))
                help = rp->getName (64);

        if (help.isNotEmpty())
        {
            slider.setTooltip (help);
            setTooltip (help);       // the caption underneath explains too
        }
    }

    void LabeledKnob::resized()
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (isBig ? 18 : 15));
        slider.setBounds (r.reduced (2));
    }

    void LabeledKnob::paint (juce::Graphics& g)
    {
        if (isBig)
        {
            g.setColour (bg0.withAlpha (0.35f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f);
        }
    }

    // -----------------------------------------------------------------

    void PanelFrame::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (stroke.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        auto header = r.removeFromTop (24.0f).reduced (10.0f, 0.0f);
        g.setColour (accentColour);
        g.fillRoundedRectangle (header.removeFromLeft (4.0f).withSizeKeepingCentre (3.0f, 12.0f), 1.5f);
        g.setColour (text);
        g.setFont (juce::Font (12.5f, juce::Font::bold));
        g.drawText (title.toUpperCase(), header.withTrimmedLeft (6.0f),
                    juce::Justification::centredLeft);
    }

    juce::Rectangle<int> PanelFrame::contentArea() const
    {
        return getLocalBounds().withTrimmedTop (28).reduced (10, 8);
    }

    // -----------------------------------------------------------------

    void SpeciesMixBar::setCounts (int grain, int spectral, int resonator)
    {
        g = juce::jmax (0, grain);
        s = juce::jmax (0, spectral);
        r = juce::jmax (0, resonator);
        repaint();
    }

    void SpeciesMixBar::paint (juce::Graphics& gr)
    {
        auto r0 = getLocalBounds().toFloat();
        gr.setColour (bg0);
        gr.fillRoundedRectangle (r0, 3.0f);

        const float total = (float) juce::jmax (1, g + s + r);
        auto seg = r0.reduced (2.0f);
        const float w = seg.getWidth();

        auto drawSeg = [&] (float frac, juce::Colour c)
        {
            if (frac <= 0.0001f) return;
            auto piece = seg.removeFromLeft (w * frac);
            gr.setColour (c.withAlpha (0.85f));
            gr.fillRoundedRectangle (piece.reduced (0.5f), 2.0f);
        };
        drawSeg ((float) g / total, grain);
        drawSeg ((float) s / total, spectral);
        drawSeg ((float) r / total, resonator);

        gr.setColour (text.withAlpha (0.8f));
        gr.setFont (10.0f);
        gr.drawText (juce::String (g) + "g  " + juce::String (s) + "s  " + juce::String (r) + "r",
                     getLocalBounds(), juce::Justification::centred);
    }
}
