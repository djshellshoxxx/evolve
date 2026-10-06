#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace mutagen
{
    /*  ------------------------------------------------------------------
        GestureKnob

        Not a parameter. It has no value to read back and it springs to
        centre when released. What it sends is a *gesture*: which way you
        turned it, how fast, and when. The engine decides what that means,
        partly at random, and it can improve the sound or damage it.

        The spring-back matters. A knob that stayed where you left it would
        imply a setting you could return to, and there isn't one.
        ------------------------------------------------------------------ */
    class GestureKnob : public juce::Component,
                        private juce::Timer
    {
    public:
        GestureKnob (juce::String labelText, juce::Colour accentColour);

        /** amount is signed (-1..1, the direction and size of the turn),
            speed is 0..1. */
        std::function<void (float amount, float speed)> onGesture;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;

        juce::String label;
        juce::Colour colour;
        float angle = 0.0f;         // visual, springs back to 0
        float lastY = 0.0f;
        double lastEmitTime = 0.0;
        float  speedEstimate = 0.0f;
        float  glow = 0.0f;
        bool   dragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GestureKnob)
    };

    // =====================================================================

    /*  The action strip: the one-shot gestures that change the sound, the
        three gesture knobs, and the run controls. Everything here is a verb.  */
    class GameBar : public juce::Component
    {
    public:
        GameBar();
        ~GameBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /** Show the outcome of the last RADIATE for a moment. */
        void flashRadiation (int outcome);

        /** Live state for the readouts. */
        void setStatus (bool micArmed, bool micCapturing, float micLevel,
                        float entropyLevel, bool entropyLive);

        std::function<void()> onEnzyme, onCatalyst, onHeat, onWater, onRadiate;
        std::function<void()> onNewWorld, onSaveRun, onLoadRun, onScores, onJournal;
        std::function<void()> onMicArm, onMicCapture;
        std::function<void (int which, float amount, float speed)> onKnob;

        void tick (float dt);

    private:
        juce::TextButton enzyme { "ADD ENZYME" };
        juce::TextButton catalyst { "ADD CATALYST" };
        juce::TextButton heat { "ADD HEAT" };
        juce::TextButton water { "ADD WATER" };
        juce::TextButton radiate { "RADIATE" };
        juce::TextButton newWorld { "NEW WORLD" };
        juce::TextButton saveRun { "SAVE RUN" };
        juce::TextButton loadRun { "LOAD RUN" };
        juce::TextButton scores { "SCORES" };
        juce::TextButton journal { "JOURNAL" };
        // Where resized() left room for the entropy / mic read-out, so paint()
        // draws it in the gap rather than underneath the bottom row.
        juce::Rectangle<int> statusArea;

        juce::TextButton micArm { "ARM MIC" };
        juce::TextButton micCapture { "CAPTURE 4s" };

        GestureKnob pitchKnob { "PITCH", juce::Colour (0xff7fd4ff) };
        GestureKnob lfoKnob   { "LFO",   juce::Colour (0xffb98cff) };
        GestureKnob oscKnob   { "OSC",   juce::Colour (0xffffc46b) };

        float radiationFlash = 0.0f;
        int   radiationResult = 0;

        bool  micIsArmed = false, micIsCapturing = false;
        float micLevel = 0.0f;
        float entropy = 0.0f;
        bool  entropyIsLive = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GameBar)
    };
}
