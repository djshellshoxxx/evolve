#include "EnvironmentPanel.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    namespace
    {
        struct SteerDef
        {
            const char* label;
            steering::Direction direction;
            const char* tip;
        };

        constexpr SteerDef steerDefs[8] = {
            { "DARK",   steering::Direction::dark,   "Push evolution toward a darker spectrum." },
            { "BRIGHT", steering::Direction::bright, "Push evolution toward a brighter spectrum." },
            { "SPARSE", steering::Direction::sparse, "Push evolution toward fewer, more separated elements." },
            { "DENSE",  steering::Direction::dense,  "Push evolution toward a denser sound." },
            { "NOISE",  steering::Direction::noise,  "Push evolution toward noisier, less harmonic material." },
            { "TONE",   steering::Direction::tone,   "Push evolution toward clearer harmonic material." },
            { "CALM",   steering::Direction::calm,   "Push evolution toward smoother, less aggressive behaviour." },
            { "FIERCE", steering::Direction::fierce, "Push evolution toward rougher, more aggressive behaviour." },
        };
    }

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

        steerHeader.setText ("PUSH THE SOUND", juce::dontSendNotification);
        steerHeader.setColour (juce::Label::textColourId, textDim);
        steerHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        addAndMakeVisible (steerHeader);

        for (int i = 0; i < 8; ++i)
        {
            steer[(size_t) i] = std::make_unique<juce::TextButton> (steerDefs[i].label);
            steer[(size_t) i]->setTooltip (steerDefs[i].tip);
            steer[(size_t) i]->onClick = [this, i]
            {
                applySteeringProfile (steering::directionProfile (steerDefs[i].direction), 0.82f, false);
            };
            addAndMakeVisible (*steer[(size_t) i]);
        }

        counterButton.setTooltip (
            "Listen to the current result and push evolution away from its measured brightness, tone and roughness.");
        counterButton.onClick = [this] { runCounterEvolve(); };
        addAndMakeVisible (counterButton);

        diceButton.setTooltip (
            "Throw a new four-axis selection target, add divergence, then force a mutation pass.");
        diceButton.onClick = [this] { rollMutationDice(); };
        addAndMakeVisible (diceButton);
    }

    void EnvironmentPanel::applySteeringProfile (const steering::Profile& profile,
                                                  float strength,
                                                  bool mutateAfter)
    {
        auto setActual = [this] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                parameter->endChangeGesture();
            }
        };

        setActual (params::selBrightness,  profile.brightness);
        setActual (params::selDensity,     profile.density);
        setActual (params::selHarmonicity, profile.harmonicity);
        setActual (params::selAggression,  profile.aggression);
        setActual (params::selDivergence,  profile.divergence);

        if (auto* selectionParam = processor.apvts.getParameter (params::selection))
        {
            const float current = *processor.apvts.getRawParameterValue (params::selection);
            if (current < 0.72f)
            {
                selectionParam->beginChangeGesture();
                selectionParam->setValueNotifyingHost (selectionParam->convertTo0to1 (0.72f));
                selectionParam->endChangeGesture();
            }
        }

        EngineCommand select;
        select.type = CommandType::applySelection;
        select.fa = juce::jlimit (0.1f, 1.0f, strength);
        processor.pushCommand (select);

        if (mutateAfter)
        {
            EngineCommand mutate;
            mutate.type = CommandType::mutateNow;
            processor.pushCommand (mutate);
        }

        processor.noteUserGesture ((profile.brightness + 1.0f) * 0.5f,
                                   (profile.harmonicity + 1.0f) * 0.5f);
    }

    void EnvironmentPanel::runCounterEvolve()
    {
        EngineSnapshot snap;
        processor.copyLatestSnapshot (snap);
        applySteeringProfile (
            steering::counterProfile (snap.centroid, snap.tonalness, snap.roughness),
            0.9f, true);
    }

    void EnvironmentPanel::rollMutationDice()
    {
        const auto roll = (uint32_t) juce::Random::getSystemRandom().nextInt();
        const auto profile = steering::diceProfile (roll);

        // DICE is intentionally more disruptive than a normal directional
        // push, but it still stays inside the existing safe parameter ranges.
        if (auto* mutationParam = processor.apvts.getParameter (params::mutation))
        {
            const float amount = 0.38f + 0.34f
                               * ((float) (steering::mixBits (roll ^ 0x51ed270bu) & 0xffffu) / 65535.0f);
            mutationParam->beginChangeGesture();
            mutationParam->setValueNotifyingHost (mutationParam->convertTo0to1 (amount));
            mutationParam->endChangeGesture();
        }

        if (auto* depthParam = processor.apvts.getParameter (params::mutationDepth))
        {
            const float depth = 0.30f + 0.48f
                              * ((float) (steering::mixBits (roll ^ 0x68bc21ebu) & 0xffffu) / 65535.0f);
            depthParam->beginChangeGesture();
            depthParam->setValueNotifyingHost (depthParam->convertTo0to1 (depth));
            depthParam->endChangeGesture();
        }

        applySteeringProfile (profile, 0.95f, true);
    }

    void EnvironmentPanel::resized()
    {
        auto r = contentArea();

        // The original panel used almost all available height. These rows are
        // slightly tighter so the steering controls fit even at the minimum
        // editor size without overlapping the game bar below.
        auto bigRow = r.removeFromTop (82);
        const int bw = bigRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
            big[(size_t) i]->setBounds (bigRow.removeFromLeft (i == 4 ? bigRow.getWidth() : bw).reduced (2));

        r.removeFromTop (4);

        auto grid = r.removeFromTop (124);
        const int cw = grid.getWidth() / 5;
        const int chh = grid.getHeight() / 2;
        for (int i = 0; i < 10; ++i)
        {
            const int col = i % 5, rowi = i / 5;
            small[(size_t) i]->setBounds (grid.getX() + col * cw,
                                          grid.getY() + rowi * chh, cw, chh);
        }

        r.removeFromTop (3);
        selHeader.setBounds (r.removeFromTop (14));
        auto selRow = r.removeFromTop (58);
        const int sw = selRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
            sel[(size_t) i]->setBounds (selRow.removeFromLeft (i == 4 ? selRow.getWidth() : sw));

        r.removeFromTop (3);
        steerHeader.setBounds (r.removeFromTop (14));

        for (int row = 0; row < 2; ++row)
        {
            auto buttonRow = r.removeFromTop (22);
            const int w = buttonRow.getWidth() / 4;
            for (int col = 0; col < 4; ++col)
            {
                const int i = row * 4 + col;
                steer[(size_t) i]->setBounds (
                    buttonRow.removeFromLeft (col == 3 ? buttonRow.getWidth() : w).reduced (1));
            }
        }

        r.removeFromTop (2);
        auto actionRow = r.removeFromTop (24);
        const int half = actionRow.getWidth() / 2;
        counterButton.setBounds (actionRow.removeFromLeft (half).reduced (1));
        diceButton.setBounds (actionRow.reduced (1));
    }
}
