#include "BreedingLabView.h"
#include "../PluginProcessor.h"
#include "../Engine/OrganismSerialization.h"

namespace mutagen
{
    using namespace theme;

    static const char* kTraitShort[numTraits] = {
        "Pit","Dur","Bri","For","Dir","SpP","SpM","Res","Dec","Atk",
        "Rel","Lif","Rep","Mut","Met","Agg","Sym","Inf","Den","N/T"
    };

    // =====================================================================
    class BreedingLabView::ParentSlot : public juce::Component
    {
    public:
        ParentSlot (const juce::String& title) : caption (title) {}

        void set (const OrganismState& o, const juce::String& n)
        {
            organism = o; name = n; filled = true; repaint();
        }
        void clear() { filled = false; name = {}; repaint(); }
        bool isFilled() const { return filled; }
        const OrganismState& get() const { return organism; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (filled ? panelHi : bg0);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (filled ? spectral : stroke);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);

            g.setColour (textDim);
            g.setFont (10.5f);
            g.drawText (caption, r.reduced (8, 4).removeFromTop (14), juce::Justification::topLeft);

            if (! filled)
            {
                g.setColour (textDim);
                g.setFont (12.0f);
                g.drawText ("empty - send from timeline or a cell",
                            r.reduced (8), juce::Justification::centred);
                return;
            }

            g.setColour (text);
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText (name, r.reduced (8, 4).withTrimmedTop (14).removeFromTop (18),
                        juce::Justification::topLeft);

            // species mix
            auto bar = r.reduced (10, 8).removeFromBottom (10);
            const float tot = (float) juce::jmax (1, organism.popBySpecies[0]
                                + organism.popBySpecies[1] + organism.popBySpecies[2]);
            for (int s = 0; s < numSpecies; ++s)
            {
                auto seg = bar.removeFromLeft (bar.getWidth() * organism.popBySpecies[s] / tot);
                g.setColour (speciesColour (s).withAlpha (0.85f));
                g.fillRect (seg.reduced (0.5f, 0.0f));
            }
            g.setColour (textDim);
            g.setFont (10.0f);
            g.drawText ("gen " + juce::String (organism.generation)
                        + "  pop " + juce::String (organism.cellCount),
                        r.reduced (10, 8).removeFromBottom (24).removeFromTop (12),
                        juce::Justification::bottomLeft);
        }

