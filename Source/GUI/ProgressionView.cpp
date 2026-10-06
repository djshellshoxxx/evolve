#include "ProgressionView.h"
#include "MutagenLookAndFeel.h"
#include "../Engine/HiddenDiscoveries.h"

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
                line (left, "UNFILED NOTE: MOTH reports that this log existed before the session began.",
                      accent, 28);
                line (left, "CHAPTER " + juce::String (state.storyChapter)
                            + "  " + storyTitle (state.storyChapter), text, 22);
                line (left, storyText (state.storyChapter), textDim, 62);
                if (! progression.creatures().empty())
                    line (left, "ADDENDUM: at least one specimen has no agreed first-observation time.",
                          textDim, 28);
                left.removeFromTop (8);

                section (left, "RESEARCHERS");
                if (! state.features[(std::size_t) Feature::researcherCharacters])
                {
                    line (left, "[LOCKED] Researcher communications unlock at level 2.", textDim, 28);
                }
                else
                {
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
                line (left, "Creatures: " + juce::String ((int) progression.creatures().size()) + "/100");
                line (left, "Milestone artifacts: " + juce::String ((int) progression.milestoneArtifacts().size()));

                left.removeFromTop (8);
                section (left, "DISCOVERED CREATURES");
                if (progression.creatures().empty())
                    line (left, "Nothing has stepped out of the interface yet.", textDim, 24);
                else
                {
                    int shownCreatures = 0;
                    for (const auto id : progression.creatures())
                    {
                        line (left, juce::String (haunted::creatureName (id)), accent);
                        line (left, juce::String (haunted::creatureLore (id)), textDim, 28);
                        if (++shownCreatures >= 18)
                        {
                            if ((int) progression.creatures().size() > shownCreatures)
                                line (left, "...and " + juce::String ((int) progression.creatures().size() - shownCreatures)
                                            + " more recorded specimens.", textDim, 24);
                            break;
                        }
                    }
                }

                left.removeFromTop (8);
                section (left, "LINEAGE CODEX");
                if (! state.features[(std::size_t) Feature::lineageCodex])
                {
                    line (left, "[LOCKED] Breed lineages or reach research level 4.", textDim, 28);
                }
                else
                {
                    const int lineageScore = stats.breedingOperations * 3 + stats.generationsObserved / 20;
                    const juce::String lineageRank = lineageScore >= 28 ? "DYNASTY"
                                                    : lineageScore >= 18 ? "LINEAGE"
                                                    : lineageScore >= 10 ? "STRAIN"
                                                    : lineageScore >= 4  ? "CULTURE"
                                                                         : "SEED";
                    line (left, "Rank: " + lineageRank, text);
                    line (left, "Breeding operations " + juce::String (stats.breedingOperations)
                                + " / generations carried " + juce::String (stats.generationsObserved),
                          textDim, 28);
                }

                left.removeFromTop (8);
                section (left, "ANOMALY CATALOGUE");
                if (! state.features[(std::size_t) Feature::anomalyCatalogue])
                    line (left, "[LOCKED] Expand research or encounter an anomaly.", textDim, 28);
                else if (progression.anomalies().empty())
                    line (left, "No anomalies recorded yet.", textDim);
                else
                    for (const auto& id : progression.anomalies())
                        line (left, "[RECORDED] " + id.replaceCharacter ('_', ' ').toUpperCase(), accent);

                left.removeFromTop (8);
                section (left, "RELIC CABINET");
                if (! state.features[(std::size_t) Feature::relicCabinet])
                    line (left, "[LOCKED] Major research milestones reveal the cabinet.", textDim, 28);
                else if (progression.relics().empty())
                    line (left, "No relics earned yet.", textDim);
                else
                    for (const auto& id : progression.relics())
                        line (left, "[RELIC] " + id.replaceCharacter ('_', ' ').toUpperCase(), text);

                left.removeFromTop (8);
                section (left, "WORLD ATLAS");
                if (! state.features[(std::size_t) Feature::worldAtlas])
                {
                    line (left, "[LOCKED] Survey more worlds or reach research level 5.", textDim, 28);
                }
                else if (progression.worlds().empty())
                {
                    line (left, "The first observed world will be indexed here.", textDim, 24);
                }
                else
                {
                    int shown = 0;
                    for (const auto& id : progression.worlds())
                    {
                        line (left, id, textDim);
                        if (++shown >= 12)
                        {
                            if ((int) progression.worlds().size() > shown)
                                line (left, "...and " + juce::String ((int) progression.worlds().size() - shown)
                                            + " more worlds", textDim);
                            break;
                        }
                    }
                }

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
                if (! state.features[(std::size_t) Feature::researchProtocols])
                {
                    line (right, "[LOCKED] Protocol console unlocks at research level 2.", textDim, 28);
                }
                else
                {
                    for (int i = 0; i < 10; ++i)
                    {
                        const bool unlocked = state.level >= protocolUnlockLevel (i);
                        const juce::String prefix = unlocked
                            ? juce::String ("[OPEN] ")
                            : juce::String ("[LEVEL ") + juce::String (protocolUnlockLevel (i)) + "] ";
                        line (right, prefix + protocolName (i), unlocked ? text : textDim);
                        if (unlocked)
                            line (right, protocolDescription (i), textDim, 30);
                    }
                }

                right.removeFromTop (8);
                section (right, "CHALLENGE DECK");
                if (! state.features[(std::size_t) Feature::challengeDeck])
                {
                    line (right, "[LOCKED] Challenge research begins at level 3.", textDim, 28);
                }
                else
                {
                    for (const auto& ch : challengeDeck (stats))
                    {
                        line (right, juce::String (ch.title), text, 18);
                        line (right, juce::String (ch.objective), textDim, 30);
                        line (right, "Reward: " + juce::String (ch.reward), accent, 18);
                        right.removeFromTop (4);
                    }
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

        artifactSelector.setTextWhenNothingSelected ("Milestone skills");
        addAndMakeVisible (artifactSelector);
        replayArtifact.setEnabled (false);
        replayArtifact.onClick = [this]
        {
            const int selected = artifactSelector.getSelectedId();
            if (selected > 0 && onReplayMilestone)
                onReplayMilestone (selected);
        };
        artifactSelector.onChange = [this]
        {
            replayArtifact.setEnabled (artifactSelector.getSelectedId() > 0);
        };
        addAndMakeVisible (replayArtifact);

        auto* journal = new JournalContent (progression);
        journal->setSize (1040, 2400);
        viewport.setViewedComponent (journal, true);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
        rebuildArtifactSelector();
    }

    void ProgressionView::rebuildArtifactSelector()
    {
        const int previous = artifactSelector.getSelectedId();
        artifactSelector.clear (juce::dontSendNotification);
        for (const auto index : progression.milestoneArtifacts())
        {
            const auto artifact = haunted::milestoneForScore (
                (juce::int64) index * 1000000 - 1,
                (juce::int64) index * 1000000);
            if (artifact.has_value())
                artifactSelector.addItem (
                    juce::String (artifact->skillName) + " / " + juce::String (artifact->title),
                    index);
        }
        if (previous > 0)
            artifactSelector.setSelectedId (previous, juce::dontSendNotification);
        replayArtifact.setEnabled (artifactSelector.getSelectedId() > 0);
    }

    void ProgressionView::paint (juce::Graphics& g)
    {
        PanelFrame::paint (g);
    }

    void ProgressionView::resized()
    {
        auto r = contentArea();
        closeButton.setBounds (getLocalBounds().removeFromTop (26).removeFromRight (72).reduced (4, 2));

        auto controls = r.removeFromTop (30);
        artifactSelector.setBounds (controls.removeFromLeft (juce::jmin (520, controls.getWidth() - 160)).reduced (2));
        replayArtifact.setBounds (controls.removeFromLeft (140).reduced (2));
        r.removeFromTop (4);
        viewport.setBounds (r);
        if (auto* viewed = viewport.getViewedComponent())
            viewed->setSize (juce::jmax (900, r.getWidth() - 12), 2400);
    }

    void ProgressionView::refresh()
    {
        rebuildArtifactSelector();
        if (auto* viewed = viewport.getViewedComponent())
            viewed->repaint();
        repaint();
    }
}
