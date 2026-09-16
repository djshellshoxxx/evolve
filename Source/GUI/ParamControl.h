#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace mutagen
{
    /*  ==================================================================
        The right-click contract.

        Right-clicking any control offers: map it to a MIDI CC, reset it to
        its default, or type an exact value.

        This is implemented by subclassing rather than by a mouse listener
        on purpose. A listener cannot stop juce::Slider treating the
        right-button drag as a value drag - the slider's own mouseDown runs
        first and arms the drag - so a listener-based menu would leave every
        knob jumping around under the menu it just opened.

        A control says which parameter it drives through the "paramID"
        component property. Controls without one still get Set Value and
        Reset, operating on the widget directly.
        ================================================================== */
    namespace paramMenu
    {
        inline constexpr const char* idProperty = "paramID";

        /** Tag a control with the parameter it drives. */
        void tag (juce::Component&, const juce::String& paramID);

        juce::String tagOf (const juce::Component&);

        /** Open the right-click menu for a slider. Resolves the processor by
            walking up to the editor, so it works per plugin instance. */
        void showForSlider (juce::Slider&);

        /** Open the right-click menu for a non-slider control (toggle, combo,
            button). "Set value" is omitted where it has no meaning. */
        void showForComponent (juce::Component&);
    }

    // =====================================================================

    /** A juce::Slider that honours the right-click contract. */
    class ParamSlider : public juce::Slider
    {
    public:
        ParamSlider() = default;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                paramMenu::showForSlider (*this);
                return;     // deliberately not forwarded: no right-drag
            }
            juce::Slider::mouseDown (e);
        }

        /** The house drag rule: vertical for fine control, Shift coarse,
            Ctrl/Cmd ultra-fine. JUCE gives us the modifier hooks; the ratios
            are ours. */
        void mouseDrag (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDrag (e);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamSlider)
    };

    /** A TextButton that honours the right-click contract. */
    class ParamButton : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                paramMenu::showForComponent (*this);
                return;
            }
            juce::TextButton::mouseDown (e);
        }
    };

    /** A ToggleButton that honours the right-click contract. */
    class ParamToggle : public juce::ToggleButton
    {
    public:
        using juce::ToggleButton::ToggleButton;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                paramMenu::showForComponent (*this);
                return;
            }
            juce::ToggleButton::mouseDown (e);
        }
    };

    /** A ComboBox that honours the right-click contract. */
    class ParamCombo : public juce::ComboBox
    {
    public:
        using juce::ComboBox::ComboBox;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                paramMenu::showForComponent (*this);
                return;
            }
            juce::ComboBox::mouseDown (e);
        }
    };
}
