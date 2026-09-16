#include "MutagenLookAndFeel.h"

namespace mutagen::theme
{
    /*  Resolve a face once from a preference list. A miss is not an error -
        every list ends at the platform sans / mono, which always exists. */
    static juce::String resolveFace (const juce::StringArray& preferences,
                                     const juce::String& lastResort)
    {
        const auto installed = juce::Font::findAllTypefaceNames();
        for (const auto& want : preferences)
            if (installed.contains (want))
                return want;
        return lastResort;
    }

    const juce::String& uiTypeface()
    {
        static const juce::String face = resolveFace (
            { "Inter", "Inter Display", "Space Grotesk", "Segoe UI Variable Text", "Segoe UI" },
            juce::Font::getDefaultSansSerifFontName());
        return face;
    }

    const juce::String& monoTypeface()
    {
        static const juce::String face = resolveFace (
            { "JetBrains Mono", "IBM Plex Mono", "Cascadia Mono", "Consolas" },
            juce::Font::getDefaultMonospacedFontName());
        return face;
    }

    juce::Font uiFont (float size, bool semibold)
    {
        return juce::Font (uiTypeface(), size, semibold ? juce::Font::bold : juce::Font::plain);
    }

    juce::Font monoFont (float size)
    {
        return juce::Font (monoTypeface(), size, juce::Font::plain);
    }

    juce::Font labelFont()
    {
        return uiFont (11.0f);
    }

    void drawTracked (juce::Graphics& g, const juce::String& textToDraw,
                      juce::Rectangle<int> area, juce::Justification just,
                      const juce::Font& font, float trackingEm)
    {
        if (textToDraw.isEmpty()) return;

        const float extra = font.getHeight() * trackingEm;

        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (font, textToDraw, 0.0f, 0.0f);

        // Push each glyph right by the accumulated tracking. JUCE positions
        // glyphs absolutely, so the shift has to be cumulative, not per-glyph.
        const int n = arrangement.getNumGlyphs();
        float shift = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            arrangement.getGlyph (i).moveBy (shift, 0.0f);
            shift += extra;
        }

        const auto bounds = arrangement.getBoundingBox (0, -1, true);
        const auto box = area.toFloat();

        float x = box.getX();
        if (just.testFlags (juce::Justification::horizontallyCentred))
            x = box.getCentreX() - bounds.getWidth() * 0.5f;
        else if (just.testFlags (juce::Justification::right))
            x = box.getRight() - bounds.getWidth();

        float y = box.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.25f;
        if (just.testFlags (juce::Justification::top))
            y = box.getY() + font.getAscent();
        else if (just.testFlags (juce::Justification::bottom))
            y = box.getBottom() - font.getDescent();

