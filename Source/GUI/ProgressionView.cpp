#include "ProgressionView.h"
#include "MutagenLookAndFeel.h"

namespace mutagen
{
    using namespace theme;
    using namespace progression;

    namespace
    {
        class JournalContent : public juce::Component
        {
        public:
            explicit JournalContent (ProgressionSystem& p) : progression (p) {}

            void paint (juce::Graphics& g) override
            {
                const auto stats = progression.stats();
                const auto state = progression.state();

                g.fillAll (bg0);
                auto r = getLocalBounds().reduced (18);

                g.setColour (text);
                g.setFont (juce::Font (22.0f, juce::Font::bold));
                g.drawText ("FIELD JOURNAL  /  RESEARCH LEVEL " + juce::String (state.level),
                            r.removeFromTop (30), juce::Justification::centredLeft);

                g.setColour (textDim);
                g.setFont (11.5f);
                g.drawText ("Insight " + juce::String (state.insight)
                            + "    Lifetime score " + juce::String (stats.lifetimeScore)
                            + "    Generations " + juce::String (stats.generationsObserved)
                            + "    Discoveries " + juce::String (stats.lifetimeDiscoveries)
                            + "    Worlds " + juce::String (stats.worldsVisited),
                            r.removeFromTop (22), juce::Justification::centredLeft);

                r.removeFromTop (8);
                const int gutter = 18;
                auto left = r.removeFromLeft ((r.getWidth() - gutter) / 2);
                r.removeFromLeft (gutter);
                auto right = r;

                auto section = [&] (juce::Rectangle<int>& area, const juce::String& title)
                {
                    auto head = area.removeFromTop (24);
                    g.setColour (accent);
                    g.fillRect (head.removeFromLeft (3).reduced (0, 5));
                    g.setColour (text);
                    g.setFont (juce::Font (13.0f, juce::Font::bold));
                    g.drawText (title, head.reduced (8, 0), juce::Justification::centredLeft);
                };

                auto line = [&] (juce::Rectangle<int>& area, const juce::String& s,
                                juce::Colour colour = textDim, int height = 18)
                {
                    g.setColour (colour);
                    g.setFont (11.0f);
                    g.drawFittedText (s, area.removeFromTop (height), juce::Justification::centredLeft, 2);
                };

                section (left, "CURRENT STORY");
                line (left, "CHAPTER " + juce::String (state.storyChapter)
                            + "  " + storyTitle (state.storyChapter), text, 22);
                line (left, storyText (state.storyChapter), textDim, 54);
                left.removeFromTop (8);

                section (left, "RESEARCHERS");
                for (std::size_t i = 0; i < (std::size_t) Character::count; ++i)
                {
                    const auto who = (Character) i;
                    const bool unlocked = state.characters[i];
                    line (left, juce::String (unlocked ? "[OPEN] " : "[LOCKED] ")
                                + characterName (who) + " / " + characterRole (who),
                          unlocked ? text : textDim);
                    if (unlocked)
                        line (left, characterBriefing (who), textDim, 34);
                }

                left.removeFromTop (8);
                section (left, "LIFETIME RECORD");
                line (left, "Noise rescues: " + juce::String (stats.noiseRescues));
                line (left, "Samples digested: " + juce::String (stats.samplesDigested));
                line (left, "Radiation exposures: " + juce::String (stats.radiationExposures));
                line (left, "Breeding operations: " + juce::String (stats.breedingOperations));
                line (left, "Steering interventions: " + juce::String (stats.steeringInterventions));
                line (left, "Peak combo: " + juce::String (stats.peakCombo));
                line (left, "Anomalies: " + juce::String ((int) progression.anomalies().size()));
                line (left, "Relics: " + juce::String ((int) progression.relics().size()));

                section (right, "TEN DEPTH SYSTEMS");
                for (std::size_t i = 0; i < (std::size_t) Feature::count; ++i)
                {
                    const bool unlocked = state.features[i];
                    line (right, juce::String (unlocked ? "[OPEN] " : "[LOCKED] ")
                                 + featureName ((Feature) i),
                          unlocked ? text : textDim);
                }

                right.removeFromTop (8);
                section (right, "RESEARCH PROTOCOLS");
                for (int i = 0; i < 10; ++i)
                {
                    const bool unlocked = state.level >= protocolUnlockLevel (i);
                    line (right, juce::String (unlocked ? "[OPEN] " : "[LEVEL "
                                          + juce::String (protocolUnlockLevel (i)) + "] ")
                                 + protocolName (i),
                          unlocked ? text : textDim);
                    if (unlocked)
                        line (right, protocolDescription (i), textDim, 30);
                }

                right.removeFromTop (8);
                section (right, "CHALLENGE DECK");
                for (const auto& ch : challengeDeck (stats))
                {
                    line (right, juce::String (ch.title), text, 18);
                    line (right, juce::String (ch.objective), textDim, 30);
                    line (right, "Reward: " + juce::String (ch.reward), accent, 18);
                    right.removeFromTop (4);
                }

                if (state.features[(std::size_t) Feature::storyBranches])
                {
                    right.removeFromTop (8);
                    section (right, "THREE DIRECTIVES");
                    const auto recommended = recommendedDirective (stats);
                    for (std::size_t i = 0; i < (std::size_t) Directive::count; ++i)
                    {
                        const auto d = (Directive) i;
                        line (right, juce::String (d == recommended ? "> " : "  ")
                                    + directiveName (d),
                              d == recommended ? accent : text);
                    }
                    line (right, "No ending locks the others. The recommendation reflects your history.", textDim, 34);
                }
            }

        private:
            ProgressionSystem& progression;
        };
    }

    ProgressionView::ProgressionView (ProgressionSystem& p)
        : PanelFrame ("Field Journal"), progression (p)
    {
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);

        auto* journal = new JournalContent (progression);
        journal->setSize (1040, 1160);
        viewport.setViewedComponent (journal, true);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void ProgressionView::paint (juce::Graphics& g)
    {
        PanelFrame::paint (g);
    }

    void ProgressionView::resized()
    {
        auto r = contentArea();
        closeButton.setBounds (getLocalBounds().removeFromTop (26).removeFromRight (72).reduced (4, 2));
        viewport.setBounds (r);
        if (auto* viewed = viewport.getViewedComponent())
            viewed->setSize (juce::jmax (900, r.getWidth() - 12), 1160);
    }

    void ProgressionView::refresh()
    {
        if (auto* viewed = viewport.getViewedComponent())
            viewed->repaint();
        repaint();
    }
}
