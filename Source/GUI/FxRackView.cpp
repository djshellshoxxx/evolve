#include "FxRackView.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"

namespace mutagen
{
    using namespace theme;

    struct FxRackView::Section
    {
        juce::String title;
        juce::Colour accent;
        std::vector<juce::Component*> items;
        std::vector<juce::Point<int>> sizes;   // w,h per item
        juce::Rectangle<int> bounds;
    };

    struct FxRackView::Canvas : juce::Component
    {
        explicit Canvas (FxRackView& o) : owner (o) {}
        void paint (juce::Graphics& g) override
        {
            g.fillAll (bg0);
            for (auto* sec : owner.sections)
            {
                auto b = sec->bounds.toFloat();
                g.setColour (panel);
                g.fillRoundedRectangle (b, 8.0f);
                g.setColour (sec->accent.withAlpha (0.5f));
                g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

                auto head = b.removeFromTop (22.0f).reduced (10.0f, 0.0f);
                g.setColour (sec->accent);
                g.fillRoundedRectangle (head.removeFromLeft (4.0f).withSizeKeepingCentre (3.0f, 12.0f), 1.5f);
                g.setColour (text);
                g.setFont (juce::Font (12.0f, juce::Font::bold));
                g.drawText (sec->title, head.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
            }
        }
        FxRackView& owner;
    };

    // -----------------------------------------------------------------

