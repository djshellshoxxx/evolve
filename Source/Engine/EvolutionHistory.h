#pragma once

#include <vector>
#include <juce_data_structures/juce_data_structures.h>
#include "OrganismState.h"

namespace mutagen
{
    /*  A branching record of where the colony has been. Lives entirely on the
        message thread: the audio thread hands finished OrganismStates across a
        lock-free FIFO and this class files them into the tree.                 */
    struct GenerationNode
    {
        int id = 0;
        int parentId = -1;
        std::vector<int> childIds;
        OrganismState state;
        juce::String label;
        double timeStamp = 0.0;
        bool  preserved = false;
        bool  isMutationEvent = false;
        int   depth = 0;
        int   lane  = 0;      // vertical slot for drawing
    };

    class EvolutionHistory
    {
    public:
        EvolutionHistory() = default;

        void clear();
        int  addNode (const OrganismState& s, int parentId,
                      const juce::String& label, bool mutationEvent = false);

        int  size() const { return (int) tree.size(); }
        const std::vector<GenerationNode>& nodes() const { return tree; }
        GenerationNode*       get (int id);
        const GenerationNode* get (int id) const;

        int  currentId() const { return current; }
        void setCurrent (int id) { if (get (id) != nullptr) current = id; }
        int  rootId() const { return tree.empty() ? -1 : 0; }

        /** The next capture will hang off `id` instead of the current node,
            creating a visible fork. */
        void beginBranchFrom (int id) { branchParent = id; }
        int  consumeBranchParent();

        void setPreserved (int id, bool p) { if (auto* n = get (id)) n->preserved = p; }

        juce::ValueTree toValueTree() const;
        void            fromValueTree (const juce::ValueTree& vt);

    private:
        void relayout();

        std::vector<GenerationNode> tree;
        int current = -1;
        int branchParent = -1;
        double startTime = 0.0;
    };
}
