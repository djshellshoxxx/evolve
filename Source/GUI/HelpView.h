#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "MutagenLookAndFeel.h"
#include <functional>
#include <vector>

namespace mutagen
{
    /*  A block of help. An entry with an empty `term` is a paragraph; an
        entry with one is a definition - the control or feature on the left,
        what it does on the right. */
    struct HelpEntry
    {
        juce::String term;
        juce::String body;
    };

    struct HelpSection
    {
        juce::String title;
        std::vector<HelpEntry> entries;
    };

    /*  ==================================================================
        HelpView

        The manual, inside the plugin. Sections down the left, the text on
        the right, version number in the footer.

        It is laid out and drawn rather than dumped into a TextEditor
        because the definition list - control on the left, meaning on the
        right - is most of the content, and that alignment is what makes it
        skimmable when you are looking for one knob.
        ================================================================== */
    class HelpView : public juce::Component
    {
    public:
        HelpView();
        ~HelpView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onClose;

        static const std::vector<HelpSection>& content();

    private:
        class Page;

        static constexpr int navWidth = 168;

        void showSection (int index);

        juce::Viewport viewport;
        std::unique_ptr<Page> page;
        juce::OwnedArray<juce::TextButton> navButtons;
        juce::TextButton closeBtn { "Close" };
        int currentSection = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpView)
    };
}
