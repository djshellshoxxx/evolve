#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "MutagenLookAndFeel.h"
#include "ParamControl.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  ==================================================================
        OptionsView

        Preferences that belong to the installation: tooltips, hover
        readouts, the audio and MIDI device setup, and the MIDI mappings.

        The device section only exists in the standalone app. Inside a host
        the host owns the soundcard, so the page says that plainly rather
        than showing controls that would do nothing.
        ================================================================== */
    class OptionsView : public juce::Component
    {
    public:
        explicit OptionsView (MutagenProcessor&);
        ~OptionsView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /** Re-read the MIDI mapping count and the option toggles. */
        void refresh();

        std::function<void()> onClose;
        std::function<void (bool)> onTooltipsChanged;

    private:
        MutagenProcessor& processor;

        juce::ToggleButton tooltipsToggle  { "Show hover tooltips" };
        juce::ToggleButton hoverValueToggle { "Show values on hover" };

        juce::Label  midiMapLabel;
        ParamButton  clearMidiBtn { "Clear All MIDI Mappings" };

        juce::Label  presetFolderLabel;
        ParamButton  revealPresetsBtn { "Show Preset Folder" };

        juce::Label  deviceHeading;
        std::unique_ptr<juce::Component> deviceSelector;   // standalone only
        juce::Label  deviceNote;

        juce::TextButton closeBtn { "Close" };

        // Horizontal separators, positioned by resized() and drawn by paint()
        // so the rules always land in the gaps the layout actually left.
        juce::Array<int> sectionRules;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionsView)
    };
}
