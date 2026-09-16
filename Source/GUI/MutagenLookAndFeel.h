#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

/*  =====================================================================
    MUTAGEN visual identity.

    The palette, type scale, control shapes and spacing here are the house
    style shared by every plugin in the range (see theme.md). The neutrals,
    the layout grid and the control geometry are fixed; a plugin is allowed
    to re-tint exactly one accent for its own identity.

    MUTAGEN keeps the house accents as they are. What it adds on top is a
    set of *data* colours - the species hues - which are not chrome: they
    encode which organism you are looking at, the way a chart's categorical
    palette encodes a series, so they sit outside the chrome rule.
    ===================================================================== */
namespace mutagen::theme
{
    // ---- neutrals (identical across the range) -------------------------
    inline const juce::Colour bg0        { 0xff0e1116 };  // window base
    inline const juce::Colour bg1        { 0xff12161c };  // recessed wells
    inline const juce::Colour panel      { 0xff171b22 };  // raised sections
    inline const juce::Colour panelHi    { 0xff1f242d };  // hovered / raised further
    inline const juce::Colour stroke     { 0xff2a303a };  // 1px edges and separators
    inline const juce::Colour text       { 0xffe6e8ec };  // labels, values
    inline const juce::Colour textDim    { 0xff8a929e };  // units, hints, inactive

    // ---- accents -------------------------------------------------------
    inline const juce::Colour accent     { 0xffe8532a };  // primary: burnt orange
    inline const juce::Colour accent2    { 0xff4fb6c4 };  // secondary: muted teal
    inline const juce::Colour success    { 0xff7bc96f };  // signal present
    inline const juce::Colour warning    { 0xfff2c14e };  // approaching clip
    inline const juce::Colour danger     { 0xffe04b4b };  // clipped / fatal

    inline const juce::Colour shadow     { 0x8c000000 };  // rgba(0,0,0,0.55)

    // ---- data colours (species identity, not chrome) -------------------
    inline const juce::Colour grain      { 0xffe8532a };  // burnt orange
    inline const juce::Colour spectral   { 0xff4fb6c4 };  // muted teal
    inline const juce::Colour spectralV  { 0xff9b7bd6 };  // violet
    inline const juce::Colour resonator  { 0xff7bc96f };  // green
    inline const juce::Colour infection  { 0xffe8556f };  // magenta
    inline const juce::Colour nutrient   { 0xff4fc4a8 };  // teal-green
    inline const juce::Colour selectRing { 0xfff2c14e };  // selection halo

    // ---- geometry (8px base grid) --------------------------------------
    inline constexpr int   gridUnit      = 8;
    inline constexpr float radiusWindow  = 6.0f;
    inline constexpr float radiusPanel   = 4.0f;
    inline constexpr float radiusControl = 2.0f;
    inline constexpr int   edgePadding   = 16;   // window edge -> any control
    inline constexpr int   headerHeight  = 32;
    inline constexpr int   buttonHeight  = 28;
    inline constexpr int   knobSmall     = 36;
    inline constexpr int   knobDefault   = 48;
    inline constexpr int   knobLarge     = 64;
    inline constexpr int   tooltipDelayMs = 400;

    // Value arc: 270 degrees, 7 o'clock to 5 o'clock.
    inline constexpr float arcStart     = juce::MathConstants<float>::pi * 1.25f;
    inline constexpr float arcEnd       = juce::MathConstants<float>::pi * 2.75f;
    inline constexpr float arcGap       = 4.0f;   // gap between knob body and arc
    inline constexpr float arcThickness = 3.0f;

    /*  Type. The house faces are Inter for UI and JetBrains Mono for numeric
        readouts, with documented fallbacks - a host machine may have neither,
        and a numeric readout that silently loses tabular figures jitters as
        the value changes, so the fallback order matters. Resolved once. */
    const juce::String& uiTypeface();
    const juce::String& monoTypeface();

    /** UI face. Weight 500 for labels, 600 for section headers; JUCE exposes
        plain and bold, so semibold maps to bold at these sizes. */
    juce::Font uiFont (float size, bool semibold = false);

    /** Numeric readouts, tabular figures where the resolved face has them. */
    juce::Font monoFont (float size);

    /** 11px uppercase - the house label. Tracking is applied by drawTracked,
        because JUCE's Font cannot express letter spacing on its own. */
    juce::Font labelFont();

    /** Draws text with letter spacing, which JUCE's Font cannot express. */
    void drawTracked (juce::Graphics&, const juce::String& textToDraw,
                      juce::Rectangle<int> area, juce::Justification,
                      const juce::Font&, float trackingEm);

    /** The house section header: uppercase text with a 2x12 accent bar. */
    void drawSectionHeader (juce::Graphics&, juce::Rectangle<int> area,
                            const juce::String& title, juce::Colour barColour);

    /** The signature mark: a 2px accent diagonal notch in the top-left. */
    void drawSignatureNotch (juce::Graphics&, juce::Rectangle<int> windowBounds,
                             juce::Colour c = accent);

    /** Meter gradient colour for a 0..1 normalised level:
        teal -> orange -> yellow -> red. */
    juce::Colour meterColour (float norm);

    inline juce::Colour speciesColour (int species)
    {
        switch (species)
        {
            case 0:  return grain;
            case 1:  return spectral;
            case 2:  return resonator;
            default: return accent;
        }
    }

    inline juce::Colour infectionColour (int type)
    {
        switch (type)
        {
            case 1:  return juce::Colour (0xffb9c0c8); // metallize
            case 2:  return juce::Colour (0xfff2c14e); // reverse
            case 3:  return spectralV;                 // vocalise
            case 4:  return infection;                 // destabilise
            default: return infection;
        }
    }
}

namespace mutagen
{
    class MutagenLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        MutagenLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float pos, float startAngle, float endAngle,
                               juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                               float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool over, bool down) override;

        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;

        void drawComboBox (juce::Graphics&, int w, int h, bool down,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;

        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;

        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        // per-slider accent colour lives in the slider's component-property "tint"
        static juce::Colour tintOf (juce::Component& c, juce::Colour fallback);
    };
}