        std::function<void()> onClear;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onClear) onClear();
        }

    private:
        juce::String caption, name;
        OrganismState organism;
        bool filled = false;
    };

    // =====================================================================
    class BreedingLabView::SpecimenCard : public juce::Component
    {
    public:
        SpecimenCard (MutagenProcessor& p, const Specimen& sp, int idx)
            : processor (p), specimen (sp), index (idx)
        {
            sendBtn.setButtonText (juce::String::fromUTF8 ("\xE2\x86\x92 Colony"));
            saveBtn.setButtonText ("Save");
            rejectBtn.setButtonText ("Reject");
            preserveBtn.setButtonText ("Keep");
            rejectBtn.setClickingTogglesState (true);
            preserveBtn.setClickingTogglesState (true);

            for (auto* b : { &sendBtn, &saveBtn, &rejectBtn, &preserveBtn })
                addAndMakeVisible (b);

            sendBtn.onClick = [this]
            {
                EngineCommand c;
                c.type = CommandType::breedInject;
                c.payloadIndex = processor.stageOrganismPayload (specimen.organism);
                processor.pushCommand (c);
            };
            saveBtn.onClick = [this] { save(); };
            rejectBtn.onClick = [this] { repaint(); };
            preserveBtn.onClick = [this] { repaint(); };
        }

        void save()
        {
            chooser = std::make_unique<juce::FileChooser> ("Save specimen",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile (specimen.name + ".mutagen"),
                "*.mutagen");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f != juce::File{})
                        f.replaceWithText (organismToBase64 (specimen.organism));
                });
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const bool rej = rejectBtn.getToggleState();
            g.setColour (rej ? bg0.withAlpha (0.5f) : panelHi);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (preserveBtn.getToggleState() ? selectRing : stroke);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);

            auto body = r.reduced (8.0f);
            g.setColour (rej ? textDim : text);
            g.setFont (juce::Font (12.5f, juce::Font::bold));
            g.drawText (specimen.name, body.removeFromTop (16), juce::Justification::topLeft);

            // genome fingerprint
            auto fp = body.removeFromTop (34);
            const float bw = fp.getWidth() / (float) numTraits;
            for (int t = 0; t < numTraits; ++t)
            {
                const float v = specimen.organism.baseline.value[t];
                auto col = fp.withX (fp.getX() + t * bw).withWidth (bw - 1.0f);
                g.setColour ((t == specimen.dominantTrait ? accent : spectral).withAlpha (0.85f));
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * v));
            }
            body.removeFromTop (4);

            // similarity bars
            auto sim = body.removeFromTop (26);
            auto drawSim = [&] (const juce::String& tag, float val, juce::Colour c)
            {
                auto row = sim.removeFromTop (12);
                g.setColour (textDim); g.setFont (9.5f);
                g.drawText (tag, row.removeFromLeft (28), juce::Justification::centredLeft);
                g.setColour (panel); g.fillRoundedRectangle (row.toFloat(), 2.0f);
                g.setColour (c);
                g.fillRoundedRectangle (row.toFloat().withWidth (row.getWidth() * juce::jlimit (0.0f, 1.0f, val)), 2.0f);
            };
            drawSim ("~A", specimen.similarityA, grain);
            drawSim ("~B", specimen.similarityB, resonator);

            body.removeFromTop (2);
            // species mix
            auto mix = body.removeFromTop (8);
            const float tot = (float) juce::jmax (1, specimen.popBySpecies[0]
                                + specimen.popBySpecies[1] + specimen.popBySpecies[2]);
            for (int s = 0; s < numSpecies; ++s)
            {
                auto seg = mix.removeFromLeft (mix.getWidth() * specimen.popBySpecies[s] / tot);
                g.setColour (speciesColour (s).withAlpha (0.8f));
                g.fillRect (seg.reduced (0.5f, 0.0f));
            }

            g.setColour (textDim);
            g.setFont (9.5f);
            g.drawText (juce::String ("dominant: ") + kTraitShort[specimen.dominantTrait],
                        body.removeFromTop (12), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto btns = r.removeFromBottom (44);
            auto top = btns.removeFromTop (20);
            sendBtn.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (1));
            saveBtn.setBounds (top.reduced (1));
            btns.removeFromTop (2);
            rejectBtn.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (1));
            preserveBtn.setBounds (btns.reduced (1));
        }

    private:
        MutagenProcessor& processor;
        Specimen specimen;
        int index = 0;
        juce::TextButton sendBtn, saveBtn, rejectBtn, preserveBtn;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    // =====================================================================

    BreedingLabView::BreedingLabView (MutagenProcessor& p)
        : PanelFrame ("Breeding Lab"), processor (p)
    {
        accentColour = spectralV;

        parentA = std::make_unique<ParentSlot> ("PARENT A");
        parentB = std::make_unique<ParentSlot> ("PARENT B");
        addAndMakeVisible (*parentA);
        addAndMakeVisible (*parentB);
        parentA->onClear = [this] { parentA->clear(); };
        parentB->onClear = [this] { parentB->clear(); };

        const char* names[6] = { "Body", "Voice", "Texture", "Movement", "Lifecycle", "Env Behaviour" };
        for (int i = 0; i < 6; ++i)
        {
            recipeBoxes[(size_t) i].addItemList ({ "Parent A", "Parent B", "Blend" }, 1);
            recipeBoxes[(size_t) i].setSelectedId (3, juce::dontSendNotification);
            addAndMakeVisible (recipeBoxes[(size_t) i]);

            recipeLabels[(size_t) i].setText (names[i], juce::dontSendNotification);
            recipeLabels[(size_t) i].setColour (juce::Label::textColourId, textDim);
            recipeLabels[(size_t) i].setFont (11.0f);
            addAndMakeVisible (recipeLabels[(size_t) i]);
        }

        addAndMakeVisible (autoRecombine);
        autoRecombine.getProperties().set ("tint", (int) spectralV.getARGB());

        auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name,
                                   double lo, double hi, double def)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
            s.setRange (lo, hi, name == "Count" ? 1.0 : 0.001);
            s.setValue (def);
            s.getProperties().set ("tint", (int) spectralV.getARGB());
            addAndMakeVisible (s);
            l.setText (name, juce::dontSendNotification);
            l.setColour (juce::Label::textColourId, textDim);
            l.setFont (11.0f);
            addAndMakeVisible (l);
        };
        setupSlider (countSlider,    countLabel,    "Count",    2, 12, 6);
        setupSlider (mutationSlider, mutationLabel, "Mutation", 0.0, 1.0, 0.25);
        setupSlider (spreadSlider,   spreadLabel,   "Spread",   0.0, 1.0, 0.5);

        breedBtn.getProperties().set ("tint", (int) spectralV.getARGB());
        breedBtn.onClick = [this] { doBreed(); };
        addAndMakeVisible (breedBtn);

        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeBtn);

        viewport.setViewedComponent (&cardHost, false);
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);
    }

    BreedingLabView::~BreedingLabView() = default;

    void BreedingLabView::setParent (int which, const OrganismState& o, const juce::String& name)
    {
        (which == 0 ? parentA : parentB)->set (o, name);
    }

    BreedingRecipe BreedingLabView::currentRecipe() const
    {
        BreedingRecipe r;
        auto pick = [] (const juce::ComboBox& b) { return b.getSelectedId() - 1; }; // 0=A,1=B,2=Blend
        r.body      = pick (recipeBoxes[0]);
        r.voice     = pick (recipeBoxes[1]);
        r.texture   = pick (recipeBoxes[2]);
        r.movement  = pick (recipeBoxes[3]);
        r.lifecycle = pick (recipeBoxes[4]);
        r.envBehav  = pick (recipeBoxes[5]);
        r.autoRecombine  = autoRecombine.getToggleState();
        r.mutationAmount  = (float) mutationSlider.getValue();
        r.variationSpread = (float) spreadSlider.getValue();
        return r;
    }

    void BreedingLabView::doBreed()
    {
        if (! parentA->isFilled())
        {
            // fall back to the live colony as parent A
            processor.breedingLab.setParents (processor.latestOrganism(),
                                              processor.latestOrganism(), false);
        }
        else
        {
            processor.breedingLab.setParents (parentA->get(),
                                              parentB->isFilled() ? parentB->get() : parentA->get(),
                                              parentB->isFilled());
        }

        const uint64_t seed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
        processor.breedingLab.breed ((int) countSlider.getValue(), currentRecipe(), seed);
        if (onBreed) onBreed();
        refreshFromLab();
    }

    void BreedingLabView::refreshFromLab()
    {
        cards.clear();
        auto& specimens = processor.breedingLab.results();
        for (int i = 0; i < (int) specimens.size(); ++i)
            cards.add (new SpecimenCard (processor, specimens[(size_t) i], i));
        for (auto* c : cards) cardHost.addAndMakeVisible (c);
        resized();
    }

    void BreedingLabView::resized()
    {
        auto r = contentArea();

        closeBtn.setBounds (getLocalBounds().removeFromTop (24).removeFromRight (70).reduced (4, 2));

        auto top = r.removeFromTop (86);
        parentA->setBounds (top.removeFromLeft (top.getWidth() / 2 - 6));
        top.removeFromLeft (12);
        parentB->setBounds (top);
        r.removeFromTop (8);

        auto recipe = r.removeFromTop (46);
        const int cw = recipe.getWidth() / 6;
        for (int i = 0; i < 6; ++i)
        {
            auto col = recipe.removeFromLeft (cw).reduced (3, 0);
            recipeLabels[(size_t) i].setBounds (col.removeFromTop (14));
            recipeBoxes[(size_t) i].setBounds (col.removeFromTop (24));
        }
        r.removeFromTop (6);

        auto ctrl = r.removeFromTop (24);
        autoRecombine.setBounds (ctrl.removeFromLeft (200));
        breedBtn.setBounds (ctrl.removeFromRight (110));
        r.removeFromTop (4);

        auto sliders = r.removeFromTop (24);
        const int sw = sliders.getWidth() / 3;
        auto place = [&] (juce::Label& l, juce::Slider& s, juce::Rectangle<int> a)
        {
            l.setBounds (a.removeFromLeft (56));
            s.setBounds (a);
        };
        place (countLabel,    countSlider,    sliders.removeFromLeft (sw).reduced (3, 0));
        place (mutationLabel, mutationSlider, sliders.removeFromLeft (sw).reduced (3, 0));
        place (spreadLabel,   spreadSlider,   sliders.reduced (3, 0));
        r.removeFromTop (8);

        viewport.setBounds (r);
        const int cardW = 168, cardH = r.getHeight() - 12;
        cardHost.setSize (juce::jmax (r.getWidth(), (int) cards.size() * (cardW + 8) + 8), r.getHeight());
        for (int i = 0; i < (int) cards.size(); ++i)
            cards[i]->setBounds (8 + i * (cardW + 8), 4, cardW, cardH);
    }
}
