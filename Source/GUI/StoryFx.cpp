// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "StoryFx.h"
#include "MutagenLookAndFeel.h"

namespace mutagen
{
    namespace
    {
        constexpr float kPi = juce::MathConstants<float>::pi;
        constexpr float kFrame = 1.0f / 30.0f;
        constexpr float kBannerLife = 4.2f;
        constexpr float kToastLife = 7.0f;

        /** Deterministic 0..1 hash of (seed, index, channel). */
        float rnd (std::uint32_t seed, int i, int k = 0)
        {
            std::uint32_t h = seed * 747796405u + (std::uint32_t) i * 2891336453u + (std::uint32_t) k * 1274126177u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return (float) (h & 0xffffffu) / 16777216.0f;
        }

        /** Rises, holds, falls: 0..1 across the effect's life. */
        float envelope (float t, float attack = 0.15f, float release = 0.35f)
        {
            return juce::jlimit (0.0f, 1.0f, juce::jmin (t / attack, (1.0f - t) / release));
        }

        float lifeFor (chance::Anim a)
        {
            using A = chance::Anim;
            switch (a)
            {
                case A::ripples: return 3.2f;   case A::sporeRain: return 5.0f;
                case A::comet: return 3.6f;     case A::eclipse: return 6.0f;
                case A::goldDust: return 5.5f;  case A::staticBurst: return 1.1f;
                case A::lightning: return 1.6f; case A::bubbles: return 5.0f;
                case A::aurora: return 6.5f;    case A::fireflies: return 6.0f;
                case A::snowSines: return 5.5f; case A::heartbeat: return 6.0f;
            }
            return 3.0f;
        }
    }