        arrangement.draw (g, juce::AffineTransform::translation (x - bounds.getX(), y));
    }

    void drawSectionHeader (juce::Graphics& g, juce::Rectangle<int> area,
                            const juce::String& title, juce::Colour barColour)
    {
        auto r = area;
        // 2px wide, 12px tall accent bar, then the label.
        auto barSlot = r.removeFromLeft (2);
        g.setColour (barColour);
        g.fillRect (barSlot.withSizeKeepingCentre (2, 12));

        r.removeFromLeft (gridUnit);
        g.setColour (text);
        drawTracked (g, title.toUpperCase(), r, juce::Justification::centredLeft,
                     uiFont (11.0f, true), 0.08f);
    }

    void drawSignatureNotch (juce::Graphics& g, juce::Rectangle<int> windowBounds,
                             juce::Colour c)
    {
        // 12px long, 45 degrees, 2px wide, inset from the corner.
        const float inset = 6.0f;
        const float run   = 12.0f / juce::MathConstants<float>::sqrt2;
        const auto x = (float) windowBounds.getX() + inset;
        const auto y = (float) windowBounds.getY() + inset;

        g.setColour (c);
        g.drawLine (x, y + run, x + run, y, 2.0f);
    }

    juce::Colour meterColour (float norm)
    {
        const float n = juce::jlimit (0.0f, 1.0f, norm);
        // teal (low) -> orange (nominal) -> yellow (near clip) -> red (clip)
        if (n < 0.55f)  return accent2.interpolatedWith (accent,  n / 0.55f);
        if (n < 0.82f)  return accent .interpolatedWith (warning, (n - 0.55f) / 0.27f);
        return warning.interpolatedWith (danger, (n - 0.82f) / 0.18f);
    }
}

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
        setColour (juce::TextButton::textColourOnId, accent);
        setColour (juce::TextButton::textColourOffId, textDim);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, stroke);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.15f));
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TooltipWindow::backgroundColourId, bg0);
        setColour (juce::TooltipWindow::textColourId, textDim);
        setColour (juce::TooltipWindow::outlineColourId, stroke);
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::TextEditor::backgroundColourId, bg1);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, stroke);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
        setColour (juce::ScrollBar::thumbColourId, stroke.brighter (0.2f));
        setColour (juce::ScrollBar::trackColourId, bg1);
    }

    juce::Colour MutagenLookAndFeel::tintOf (juce::Component& c, juce::Colour fallback)
    {
        const auto v = c.getProperties()["tint"];
        if (v.isVoid()) return fallback;
        return juce::Colour ((juce::uint32) (int64_t) v);
    }

    juce::Font MutagenLookAndFeel::getLabelFont (juce::Label& l)
    {
        return uiFont (juce::jlimit (11.0f, 15.0f, (float) l.getHeight() * 0.7f));
    }
    juce::Font MutagenLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (12.0f); }
    juce::Font MutagenLookAndFeel::getPopupMenuFont()                { return uiFont (13.0f); }
    juce::Font MutagenLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (11.0f); }

    // =====================================================================
    //  Knob: flat-shaded body, indicator line, 270deg value arc drawn
    //  outside the body with a 4px gap.
    // =====================================================================

    void MutagenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                               float pos, float startAngle, float endAngle,
                                               juce::Slider& s)
    {
        const auto tint  = tintOf (s, accent);
        const auto area  = juce::Rectangle<int> (x, y, w, h).toFloat();

        // The arc lives outside the body, so the body has to give up the arc
        // thickness plus the gap on every side.
        const float outer     = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        const float arcRadius = outer - arcThickness * 0.5f;
        const float bodyR     = juce::jmax (6.0f, arcRadius - arcThickness * 0.5f - arcGap);
        const auto  centre    = area.getCentre();
        const auto  body      = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);

        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool  atDefault = std::abs (s.getValue() - s.getDoubleClickReturnValue()) < 1.0e-6
                                && s.isDoubleClickReturnEnabled();

        // ---- drop shadow: 8px blur, y+2 ------------------------------
        {
            juce::DropShadow ds (shadow, 8, { 0, 2 });
            juce::Path bodyPath;
            bodyPath.addEllipse (body);
            ds.drawForPath (g, bodyPath);
        }

        // ---- body: radial gradient #232833 (top) -> #14181F (bottom) ---
        juce::ColourGradient grad (juce::Colour (0xff232833), centre.x, body.getY(),
                                   juce::Colour (0xff14181f), centre.x, body.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (body);
        g.setColour (stroke);
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        // ---- unfilled arc ---------------------------------------------
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                             0.0f, startAngle, endAngle, true);
        g.setColour (stroke.withAlpha (0.6f));
        g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ---- filled arc ------------------------------------------------
        if (std::abs (angle - startAngle) > 1.0e-4f)
        {
            juce::Path val;
            val.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                               0.0f, startAngle, angle, true);
            g.setColour (tint);
            g.strokePath (val, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // ---- indicator: 2px line, centre to rim, rounded cap ----------
        const float ca = std::cos (angle - juce::MathConstants<float>::halfPi);
        const float sa = std::sin (angle - juce::MathConstants<float>::halfPi);
        g.setColour (tint);
        g.drawLine (centre.x, centre.y,
                    centre.x + ca * (bodyR - 2.0f),
                    centre.y + sa * (bodyR - 2.0f),
                    2.0f);

        // ---- centre dot: 4px, muted while the knob sits at its default --
        g.setColour (atDefault ? textDim.withAlpha (0.55f) : tint);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));
    }

    // =====================================================================
    //  Slider: 4px track, accent fill, 16x24 thumb.
    // =====================================================================

    void MutagenLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                               float pos, float minPos, float maxPos,
                                               juce::Slider::SliderStyle style, juce::Slider& s)
    {
        juce::ignoreUnused (minPos, maxPos);

        const auto tint = tintOf (s, accent);
        const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
        const bool horizontal = (style == juce::Slider::LinearHorizontal
                                 || style == juce::Slider::LinearBar);

        auto drawThumb = [&] (juce::Point<float> centre)
        {
            auto thumb = juce::Rectangle<float> (horizontal ? 16.0f : 24.0f,
                                                 horizontal ? 24.0f : 16.0f)
                             .withCentre (centre);
            if (! horizontal) thumb = juce::Rectangle<float> (24.0f, 16.0f).withCentre (centre);

            juce::DropShadow ds (shadow, 8, { 0, 2 });
            juce::Path p;
            p.addRoundedRectangle (thumb, radiusPanel);
            ds.drawForPath (g, p);

            juce::ColourGradient grad (juce::Colour (0xff232833), centre.x, thumb.getY(),
                                       juce::Colour (0xff14181f), centre.x, thumb.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (thumb, radiusPanel);
            g.setColour (tint);
            g.drawRoundedRectangle (thumb.reduced (0.5f), radiusPanel, 1.0f);
        };

        if (horizontal)
        {
            auto track = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
            g.setColour (stroke);
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (tint);
            g.fillRoundedRectangle (track.withRight (pos), 2.0f);
            drawThumb ({ pos, track.getCentreY() });
        }
        else
        {
            auto track = r.withSizeKeepingCentre (4.0f, r.getHeight());
            g.setColour (stroke);
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (tint);
            g.fillRoundedRectangle (track.withTop (pos), 2.0f);
            drawThumb ({ track.getCentreX(), pos });
        }
    }

    // =====================================================================
    //  Buttons: 4px radius. Off = panel + muted text. On = 15% accent fill,
    //  accent text, 1px accent border.
    // =====================================================================

    void MutagenLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                   const juce::Colour& bc, bool over, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const auto tint = tintOf (b, bc == theme::panel ? accent : bc);
        const bool on = b.getToggleState();

        if (on)
        {
            g.setColour (tint.withAlpha (0.15f));
            g.fillRoundedRectangle (r, radiusPanel);
            g.setColour (tint);
            g.drawRoundedRectangle (r, radiusPanel, 1.0f);
        }
        else
        {
            // Hover brightens the surface by about 8%.
            g.setColour (over ? panel.brighter (0.08f) : panel);
            g.fillRoundedRectangle (r, radiusPanel);
            g.setColour (over ? tint.withAlpha (0.55f) : stroke);
            g.drawRoundedRectangle (r, radiusPanel, 1.0f);
        }

        // Momentary press: a brief accent flash. JUCE redraws on release, so
        // the held-down state is what carries it.
        if (down)
        {
            g.setColour (tint.withAlpha (0.28f));
            g.fillRoundedRectangle (r, radiusPanel);
        }
    }

    void MutagenLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                             bool over, bool down)
    {
        juce::ignoreUnused (down);
        const bool on = b.getToggleState();
        const auto tint = tintOf (b, accent);

        g.setColour (on ? tint : (over ? text : textDim));
        drawTracked (g, b.getButtonText().toUpperCase(),
                     b.getLocalBounds().reduced (gridUnit, 0),
                     juce::Justification::centred, uiFont (11.0f), 0.08f);
    }

    void MutagenLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down,
                                           int, int, int, int, juce::ComboBox& box)
    {
        auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
        const bool over = box.isMouseOver();

        g.setColour (over ? panel.brighter (0.08f) : panel);
        g.fillRoundedRectangle (r, radiusPanel);
        g.setColour (down || over ? accent.withAlpha (0.55f) : stroke);
        g.drawRoundedRectangle (r, radiusPanel, 1.0f);

        // A chevron, not a filled triangle - lighter at this size.
        const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 3.5f, cy - 2.0f);
        p.lineTo (cx, cy + 2.0f);
        p.lineTo (cx + 3.5f, cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    void MutagenLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                               bool over, bool)
    {
        auto r = b.getLocalBounds().toFloat();
        auto box = r.removeFromLeft (juce::jmin (r.getHeight(), 18.0f)).withSizeKeepingCentre (16.0f, 16.0f);
        const auto tint = tintOf (b, accent);
        const bool on = b.getToggleState();

        g.setColour (on ? tint.withAlpha (0.15f) : panel);
        g.fillRoundedRectangle (box, radiusControl);
        g.setColour (on ? tint : (over ? tint.withAlpha (0.55f) : stroke));
        g.drawRoundedRectangle (box.reduced (0.5f), radiusControl, 1.0f);

        if (on)
        {
            g.setColour (tint);
            juce::Path tick;
            tick.startNewSubPath (box.getX() + box.getWidth() * 0.24f, box.getCentreY());
            tick.lineTo (box.getCentreX() - 1.0f, box.getBottom() - box.getHeight() * 0.28f);
            tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.26f);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        g.setColour (on ? text : textDim);
        drawTracked (g, b.getButtonText().toUpperCase(), r.toNearestInt().withTrimmedLeft (gridUnit),
                     juce::Justification::centredLeft, uiFont (11.0f), 0.08f);
    }

    // =====================================================================

    void MutagenLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int w, int h)
    {
        // A dark pill with muted text.
        auto r = juce::Rectangle<float> ((float) w, (float) h);
        g.setColour (bg0.withAlpha (0.97f));
        g.fillRoundedRectangle (r.reduced (0.5f), h * 0.5f);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), h * 0.5f, 1.0f);

        g.setColour (textDim);
        g.setFont (uiFont (11.5f));
        g.drawFittedText (tip, juce::Rectangle<int> (w, h).reduced (gridUnit + 2, 2),
                          juce::Justification::centred, 3);
    }

    void MutagenLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        auto r = juce::Rectangle<float> ((float) w, (float) h);
        g.setColour (panel);
        g.fillRoundedRectangle (r, radiusPanel);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radiusPanel, 1.0f);
    }
}
