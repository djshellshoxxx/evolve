#include "../Source/GUI/GameBar.h"
#include "../Source/GUI/EncounterOverlay.h"
#include "../Source/GUI/PhreakOverlay.h"
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

    // The fact card draws, shows the demo bonus line, and clears itself.
    bool factToastDraws = false;
    {
        StoryFx toastLayer;
        toastLayer.setBounds (0, 0, 900, 520);
        toastLayer.factToast ("Acoustics", "Doubling your distance from a source drops its level by about 6 dB.", 3, 9344, juce::Colour (0xff7ee0c3));
        toastLayer.advance (3.0f);
        juce::Image frame (juce::Image::ARGB, 900, 520, true);
        { juce::Graphics fg (frame); toastLayer.paint (fg); }
        int lit = 0;
        for (int y = 60; y < 220; y += 4)
            for (int x = 120; x < 780; x += 4)
                if (frame.getPixelAt (x, y).getAlpha() > 8) ++lit;
        toastLayer.advance (30.0f);
        factToastDraws = lit > 300 && ! toastLayer.isPlaying();
        if (const char* dump = std::getenv ("MUTAGEN_FX_DUMP"))
        {
            juce::FileOutputStream out (juce::File (dump).getChildFile ("toast.png"));
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
    std::printf ("Fact toast: %s\n", factToastDraws ? "PASS" : "FAIL");
    std::printf ("Steering intensity: %s\n", steeringIntensity ? "PASS" : "FAIL");

    {
        mutagen::PhreakOverlay phreak;
        phreak.setSize (800, 500);
        int ran = 0;
        phreak.setTree ({ { "ONE", { { "a", [&] { ++ran; } }, { "b", [&] { ran += 10; } } } }, { "TWO", { { "c", [&] { ran += 100; } } } } });
        phreak.open();
        juce::Image img (juce::Image::ARGB, 800, 500, true);
        { juce::Graphics g (img); phreak.paintEntireComponent (g, false); }
        const bool ok = phreak.isVisible() && phreak.categoryCount() == 2 && phreak.itemCount (0) == 2
                        && phreak.run (0, 1) && ! phreak.isVisible() && ran == 10
                        && ! phreak.run (2, 0) && ! phreak.run (0, 5) && phreak.run (1, 0) && ran == 110;
        std::printf ("Phreak menu: %s\n", ok ? "PASS" : "FAIL");
        if (! ok) return 1;
    }

    mutagen::EncounterOverlay encounter;
    encounter.setSize (700, 450);
    int noteCount = 0, completions = 0;
    encounter.onNote = [&] (int n, double seconds) { if (n >= 0 && n <= 127 && seconds > 0) ++noteCount; };
    encounter.onFinished = [&] (auto, bool, std::uint32_t) { ++completions; };
    encounter.begin (mutagen::EncounterOverlay::Kind::phantom, 42, 2, 0, 60, 0);
    encounter.tick (0.1);
    const bool phantomPlays = noteCount > 0;
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    const bool oneCompletion = completions == 1 && ! encounter.isVisible();
    bool roomAudio = false;
    encounter.onRoomTargets = [&] (const auto& t) { roomAudio |= t.combMix > 0 || t.reverbMacro > 0 || t.dopplerDepth > 0 || t.lowShelfDb != 0 || t.phaseOffset > 0; };
    encounter.begin (mutagen::EncounterOverlay::Kind::room, 42, 2, 0, 60, 0);
    for (int i = 0; i < 20; ++i) encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::chorale, 42, 2, 0, 60, 0);
    for (int i = 0; i < 60; ++i) encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress ('1'));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::pulse, 42, 2, 0, 60, 0);
    for (int i = 0; i < 40; ++i) encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::hunt, 42, 2, 0, 60, 0);
    encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress ('r'));
    encounter.keyPressed (juce::KeyPress ('2'));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::seek, 42, 2, 0, 60, 0);
    for (int i = 0; i < 20; ++i) encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::flash, 42, 2, 0, 60, 0);
    encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress ('3'));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::freq, 42, 2, 0, 60, 0);
    encounter.tick (0.1);
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey));
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    encounter.begin (mutagen::EncounterOverlay::Kind::tuner, 42, 2, 0, 60, 0);
    const int before = noteCount;
    encounter.tick (2.1);
    const bool tunerPlays = noteCount > before;
    encounter.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    std::printf ("Encounter integration: phantom=%d room=%d tuner=%d close=%d\n", phantomPlays, roomAudio, tunerPlays, oneCompletion);

    GameBar gameBar;
    bool allActionTooltips = true;
    for (auto* child : gameBar.getChildren())
        if (dynamic_cast<juce::Button*> (child) || dynamic_cast<GestureKnob*> (child))
        { auto* tooltip = dynamic_cast<juce::TooltipClient*> (child); allActionTooltips &= tooltip != nullptr && tooltip->getTooltip().isNotEmpty(); }
    GestureKnob gesture ("Test", juce::Colours::white); int gestureCalls = 0;
    gesture.onGesture = [&] (float amount, float speed) { if (amount != 0 && speed > 0) ++gestureCalls; };
    gesture.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    gesture.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    const bool keyboardGestures = gestureCalls == 2 && gesture.getWantsKeyboardFocus();
    std::printf ("Action tooltips=%d keyboard gestures=%d\n", allActionTooltips, keyboardGestures);
    return (allActionTooltips && keyboardGestures && phantomPlays && roomAudio && tunerPlays && oneCompletion && chamberPassesThrough && scoreCaptures && tableCaptures
            && directionProfiles && counterProfile && diceProfiles
            && steeringIntensity && storyFxDraws && factToastDraws) ? 0 : 1;
}
