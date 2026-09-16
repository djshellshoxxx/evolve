#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MutagenLookAndFeel.h"
#include "ParamControl.h"

namespace mutagen
{
    /*  A rotary slider with a caption underneath, wired to an APVTS parameter,
        carrying its own accent tint. The building block of every panel.        */
    class LabeledKnob : public juce::Component,
                        public juce::SettableTooltipClient
    {
    public:
        LabeledKnob (juce::AudioProcessorValueTreeState& state,
                     const juce::String& paramID,
                     const juce::String& caption,
                     juce::Colour tint,
                     bool big = false);

        void resized() override;
        void paint (juce::Graphics&) override;

        ParamSlider slider;

    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        bool isBig = false;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabeledKnob)
    };

    /*  A titled panel frame with the laboratory look. */
    class PanelFrame : public juce::Component
    {
    public:
        explicit PanelFrame (juce::String titleText) : title (std::move (titleText)) {}
        void paint (juce::Graphics&) override;
        juce::Rectangle<int> contentArea() const;
        juce::String title;
        juce::Colour accentColour { theme::spectral };
    };

    /*  A small horizontal stacked bar showing the grain/spectral/resonator mix. */
    class SpeciesMixBar : public juce::Component
    {
    public:
        void setCounts (int grain, int spectral, int resonator);
        void paint (juce::Graphics&) override;
    private:
        int g = 1, s = 1, r = 1;
    };
}