    StoryFx::StoryFx()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
    }

    StoryFx::~StoryFx() { stopTimer(); }

    void StoryFx::play (chance::Anim a, juce::Colour c, float intensity)
    {
        if (fx.size() >= 4) fx.erase (fx.begin());
        fx.push_back ({ a, c, 0.0f, lifeFor (a), juce::jlimit (0.3f, 2.0f, intensity), counter * 2654435761u, false });
        ++counter;
        startTimerHz (30);
    }

    void StoryFx::glitch (juce::Colour c)
    {
        fx.push_back ({ chance::Anim::staticBurst, c, 0.0f, 1.4f, 1.6f, counter * 2654435761u, true });
        ++counter;
        startTimerHz (30);
    }

    void StoryFx::factToast (const juce::String& category, const juce::String& text,
                             int knowledgePoints, int demoBonus, juce::Colour c)
    {
        if (toasts.size() >= 2) toasts.erase (toasts.begin());
        toasts.push_back ({ category, text, knowledgePoints, demoBonus, c, 0.0f });
        startTimerHz (30);
    }

    void StoryFx::banner (const juce::String& title, const juce::String& subtitle, juce::Colour c)
    {
        if (banners.size() >= 2) banners.erase (banners.begin());
        banners.push_back ({ title, subtitle, c, 0.0f });
        startTimerHz (30);
    }

    void StoryFx::timerCallback()
    {
        advance (kFrame);
        if (! isPlaying()) stopTimer();
        repaint();
    }

    void StoryFx::advance (float dt)
    {
        for (auto& f : fx) f.age += dt;
        for (auto& b : banners) b.age += dt;
        for (auto& t : toasts) t.age += dt;
        toasts.erase (std::remove_if (toasts.begin(), toasts.end(),
                                      [] (const Toast& t) { return t.age >= kToastLife; }), toasts.end());
        fx.erase (std::remove_if (fx.begin(), fx.end(), [] (const Fx& f) { return f.age >= f.life; }), fx.end());
        banners.erase (std::remove_if (banners.begin(), banners.end(),
                                       [] (const Banner& b) { return b.age >= kBannerLife; }), banners.end());
    }

    void StoryFx::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat();
        for (const auto& f : fx) drawFx (g, f, area);
        for (const auto& b : banners) drawBanner (g, b, area);
        for (const auto& t : toasts) drawToast (g, t, area);
    }

    // ---- chance animations --------------------------------------------------

    void StoryFx::drawFx (juce::Graphics& g, const Fx& f, juce::Rectangle<float> a) const
    {
        using A = chance::Anim;
        const float t = f.age / f.life;
        const float w = a.getWidth(), h = a.getHeight();
        const float env = envelope (t);
        const auto col = f.colour;

        switch (f.anim)
        {
            case A::ripples:
            {
                const float cx = a.getX() + w * (0.25f + 0.5f * rnd (f.seed, 0));
                const float cy = a.getY() + h * (0.25f + 0.5f * rnd (f.seed, 1));
                for (int i = 0; i < 4; ++i)
                {
                    const float u = t * 1.4f - i * 0.18f;
                    if (u <= 0.0f || u >= 1.0f) continue;
                    const float r = u * juce::jmax (w, h) * 0.45f;
                    g.setColour (col.withAlpha ((1.0f - u) * 0.55f * f.intensity));
                    g.drawEllipse (cx - r, cy - r * 0.8f, r * 2.0f, r * 1.6f, 1.6f);
                }
                break;
            }
            case A::sporeRain:
                for (int i = 0; i < 70; ++i)
                {
                    const float x0 = rnd (f.seed, i, 0), speed = 0.5f + rnd (f.seed, i, 1);
                    const float y = std::fmod (t * speed * 1.2f + rnd (f.seed, i, 2), 1.0f);
                    const float x = x0 + 0.03f * std::sin (f.age * 1.5f + i);
                    const float s = 1.5f + rnd (f.seed, i, 3) * 3.0f;
                    g.setColour (col.withAlpha (env * (0.25f + 0.5f * rnd (f.seed, i, 4)) * 0.8f));
                    g.fillEllipse (a.getX() + x * w, a.getY() + y * h, s, s);
                }
                break;
            case A::comet:
                for (int c = 0; c < 6; ++c)
                {
                    const float start = c * 0.09f;
                    const float u = (t - start) * 1.6f;
                    if (u <= 0.0f || u >= 1.0f) continue;
                    const float sx = w * (0.35f + 0.7f * rnd (f.seed, c, 0)), sy = -20.0f;
                    const float dx = -w * 0.35f, dy = h * 1.15f;
                    for (int k = 0; k < 14; ++k)
                    {
                        const float uk = u - k * 0.012f;
                        if (uk < 0.0f) break;
                        const float s = (k == 0 ? 5.0f : 4.0f - k * 0.25f);
                        g.setColour ((k == 0 ? juce::Colours::white : col).withAlpha ((1.0f - k / 14.0f) * (1.0f - u * 0.5f) * 0.9f));
                        g.fillEllipse (a.getX() + sx + dx * uk - s * 0.5f, a.getY() + sy + dy * uk - s * 0.5f, s, s);
                    }
                }
                break;
            case A::eclipse:
            {
                const float dark = std::sin (juce::jlimit (0.0f, 1.0f, t) * kPi);
                juce::ColourGradient grad (juce::Colours::black.withAlpha (0.0f), a.getCentre(),
                                           juce::Colours::black.withAlpha (0.78f * dark),
                                           { a.getX(), a.getY() }, true);
                grad.addColour (0.35, juce::Colours::black.withAlpha (0.15f * dark));
                g.setGradientFill (grad);
                g.fillRect (a);
                const float r = juce::jmin (w, h) * 0.16f;
                g.setColour (col.withAlpha (dark * dark * 0.8f));
                g.drawEllipse (a.getCentreX() - r, a.getCentreY() - r, r * 2.0f, r * 2.0f, 2.5f);
                g.setColour (juce::Colours::black.withAlpha (dark));
                g.fillEllipse (a.getCentreX() - r, a.getCentreY() - r, r * 2.0f, r * 2.0f);
                break;
            }
            case A::goldDust:
                for (int i = 0; i < 110; ++i)
                {
                    const float x = rnd (f.seed, i, 0);
                    const float y = 1.0f - std::fmod (t * (0.25f + 0.5f * rnd (f.seed, i, 1)) + rnd (f.seed, i, 2), 1.0f);
                    const float tw = 0.5f + 0.5f * std::sin (f.age * (4.0f + 6.0f * rnd (f.seed, i, 3)) + i);
                    const float s = 1.0f + rnd (f.seed, i, 4) * 3.0f;
                    g.setColour (col.withAlpha (env * tw * 0.9f));
                    g.fillEllipse (a.getX() + x * w, a.getY() + y * h, s, s);
                    if (s > 3.2f)
                    {
                        g.setColour (juce::Colours::white.withAlpha (env * tw * 0.7f));
                        g.drawLine (a.getX() + x * w - 4, a.getY() + y * h + s * 0.5f, a.getX() + x * w + 4 + s, a.getY() + y * h + s * 0.5f, 0.8f);
                    }
                }
                break;
            case A::staticBurst:
            {
                const float flick = f.glitch ? 1.0f : env;
                const int frame = (int) (f.age * 30.0f);
                for (int i = 0; i < 26; ++i)
                {
                    const float y = rnd (f.seed, frame * 31 + i, 0) * h;
                    const float bh = 1.0f + rnd (f.seed, frame * 31 + i, 1) * (f.glitch ? 16.0f : 7.0f);
                    const float off = (rnd (f.seed, frame * 31 + i, 2) - 0.5f) * w * 0.12f * f.intensity;
                    g.setColour ((i % 3 == 0 ? col : juce::Colours::white).withAlpha ((1.0f - t) * 0.35f * flick));
                    g.fillRect (a.getX() + off, a.getY() + y, w, bh);
                }
                if (f.glitch && (frame % 4) < 2)
                {
                    g.setColour (juce::Colour (0xffff3860).withAlpha (0.10f * (1.0f - t)));
                    g.fillRect (a.translated (-5.0f, 0.0f));
                    g.setColour (juce::Colour (0xff38e8ff).withAlpha (0.10f * (1.0f - t)));
                    g.fillRect (a.translated (5.0f, 0.0f));
                }
                break;
            }
            case A::lightning:
            {
                const float flash = (t < 0.12f || (t > 0.26f && t < 0.34f)) ? 1.0f : juce::jmax (0.0f, 0.5f - t);
                g.setColour (juce::Colours::white.withAlpha (flash * 0.18f * f.intensity));
                g.fillRect (a);
                if (flash <= 0.0f) break;
                for (int bolt = 0; bolt < 2; ++bolt)
                {
                    juce::Path p;
                    float x = a.getX() + w * (0.2f + 0.6f * rnd (f.seed, bolt, 9)), y = a.getY();
                    p.startNewSubPath (x, y);
                    const int steps = 14;
                    for (int s = 1; s <= steps; ++s)
                    {
                        x += (rnd (f.seed, bolt * 40 + s, 5) - 0.5f) * w * 0.10f;
                        y = a.getY() + h * s / (float) steps;
                        p.lineTo (x, y);
                    }
                    g.setColour (col.withAlpha (0.35f * flash));
                    g.strokePath (p, juce::PathStrokeType (5.0f));
                    g.setColour (juce::Colours::white.withAlpha (0.95f * flash));
                    g.strokePath (p, juce::PathStrokeType (1.6f));
                }
                break;
            }
            case A::bubbles:
                for (int i = 0; i < 26; ++i)
                {
                    const float u = std::fmod (t * (0.6f + rnd (f.seed, i, 0)) + rnd (f.seed, i, 1), 1.0f);
                    const float x = rnd (f.seed, i, 2) + 0.02f * std::sin (f.age * 3.0f + i);
                    const float s = 5.0f + rnd (f.seed, i, 3) * 16.0f;
                    const float y = a.getBottom() - u * (h + s);
                    g.setColour (col.withAlpha (env * 0.55f));
                    g.drawEllipse (a.getX() + x * w, y, s, s, 1.1f);
                    g.setColour (juce::Colours::white.withAlpha (env * 0.45f));
                    g.fillEllipse (a.getX() + x * w + s * 0.2f, y + s * 0.18f, s * 0.2f, s * 0.2f);
                }
                break;
            case A::aurora:
                for (int band = 0; band < 4; ++band)
                {
                    juce::Path p;
                    const float baseY = a.getY() + h * (0.14f + 0.07f * band);
                    for (int i = 0; i <= 40; ++i)
                    {
                        const float u = i / 40.0f;
                        const float y = baseY + 18.0f * std::sin (u * 7.0f + f.age * (0.8f + band * 0.25f) + band)
                                              + 9.0f * std::sin (u * 15.0f - f.age * 1.3f);
                        if (i == 0) p.startNewSubPath (a.getX(), y); else p.lineTo (a.getX() + u * w, y);
                    }
                    const auto c = col.withRotatedHue (band * 0.07f);
                    g.setColour (c.withAlpha (env * 0.10f));
                    g.strokePath (p, juce::PathStrokeType (46.0f));
                    g.setColour (c.withAlpha (env * 0.20f));
                    g.strokePath (p, juce::PathStrokeType (20.0f));
                    g.setColour (c.brighter (0.4f).withAlpha (env * 0.45f));
                    g.strokePath (p, juce::PathStrokeType (2.0f));
                }
                break;
            case A::fireflies:
                for (int i = 0; i < 28; ++i)
                {
                    const float fa = 0.5f + rnd (f.seed, i, 0) * 1.5f, fb = 0.4f + rnd (f.seed, i, 1) * 1.6f;
                    const float x = 0.5f + 0.42f * std::sin (f.age * 0.45f * fa + i * 1.7f);
                    const float y = 0.5f + 0.40f * std::sin (f.age * 0.38f * fb + i * 2.9f);
                    const float pulse = 0.5f + 0.5f * std::sin (f.age * (2.0f + rnd (f.seed, i, 2) * 3.0f) + i);
                    const float cx = a.getX() + x * w, cy = a.getY() + y * h;
                    g.setColour (col.withAlpha (env * pulse * 0.25f));
                    g.fillEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    g.setColour (juce::Colours::white.withAlpha (env * pulse * 0.9f));
                    g.fillEllipse (cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);
                }
                break;
            case A::snowSines:
                for (int i = 0; i < 38; ++i)
                {
                    const float x0 = rnd (f.seed, i, 0);
                    const float y = std::fmod (t * (0.35f + 0.4f * rnd (f.seed, i, 1)) + rnd (f.seed, i, 2), 1.0f);
                    const float x = a.getX() + (x0 + 0.03f * std::sin (f.age + i)) * w, yy = a.getY() + y * h;
                    const float sz = 6.0f + rnd (f.seed, i, 3) * 9.0f;
                    juce::Path p;
                    for (int k = 0; k <= 8; ++k)
                    {
                        const float u = k / 8.0f;
                        const float px = x + (u - 0.5f) * sz * 2.0f, py = yy + std::sin (u * 2.0f * kPi) * sz * 0.4f;
                        if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                    }
                    g.setColour (col.withAlpha (env * 0.7f));
                    g.strokePath (p, juce::PathStrokeType (1.0f));
                }
                break;
            case A::heartbeat:
            {
                const float beat = std::fmod (f.age, 1.0f);   // 60 BPM: lub at 0, dub at 0.28
                const float lub = std::exp (-beat * 9.0f), dub = beat > 0.28f ? 0.6f * std::exp (-(beat - 0.28f) * 10.0f) : 0.0f;
                const float pulse = juce::jmax (lub, dub) * env;
                juce::ColourGradient grad (col.withAlpha (0.0f), a.getCentre(), col.withAlpha (0.35f * pulse),
                                           { a.getX(), a.getY() }, true);
                g.setGradientFill (grad);
                g.fillRect (a);
                const float r = (1.0f - lub) * juce::jmin (w, h) * 0.45f;
                g.setColour (col.withAlpha (lub * env * 0.5f));
                g.drawEllipse (a.getCentreX() - r, a.getCentreY() - r, r * 2.0f, r * 2.0f, 2.0f);
                break;
            }
        }
    }

    // ---- banners ------------------------------------------------------------

    void StoryFx::drawBanner (juce::Graphics& g, const Banner& b, juce::Rectangle<float> a) const
    {
        const float in = juce::jlimit (0.0f, 1.0f, b.age / 0.5f);
        const float out = juce::jlimit (0.0f, 1.0f, (kBannerLife - b.age) / 0.6f);
        const float e = in * in * (3.0f - 2.0f * in);       // smoothstep wipe in
        const float alpha = out;
        const float bandH = 74.0f;
        auto band = juce::Rectangle<float> (a.getX(), a.getCentreY() - bandH * 0.5f, a.getWidth() * e, bandH);

        g.setColour (juce::Colours::black.withAlpha (0.62f * alpha));
        g.fillRect (band);
        g.setColour (b.colour.withAlpha (0.85f * alpha));
        g.fillRect (band.withHeight (1.5f));
        g.fillRect (band.withTop (band.getBottom() - 1.5f));

        if (e < 0.55f) return;
        const float tIn = juce::jlimit (0.0f, 1.0f, (b.age - 0.3f) / 0.7f);
        const int chars = (int) (b.title.length() * tIn);
        auto text = a.withSizeKeepingCentre (a.getWidth() - 40.0f, bandH).reduced (6.0f, 8.0f);
        g.setFont (theme::uiFont (22.0f, true));
        g.setColour (b.colour.brighter (0.3f).withAlpha (alpha));
        g.drawText (b.title.substring (0, chars), text.removeFromTop (30.0f), juce::Justification::centred, true);
        g.setFont (theme::uiFont (11.5f));
        g.setColour (theme::text.withAlpha (alpha * tIn));
        g.drawText (b.subtitle, text, juce::Justification::centred, true);
    }

    // ---- fact card ------------------------------------------------------------

    void StoryFx::drawToast (juce::Graphics& g, const Toast& t, juce::Rectangle<float> a) const
    {
        const float in = juce::jlimit (0.0f, 1.0f, t.age / 0.4f);
        const float out = juce::jlimit (0.0f, 1.0f, (kToastLife - t.age) / 1.0f);
        const float alpha = juce::jmin (in, out);
        const float w = juce::jmin (a.getWidth() - 40.0f, 640.0f);
        const float h = t.bonus > 0 ? 100.0f : 84.0f;
        const float slide = (1.0f - in) * -14.0f;
        auto card = juce::Rectangle<float> (a.getCentreX() - w * 0.5f, a.getY() + 76.0f + slide, w, h);

        g.setColour (juce::Colours::black.withAlpha (0.66f * alpha));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (t.colour.withAlpha (0.8f * alpha));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.2f);

        auto inner = card.reduced (14.0f, 9.0f);
        auto head = inner.removeFromTop (14.0f);
        g.setFont (theme::monoFont (9.5f));
        g.setColour (t.colour.brighter (0.2f).withAlpha (alpha));
        g.drawText ("DID YOU KNOW  /  " + t.category.toUpperCase(), head, juce::Justification::centredLeft, true);
        g.setColour (theme::text.withAlpha (0.9f * alpha));
        g.drawText ("+" + juce::String (t.points) + " KNOWLEDGE", head, juce::Justification::centredRight, true);

        const int chars = (int) juce::jmin ((float) t.text.length(), t.age * 55.0f);
        auto body = inner.removeFromTop (t.bonus > 0 ? 50.0f : 56.0f);
        g.setFont (theme::uiFont (13.0f));
        g.setColour (theme::text.withAlpha (alpha));
        g.drawFittedText (t.text.substring (0, chars), body.toNearestInt(), juce::Justification::topLeft, 3, 0.95f);

        if (t.bonus > 0)
        {
            juce::String s (t.bonus);
            for (int i = s.length() - 3; i > 0; i -= 3) s = s.substring (0, i) + "," + s.substring (i);
            g.setFont (theme::uiFont (11.5f, true));
            g.setColour (juce::Colour (0xffffd36b).withAlpha (alpha));
            g.drawText ("HEARD IT?  +" + s + " POINTS", inner, juce::Justification::centredRight, true);
        }
    }
}
