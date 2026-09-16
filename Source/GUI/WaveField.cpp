#include "WaveField.h"
#include <cmath>

namespace mutagen
{
    namespace
    {
        /*  A direct HSV->RGB. juce::Colour::fromHSV allocates nothing but it
            does build a Colour and then hand back three components through
            accessors, and at 8000 pixels a frame that adds up. This is the
            same maths without the round trip. */
        inline void hsvToRgb (float h, float s, float v,
                              juce::uint8& r, juce::uint8& g, juce::uint8& b) noexcept
        {
            h = h - std::floor (h);
            s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

            const float hh = h * 6.0f;
            const int   i  = (int) hh;
            const float f  = hh - (float) i;
            const float p  = v * (1.0f - s);
            const float q  = v * (1.0f - s * f);
            const float t  = v * (1.0f - s * (1.0f - f));

            float rf, gf, bf;
            switch (i % 6)
            {
                case 0:  rf = v; gf = t; bf = p; break;
                case 1:  rf = q; gf = v; bf = p; break;
                case 2:  rf = p; gf = v; bf = t; break;
                case 3:  rf = p; gf = q; bf = v; break;
                case 4:  rf = t; gf = p; bf = v; break;
                default: rf = v; gf = p; bf = q; break;
            }

            r = (juce::uint8) (rf * 255.0f);
            g = (juce::uint8) (gf * 255.0f);
            b = (juce::uint8) (bf * 255.0f);
        }
    }

    WaveField::WaveField()
        : cur ((size_t) (gridW * gridH), 0.0f),
          prev ((size_t) (gridW * gridH), 0.0f),
          dmg ((size_t) (gridW * gridH), 0.0f),
          image (juce::Image::ARGB, gridW, gridH, true)
    {
    }

    void WaveField::reset()
    {
        std::fill (cur.begin(), cur.end(), 0.0f);
        std::fill (prev.begin(), prev.end(), 0.0f);
        std::fill (dmg.begin(), dmg.end(), 0.0f);
        agitationValue = 0.0f;
    }

    // -----------------------------------------------------------------------

    void WaveField::ripple (float nx, float ny, float strength, bool destructive)
    {
        const int cx = juce::jlimit (0, gridW - 1, (int) (nx * gridW));
        const int cy = juce::jlimit (0, gridH - 1, (int) (ny * gridH));

        strength = juce::jlimit (0.0f, 1.0f, strength);
        const int radius = 2 + (int) (strength * 5.0f);
        const float amp = strength * (destructive ? -1.6f : 1.4f);

        for (int y = -radius; y <= radius; ++y)
        {
            for (int x = -radius; x <= radius; ++x)
            {
                const int px = cx + x, py = cy + y;
                if (px < 0 || py < 0 || px >= gridW || py >= gridH) continue;

                const float d = std::sqrt ((float) (x * x + y * y));
                if (d > (float) radius) continue;

                // Gaussian-ish drop so the impact has a soft edge.
                const float falloff = std::exp (-(d * d) / ((float) radius * 0.6f));
                cur[(size_t) index (px, py)] += amp * falloff;

                if (destructive)
                    dmg[(size_t) index (px, py)] =
                        juce::jmin (1.0f, dmg[(size_t) index (px, py)] + strength * falloff);
            }
        }
    }

    void WaveField::swell (float strength, bool destructive)
    {
        // A ring around the edge collapsing inward reads as "the whole world
        // just did something" rather than as a click somewhere.
        for (int i = 0; i < 24; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
            ripple (0.5f + std::cos (a) * 0.45f,
                    0.5f + std::sin (a) * 0.45f,
                    strength * 0.6f, destructive);
        }
    }

    // -----------------------------------------------------------------------

    void WaveField::step (float dt)
    {
        /*  Discrete wave equation with damping:
              next = 2*cur - prev + c^2 * laplacian(cur), all scaled by decay.
            c is kept comfortably below the stability limit for this grid. */
        constexpr float c2 = 0.22f;
        const float decay = std::exp (-dt * 1.35f);

        for (int y = 1; y < gridH - 1; ++y)
        {
            for (int x = 1; x < gridW - 1; ++x)
            {
                const int i = index (x, y);
                const float lap = cur[(size_t) (i - 1)] + cur[(size_t) (i + 1)]
                                + cur[(size_t) (i - gridW)] + cur[(size_t) (i + gridW)]
                                - 4.0f * cur[(size_t) i];

                float next = 2.0f * cur[(size_t) i] - prev[(size_t) i] + c2 * lap;
                next *= decay;
                prev[(size_t) i] = next;
            }
        }

        // edges: absorb rather than reflect, so ripples leave instead of
        // bouncing around forever and turning the surface to mush
        for (int x = 0; x < gridW; ++x)
        {
            prev[(size_t) index (x, 0)] = cur[(size_t) index (x, 1)] * 0.4f;
            prev[(size_t) index (x, gridH - 1)] = cur[(size_t) index (x, gridH - 2)] * 0.4f;
        }
        for (int y = 0; y < gridH; ++y)
        {
            prev[(size_t) index (0, y)] = cur[(size_t) index (1, y)] * 0.4f;
            prev[(size_t) index (gridW - 1, y)] = cur[(size_t) index (gridW - 2, y)] * 0.4f;
        }

        cur.swap (prev);

        const float dmgDecay = std::exp (-dt * 0.55f);
        for (auto& d : dmg) d *= dmgDecay;
    }

