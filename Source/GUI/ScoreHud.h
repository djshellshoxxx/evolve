#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "../Engine/ScoreSystem.h"

namespace mutagen
{
    /*  ------------------------------------------------------------------
        FractalGhost - the reward.

        When the colony sounds genuinely good and the score is climbing, a
        ghostly recursive structure flashes over the chamber and fades. It
        is drawn once per earning, lasts about a second, and the ScoreSystem
        puts a 30-70 second random cooldown behind it.

        The scarcity is the feature. A reward that appears whenever things
        are going fine stops registering as a reward almost immediately, so
        the gate is deliberately hard to trip: appealing *and* varied *and*
        scoring well, held for seven continuous seconds.
        ------------------------------------------------------------------ */
    class FractalGhost
    {
    public:
        void trigger (juce::Point<float> origin, float hue, juce::Random& rnd);
        void update (float dt);
        void render (juce::Graphics& g, juce::Rectangle<float> area) const;

        bool  active() const { return life > 0.0f; }
        float intensity() const { return life; }

    private:
        void drawBranch (juce::Graphics& g, juce::Point<float> p, float angle,
                         float length, int depth, float alpha) const;

        juce::Point<float> root { 0.5f, 0.9f };
        float life = 0.0f;       // 1 -> 0
        float hueValue = 0.5f;
        float spread = 0.5f;
        float twist  = 0.0f;
        int   arms = 5;
        int   depthLimit = 7;
        float seedAngle = 0.0f;
    };

    // =====================================================================

    /*  The score read-out. Score, multiplier, combo, the two meters that
        explain *why* the number is moving at the speed it is, a live event
        feed, and the high-score table. */
    class ScoreHud : public juce::Component
    {
    public:
        ScoreHud();

        void setState (const ScoreSystem& score, const EngineSnapshot& snap);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool hitTest (int x, int y) override;

        /** Show or hide the high-score table overlay. */
        void setTableVisible (bool shouldShow) { showTable = shouldShow; repaint(); }
        bool isTableVisible() const { return showTable; }

        /** Clicking the score area toggles the table; the editor listens so it
            can pause other overlays. */
        std::function<void()> onTableToggled;

        /** True where the HUD wants the mouse - the chamber below should not
            treat a click here as a mutation. */
        bool hitsInteractive (juce::Point<int> p) const;

    private:
        void paintMeters (juce::Graphics&, juce::Rectangle<float>);
        void paintTable (juce::Graphics&, juce::Rectangle<float>);

        juce::int64 displayedScore = 0;     // eased toward the real one
        juce::int64 targetScore = 0;
        float  multiplier = 1.0f;
        int    combo = 0;
        bool   frozen = false;
        juce::String freezeReason;
        float  rate = 0.0f;
        float  variety = 0.5f;
        float  greyness = 0.0f;
        float  appeal = 0.5f;
        float  coverage = 0.0f;
        int    rank = 0;
        int    discoveries = 0;
        double elapsed = 0.0;
        juce::String worldName { "WORLD" };

        std::vector<ScoreEvent> events;
        std::vector<HighScore>  table;

        bool showTable = false;
        float flashPhase = 0.0f;
        juce::Rectangle<float> scoreArea, tableButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreHud)
    };
}
