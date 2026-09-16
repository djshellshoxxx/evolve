#pragma once

#include "Widgets.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  The FX rack: the post-colony signal chain the musician can dial in -
        a subtractive synth layer (which can be *subtracted* from the colony),
        an LFO-swept filter, a 3-band EQ, a tempo-syncable gator, a glitch /
        beat-repeat unit, a MIDI-reactivity section, and a fourth LFO that
        modulates the colony's own mutation rate. Full-window overlay.         */
    class FxRackView : public juce::Component
    {
    public:
        explicit FxRackView (MutagenProcessor&);
        ~FxRackView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onClose;

    private:
        struct Section;
        struct Canvas;

        LabeledKnob*      addKnob (const juce::String& id, const juce::String& caption, juce::Colour tint);
        ParamToggle*      addToggle (const juce::String& id, const juce::String& caption, juce::Colour tint);
        ParamCombo*       addCombo (const juce::String& id, const juce::StringArray& choices);
        ParamButton*      addStep (const juce::String& id);

        MutagenProcessor& processor;

        juce::TextButton closeBtn { "Close" };
        juce::Viewport   viewport;
        std::unique_ptr<Canvas> content;

        juce::OwnedArray<LabeledKnob> knobs;
        juce::OwnedArray<ParamToggle> toggles;
        juce::OwnedArray<ParamCombo>  combos;
        juce::OwnedArray<ParamButton> steps;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment>  knobAtt;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment>  toggleAtt;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;

        juce::OwnedArray<Section> sections;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRackView)
    };
}
