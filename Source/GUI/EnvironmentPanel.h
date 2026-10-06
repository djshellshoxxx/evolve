#pragma once

#include "Widgets.h"
#include "../Engine/EvolutionSteering.h"
#include <array>

namespace mutagen
{
    class MutagenProcessor;

    /*  Environment: the ecology the musician cultivates. Five large controls
        up top, the deeper ecological parameters beneath, and the selection
        target (what "fitness" means right now) along the bottom.               */
    class EnvironmentPanel : public PanelFrame
    {
    public:
        explicit EnvironmentPanel (MutagenProcessor&);
        void resized() override;

    private:
        void applySteeringProfile (const steering::Profile&, float strength, bool mutateAfter);
        void runCounterEvolve();
        void rollMutationDice();

        MutagenProcessor& processor;

        std::array<std::unique_ptr<LabeledKnob>, 5>  big;
        std::array<std::unique_ptr<LabeledKnob>, 10> small;
        std::array<std::unique_ptr<LabeledKnob>, 5>  sel;
        std::array<std::unique_ptr<juce::TextButton>, 8> steer;

        juce::Label selHeader;
        juce::Label steerHeader;
        juce::TextButton counterButton { "COUNTER-EVOLVE" };
        juce::TextButton diceButton    { "MUTATION DICE" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvironmentPanel)
    };
}