    FxRackView::FxRackView (MutagenProcessor& p) : processor (p)
    {
        setOpaque (true);
        addAndMakeVisible (closeBtn);
        closeBtn.onClick = [this] { if (onClose) onClose(); };

        content = std::make_unique<Canvas> (*this);
        viewport.setViewedComponent (content.get(), false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        auto* sec = sections.add (new Section());
        auto begin = [&] (const juce::String& t, juce::Colour c)
        {
            sec = sections.add (new Section());
            sec->title = t;
            sec->accent = c;
        };
        auto item = [&] (juce::Component* c, int w, int h)
        {
            sec->items.push_back (c);
            sec->sizes.push_back ({ w, h });
        };
        // drop the initial placeholder
        sections.clear();
        sec = nullptr;

        const int K = 62, KH = 78, T = 168, TH = 26, C = 132, CH = 40, S = 26;

        // ---- 1. Subtractive synth ----
        begin ("SUBTRACTIVE SYNTH  (blend is bipolar: - subtracts from the colony, + adds a layer)", spectralV);
        for (int i = 0; i < params::numOscillators; ++i)
        {
            const juce::String n = "O" + juce::String (i + 1) + " ";
            item (addToggle (params::oscParam (i, "on"), n + "On", spectralV), T, TH);
            item (addCombo (params::oscParam (i, "wave"), params::oscWaveChoices()), C, CH);
            item (addKnob (params::oscParam (i, "tune"),  n + "Tune",  spectralV), K, KH);
            item (addKnob (params::oscParam (i, "fine"),  n + "Fine",  spectralV), K, KH);
            item (addKnob (params::oscParam (i, "level"), n + "Level", spectralV), K, KH);
            item (addKnob (params::oscParam (i, "pan"),   n + "Pan",   spectralV), K, KH);
        }
        item (addKnob (params::oscLevel,  "Synth Lvl", accent), K, KH);
        item (addKnob (params::oscBlend,  "Blend",     accent), K, KH);
        item (addKnob (params::oscFreeHz, "Free Hz",   spectralV), K, KH);
        item (addKnob (params::oscSpread, "Spread",    spectralV), K, KH);
        item (addToggle (params::oscKeytrack, "Key Track", spectralV), T, TH);
        item (addKnob (params::synthAttack,  "Attack",  grain), K, KH);
        item (addKnob (params::synthDecay,   "Decay",   grain), K, KH);
        item (addKnob (params::synthSustain, "Sustain", grain), K, KH);
        item (addKnob (params::synthRelease, "Release", grain), K, KH);
        item (addToggle (params::synthDrone, "Drone", spectralV), T, TH);

        // ---- 2. Filter ----
        begin ("FILTER  (swept by LFO 1)", spectral);
        item (addToggle (params::filterOn, "Filter On", spectral), T, TH);
        item (addCombo (params::filterType, params::filterTypeChoices()), C, CH);
        item (addKnob (params::filterCutoff, "Cutoff", spectral), K, KH);
        item (addKnob (params::filterRes,    "Reso",   spectral), K, KH);
        item (addKnob (params::filterDrive,  "Drive",  spectral), K, KH);

        // ---- 3. LFOs ----
        const char* lfoN[params::numLfos] = { "FILTER", "VOLUME", "PAN", "MUTATION" };
        begin ("LFOs  (1 filter, 2 tremolo, 3 auto-pan, 4 modulates the colony's mutation)", resonator);
        for (int i = 0; i < params::numLfos; ++i)
        {
            const juce::String n = juce::String (lfoN[i]) + " ";
            item (addToggle (params::lfoParam (i, "sync"), n + "Sync", resonator), T, TH);
            item (addKnob (params::lfoParam (i, "rate"),  n + "Rate",  resonator), K, KH);
            item (addCombo (params::lfoParam (i, "div"), params::syncDivChoices()), C, CH);
            item (addKnob (params::lfoParam (i, "depth"), n + "Depth", resonator), K, KH);
            item (addCombo (params::lfoParam (i, "shape"), params::lfoShapeChoices()), C, CH);
            item (addKnob (params::lfoParam (i, "phase"), n + "Phase", resonator), K, KH);
            if (i == params::envLfoIndex)
                item (addCombo (params::lfoParam (i, "dest"), params::envLfoDestChoices()), C, CH);
        }

        // ---- 4. EQ ----
        begin ("EQ", grain);
        item (addToggle (params::eqOn, "EQ On", grain), T, TH);
        item (addKnob (params::eqLowFreq,  "Low Hz",  grain), K, KH);
        item (addKnob (params::eqLowGain,  "Low dB",  grain), K, KH);
        item (addKnob (params::eqMidFreq,  "Mid Hz",  grain), K, KH);
        item (addKnob (params::eqMidGain,  "Mid dB",  grain), K, KH);
        item (addKnob (params::eqMidQ,     "Mid Q",   grain), K, KH);
        item (addKnob (params::eqHighFreq, "High Hz", grain), K, KH);
        item (addKnob (params::eqHighGain, "High dB", grain), K, KH);

        // ---- 5. Gator ----
        begin ("GATOR  (BPM-syncable rhythmic gate)", nutrient);
        item (addToggle (params::gatorOn,   "Gator On", nutrient), T, TH);
        item (addToggle (params::gatorSync, "Sync",     nutrient), T, TH);
        item (addCombo (params::gatorDiv, params::syncDivChoices()), C, CH);
        item (addKnob (params::gatorRate,    "Rate",    nutrient), K, KH);
        item (addKnob (params::gatorLength,  "Length",  nutrient), K, KH);
        item (addKnob (params::gatorAttack,  "Attack",  nutrient), K, KH);
        item (addKnob (params::gatorRelease, "Release", nutrient), K, KH);
        item (addKnob (params::gatorDepth,   "Depth",   nutrient), K, KH);
        for (int s = 0; s < params::gatorSteps; ++s)
            item (addStep (params::gatorStepParam (s)), S, S);

        // ---- 6. Glitch ----
        begin ("GLITCH  (beat-repeat / stutter / reverse / crush / tape-stop)", infection);
        item (addToggle (params::glitchOn,   "Glitch On", infection), T, TH);
        item (addToggle (params::glitchSync, "Sync",      infection), T, TH);
        item (addCombo (params::glitchDiv, params::syncDivChoices()), C, CH);
        item (addKnob (params::glitchAmount,  "Amount",  infection), K, KH);
        item (addKnob (params::glitchRate,    "Rate",    infection), K, KH);
        item (addKnob (params::glitchRepeat,  "Repeat",  infection), K, KH);
        item (addKnob (params::glitchReverse, "Reverse", infection), K, KH);
        item (addKnob (params::glitchCrush,   "Crush",   infection), K, KH);
        item (addKnob (params::glitchTape,    "Tape",    infection), K, KH);
        item (addKnob (params::glitchMix,     "Mix",     infection), K, KH);

        // ---- 7. MIDI reactivity ----
        begin ("MIDI REACTIVE", accent);
        item (addToggle (params::midiReactive,    "MIDI Reactive",     accent), T, TH);
        item (addToggle (params::lfoKeyRetrigger, "LFO Key Retrigger", accent), T, TH);
        item (addToggle (params::gatorRetrigger,  "Gator Key Retrigger", accent), T, TH);
        item (addKnob (params::midiBendRange,  "Bend st",  accent), K, KH);
        item (addCombo (params::modWheelDest, params::modWheelDestChoices()), C, CH);
        item (addKnob (params::modWheelAmount, "Mod Amt",  accent), K, KH);
        item (addKnob (params::velToSynth,     "Vel>Synth", accent), K, KH);
        item (addKnob (params::velToFilter,    "Vel>Filt",  accent), K, KH);
    }

    FxRackView::~FxRackView() = default;

    // -----------------------------------------------------------------

    LabeledKnob* FxRackView::addKnob (const juce::String& id, const juce::String& caption, juce::Colour tint)
    {
        auto* k = knobs.add (new LabeledKnob (processor.apvts, id, caption, tint, false));
        content->addAndMakeVisible (k);
        return k;
    }

    ParamToggle* FxRackView::addToggle (const juce::String& id, const juce::String& caption, juce::Colour tint)
    {
        auto* t = toggles.add (new ParamToggle (caption));
        t->getProperties().set ("tint", (int) tint.getARGB());
        paramMenu::tag (*t, id);
        t->setTooltip (params::describe (id));
        content->addAndMakeVisible (t);
        toggleAtt.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (processor.apvts, id, *t));
        return t;
    }