    void WaveField::update (float dt)
    {
        dt = juce::jlimit (0.0f, 0.1f, dt);
        phase += dt;

        // Fixed 120 Hz substeps keep the wave speed frame-rate independent.
        constexpr float fixed = 1.0f / 120.0f;
        accumulator += dt;
        int guard = 0;
        while (accumulator >= fixed && guard++ < 8)
        {
            step (fixed);
            accumulator -= fixed;
        }

        float sum = 0.0f;
        for (const auto v : cur) sum += std::fabs (v);
        agitationValue = juce::jlimit (0.0f, 1.0f, sum / (float) cur.size() * 6.0f);
    }

    float WaveField::amplitudeAt (float nx, float ny) const
    {
        const int x = juce::jlimit (0, gridW - 1, (int) (nx * gridW));
        const int y = juce::jlimit (0, gridH - 1, (int) (ny * gridH));
        return juce::jlimit (0.0f, 1.0f, std::fabs (cur[(size_t) index (x, y)]));
    }

    // -----------------------------------------------------------------------

    void WaveField::render (juce::Graphics& g, juce::Rectangle<float> area, const Palette& p)
    {
        {
            juce::Image::BitmapData bits (image, juce::Image::BitmapData::writeOnly);

            /*  Colour rules, straight from the brief:
                  more varied  -> more colourful
                  more noise   -> more grey
                So saturation is variety scaled down by greyness, and we let
                greyness also pull the hue toward a flat neutral.               */
            const float sat = juce::jlimit (0.0f, 1.0f,
                                            (0.18f + 0.82f * p.variety) * (1.0f - p.greyness * 0.95f));
            const float baseBright = 0.10f + 0.30f * juce::jlimit (0.0f, 1.0f, p.energy);

            /*  Which island owns each patch of the field changes slowly and is
                a nearest-neighbour search over up to eight points. Resolving it
                per pixel meant ~64k distance tests a frame; on a 1/4-resolution
                grid it is 4k, and nothing about the result is visible at full
                resolution anyway because the wave amplitude dominates. */
            constexpr int cw = (gridW + 3) / 4, ch = (gridH + 3) / 4;
            float coarseHue[cw * ch];
            for (int cy = 0; cy < ch; ++cy)
            {
                const float ny = ((float) cy * 4.0f + 2.0f) / (float) gridH;
                for (int cx = 0; cx < cw; ++cx)
                {
                    const float nx = ((float) cx * 4.0f + 2.0f) / (float) gridW;
                    float hue = p.baseHue + nx * 0.08f + ny * 0.05f;
                    float nearest = 1.0e9f;
                    for (int n = 0; n < p.nicheCount && n < 8; ++n)
                    {
                        const float dx = nx - p.nicheX[n], dy = ny - p.nicheY[n];
                        const float d = dx * dx + dy * dy;
                        if (d < nearest) { nearest = d; hue = p.baseHue * 0.35f + p.nicheHue[n] * 0.65f; }
                    }
                    coarseHue[cy * cw + cx] = hue + p.heat * 0.06f;
                }
            }

            for (int y = 0; y < gridH; ++y)
            {
                auto* row = bits.getLinePointer (y);
                const int cyRow = (y / 4) * cw;

                for (int x = 0; x < gridW; ++x)
                {
                    const int i = index (x, y);
                    const float v = cur[(size_t) i];
                    const float damage = dmg[(size_t) i];

                    // Hue comes from whichever island owns this patch; crests
                    // shift it slightly so waves are visible as colour as well
                    // as brightness.
                    float hue = coarseHue[cyRow + (x / 4)] - 0.5f * v * 0.12f;

                    float bright = baseBright + std::fabs (v) * 0.9f;
                    float s = sat;

                    // Damage desaturates and darkens its own patch: subtraction
                    // should look like something being taken away.
                    if (damage > 0.01f)
                    {
                        s *= 1.0f - damage * 0.9f;
                        bright *= 1.0f - damage * 0.35f;
                        hue = 0.02f;                   // a dull red bruise
                    }

                    juce::uint8 r8, g8, b8;
                    hsvToRgb (hue, s, bright, r8, g8, b8);
                    auto* px = reinterpret_cast<juce::PixelARGB*> (row + x * bits.pixelStride);
                    px->setARGB (255, r8, g8, b8);
                }
            }

        }   // BitmapData must go out of scope before the image is drawn

        g.setOpacity (1.0f);
        g.drawImage (image, area, juce::RectanglePlacement::stretchToFit, false);
    }
}
