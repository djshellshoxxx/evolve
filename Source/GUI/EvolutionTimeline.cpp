#include "EvolutionTimeline.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    EvolutionTimeline::EvolutionTimeline (MutagenProcessor& p)
        : PanelFrame ("Evolution Timeline"), processor (p)
    {
        accentColour = spectral;

        for (auto* b : { &restoreBtn, &branchBtn, &preserveBtn, &toLabBtn })
            addAndMakeVisible (b);

        restoreBtn.onClick = [this] { restoreSelected(); };

        branchBtn.onClick = [this]
        {
            if (selectedNode < 0) return;
            processor.history.beginBranchFrom (selectedNode);
            restoreSelected();               // diverge from here
        };

        preserveBtn.onClick = [this]
        {
            if (auto* n = processor.history.get (selectedNode))
                processor.history.setPreserved (selectedNode, ! n->preserved);
            repaint();
        };

        toLabBtn.onClick = [this]
        {
            if (auto* n = processor.history.get (selectedNode))
                if (onSendOrganismToLab) onSendOrganismToLab (n->state);
        };
    }

    void EvolutionTimeline::refresh()
    {
        if (selectedNode < 0 || selectedNode >= processor.history.size())
            selectedNode = processor.history.currentId();
        repaint();
    }

    juce::Point<float> EvolutionTimeline::nodePos (int index) const
    {
        const auto& nodes = processor.history.nodes();
        if (index < 0 || index >= (int) nodes.size()) return {};
        const auto& n = nodes[(size_t) index];
        return { (float) graphArea.getX() + 24.0f + n.depth * dx - scrollX,
                 (float) graphArea.getY() + 20.0f + n.lane * dy };
    }

    int EvolutionTimeline::nodeAt (juce::Point<float> p) const
    {
        const auto& nodes = processor.history.nodes();
        for (int i = 0; i < (int) nodes.size(); ++i)
            if (nodePos (i).getDistanceFrom (p) < 10.0f)
                return i;
        return -1;
    }

    void EvolutionTimeline::paint (juce::Graphics& g)
    {
        PanelFrame::paint (g);

        const auto& nodes = processor.history.nodes();
        const int current = processor.history.currentId();

        g.saveState();
        g.reduceClipRegion (graphArea);

        // edges
        for (const auto& n : nodes)
        {
            if (n.parentId < 0) continue;
            const auto a = nodePos (n.parentId);
            const auto b = nodePos (n.id);
            juce::Path path;
            path.startNewSubPath (a);
            path.cubicTo ({ (a.x + b.x) * 0.5f, a.y }, { (a.x + b.x) * 0.5f, b.y }, b);
            g.setColour (stroke.withAlpha (0.8f));
            g.strokePath (path, juce::PathStrokeType (1.6f));
        }

        // nodes
        for (int i = 0; i < (int) nodes.size(); ++i)
        {
            const auto& n = nodes[(size_t) i];
            const auto pos = nodePos (i);
            if (pos.x < graphArea.getX() - 20 || pos.x > graphArea.getRight() + 20) continue;

            const float rad = (i == current) ? 8.0f : 6.0f;
            juce::Colour c = n.isMutationEvent ? spectralV : spectral;
            if (n.preserved) c = selectRing;

            g.setColour (c.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (rad * 3.0f, rad * 3.0f).withCentre (pos));

            g.setColour (n.preserved ? selectRing : c);
            if (n.isMutationEvent)
            {
                juce::Path d; d.addPolygon (pos, 4, rad, 0.0f);
                g.fillPath (d);
            }
            else
                g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos));

            if (i == current)
            {
                g.setColour (accent);
                g.drawEllipse (juce::Rectangle<float> (rad * 2.8f, rad * 2.8f).withCentre (pos), 2.0f);
            }
            if (i == selectedNode)
            {
                g.setColour (selectRing);
                g.drawEllipse (juce::Rectangle<float> (rad * 3.4f, rad * 3.4f).withCentre (pos), 1.4f);
            }
            if (i == hoverNode)
            {
                g.setColour (text.withAlpha (0.9f));
                g.setFont (10.5f);
                g.drawText (n.label + "  p" + juce::String (n.state.cellCount),
                            juce::Rectangle<float> (pos.x - 40, pos.y - 24, 80, 14),
                            juce::Justification::centred);
            }
        }

        g.restoreState();

        // fade edges
        juce::ColourGradient lg (panel, (float) graphArea.getX(), 0,
                                 juce::Colours::transparentBlack, (float) graphArea.getX() + 16.0f, 0, false);
        g.setGradientFill (lg);
        g.fillRect (graphArea.withWidth (16));
    }

    void EvolutionTimeline::restoreSelected()
    {
        auto* n = processor.history.get (selectedNode);
        if (n == nullptr) return;

        EngineCommand c;
        c.type = CommandType::restoreOrganism;
        c.payloadIndex = processor.stageOrganismPayload (n->state);
        processor.pushCommand (c);

        processor.history.setCurrent (selectedNode);
        if (onColonyChanged) onColonyChanged();
        repaint();
    }

    void EvolutionTimeline::mouseDown (const juce::MouseEvent& e)
    {
        dragStartScrollX = scrollX;
        const int hit = nodeAt (e.position);
        if (hit >= 0) { selectedNode = hit; repaint(); }
    }

    void EvolutionTimeline::mouseDrag (const juce::MouseEvent& e)
    {
        // pan the graph horizontally by the drag delta
        scrollX = juce::jmax (0.0f, dragStartScrollX - (float) e.getDistanceFromDragStartX());
        hoverNode = nodeAt (e.position);
        repaint();
    }

    void EvolutionTimeline::mouseMove (const juce::MouseEvent& e)
    {
        const int h = nodeAt (e.position);
        if (h != hoverNode) { hoverNode = h; repaint(); }
    }

    void EvolutionTimeline::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int hit = nodeAt (e.position);
        if (hit >= 0) { selectedNode = hit; restoreSelected(); }
    }

    void EvolutionTimeline::mouseWheelMove (const juce::MouseEvent&,
                                            const juce::MouseWheelDetails& w)
    {
        scrollX = juce::jmax (0.0f, scrollX - w.deltaY * 120.0f - w.deltaX * 120.0f);
        repaint();
    }

    void EvolutionTimeline::resized()
    {
        auto r = contentArea();
        auto btns = r.removeFromLeft (86);
        restoreBtn.setBounds  (btns.removeFromTop (24));
        btns.removeFromTop (4);
        branchBtn.setBounds   (btns.removeFromTop (24));
        btns.removeFromTop (4);
        preserveBtn.setBounds (btns.removeFromTop (24));
        btns.removeFromTop (4);
        toLabBtn.setBounds    (btns.removeFromTop (24));

        r.removeFromLeft (8);
        graphArea = r;
    }
}
