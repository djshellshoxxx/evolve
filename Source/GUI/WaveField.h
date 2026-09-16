#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

namespace mutagen
{
    /*  ------------------------------------------------------------------
        WaveField - the living surface of the culture chamber.

        A small damped 2-D wave simulation. Clicking drops a ripple in;
        dragging drags a continuous wake behind the cursor. The waves are not
        decoration: the chamber samples the field's amplitude to decide how
        hard to mutate the cells each ripple passes through, so what the
        player sees spreading outward and what the colony does are the same
        event.

        Colour is the game's status display. Saturation tracks how varied the
        sound is; a colony drifting into noise desaturates toward grey and a
        varied one blooms. Destructive ripples (right-click) are drawn in a
        separate channel so damage reads as a different *kind* of wave rather
        than a differently-coloured one.

        The simulation runs at grid resolution into an Image and is scaled up
        on the way out, so its cost does not depend on the window size.
        ------------------------------------------------------------------ */
    class WaveField
    {
    public:
        static constexpr int gridW = 112;
        static constexpr int gridH = 72;

        struct Palette
        {
            float baseHue  = 0.55f;   // the run's signature colour
            float variety  = 0.5f;    // 0 grey .. 1 fully saturated
            float greyness = 0.0f;    // how far into the noise state we are
            float energy   = 0.3f;    // output level, drives brightness
            float heat     = 0.0f;    // -1 water (blue) .. +1 heat (red)
            float nicheHue[8] {};
            float nicheX[8] {};
            float nicheY[8] {};
            int   nicheCount = 0;
        };

        WaveField();

        void reset();

        /** Advance the simulation. Uses fixed substeps internally so the
            behaviour does not change with frame rate. */
        void update (float dtSeconds);

        /** Drop a ripple. `strength` 0..1. `destructive` marks it as damage,
            which propagates in its own channel and is drawn differently. */
        void ripple (float nx, float ny, float strength, bool destructive);

        /** A broad, soft swell rather than a sharp ripple - used for colony
            wide events like a radiation flash or a stagnation storm. */
        void swell (float strength, bool destructive);

        void render (juce::Graphics& g, juce::Rectangle<float> area, const Palette& p);

        /** Field amplitude at a normalised position, 0..1 in magnitude.
            The chamber uses this to scale mutation strength. */
        float amplitudeAt (float nx, float ny) const;

        /** Total energy in the field - how agitated the surface is. */
        float agitation() const { return agitationValue; }

    private:
        int index (int x, int y) const { return y * gridW + x; }
        void step (float dt);

        std::vector<float> cur, prev, dmg;
        juce::Image image;
        float agitationValue = 0.0f;
        float accumulator = 0.0f;
        float phase = 0.0f;
    };
}
