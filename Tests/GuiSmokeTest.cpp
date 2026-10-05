#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/GUI/ScoreHud.h"

#include <cstdio>

using namespace mutagen;

int main()
{
    ScoreHud hud;
    hud.setBounds (0, 0, 900, 600);
    hud.setInterceptsMouseClicks (true, false);

    // paint once so the score-card and SCORES button rectangles are populated
    juce::Image image (juce::Image::ARGB, 900, 600, true);
    juce::Graphics g (image);
    hud.paint (g);

    const bool chamberPassesThrough = ! hud.hitTest (100, 300);
    const bool scoreCaptures        =   hud.hitTest (750, 40);

    hud.setTableVisible (true);
    const bool tableCaptures        = hud.hitTest (450, 300);

    std::printf ("ScoreHud hit-test: chamber=%s score=%s table=%s\n",
                 chamberPassesThrough ? "PASS" : "FAIL",
                 scoreCaptures ? "PASS" : "FAIL",
                 tableCaptures ? "PASS" : "FAIL");

    return (chamberPassesThrough && scoreCaptures && tableCaptures) ? 0 : 1;
}
