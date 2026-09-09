#include "EvolutionHistory.h"
#include "OrganismSerialization.h"
#include <functional>

namespace mutagen
{
    void EvolutionHistory::clear()
    {
        tree.clear();
        current = -1;
        branchParent = -1;
        startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    int EvolutionHistory::consumeBranchParent()
    {
        const int p = branchParent;
        branchParent = -1;
        return p;
    }

    int EvolutionHistory::addNode (const OrganismState& s, int parentId,
                                   const juce::String& label, bool mutationEvent)
    {
        if (startTime == 0.0)
            startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

        GenerationNode n;
        n.id       = (int) tree.size();
        n.parentId = (get (parentId) != nullptr) ? parentId : (tree.empty() ? -1 : current);
        n.state    = s;
        n.label    = label;
        n.timeStamp = juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime;
        n.isMutationEvent = mutationEvent;

        const int myId = n.id;
        tree.push_back (std::move (n));

        if (auto* parent = get (tree.back().parentId))
            parent->childIds.push_back (myId);

        current = myId;
        relayout();
        return myId;
    }

    GenerationNode* EvolutionHistory::get (int id)
    {
        return (id >= 0 && id < (int) tree.size()) ? &tree[(size_t) id] : nullptr;
    }
    const GenerationNode* EvolutionHistory::get (int id) const
    {
        return (id >= 0 && id < (int) tree.size()) ? &tree[(size_t) id] : nullptr;
    }

    void EvolutionHistory::relayout()
    {
        if (tree.empty()) return;

        int nextLane = 0;
        std::function<void (int, int)> visit = [&] (int id, int depth)
        {
            auto& node = tree[(size_t) id];
            node.depth = depth;
            if (node.childIds.empty())
            {
                node.lane = nextLane++;
            }
            else
            {
                for (int c : node.childIds) visit (c, depth + 1);
                // sit the parent at the mean lane of its children
                int sum = 0;
                for (int c : node.childIds) sum += tree[(size_t) c].lane;
                node.lane = sum / (int) node.childIds.size();
            }
        };
        visit (0, 0);
    }

    juce::ValueTree EvolutionHistory::toValueTree() const
    {
        juce::ValueTree vt ("EVOLUTION");
        vt.setProperty ("current", current, nullptr);
        for (const auto& n : tree)
        {
            juce::ValueTree c ("NODE");
            c.setProperty ("id", n.id, nullptr);
            c.setProperty ("parent", n.parentId, nullptr);
            c.setProperty ("label", n.label, nullptr);
            c.setProperty ("time", n.timeStamp, nullptr);
            c.setProperty ("preserved", n.preserved, nullptr);
            c.setProperty ("mutation", n.isMutationEvent, nullptr);
            c.setProperty ("organism", organismToBase64 (n.state), nullptr);
            vt.appendChild (c, nullptr);
        }
        return vt;
    }

    void EvolutionHistory::fromValueTree (const juce::ValueTree& vt)
    {
        clear();
        if (! vt.hasType ("EVOLUTION")) return;

        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            const auto c = vt.getChild (i);
            GenerationNode n;
            n.id       = (int) c.getProperty ("id", (int) tree.size());
            n.parentId = (int) c.getProperty ("parent", -1);
            n.label    = c.getProperty ("label", "gen").toString();
            n.timeStamp = (double) c.getProperty ("time", 0.0);
            n.preserved = (bool) c.getProperty ("preserved", false);
            n.isMutationEvent = (bool) c.getProperty ("mutation", false);
            organismFromBase64 (c.getProperty ("organism").toString(), n.state);
            tree.push_back (std::move (n));
        }
        for (auto& n : tree)
            if (auto* p = get (n.parentId))
                p->childIds.push_back (n.id);

        current = (int) vt.getProperty ("current", (int) tree.size() - 1);
        if (current >= (int) tree.size()) current = (int) tree.size() - 1;
        relayout();
    }
}