    ParamCombo* FxRackView::addCombo (const juce::String& id, const juce::StringArray& choices)
    {
        auto* c = combos.add (new ParamCombo());
        c->addItemList (choices, 1);
        paramMenu::tag (*c, id);
        c->setTooltip (params::describe (id));
        content->addAndMakeVisible (c);
        comboAtt.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (processor.apvts, id, *c));
        return c;
    }

    ParamButton* FxRackView::addStep (const juce::String& id)
    {
        auto* b = steps.add (new ParamButton());
        b->setClickingTogglesState (true);
        b->getProperties().set ("tint", (int) nutrient.getARGB());
        paramMenu::tag (*b, id);
        b->setTooltip ("One step of the gate pattern. Right-click to map it to MIDI.");
        content->addAndMakeVisible (b);
        toggleAtt.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (processor.apvts, id, *b));
        return b;
    }

    // -----------------------------------------------------------------

    void FxRackView::paint (juce::Graphics& g)
    {
        g.fillAll (bg0);
        auto top = getLocalBounds().removeFromTop (34).toFloat();
        juce::ColourGradient grad (panelHi, top.getTopLeft(), bg1, top.getBottomLeft(), false);
        g.setGradientFill (grad);
        g.fillRect (top);
        g.setColour (accent);
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText ("FX RACK", top.reduced (14, 0), juce::Justification::centredLeft);
        g.setColour (stroke);
        g.drawHorizontalLine (33, 0.0f, (float) getWidth());
    }

    void FxRackView::resized()
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (34);
        closeBtn.setBounds (header.removeFromRight (78).reduced (6, 5));
        viewport.setBounds (r);

        const int W = juce::jmax (600, viewport.getWidth() - 16);
        const int pad = 10, gap = 6;
        int y = 8;

        for (auto* sec : sections)
        {
            const int secX = 8;
            y += 4;
            const int headerH = 22;
            int cx = secX + pad;
            int cy = y + headerH + 6;
            int rowH = 0;

            for (size_t i = 0; i < sec->items.size(); ++i)
            {
                const int w = sec->sizes[i].x;
                const int h = sec->sizes[i].y;
                if (cx + w > secX + W - pad)
                {
                    cx = secX + pad;
                    cy += rowH + gap;
                    rowH = 0;
                }
                sec->items[i]->setBounds (cx, cy, w, h);
                cx += w + gap;
                rowH = juce::jmax (rowH, h);
            }
            cy += rowH + 8;
            sec->bounds = { secX, y, W, cy - y };
            y = cy + 10;
        }

        content->setSize (W + 16, y + 20);
    }
}
