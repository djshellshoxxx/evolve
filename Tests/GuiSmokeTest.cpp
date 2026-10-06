#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/GUI/ScoreHud.h"
#include "../Source/Engine/EvolutionSteering.h"

#include <cmath>
#include <cstdio>

using namespace mutagen;

namespace
{
    bool near (float a, float b, float eps = 0.001f)
    {
        return std::abs (a - b) <= eps;
    }
}

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

    // Direction buttons must map to a single strong, predictable selection axis.
    const auto bright = steering::directionProfile (steering::Direction::bright);
    const auto calm   = steering::directionProfile (steering::Direction::calm);
    const bool directionProfiles =
        near (bright.brightness, 0.9f) && near (bright.harmonicity, 0.0f)
        && near (calm.aggression, -0.9f) && near (calm.brightness, 0.0f);

    // COUNTER-EVOLVE should push away from the sound currently being measured.
    const auto counter = steering::counterProfile (0.82f, 0.76f, 0.71f);
    const bool counterProfile =
        counter.brightness < 0.0f && counter.harmonicity < 0.0f
        && counter.aggression < 0.0f && counter.divergence >= 0.6f;

    // Steering intensity must scale the target without changing its direction.
    const auto hardBright = steering::directionProfile (steering::Direction::bright);
    const auto softBright = steering::scaledProfile (hardBright, 0.25f);
    const auto fullBright = steering::scaledProfile (hardBright, 1.0f);
    const bool steeringIntensity =
        softBright.brightness > 0.0f
        && softBright.brightness < fullBright.brightness
        && near (softBright.brightness, hardBright.brightness * 0.25f)
        && near (fullBright.brightness, hardBright.brightness);

    // Mutation dice is deterministic for a supplied roll (testable) but must
    // still cover the full bipolar steering space and produce different throws.
    const auto diceA = steering::diceProfile (0x12345678u);
    const auto diceB = steering::diceProfile (0x89abcdefu);
    const auto inRange = [] (float v) { return v >= -1.0f && v <= 1.0f; };
    const bool diceProfiles =
        inRange (diceA.brightness) && inRange (diceA.density)
        && inRange (diceA.harmonicity) && inRange (diceA.aggression)
        && diceA.divergence >= 0.35f && diceA.divergence <= 1.0f
        && (! near (diceA.brightness, diceB.brightness)
            || ! near (diceA.harmonicity, diceB.harmonicity)
            || ! near (diceA.aggression, diceB.aggression));

    std::printf ("ScoreHud hit-test: chamber=%s score=%s table=%s\n",
                 chamberPassesThrough ? "PASS" : "FAIL",
                 scoreCaptures ? "PASS" : "FAIL",
                 tableCaptures ? "PASS" : "FAIL");
    std::printf ("Evolution steering: directions=%s counter=%s dice=%s\n",
                 directionProfiles ? "PASS" : "FAIL",
                 counterProfile ? "PASS" : "FAIL",
                 diceProfiles ? "PASS" : "FAIL");
    std::printf ("Steering intensity: %s\n", steeringIntensity ? "PASS" : "FAIL");

    return (chamberPassesThrough && scoreCaptures && tableCaptures
            && directionProfiles && counterProfile && diceProfiles
            && steeringIntensity) ? 0 : 1;
}
