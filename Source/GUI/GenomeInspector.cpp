#include "GenomeInspector.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    static const char* kTraitNames[numTraits] = {
        "Pitch", "Duration", "Brightness", "Formant", "Direction",
        "Space Pos", "Space Motion", "Resonance", "Decay", "Attack",
        "Release", "Lifespan", "Reproduce", "Mutability", "Metabolism",
        "Aggression", "Symbiosis", "Infect Resist", "Density", "Noise / Tone"
    };
    static const char kDomChar[4] = { 'D', 'R', 'o', 'A' };
    static juce::Colour domColour (int d)
    {
        switch (d)
        {
            case 0: return theme::accent;        // dominant
            case 1: return theme::spectral;      // recessive
            case 2: return theme::textDim;       // dormant
            default: return theme::resonator;    // activated
        }
    }

    // =====================================================================

    class GenomeInspector::TraitRow : public juce::Component
    {
    public:
        TraitRow (int traitIndex, GenomeInspector& ownerRef)
            : trait (traitIndex), owner (ownerRef)
        {
            name.setText (kTraitNames[traitIndex], juce::dontSendNotification);
            name.setColour (juce::Label::textColourId, text);
            name.setFont (12.0f);
            addAndMakeVisible (name);

            value.setSliderStyle (juce::Slider::LinearHorizontal);
            value.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            value.setRange (0.0, 1.0, 0.0001);
            value.getProperties().set ("tint", (int) theme::spectral.getARGB());
            value.onValueChange = [this]
            {
                if (value.isMouseButtonDown())
                    owner.pushValue (trait, (float) value.getValue());
            };
            value.onDragEnd = [this] { owner.pushValue (trait, (float) value.getValue()); };
            addAndMakeVisible (value);

            dom.setButtonText (juce::String::charToString (kDomChar[0]));
            dom.onClick = [this]
            {
                domIndex = (domIndex + 1) % 4;
                dom.setButtonText (juce::String::charToString (kDomChar[domIndex]));
                owner.pushDominance (trait, domIndex);
                repaint();
            };
            addAndMakeVisible (dom);

            lock.setButtonText ("L");
            lock.setClickingTogglesState (true);
            lock.getProperties().set ("tint", (int) theme::selectRing.getARGB());
            lock.onClick = [this] { owner.pushLock (trait, lock.getToggleState()); };
            addAndMakeVisible (lock);
        }

        void set (float v, int dominance, bool locked)
        {
            if (! value.isMouseButtonDown())
                value.setValue (v, juce::dontSendNotification);
            domIndex = juce::jlimit (0, 3, dominance);
            dom.setButtonText (juce::String::charToString (kDomChar[domIndex]));
            lock.setToggleState (locked, juce::dontSendNotification);
            repaint();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (2);
            name.setBounds (r.removeFromLeft (96));
            lock.setBounds (r.removeFromRight (24));
            r.removeFromRight (3);
            dom.setBounds (r.removeFromRight (24));
            r.removeFromRight (5);
            value.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (domColour (domIndex).withAlpha (0.9f));
            g.fillRect (getLocalBounds().removeFromLeft (2));
        }

    private:
        int trait = 0;
        int domIndex = 0;
        GenomeInspector& owner;
        juce::Label name;
        juce::Slider value;
        juce::TextButton dom, lock;
    };

    // =====================================================================

    GenomeInspector::GenomeInspector (MutagenProcessor& p)
        : PanelFrame ("Genome Inspector"), processor (p)
    {
        accentColour = spectralV;

        scopeLabel.setText ("Whole colony", juce::dontSendNotification);
        scopeLabel.setColour (juce::Label::textColourId, accent);
        scopeLabel.setFont (juce::Font (12.5f, juce::Font::bold));
        addAndMakeVisible (scopeLabel);

        for (auto* b : { &mutateBtn, &selectBtn, &lockAllBtn, &unlockAllBtn })
            addAndMakeVisible (b);

        mutateBtn.onClick = [this] { mutateScope(); };
        // Four buttons share one narrow row, so the captions have to fit the
        // width the panel actually has; the tooltip carries the full meaning.
        mutateBtn.setTooltip ("Mutate whatever is selected - a cell, a family, a "
                              "species, or the whole colony.");
        selectBtn.setTooltip ("Push the selected genome into the environment's selection "
                              "targets, so the colony starts evolving toward it.");
        lockAllBtn.setTooltip ("Lock every gene, so mutation leaves them alone.");
        unlockAllBtn.setTooltip ("Unlock every gene.");

        selectBtn.onClick = [this]
        {
            EngineCommand c;
            c.type = CommandType::applySelection;
            c.scope = selection.active ? selection.level : ScopeLevel::colony;
            c.scopeId = selection.id;
            c.fa = 0.5f;
            processor.pushCommand (c);
        };
        lockAllBtn.onClick   = [this] { for (int i = 0; i < numTraits; ++i) pushLock (i, true); refresh (processor.latestOrganism()); };
        unlockAllBtn.onClick = [this] { for (int i = 0; i < numTraits; ++i) pushLock (i, false); refresh (processor.latestOrganism()); };

        for (int i = 0; i < numTraits; ++i)
        {
            rows[(size_t) i] = std::make_unique<TraitRow> (i, *this);
            rowHost.addAndMakeVisible (*rows[(size_t) i]);
        }
        viewport.setViewedComponent (&rowHost, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    GenomeInspector::~GenomeInspector() = default;

    void GenomeInspector::setSelection (const Selection& s)
    {
        selection = s;
        scopeLabel.setText (s.describe(), juce::dontSendNotification);
        refresh (processor.latestOrganism());
    }

    void GenomeInspector::refresh (const OrganismState& o)
    {
        // average the matching cells' genomes for a representative reading
        double acc[numTraits] = {};
        int    dom[numTraits] = {};
        int    lockCount[numTraits] = {};
        int    n = 0;

        for (int i = 0; i < o.cellCount && i < OrganismState::maxCells; ++i)
        {
            const auto& c = o.cells[i];
            if (! c.alive) continue;

            bool match = true;
            if (selection.active)
            {
                switch (selection.level)
                {
                    case ScopeLevel::colony:  match = true; break;
                    case ScopeLevel::species: match = (c.species == selection.id); break;
                    case ScopeLevel::family:  match = (c.familyId == selection.id); break;
                    case ScopeLevel::cell:    match = (i == selection.cellSlot); break;
                }
            }
            if (! match) continue;

            for (int t = 0; t < numTraits; ++t)
            {
                acc[t] += c.genome.value[t];
                if (c.genome.locked[t]) lockCount[t]++;
                dom[t] = c.genome.dominance[t];
            }
            ++n;
        }

        if (n == 0)
        {
            for (int t = 0; t < numTraits; ++t)
            {
                acc[t]       = o.baseline.value[t];
                dom[t]       = o.baseline.dominance[t];
                lockCount[t] = o.baseline.locked[t];
            }
        }

        const int div = juce::jmax (1, n);
        for (int t = 0; t < numTraits; ++t)
            rows[(size_t) t]->set ((float) (acc[t] / div), dom[t],
                                   n == 0 ? lockCount[t] != 0 : (lockCount[t] * 2 >= n));
    }

    void GenomeInspector::pushValue (int trait, float v)
    {
        EngineCommand c;
        c.type = CommandType::setGeneValue;
        c.scope = selection.active ? selection.level : ScopeLevel::colony;
        c.scopeId = selection.level == ScopeLevel::cell ? selection.cellSlot : selection.id;
        c.ia = trait; c.fa = v;
        processor.pushCommand (c);
    }

    void GenomeInspector::pushDominance (int trait, int dom)
    {
        EngineCommand c;
        c.type = CommandType::setGeneDominance;
        c.scope = selection.active ? selection.level : ScopeLevel::colony;
        c.scopeId = selection.level == ScopeLevel::cell ? selection.cellSlot : selection.id;
        c.ia = trait; c.ib = dom;
        processor.pushCommand (c);
    }

    void GenomeInspector::pushLock (int trait, bool locked)
    {
        EngineCommand c;
        c.type = locked ? CommandType::lockTraits : CommandType::unlockTraits;
        c.scope = selection.active ? selection.level : ScopeLevel::colony;
        c.scopeId = selection.id;
        c.traitMask = 1u << trait;
        processor.pushCommand (c);
    }

    void GenomeInspector::mutateScope()
    {
        EngineCommand c;
        c.type = CommandType::mutateNow;
        c.scope = selection.active ? selection.level : ScopeLevel::colony;
        c.scopeId = selection.level == ScopeLevel::cell ? selection.cellSlot : selection.id;
        processor.pushCommand (c);
    }

    void GenomeInspector::resized()
    {
        auto r = contentArea();
        scopeLabel.setBounds (r.removeFromTop (20));

        auto btns = r.removeFromTop (24);
        const int bw = btns.getWidth() / 4;
        mutateBtn.setBounds   (btns.removeFromLeft (bw).reduced (2, 0));
        selectBtn.setBounds   (btns.removeFromLeft (bw).reduced (2, 0));
        lockAllBtn.setBounds  (btns.removeFromLeft (bw).reduced (2, 0));
        unlockAllBtn.setBounds(btns.reduced (2, 0));
        r.removeFromTop (6);

        viewport.setBounds (r);
        const int rowH = 26;
        rowHost.setSize (r.getWidth() - 10, rowH * numTraits);
        for (int i = 0; i < numTraits; ++i)
            rows[(size_t) i]->setBounds (0, i * rowH, rowHost.getWidth(), rowH);
    }
}
