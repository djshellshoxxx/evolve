#include "GameBar.h"
#include "MutagenLookAndFeel.h"
#include <cmath>

namespace mutagen
{
    using namespace theme;

    // =====================================================================
    //  GestureKnob
    // =====================================================================

    GestureKnob::GestureKnob (juce::String labelText, juce::Colour accentColour)
        : label (std::move (labelText)), colour (accentColour)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        startTimerHz (40);
    }

    void GestureKnob::timerCallback()
    {
        bool dirty = false;

        // Spring back to centre when released: there is no setting to hold.
        if (! dragging && std::fabs (angle) > 0.001f)
        {
            angle *= 0.82f;
            if (std::fabs (angle) < 0.001f) angle = 0.0f;
            dirty = true;
        }
        if (glow > 0.001f) { glow *= 0.90f; dirty = true; }
        if (speedEstimate > 0.001f) speedEstimate *= 0.90f;

        if (dirty) repaint();
    }

    void GestureKnob::mouseDown (const juce::MouseEvent& e)
    {
        dragging = true;
        lastY = e.position.y;
        lastEmitTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        speedEstimate = 0.0f;
    }

    void GestureKnob::mouseDrag (const juce::MouseEvent& e)
    {
        const float dy = lastY - e.position.y;        // up = positive
        if (std::fabs (dy) < 2.0f) return;

        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double dt = juce::jmax (0.001, now - lastEmitTime);

        // px/sec, normalised against a brisk-but-not-frantic turn
        const float instantaneous = juce::jlimit (0.0f, 1.0f, (float) (std::fabs (dy) / dt) / 900.0f);
        speedEstimate = speedEstimate * 0.5f + instantaneous * 0.5f;

        const float amount = juce::jlimit (-1.0f, 1.0f, dy / 55.0f);

        angle = juce::jlimit (-1.0f, 1.0f, angle + amount * 0.6f);
        glow = juce::jmin (1.0f, glow + std::fabs (amount) + speedEstimate * 0.4f);

        if (onGesture) onGesture (amount, speedEstimate);

        lastY = e.position.y;
        lastEmitTime = now;
        repaint();
    }

    void GestureKnob::mouseUp (const juce::MouseEvent&)
    {
        dragging = false;
    }

    void GestureKnob::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        auto labelArea = b.removeFromBottom (13.0f);
        auto dial = b.reduced (3.0f);
        const float r = juce::jmin (dial.getWidth(), dial.getHeight()) * 0.5f;
        const auto centre = dial.getCentre();

        // track
        g.setColour (panelHi.withAlpha (0.75f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));

        if (glow > 0.01f)
        {
            g.setColour (colour.withAlpha (glow * 0.22f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.8f, r * 2.8f).withCentre (centre));
        }

        g.setColour (stroke.withAlpha (0.45f));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.0f);

        // The indicator sweeps from twelve o'clock and springs back, so the
        // knob always reads as "at rest" rather than "set to something".
        const float a = angle * 2.4f - juce::MathConstants<float>::halfPi;
        const juce::Point<float> tip { centre.x + std::cos (a) * r * 0.75f,
                                       centre.y + std::sin (a) * r * 0.75f };
        g.setColour (colour.withAlpha (0.55f + 0.45f * glow));
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.4f);

        // A tick at rest position
        g.setColour (stroke.withAlpha (0.5f));
        g.drawLine (centre.x, centre.y - r * 0.95f, centre.x, centre.y - r * 0.75f, 1.0f);

        g.setColour (text.withAlpha (0.7f));
        g.setFont (9.5f);
        g.drawText (label, labelArea, juce::Justification::centred);
    }

    // =====================================================================
    //  GameBar
    // =====================================================================

    GameBar::GameBar()
    {
        auto setup = [this] (juce::TextButton& b, juce::Colour c, std::function<void()>* cb)
        {
            b.setColour (juce::TextButton::buttonColourId, c.withAlpha (0.16f));
            b.setColour (juce::TextButton::textColourOffId, c.brighter (0.4f));
            b.onClick = [cb] { if (cb != nullptr && *cb) (*cb)(); };
            addAndMakeVisible (b);
        };

        setup (enzyme,   juce::Colour (0xff7fe3a0), &onEnzyme);
        setup (catalyst, juce::Colour (0xffb98cff), &onCatalyst);
        setup (heat,     juce::Colour (0xffff8a5c), &onHeat);
        setup (water,    juce::Colour (0xff5cc8ff), &onWater);
        setup (radiate,  juce::Colour (0xffff5c5c), &onRadiate);
        setup (newWorld, juce::Colour (0xffffd36e), &onNewWorld);
        setup (saveRun,  juce::Colour (0xff9fb6d4), &onSaveRun);
        setup (loadRun,  juce::Colour (0xff9fb6d4), &onLoadRun);
        setup (scores,   juce::Colour (0xff9fb6d4), &onScores);
        setup (micArm,   juce::Colour (0xffffa0c0), &onMicArm);
        setup (micCapture, juce::Colour (0xffffa0c0), &onMicCapture);

        radiate.setTooltip ("5% chance it kills the colony and your score. "
                            "10% chance of a beneficial mutation.");
        enzyme.setTooltip ("Sparkles and a usually-beneficial subtraction. "
                           "Random amount and effect every press.");
        catalyst.setTooltip ("A fast pitch wobble that quietly removes something as it fades. "
                             "Random amount and effect every press.");
        heat.setTooltip ("Speeds up one random thing: an oscillator, an LFO lane, or the loop.");
        water.setTooltip ("Slows down one random thing: an oscillator, an LFO lane, or the loop.");
        micArm.setTooltip ("Arm the microphone. Output is muted while capturing, "
                           "and a howl detector aborts if feedback starts.");

        addAndMakeVisible (pitchKnob);
        addAndMakeVisible (lfoKnob);
        addAndMakeVisible (oscKnob);

        pitchKnob.onGesture = [this] (float a, float s) { if (onKnob) onKnob (0, a, s); };
        lfoKnob.onGesture   = [this] (float a, float s) { if (onKnob) onKnob (1, a, s); };
        oscKnob.onGesture   = [this] (float a, float s) { if (onKnob) onKnob (2, a, s); };
    }

    GameBar::~GameBar() = default;

    void GameBar::flashRadiation (int outcome)
    {
        radiationFlash = 1.0f;
        radiationResult = outcome;
        repaint();
    }

    void GameBar::setStatus (bool armed, bool capturing, float level,
                             float entropyLevel, bool entropyLive)
    {
        micIsArmed = armed;
        micIsCapturing = capturing;
        micLevel = level;
        entropy = entropyLevel;
        entropyIsLive = entropyLive;

        micArm.setButtonText (armed ? "MIC ARMED" : "ARM MIC");
        micCapture.setEnabled (armed && ! capturing);
        micCapture.setButtonText (capturing ? "LISTENING..." : "CAPTURE 4s");
        repaint();
    }

    void GameBar::tick (float dt)
    {
        if (radiationFlash > 0.001f)
        {
            radiationFlash *= std::exp (-dt * 1.6f);
            repaint();
        }
    }

    void GameBar::resized()
    {
        auto b = getLocalBounds().reduced (8, 6);

        // knobs on the right
        auto knobs = b.removeFromRight (168);
        const int kw = knobs.getWidth() / 3;
        pitchKnob.setBounds (knobs.removeFromLeft (kw).reduced (4));
        lfoKnob.setBounds (knobs.removeFromLeft (kw).reduced (4));
        oscKnob.setBounds (knobs.reduced (4));

        b.removeFromRight (10);

        // two rows of verbs
        auto top = b.removeFromTop (b.getHeight() / 2).reduced (0, 2);
        auto bottom = b.reduced (0, 2);

        auto lay = [] (juce::Rectangle<int>& row, juce::Component& c, int w)
        {
            c.setBounds (row.removeFromLeft (w).reduced (3, 0));
        };

        lay (top, enzyme, 104);
        lay (top, catalyst, 112);
        lay (top, heat, 86);
        lay (top, water, 92);
        lay (top, radiate, 84);

        lay (bottom, newWorld, 96);
        lay (bottom, saveRun, 84);
        lay (bottom, loadRun, 84);
        lay (bottom, scores, 74);
        lay (bottom, micArm, 86);
        lay (bottom, micCapture, 96);
    }

    void GameBar::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (stroke.withAlpha (0.3f));
        g.drawRoundedRectangle (b, 6.0f, 1.0f);

        if (radiationFlash > 0.01f)
        {
            const auto c = radiationResult < 0 ? juce::Colour (0xffff3030)
                         : radiationResult > 0 ? juce::Colour (0xff7fe3a0)
                                               : juce::Colour (0xffffd36e);
            g.setColour (c.withAlpha (radiationFlash * 0.18f));
            g.fillRoundedRectangle (b, 6.0f);
        }

        // ---- entropy / mic read-out, bottom-right of the strip ----------
        auto strip = juce::Rectangle<float> (b.getX() + 10.0f, b.getBottom() - 15.0f,
                                             b.getWidth() - 200.0f, 12.0f);

        g.setFont (9.0f);
        g.setColour (text.withAlpha (0.4f));

        juce::String status;
        status << "ENTROPY " << (entropyIsLive ? "LIVE TAP" : "software only");
        if (micIsArmed)
            status << "    MIC " << (micIsCapturing ? "CAPTURING (output muted)" : "armed");
        g.drawText (status, strip, juce::Justification::centredLeft);

        // a small activity bar for the harvested entropy
        auto bar = juce::Rectangle<float> (strip.getRight() - 60.0f, strip.getY() + 3.0f, 56.0f, 5.0f);
        g.setColour (panelHi.withAlpha (0.7f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour ((entropyIsLive ? juce::Colour (0xff7fe3a0) : juce::Colour (0xff607080))
                         .withAlpha (0.85f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, entropy)), 2.0f);

        if (micIsCapturing)
        {
            auto mb = bar.translated (-72.0f, 0.0f);
            g.setColour (panelHi.withAlpha (0.7f));
            g.fillRoundedRectangle (mb, 2.0f);
            g.setColour (juce::Colour (0xffffa0c0));
            g.fillRoundedRectangle (mb.withWidth (mb.getWidth() * juce::jlimit (0.0f, 1.0f, micLevel)), 2.0f);
        }
    }
}
