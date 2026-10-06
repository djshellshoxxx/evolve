#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include "../Engine/OrganismState.h"
#include "Selection.h"
#include "WaveField.h"
#include "ScoreHud.h"

namespace mutagen
{
    class MutagenProcessor;

    /*  ------------------------------------------------------------------
        The Culture Chamber - the game surface.

        Mouse contract:

          left click        additive mutation burst at the click, plus a
                            ripple in the wave field
          left drag         a continuous wake; every ripple it leaves behind
                            mass-mutates the cells it washes over, scaled by
                            how fast the cursor is moving
          right click/drag  subtractive damage - strips partials, density and
                            noise colour, and kills what it has already
                            weakened. Also the cure for a noise lock.
          ctrl + right      the old inspect / isolate / preserve menu
          double click      send a family to the breeding lab, or seed a burst
                            on empty ground
          drop a file       the colony eats the sample

        Colour follows the sound: varied is saturated, noisy is grey. Nothing
        here is decorative - every ripple the player sees corresponds to a
        mutation command that was actually sent.
        ------------------------------------------------------------------ */
    class CultureChamber : public juce::Component,
                           public juce::FileDragAndDropTarget
    {
    public:
        explicit CultureChamber (MutagenProcessor&);

        void update (const EngineSnapshot& snap, double dtSeconds);

        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        // ---- file drops -------------------------------------------------
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

        // ---- callbacks ---------------------------------------------------
        std::function<void (const Selection&)> onSelectionChanged;
        std::function<void (const Selection&)> onInspect;
        std::function<void (const Selection&)> onSendToBreedingLab;

        /** The player interacted; weight 0..1, label for the score feed. */
        std::function<void (float weight, juce::String label)> onInteraction;

        /** Audio files were dropped on the chamber. */
        std::function<void (const juce::StringArray&)> onFilesDropped;

        void setSelection (const Selection& s) { selection = s; repaint(); }
        const Selection& getSelection() const { return selection; }

        /** Fire the fractal reward. The ScoreSystem decides when. */
        void triggerReward (float hue);

        // ---- lab-game orb layer ---------------------------------------
        void spawnSpecialOrbs (int count);
        void spawnRainbowOrbs (int count, bool glowing, int multiplier = 1);
        void spawnMonsterOrbs (int count);
        void spawnMiniOrbs (int count);
        void spawnMutationJackpot (int randomColourOrbs, int giantRainbowOrbs,
                                  int blackVirusOrbs, int breedRatio);
        void setOrbInversion (float seconds);
        void setOrbSpeedBoost (float multiplier, float seconds);
        void setLabPet (int pet); // 0 none, 1 cat, 2 dog
        void triggerSnowFireworks (float seconds = 8.0f);

        std::function<void(int)> onGreenOrbsBorn;
        std::function<void()> onRainbowCorner;
        std::function<void()> onMutationCatastrophe;
        std::function<void(int)> onBlackOrbsBorn;
        std::function<void(int bpm)> onTemporaryGator;

        /** Where the HUD sits, so clicks there are not treated as mutations. */
        void setHudProbe (std::function<bool (juce::Point<int>)> fn) { hudProbe = std::move (fn); }

    private:
        struct Ripple { float x, y, r, life, maxLife; juce::Colour c; bool destructive; };
        enum class OrbKind
        {
            special, rainbow, glowingRainbow, giantRainbow,
            monster, mini, green, red, pink, yellow, white, blackVirus,
            squid, mite, spider
        };
        struct Orb
        {
            juce::Point<float> p, v;
            float radius = 4.0f;
            float life = 8.0f;
            float maxLife = 8.0f;
            OrbKind kind { OrbKind::special };
            float pulse = 0.0f;
            bool multiplied = false;
        };
        struct Sprinkle
        {
            juce::Point<float> p, v;
            float life = 1.0f;
            juce::Colour c;
        };

        juce::Point<float> toPixels (float nx, float ny) const;
        juce::Point<float> toNormalised (juce::Point<float> p) const;
        int   hitTestCell (juce::Point<float> p) const;
        void  showContextMenu (int cellIndex);
        void  emitSelection();

        void  strike (juce::Point<float> pos, bool destructive, float strength, bool fromDrag);
        void  paintCells (juce::Graphics&);
        void  paintSpectrum (juce::Graphics&, juce::Rectangle<float>);
        void  paintNiches (juce::Graphics&);
        void  updateGameOrbs (double dt);
        void  paintGameOrbs (juce::Graphics&);
        void  addOrb (OrbKind kind, float speedScale, float lifeScale);
        juce::Colour orbColour (OrbKind kind, float phase) const;
        juce::Colour cellColour (const CellView& c) const;

        MutagenProcessor& processor;
        EngineSnapshot snap;
        double phase = 0.0;

        WaveField    waves;
        FractalGhost ghost;
        juce::Random rnd;

        Selection selection;
        int hoverCell = -1;
        juce::Point<float> mousePos;
        std::vector<Ripple> ripples;
        std::vector<Orb> gameOrbs;
        std::vector<Sprinkle> sprinkles;
        float orbInversionSeconds = 0.0f;
        float orbCollisionClock = 0.0f;
        float chamberShake = 0.0f;
        float mutationFlash = 0.0f;
        float orbSpeedMultiplier = 1.0f;
        float orbSpeedBoostSeconds = 0.0f;
        float celebrationSeconds = 0.0f;
        int giantRainbowBreedRatio = 1;
        int labPet = 0;
        juce::Point<float> petPos { 0.5f, 0.5f };

        // drag tracking
        bool  dragging = false;
        bool  dragDestructive = false;
        juce::Point<float> lastEmit;
        double lastEmitTime = 0.0;
        float  dragSpeed = 0.0f;

        bool  fileHover = false;

        // smoothed visual state
        float colourSat = 0.6f;
        float greyLevel = 0.0f;
        float redFlash = 0.0f;

        std::function<bool (juce::Point<int>)> hudProbe;

        juce::Rectangle<float> field;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CultureChamber)
    };
}
