#pragma once

#include "Widgets.h"
#include "../Engine/OrganismState.h"
#include "../Engine/BreedingLab.h"
#include <functional>

namespace mutagen
{
    class MutagenProcessor;

    /*  The Breeding Lab. Drop one or two preserved organisms into the parent
        slots, decide which parent contributes each functional block of the
        sound, then breed a family of specimen cards to audition, reject,
        clone, save or send back into the live colony.                         */
    class BreedingLabView : public PanelFrame
    {
    public:
        explicit BreedingLabView (MutagenProcessor&);
        ~BreedingLabView() override;

        void resized() override;

        void setParent (int which /*0 or 1*/, const OrganismState&, const juce::String& name);
        void refreshFromLab();

        std::function<void()> onClose;

    private:
        class SpecimenCard;
        class ParentSlot;

        void doBreed();
        BreedingRecipe currentRecipe() const;

        MutagenProcessor& processor;

        std::unique_ptr<ParentSlot> parentA, parentB;

        std::array<juce::ComboBox, 6> recipeBoxes;
        std::array<juce::Label, 6>    recipeLabels;
        juce::ToggleButton autoRecombine { "Auto-recombine per gene" };
        juce::Slider countSlider, mutationSlider, spreadSlider;
        juce::Label  countLabel, mutationLabel, spreadLabel;
        juce::TextButton breedBtn { "BREED" };
        juce::TextButton closeBtn { "Close" };

        juce::Viewport viewport;
        juce::Component cardHost;
        juce::OwnedArray<SpecimenCard> cards;

        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreedingLabView)
    };
}
