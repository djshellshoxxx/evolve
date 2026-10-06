#include "HauntedOverlay.h"
#include "MutagenLookAndFeel.h"
#include "../Engine/HiddenDiscoveries.h"
#include <cmath>

namespace mutagen::haunted
{
    namespace
    {
        class PhantomWindow final : public juce::Component,
                                    private juce::Timer
        {
        public:
            PhantomWindow (int id, juce::Rectangle<int> source, int recipe, float amount)
                : creatureId (id), animationRecipe (recipe),
                  intensity (juce::jlimit (0.2f, 1.5f, amount))
            {
                setInterceptsMouseClicks (false, false);
                setOpaque (false);
                setAlwaysOnTop (true);

                const int w = 120 + (id % 5) * 18;
                const int h = 90 + (id % 7) * 12;
                auto start = source.getCentre();
                setBounds (start.x - w / 2, start.y - h / 2, w, h);

                addToDesktop (juce::ComponentPeer::windowIsTemporary
                              | juce::ComponentPeer::windowIgnoresKeyPresses
                              | juce::ComponentPeer::windowIsSemiTransparent
                              | juce::ComponentPeer::windowIgnoresMouseClicks);

                const auto display = juce::Desktop::getInstance().getDisplays()
                    .getDisplayForRect (source);
                const auto* primary = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
                const auto work = display != nullptr ? display->userBounds.toNearestInt()
                                : primary != nullptr ? primary->userBounds.toNearestInt()
                                                     : source.expanded (900, 600);

                const int lane = (id * 37 + recipe * 19) % 4;
                switch (lane)
                {
                    case 0: target = { work.getRight() + w, work.getY() + (id * 73) % juce::jmax (1, work.getHeight()) }; break;
                    case 1: target = { work.getX() - w, work.getY() + (id * 61) % juce::jmax (1, work.getHeight()) }; break;
                    case 2: target = { work.getX() + (id * 97) % juce::jmax (1, work.getWidth()), work.getY() - h }; break;
                    default: target = { work.getX() + (id * 83) % juce::jmax (1, work.getWidth()), work.getBottom() + h }; break;
                }

                startCentre = getBounds().getCentre().toFloat();
                duration = 1.8f + (float) ((id + recipe) % 9) * 0.18f;
                startTimerHz (60);
            }

            ~PhantomWindow() override
            {
                stopTimer();
                removeFromDesktop();
            }

            void paint (juce::Graphics& g) override
            {
                auto b = getLocalBounds().toFloat();
                const float pulse = 0.65f + 0.35f * std::sin (age * (7.0f + creatureId % 5));
                const float alpha = juce::jlimit (0.0f, 1.0f,
                    (1.0f - age / juce::jmax (0.1f, duration)) * pulse * intensity);

                juce::Colour base = juce::Colour::fromHSV (
                    std::fmod ((float) creatureId * 0.137f + animationRecipe * 0.071f, 1.0f),
                    0.46f, 0.92f, alpha);

                g.setColour (base.withAlpha (alpha * 0.16f));
                g.fillEllipse (b.reduced (6.0f));

                g.setColour (base.withAlpha (alpha * 0.88f));
                juce::Path body;
                const auto c = b.getCentre();
                const float rx = b.getWidth() * 0.28f;
                const float ry = b.getHeight() * 0.25f;
                const int lobes = 5 + creatureId % 6;
                for (int i = 0; i <= lobes * 2; ++i)
                {
                    const float a = (float) i / (float) (lobes * 2)
                        * juce::MathConstants<float>::twoPi;
                    const float wobble = (i % 2 == 0 ? 1.0f : 0.48f)
                        * (0.8f + 0.2f * std::sin (age * 13.0f + i));
                    const juce::Point<float> p {
                        c.x + std::cos (a) * rx * wobble,
                        c.y + std::sin (a) * ry * wobble
                    };
                    if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
                }
                body.closeSubPath();
                g.fillPath (body);

                g.setColour (juce::Colours::black.withAlpha (alpha * 0.9f));
                const float eyeY = c.y - b.getHeight() * 0.05f;
                g.fillEllipse (c.x - 19.0f, eyeY - 5.0f, 8.0f, 10.0f);
                g.fillEllipse (c.x + 11.0f, eyeY - 5.0f, 8.0f, 10.0f);

                g.setColour (base.brighter (0.8f).withAlpha (alpha));
                g.setFont (9.5f);
                g.drawFittedText (juce::String (creatureName (creatureId)),
                                  getLocalBounds().removeFromBottom (18),
                                  juce::Justification::centred, 1);
            }

        private:
            void timerCallback() override
            {
                age += 1.0f / 60.0f;
                const float t = juce::jlimit (0.0f, 1.0f, age / duration);
                const float eased = t * t * (3.0f - 2.0f * t);
                auto p = startCentre + (target.toFloat() - startCentre) * eased;
                p.x += std::sin (age * (9.0f + creatureId % 7)) * 18.0f;
                p.y += std::cos (age * (6.0f + animationRecipe % 5)) * 12.0f;
                setCentrePosition (juce::roundToInt (p.x), juce::roundToInt (p.y));
                repaint();

                if (age >= duration)
                {
                    stopTimer();
                    setVisible (false);
                    juce::MessageManager::callAsync ([this] { delete this; });
                }
            }

            int creatureId = 0;
            int animationRecipe = 0;
            float intensity = 1.0f;
            float age = 0.0f, duration = 2.0f;
            juce::Point<float> startCentre;
            juce::Point<int> target;
        };
    }

    void launchDesktopPhantom (int creatureId, juce::Rectangle<int> sourceGlobal,
                               int animationRecipe, float intensity)
    {
        auto* p = new PhantomWindow (juce::jlimit (0, 99, creatureId),
                                     sourceGlobal, animationRecipe, intensity);
        p->setVisible (true);
        p->toFront (false);
    }
}
