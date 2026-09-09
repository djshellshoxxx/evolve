#pragma once

#include "Widgets.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>

namespace mutagen
{
    class MutagenProcessor;

    /*  The performance page: the whole system reduced to eight assignable
        macros, an XY field for stability vs reproductive aggression, and a
        playable keyboard.                                                     */
    class PerformanceView : public PanelFrame
    {
    public:
        explicit PerformanceView (MutagenProcessor&);
        ~PerformanceView() override;

        void resized() override;

        std::function<void()> onClose;

    private:
        class XYPad;

        MutagenProcessor& processor;

        std::array<std::unique_ptr<LabeledKnob>, 8> macros;

        juce::Slider xSlider, ySlider;   // hidden, attached to params
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAttach, yAttach;
        std::unique_ptr<XYPad> pad;

        juce::MidiKeyboardComponent keyboard;
        juce::TextButton closeBtn { "Close" };
        juce::Label xyCaption;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceView)
    };
}
