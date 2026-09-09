#include "PerformanceView.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    using namespace theme;

    // =====================================================================
    class PerformanceView::XYPad : public juce::Component
    {
    public:
        XYPad (juce::Slider& xs, juce::Slider& ys) : xSlider (xs), ySlider (ys) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            juce::ColourGradient bgg (bg1, r.getCentre(), bg0, r.getBottomRight(), true);
            g.setGradientFill (bgg);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (stroke.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

            for (int i = 1; i < 4; ++i)
            {
                g.setColour (stroke.withAlpha (0.15f));
                g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4.0f), r.getY(), r.getBottom());
                g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
            }

            const float x = (float) xSlider.getValue();
            const float y = (float) ySlider.getValue();
            juce::Point<float> p (r.getX() + x * r.getWidth(),
                                  r.getBottom() - y * r.getHeight());

            g.setColour (spectralV.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (46, 46).withCentre (p));
            g.setColour (accent);
            g.drawLine (p.x, r.getY(), p.x, r.getBottom(), 1.0f);
            g.drawLine (r.getX(), p.y, r.getRight(), p.y, 1.0f);
            g.setColour (spectralV);
            g.fillEllipse (juce::Rectangle<float> (14, 14).withCentre (p));

            g.setColour (textDim);
            g.setFont (10.0f);
            g.drawText ("stability", r.reduced (6).removeFromBottom (12), juce::Justification::centred);
            g.drawText ("aggression", r.reduced (6).removeFromLeft (12),
                        juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override { drag (e); }
        void mouseDrag (const juce::MouseEvent& e) override { drag (e); }

    private:
        void drag (const juce::MouseEvent& e)
        {
            auto r = getLocalBounds().toFloat();
            xSlider.setValue (juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth()),
                              juce::sendNotificationSync);
            ySlider.setValue (juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - r.getY()) / r.getHeight()),
                              juce::sendNotificationSync);
            repaint();
        }
        juce::Slider& xSlider;
        juce::Slider& ySlider;
    };

    // =====================================================================

    PerformanceView::PerformanceView (MutagenProcessor& p)
        : PanelFrame ("Performance"), processor (p),
          keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        accentColour = accent;

        struct Def { const char* id; const char* name; juce::Colour c; };
        const Def defs[8] = {
            { params::macroGrowth,   "GROWTH",   nutrient },
            { params::macroMutation, "MUTATION", spectralV },
            { params::macroStress,   "STRESS",   infection },
            { params::macroDensity,  "DENSITY",  grain },
            { params::macroBody,     "BODY",     resonator },
            { params::macroVoice,    "VOICE",    spectral },
            { params::macroMovement, "MOVEMENT", resonator },
            { params::macroDecay,    "DECAY",    grain },
        };
        for (int i = 0; i < 8; ++i)
        {
            macros[(size_t) i] = std::make_unique<LabeledKnob> (p.apvts, defs[i].id, defs[i].name,
                                                                defs[i].c, true);
            addAndMakeVisible (*macros[(size_t) i]);
        }

        for (auto* s : { &xSlider, &ySlider })
        {
            s->setRange (0.0, 1.0, 0.0001);
            s->setVisible (false);
            addChildComponent (s);
        }
        xAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, params::xyStability, xSlider);
        yAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, params::xyRepro, ySlider);

        pad = std::make_unique<XYPad> (xSlider, ySlider);
        addAndMakeVisible (*pad);
        xSlider.onValueChange = [this] { pad->repaint(); };
        ySlider.onValueChange = [this] { pad->repaint(); };

        keyboard.setKeyWidth (22.0f);
        keyboard.setLowestVisibleKey (36);
        addAndMakeVisible (keyboard);

        xyCaption.setText ("XY  -  reproductive aggression / stability", juce::dontSendNotification);
        xyCaption.setColour (juce::Label::textColourId, textDim);
        xyCaption.setFont (11.0f);
        addAndMakeVisible (xyCaption);

        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeBtn);
    }

    PerformanceView::~PerformanceView() = default;

    void PerformanceView::resized()
    {
        auto r = contentArea();
        closeBtn.setBounds (getLocalBounds().removeFromTop (24).removeFromRight (70).reduced (4, 2));

        auto keys = r.removeFromBottom (96);
        keyboard.setBounds (keys);
        r.removeFromBottom (10);

        auto knobArea = r.removeFromTop (juce::jmax (150, r.getHeight() - 260));
        const int cols = 4;
        const int kw = knobArea.getWidth() / cols;
        const int kh = knobArea.getHeight() / 2;
        for (int i = 0; i < 8; ++i)
        {
            const int cx = i % cols, cy = i / cols;
            macros[(size_t) i]->setBounds (knobArea.getX() + cx * kw,
                                           knobArea.getY() + cy * kh, kw, kh);
        }

        r.removeFromTop (10);
        xyCaption.setBounds (r.removeFromTop (16));
        const int side = juce::jmin (r.getWidth(), r.getHeight());
        pad->setBounds (r.withSizeKeepingCentre (side, side));
    }
}
