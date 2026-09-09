#include "EnvironmentPanel.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    EnvironmentPanel::EnvironmentPanel (MutagenProcessor& p)
        : PanelFrame ("Environment"), processor (p)
    {
        accentColour = resonator;
        auto& s = p.apvts;

        struct Def { const char* id; const char* name; juce::Colour c; };

        const Def bigDefs[5] = {
            { params::nutrients,  "Nutrients",  nutrient },
            { params::mutation,   "Mutation",   spectralV },
            { params::selection,  "Selection",  accent },
            { params::metabolism, "Metabolism", grain },
            { params::stability,  "Stability",  resonator },
        };
        for (int i = 0; i < 5; ++i)
        {
            big[(size_t) i] = std::make_unique<LabeledKnob> (s, bigDefs[i].id, bigDefs[i].name,
                                                             bigDefs[i].c, true);
            addAndMakeVisible (*big[(size_t) i]);
        }

        const Def smallDefs[10] = {
            { params::fertility,     "Fertility",  nutrient },
            { params::mutationDepth, "Mut Depth",  spectralV },
            { params::radiation,     "Radiation",  infection },
            { params::temperature,   "Temp",       grain },
            { params::competition,   "Compete",    infection },
            { params::symbiosis,     "Symbiosis",  resonator },
            { params::lifespan,      "Lifespan",   spectral },
            { params::apoptosis,     "Apoptosis",  infection },
            { params::diversity,     "Diversity",  spectral },
            { params::migration,     "Migration",  resonator },
        };
        for (int i = 0; i < 10; ++i)
        {
            small[(size_t) i] = std::make_unique<LabeledKnob> (s, smallDefs[i].id, smallDefs[i].name,
                                                               smallDefs[i].c, false);
            addAndMakeVisible (*small[(size_t) i]);
        }

        const Def selDefs[5] = {
            { params::selBrightness,  "Dark/Bright",  spectral },
            { params::selDensity,     "Sparse/Dense", grain },
            { params::selHarmonicity, "Noise/Tone",   resonator },
            { params::selAggression,  "Calm/Fierce",  infection },
            { params::selDivergence,  "Diverge",      accent },
        };
        for (int i = 0; i < 5; ++i)
        {
            sel[(size_t) i] = std::make_unique<LabeledKnob> (s, selDefs[i].id, selDefs[i].name,
                                                             selDefs[i].c, false);
            addAndMakeVisible (*sel[(size_t) i]);
        }

        selHeader.setText ("SELECTION PRESSURE", juce::dontSendNotification);
        selHeader.setColour (juce::Label::textColourId, textDim);
        selHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        addAndMakeVisible (selHeader);
    }

    void EnvironmentPanel::resized()
    {
        auto r = contentArea();

        // five large controls
        auto bigRow = r.removeFromTop (96);
        const int bw = bigRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
            big[(size_t) i]->setBounds (bigRow.removeFromLeft (i == 4 ? bigRow.getWidth() : bw).reduced (3));

        r.removeFromTop (8);

        // deeper ecology: 5 x 2 grid
        auto grid = r.removeFromTop (150);
        const int cw = grid.getWidth() / 5;
        const int chh = grid.getHeight() / 2;
        for (int i = 0; i < 10; ++i)
        {
            const int col = i % 5, rowi = i / 5;
            small[(size_t) i]->setBounds (grid.getX() + col * cw,
                                          grid.getY() + rowi * chh, cw, chh);
        }

        r.removeFromTop (6);
        selHeader.setBounds (r.removeFromTop (16));
        auto selRow = r.removeFromTop (74);
        const int sw = selRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
            sel[(size_t) i]->setBounds (selRow.removeFromLeft (i == 4 ? selRow.getWidth() : sw));
    }
}
