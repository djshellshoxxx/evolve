#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace mutagen::theme
{
    // charcoal laboratory ground
    inline const juce::Colour bg0        { 0xff0c0f11 };
    inline const juce::Colour bg1        { 0xff14181b };
    inline const juce::Colour panel      { 0xff191e22 };
    inline const juce::Colour panelHi    { 0xff222a30 };
    inline const juce::Colour stroke     { 0xff33404a };
    inline const juce::Colour textDim    { 0xff8fa3ad };
    inline const juce::Colour text       { 0xffe7eef2 };

    // restrained luminous species colours
    inline const juce::Colour grain      { 0xffe8a33d };  // amber
    inline const juce::Colour spectral   { 0xff4fd6e0 };  // cyan
    inline const juce::Colour spectralV  { 0xff9b7bd6 };  // violet
    inline const juce::Colour resonator  { 0xff8fd69b };  // pale green
    inline const juce::Colour infection  { 0xffff5c7a };  // hot pink
    inline const juce::Colour accent     { 0xfff2e9c8 };  // warm bone highlight
    inline const juce::Colour nutrient   { 0xff3ad6a0 };
    inline const juce::Colour selectRing { 0xfffff2c0 };

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
            case 2:  return juce::Colour (0xffff8a3d); // reverse
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

        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;

        // per-slider accent colour lives in the slider's component-property "tint"
        static juce::Colour tintOf (juce::Component& c, juce::Colour fallback);
    };
}
