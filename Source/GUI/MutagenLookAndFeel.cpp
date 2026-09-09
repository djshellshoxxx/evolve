#include "MutagenLookAndFeel.h"

namespace mutagen
{
    using namespace theme;

    MutagenLookAndFeel::MutagenLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg0);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOnId, bg0);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, stroke);
        setColour (juce::ComboBox::arrowColourId, spectral);
        setColour (juce::PopupMenu::backgroundColourId, bg1);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, spectral.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TooltipWindow::backgroundColourId, bg1);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::CaretComponent::caretColourId, spectral);
    }

    juce::Colour MutagenLookAndFeel::tintOf (juce::Component& c, juce::Colour fallback)
    {
        const auto v = c.getProperties()["tint"];
        if (v.isVoid()) return fallback;
        return juce::Colour ((juce::uint32) (int64_t) v);
    }

    juce::Font MutagenLookAndFeel::getLabelFont (juce::Label& l)
    {
        return { juce::jlimit (11.0f, 15.0f, (float) l.getHeight() * 0.7f), juce::Font::plain };
    }
    juce::Font MutagenLookAndFeel::getComboBoxFont (juce::ComboBox&) { return { 13.0f }; }
    juce::Font MutagenLookAndFeel::getPopupMenuFont() { return { 13.5f }; }

    void MutagenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                               float pos, float startAngle, float endAngle,
                                               juce::Slider& s)
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float angle  = startAngle + pos * (endAngle - startAngle);
        const auto tint    = tintOf (s, spectral);

        // outer well
        g.setColour (bg0.brighter (0.05f));
        g.fillEllipse (bounds);
        g.setColour (stroke.withAlpha (0.6f));
        g.drawEllipse (bounds, 1.0f);

        // track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                             0.0f, startAngle, endAngle, true);
        g.setColour (panelHi);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // glowing value arc
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                           0.0f, startAngle, angle, true);
        for (int i = 3; i >= 0; --i)
        {
            g.setColour (tint.withAlpha (i == 0 ? 0.95f : 0.12f));
            g.strokePath (val, juce::PathStrokeType (3.0f + i * 3.0f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // inner dial
        const auto inner = bounds.reduced (radius * 0.42f);
        juce::ColourGradient grad (panelHi.brighter (0.1f), inner.getTopLeft(),
                                   bg0, inner.getBottomRight(), false);
        g.setGradientFill (grad);
        g.fillEllipse (inner);
        g.setColour (tint.withAlpha (0.5f));
        g.drawEllipse (inner, 1.0f);

        // pointer
        juce::Point<float> tip (centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * (radius - 6.0f),
                                centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * (radius - 6.0f));
        juce::Point<float> root (centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * (radius * 0.42f),
                                 centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * (radius * 0.42f));
        g.setColour (accent);
        g.drawLine ({ root, tip }, 2.4f);
        g.setColour (tint);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tip));
    }

    void MutagenLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                               float pos, float, float,
                                               juce::Slider::SliderStyle style, juce::Slider& s)
    {
        const auto tint = tintOf (s, spectral);
        auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

        if (style == juce::Slider::LinearHorizontal)
        {
            auto tr = r.withSizeKeepingCentre (r.getWidth(), 5.0f);
            g.setColour (panelHi);
            g.fillRoundedRectangle (tr, 2.5f);
            auto fill = tr.withRight (pos);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (fill, 2.5f);
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ pos, tr.getCentreY() }));
        }
        else
        {
            auto tr = r.withSizeKeepingCentre (5.0f, r.getHeight());
            g.setColour (panelHi);
            g.fillRoundedRectangle (tr, 2.5f);
            auto fill = tr.withTop (pos);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (fill, 2.5f);
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ tr.getCentreX(), pos }));
        }
    }

    void MutagenLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                   const juce::Colour& bc, bool over, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto tint = tintOf (b, bc == theme::panel ? spectral : bc);
        const bool on = b.getToggleState();

        juce::Colour fill = on ? tint.withAlpha (0.9f)
                               : panel.brighter (over ? 0.12f : 0.0f);
        if (down) fill = fill.darker (0.15f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? tint : stroke.withAlpha (over ? 0.9f : 0.55f));
        g.drawRoundedRectangle (r, 5.0f, on ? 1.6f : 1.0f);

        if (over && ! on)
        {
            g.setColour (tint.withAlpha (0.10f));
            g.fillRoundedRectangle (r, 5.0f);
        }
    }

    void MutagenLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        g.setFont (juce::Font (12.5f, juce::Font::plain));
        const bool on = b.getToggleState();
        g.setColour (on ? bg0 : text);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2),
                          juce::Justification::centred, 1);
    }

    void MutagenLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool,
                                           int, int, int, int, juce::ComboBox& box)
    {
        auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (1.0f);
        g.setColour (panel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (stroke);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        juce::Path p;
        const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }

    void MutagenLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                               bool over, bool)
    {
        auto r = b.getLocalBounds().toFloat();
        auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
        const auto tint = tintOf (b, spectral);
        const bool on = b.getToggleState();

        g.setColour (on ? tint.withAlpha (0.9f) : panel);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (on ? tint : stroke.withAlpha (over ? 0.9f : 0.6f));
        g.drawRoundedRectangle (box, 4.0f, 1.2f);
        if (on)
        {
            g.setColour (bg0);
            juce::Path tick;
            tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
            tick.lineTo (box.getCentreX() - 1.0f, box.getBottom() - box.getHeight() * 0.28f);
            tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.26f);
            g.strokePath (tick, juce::PathStrokeType (2.0f));
        }

        g.setColour (text);
        g.setFont (12.5f);
        g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f),
                    juce::Justification::centredLeft, true);
    }
}
