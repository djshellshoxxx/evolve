#pragma once

#include "Widgets.h"
#include "ParamControl.h"
#include "../Engine/OrganismState.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  ==================================================================
        A small vector-drawn button: gear, question mark, chevrons and the
        A/B pair. Icons rather than words because the header strip is 32px
        tall and the house label size is 11px - two words would not fit and
        one word would not be clearer than the shape.
        ================================================================== */
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { gear, question, chevronLeft, chevronRight, dice, disk };

        IconButton (Icon i, const juce::String& name) : juce::Button (name), icon (i) {}

        void paintButton (juce::Graphics&, bool over, bool down) override;

        juce::Colour tint { theme::textDim };

    private:
        Icon icon;
    };

    /*  ==================================================================
        The top of the window, in two strips.

        The upper strip is the house header every plugin in the range
        carries: the name on the left, and on the right the preset
        selector, the A/B pair, the settings gear and the manual. The lower
        strip is MUTAGEN's own: the transport-level verbs for a colony -
        clone, freeze, reanimate, export, randomise and reset.

        The split exists because the header is a contract shared with the
        other plugins, and mixing this instrument's verbs into it would
        make each plugin's header a different shape.
        ================================================================== */
    class TopBar : public juce::Component,
                   private juce::Timer
    {
    public:
        explicit TopBar (MutagenProcessor&);
        ~TopBar() override;

        static constexpr int headerRow = theme::headerHeight;   // 32, per the spec
        static constexpr int verbRow   = 34;
        static constexpr int totalHeight = headerRow + verbRow;

        void resized() override;
        void paint (juce::Graphics&) override;
        void setStats (const EngineSnapshot&);

        /** Rebuild the preset list from the factory bank and the user folder,
            and show whatever is currently loaded. */
        void refreshPresets();

        /** Transient message in the header - "saved X", "loaded Y". Clears
            itself after a few seconds. */
        void setStatus (const juce::String&);

        std::function<void()> onClone;
        std::function<void()> onRender;         // export audio to WAV
        std::function<void()> onReset;
        std::function<void()> onHelp;
        std::function<void()> onOptions;
        std::function<void()> onSaveRun;
        std::function<void()> onLoadRun;
        std::function<void (bool)> onPerformanceToggled;
        std::function<void (bool)> onInspectorToggled;
        std::function<void (bool)> onFxToggled;

    private:
        void timerCallback() override;

        void showFileMenu();
        void presetSelected();
        void stepPreset (int delta);
        void savePresetAs();
        void savePreset();
        void openPreset();
        void doRandomise();
        void toggleAB (bool wantB);
        void copyAcross();

        MutagenProcessor& processor;

        juce::Label      wordmark;      // the plugin's name: fixed, not the organism's
        juce::Label      nameLabel;     // the organism's name: editable
        juce::Label      statsLabel;
        juce::Label      statusLabel;   // transient "saved X" / "loaded Y"

        // ---- header strip ----
        ParamButton      fileBtn      { "FILE" };
        ParamCombo       presetBox;
        IconButton       prevPreset   { IconButton::Icon::chevronLeft,  "Previous preset" };
        IconButton       nextPreset   { IconButton::Icon::chevronRight, "Next preset" };
        ParamButton      aBtn         { "A" };
        ParamButton      bBtn         { "B" };
        ParamButton      copyBtn      { "COPY" };        // this slot over the other
        IconButton       optionsBtn   { IconButton::Icon::gear,     "Options" };
        IconButton       helpBtn      { IconButton::Icon::question, "Help" };

        // ---- verb strip ----
        ParamCombo       cpuBox, roleBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cpuAttach, roleAttach;

        ParamButton      exploreToggle { "EXPLORING" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> exploreAttach;

        ParamButton      cloneBtn     { "Clone" };
        ParamButton      freezeBtn    { "Freeze" };
        ParamButton      reanimateBtn { "Reanimate" };
        ParamButton      exportBtn    { "Export" };
        ParamButton      randomBtn    { "RANDOM" };
        ParamButton      resetBtn     { "RESET" };

        ParamButton      perfToggle   { "Perform" };
        ParamButton      fxToggle     { "FX" };
        ParamButton      inspToggle   { "Inspector" };

        // A/B: two whole parameter states, switched under the running colony.
        juce::ValueTree  slotA, slotB;
        bool             onSlotB = false;

        std::unique_ptr<juce::FileChooser> chooser;
        juce::StringArray userNames;
        double statusClearAt = 0.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
