#include "ScoreHud.h"
#include "MutagenLookAndFeel.h"
#include <cmath>

namespace mutagen
{
    using namespace theme;

    // =====================================================================
    //  FractalGhost
    // =====================================================================

    void FractalGhost::trigger (juce::Point<float> origin, float hue, juce::Random& rnd)
    {
        root = origin;
        hueValue = hue;
        life = 1.0f;
        // Re-rolled per appearance so the reward is never quite the same shape.
        spread = rnd.nextFloat() * 0.55f + 0.35f;
        twist  = (rnd.nextFloat() - 0.5f) * 0.6f;
        arms   = 3 + rnd.nextInt (4);
        depthLimit = 6 + rnd.nextInt (3);
        seedAngle = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    void FractalGhost::update (float dt)
    {
        if (life <= 0.0f) return;
        // Just over a second, front-loaded: it arrives fast and lingers faintly.
        life -= dt * 0.85f;
        if (life < 0.0f) life = 0.0f;
    }

    void FractalGhost::drawBranch (juce::Graphics& g, juce::Point<float> p, float angle,
                                   float length, int depth, float alpha) const
    {
        if (depth <= 0 || length < 2.0f || alpha < 0.01f) return;

        const juce::Point<float> end { p.x + std::cos (angle) * length,
                                       p.y + std::sin (angle) * length };

        g.setColour (juce::Colour::fromHSV (hueValue, 0.35f, 1.0f, alpha));
        g.drawLine (p.x, p.y, end.x, end.y, juce::jmax (0.6f, length * 0.035f));

        // A node at each junction; the accumulation of these is what reads as
        // "fractal" rather than "a tree".
        if (depth > 2)
        {
            const float r = length * 0.05f;
            g.setColour (juce::Colour::fromHSV (hueValue, 0.2f, 1.0f, alpha * 0.8f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (end));
        }

        const float childLen = length * (0.62f + spread * 0.12f);
        const float half = spread * 0.9f;

        drawBranch (g, end, angle - half + twist, childLen, depth - 1, alpha * 0.82f);
        drawBranch (g, end, angle + half + twist, childLen, depth - 1, alpha * 0.82f);
        if (depth > 4)
            drawBranch (g, end, angle + twist * 0.5f, childLen * 0.8f, depth - 2, alpha * 0.6f);
    }

    void FractalGhost::render (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (life <= 0.0f) return;

        // Bright on arrival, ghostly on the way out.
        const float a = std::pow (life, 0.7f) * 0.85f;
        const juce::Point<float> origin { area.getX() + root.x * area.getWidth(),
                                          area.getY() + root.y * area.getHeight() };
        const float baseLen = juce::jmin (area.getWidth(), area.getHeight()) * 0.16f
                            * (0.7f + 0.5f * (1.0f - life));

        for (int i = 0; i < arms; ++i)
        {
            const float ang = seedAngle + juce::MathConstants<float>::twoPi * (float) i / (float) arms;
            drawBranch (g, origin, ang, baseLen, depthLimit, a);
        }

        // A soft halo so it reads as a flash and not just line-work.
        g.setColour (juce::Colour::fromHSV (hueValue, 0.3f, 1.0f, a * 0.10f));
        const float haloR = baseLen * 3.4f;
        g.fillEllipse (juce::Rectangle<float> (haloR * 2.0f, haloR * 2.0f).withCentre (origin));
    }

    // =====================================================================
    //  ScoreHud
    // =====================================================================

    ScoreHud::ScoreHud()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
    }

    void ScoreHud::setState (const ScoreSystem& score, const EngineSnapshot& snap)
    {
        targetScore = score.score();

        // Ease the displayed figure toward the real one so a big award rolls
        // up rather than jumping - it is most of what makes a score feel good.
        const auto diff = targetScore - displayedScore;
        if (std::abs (diff) < 3) displayedScore = targetScore;
        else                     displayedScore += (juce::int64) (diff * 0.25);

        multiplier = score.multiplier();
        combo      = score.combo();
        frozen     = score.frozen();
        freezeReason = score.frozenReason();
        rate       = score.rate();
        rank       = score.projectedRank();
        discoveries = score.discoveries();
        elapsed    = score.elapsed();
        events     = score.events();
        table      = score.table();

        variety  = snap.variety;
        greyness = snap.greyness;
        appeal   = snap.appeal;
        coverage = snap.coverage;
        worldName = juce::String (snap.worldName);

        flashPhase += 0.05f;
        repaint();
    }

    bool ScoreHud::hitsInteractive (juce::Point<int> p) const
    {
        const auto fp = p.toFloat();
        return scoreArea.contains (fp) || tableButton.contains (fp)
            || (showTable && getLocalBounds().toFloat().reduced (60.0f).contains (fp));
    }

    bool ScoreHud::hitTest (int x, int y)
    {
        // This component spans the whole CultureChamber so it can draw the
        // score and high-score overlay. It must not therefore claim the whole
        // chamber as mouse input: clicks outside the HUD's own controls belong
        // to CultureChamber's mutation/damage gestures underneath.
        return hitsInteractive ({ x, y });
    }

    void ScoreHud::mouseDown (const juce::MouseEvent& e)
    {
        if (tableButton.contains (e.position) || (showTable && ! scoreArea.contains (e.position)))
        {
            showTable = ! showTable;
            if (onTableToggled) onTableToggled();
            repaint();
        }
    }

    // ---------------------------------------------------------------------

    void ScoreHud::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();

        // ---- the score panel ----------------------------------------------
        auto card = b.removeFromTop (92.0f).removeFromRight (330.0f).reduced (10.0f, 8.0f);
        scoreArea = card;

        g.setColour (bg0.withAlpha (0.72f));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (frozen ? juce::Colour (0xffff6b6b).withAlpha (0.8f)
                            : stroke.withAlpha (0.35f));
        g.drawRoundedRectangle (card, 8.0f, frozen ? 2.0f : 1.0f);

        auto inner = card.reduced (12.0f, 8.0f);

        // world name - the run's identity
        g.setColour (text.withAlpha (0.45f));
        g.setFont (10.0f);
        g.drawText (worldName + (rank > 0 ? "   #" + juce::String (rank) : juce::String()),
                    inner.removeFromTop (12.0f), juce::Justification::centredLeft);

        // the number
        auto scoreRow = inner.removeFromTop (34.0f);
        {
            juce::String s (displayedScore);
            // thousands separators, because a bare seven-digit run is unreadable
            for (int i = s.length() - 3; i > 0; i -= 3)
                s = s.substring (0, i) + "," + s.substring (i);

            const bool pulsing = ! frozen && rate > 100.0f;
            g.setColour (frozen ? juce::Colour (0xff8a8a8a)
                                : accent.brighter (pulsing ? 0.15f * std::sin (flashPhase * 4.0f) + 0.15f : 0.0f));
            g.setFont (juce::Font (juce::FontOptions (30.0f).withStyle ("Bold")));
            g.drawText (s, scoreRow, juce::Justification::centredRight);
        }

        // multiplier + combo
        auto multRow = inner.removeFromTop (14.0f);
        if (multiplier > 1.02f || combo > 0)
        {
            g.setFont (11.0f);
            g.setColour (multiplier > 2.0f ? juce::Colour (0xffffd36e) : text.withAlpha (0.75f));
            g.drawText ("x" + juce::String (multiplier, 2)
                        + (combo > 0 ? "   combo " + juce::String (combo) : juce::String()),
                        multRow, juce::Justification::centredRight);
        }

        paintMeters (g, inner.removeFromTop (16.0f));

        // ---- frozen banner --------------------------------------------------
        if (frozen && freezeReason.isNotEmpty())
        {
            auto banner = juce::Rectangle<float> (card.getX() - 160.0f, card.getBottom() + 6.0f,
                                                  card.getWidth() + 160.0f, 22.0f);
            const float pulse = 0.55f + 0.45f * std::sin (flashPhase * 3.0f);
            g.setColour (juce::Colour (0xffff6b6b).withAlpha (0.16f * pulse));
            g.fillRoundedRectangle (banner, 5.0f);
            g.setColour (juce::Colour (0xffff9a9a).withAlpha (0.65f + 0.35f * pulse));
            g.setFont (12.0f);
            g.drawText (freezeReason, banner, juce::Justification::centred);
        }

        // ---- the table toggle ------------------------------------------------
        tableButton = juce::Rectangle<float> (scoreArea.getX() - 74.0f, scoreArea.getY(), 66.0f, 20.0f);
        g.setColour (bg0.withAlpha (0.7f));
        g.fillRoundedRectangle (tableButton, 4.0f);
        g.setColour (stroke.withAlpha (0.4f));
        g.drawRoundedRectangle (tableButton, 4.0f, 1.0f);
        g.setColour (text.withAlpha (0.8f));
        g.setFont (10.0f);
        g.drawText ("SCORES", tableButton, juce::Justification::centred);

        // ---- event toasts -----------------------------------------------------
        {
            auto feedArea = juce::Rectangle<float> (scoreArea.getX(), scoreArea.getBottom() + 34.0f,
                                                    scoreArea.getWidth(), 150.0f);
            float y = feedArea.getY();
            for (auto it = events.rbegin(); it != events.rend(); ++it)
            {
                const auto& e = *it;
                const float a = juce::jlimit (0.0f, 1.0f, e.life);
                auto row = juce::Rectangle<float> (feedArea.getX(), y - e.y * 0.35f,
                                                   feedArea.getWidth(), 17.0f);
                g.setColour (e.colour.withAlpha (a * 0.9f));
                g.setFont (12.0f);
                g.drawText (e.text, row, juce::Justification::centredRight);

                if (e.points > 0)
                {
                    g.setColour (e.colour.withAlpha (a * 0.55f));
                    g.setFont (10.0f);
                    g.drawText ("+" + juce::String (e.points),
                                row.translated (0.0f, 11.0f), juce::Justification::centredRight);
                    y += 12.0f;
                }
                y += 18.0f;
            }
        }

        if (showTable)
            paintTable (g, getLocalBounds().toFloat());
    }

