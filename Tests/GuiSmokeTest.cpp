#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/GUI/ScoreHud.h"
#include "../Source/GUI/StoryFx.h"
#include "../Source/Engine/EvolutionSteering.h"

#include <cmath>
#include <cstdlib>
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

    // Every story animation must actually draw something, mid-flight and
    // nothing once it is over. MUTAGEN_FX_DUMP=<dir> also writes PNGs to look at.
    bool storyFxDraws = true;
    {
        const char* dump = std::getenv ("MUTAGEN_FX_DUMP");
        for (int a = 0; a < 12; ++a)
        {
            StoryFx fxLayer;
            fxLayer.setBounds (0, 0, 900, 520);
            fxLayer.play ((chance::Anim) a, juce::Colour (0xffd8a64a), 1.2f);
            const bool early = a == (int) chance::Anim::lightning || a == (int) chance::Anim::staticBurst
                            || a == (int) chance::Anim::heartbeat;
            fxLayer.advance (a == (int) chance::Anim::lightning ? 0.05f : early ? 0.3f : 1.6f);
            juce::Image frame (juce::Image::ARGB, 900, 520, true);
            { juce::Graphics fg (frame); fxLayer.paint (fg); }
            int lit = 0;
            for (int y = 0; y < 520; y += 4)
                for (int x = 0; x < 900; x += 4)
                    if (frame.getPixelAt (x, y).getAlpha() > 8) ++lit;
            storyFxDraws = storyFxDraws && lit > 12;
            if (dump != nullptr)
            {
                juce::FileOutputStream out (juce::File (dump).getChildFile ("fx" + juce::String (a) + ".png"));
                if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (frame, out); }
            }
            fxLayer.advance (30.0f);
            storyFxDraws = storyFxDraws && ! fxLayer.isPlaying();
        }
        StoryFx bannerLayer;
        bannerLayer.setBounds (0, 0, 900, 520);
        bannerLayer.banner ("ACT IV", "BELOW HEARING", juce::Colour (0xff6bffb0));
        bannerLayer.glitch (juce::Colour (0xffff6b6b));
        bannerLayer.advance (1.5f);
        juce::Image frame (juce::Image::ARGB, 900, 520, true);
        { juce::Graphics fg (frame); bannerLayer.paint (fg); }
        storyFxDraws = storyFxDraws && frame.getPixelAt (450, 260).getAlpha() > 8 && ! bannerLayer.hitTest (450, 260);
        if (dump != nullptr)
        {
            juce::FileOutputStream out (juce::File (dump).getChildFile ("banner.png"));
            if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (frame, out); }
        }
    }

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
    std::printf ("Story animations: %s\n", storyFxDraws ? "PASS" : "FAIL");
    std::printf ("Steering intensity: %s\n", steeringIntensity ? "PASS" : "FAIL");

    return (chamberPassesThrough && scoreCaptures && tableCaptures
            && directionProfiles && counterProfile && diceProfiles
            && steeringIntensity && storyFxDraws) ? 0 : 1;
}