    // ---------------------------------------------------------------------

    void ScoreHud::paintMeters (juce::Graphics& g, juce::Rectangle<float> area)
    {
        /*  Two bars, because they are the two things the score actually reads.
            VARIETY is what makes the number climb; NOISE is what stops it. The
            player should be able to learn the whole scoring rule from these. */
        auto left = area.removeFromLeft (area.getWidth() * 0.5f).reduced (0.0f, 3.0f);
        auto right = area.reduced (0.0f, 3.0f);
        left.removeFromRight (6.0f);

        auto bar = [&g] (juce::Rectangle<float> r, float v, juce::Colour c, const char* label)
        {
            g.setColour (panelHi.withAlpha (0.6f));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (c.withAlpha (0.85f));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 2.0f);
            g.setColour (text.withAlpha (0.55f));
            g.setFont (8.5f);
            g.drawText (label, r, juce::Justification::centredLeft);
        };

        // variety fades toward grey exactly as the visuals do, so the meter
        // and the chamber always agree
        const auto varietyColour = juce::Colour::fromHSV (0.42f, 0.75f * (1.0f - greyness), 0.95f, 1.0f);
        bar (left, variety, varietyColour, " VARIETY");
        bar (right, greyness, juce::Colour (0xffb06060), " NOISE");
    }

    void ScoreHud::paintTable (juce::Graphics& g, juce::Rectangle<float> full)
    {
        auto sheet = full.reduced (full.getWidth() * 0.22f, full.getHeight() * 0.14f);

        g.setColour (bg0.withAlpha (0.93f));
        g.fillRoundedRectangle (sheet, 10.0f);
        g.setColour (accent.withAlpha (0.4f));
        g.drawRoundedRectangle (sheet, 10.0f, 1.5f);

        auto inner = sheet.reduced (22.0f, 18.0f);

        g.setColour (accent);
        g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
        g.drawText ("HIGH SCORES", inner.removeFromTop (26.0f), juce::Justification::centredLeft);

        g.setColour (text.withAlpha (0.4f));
        g.setFont (10.0f);
        g.drawText ("click anywhere to close", inner.removeFromTop (14.0f),
                    juce::Justification::centredLeft);
        inner.removeFromTop (8.0f);

        // header
        {
            auto row = inner.removeFromTop (16.0f);
            g.setColour (text.withAlpha (0.45f));
            g.setFont (10.0f);
            g.drawText ("#", row.removeFromLeft (26.0f), juce::Justification::centredLeft);
            g.drawText ("WORLD", row.removeFromLeft (150.0f), juce::Justification::centredLeft);
            g.drawText ("TIME", row.removeFromRight (62.0f), juce::Justification::centredRight);
            g.drawText ("FOUND", row.removeFromRight (56.0f), juce::Justification::centredRight);
            g.drawText ("SCORE", row.removeFromRight (90.0f), juce::Justification::centredRight);
        }

        g.setColour (stroke.withAlpha (0.25f));
        g.drawHorizontalLine ((int) inner.getY(), inner.getX(), inner.getRight());
        inner.removeFromTop (4.0f);

        if (table.empty())
        {
            g.setColour (text.withAlpha (0.4f));
            g.setFont (12.0f);
            g.drawText ("no runs recorded yet - press SAVE RUN to file one",
                        inner.removeFromTop (30.0f), juce::Justification::centred);
            return;
        }

        int i = 1;
        for (const auto& h : table)
        {
            if (inner.getHeight() < 18.0f) break;
            auto row = inner.removeFromTop (18.0f);

            const bool top = i == 1;
            g.setColour (top ? juce::Colour (0xffffd36e) : text.withAlpha (0.85f));
            g.setFont (top ? 13.0f : 12.0f);

            g.drawText (juce::String (i), row.removeFromLeft (26.0f), juce::Justification::centredLeft);
            g.drawText (h.world, row.removeFromLeft (150.0f), juce::Justification::centredLeft);

            const int mins = (int) (h.seconds / 60.0);
            const int secs = (int) h.seconds % 60;
            g.setColour (text.withAlpha (0.5f));
            g.setFont (11.0f);
            g.drawText (juce::String (mins) + ":" + juce::String (secs).paddedLeft ('0', 2),
                        row.removeFromRight (62.0f), juce::Justification::centredRight);
            g.drawText (juce::String (h.discoveries),
                        row.removeFromRight (56.0f), juce::Justification::centredRight);

            g.setColour (top ? juce::Colour (0xffffd36e) : accent.withAlpha (0.9f));
            g.setFont (12.0f);
            juce::String sc (h.score);
            for (int k = sc.length() - 3; k > 0; k -= 3)
                sc = sc.substring (0, k) + "," + sc.substring (k);
            g.drawText (sc, row.removeFromRight (90.0f), juce::Justification::centredRight);

            ++i;
        }
    }
}
