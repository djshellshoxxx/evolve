#include "CultureChamber.h"
#include "MutagenLookAndFeel.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace mutagen
{
    using namespace theme;

    namespace
    {
        // How often a drag is allowed to send a mutation command. Fast enough
        // that dragging feels continuous, slow enough that a vigorous scribble
        // cannot flood the command ring.
        constexpr double dragEmitInterval = 0.045;
        constexpr float  dragEmitDistance = 9.0f;
    }

    CultureChamber::CultureChamber (MutagenProcessor& p) : processor (p)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
    }

    void CultureChamber::resized()
    {
        field = getLocalBounds().toFloat().reduced (18.0f);
    }

    juce::Point<float> CultureChamber::toPixels (float nx, float ny) const
    {
        auto f = field.isEmpty() ? getLocalBounds().toFloat().reduced (18.0f) : field;
        return { f.getX() + nx * f.getWidth(), f.getY() + ny * f.getHeight() };
    }

    juce::Point<float> CultureChamber::toNormalised (juce::Point<float> p) const
    {
        auto f = field.isEmpty() ? getLocalBounds().toFloat().reduced (18.0f) : field;
        if (f.getWidth() <= 0.0f || f.getHeight() <= 0.0f) return { 0.5f, 0.5f };
        return { juce::jlimit (0.0f, 1.0f, (p.x - f.getX()) / f.getWidth()),
                 juce::jlimit (0.0f, 1.0f, (p.y - f.getY()) / f.getHeight()) };
    }

    // =====================================================================

    void CultureChamber::update (const EngineSnapshot& s, double dt)
    {
        const bool newRadiation = s.radiationFlash > snap.radiationFlash + 0.3f;
        snap = s;
        phase += dt;

        waves.update ((float) dt);
        ghost.update ((float) dt);

        // Colour follows the measurement, smoothed so it breathes rather than
        // flickers with every analysis frame.
        const float satTarget = juce::jlimit (0.0f, 1.0f, snap.variety);
        colourSat += (satTarget - colourSat) * (float) juce::jmin (1.0, dt * 2.5);
        greyLevel += (snap.greyness - greyLevel) * (float) juce::jmin (1.0, dt * 3.0);

        // RADIATE: a red flash that fades quickly back to normal.
        if (newRadiation)
        {
            redFlash = 1.0f;
            waves.swell (0.8f, snap.lastRadiation < 0);
        }
        redFlash *= std::exp (-(float) dt * 2.2f);

        for (auto& r : ripples) { r.r += (float) dt * 150.0f; r.life -= (float) dt; }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(),
                        [] (const Ripple& r) { return r.life <= 0.0f; }), ripples.end());

        updateGameOrbs (dt);
        repaint();
    }

    void CultureChamber::triggerReward (float hue)
    {
        // Somewhere in the middle of the field, so it reads as happening *to*
        // the colony rather than as chrome in a corner.
        ghost.trigger ({ 0.3f + rnd.nextFloat() * 0.4f, 0.3f + rnd.nextFloat() * 0.4f },
                       hue, rnd);
    }

    void CultureChamber::addOrb (OrbKind kind, float speedScale, float lifeScale)
    {
        if (gameOrbs.size() >= 2600)
            return;

        const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        const float speed = (0.055f + rnd.nextFloat() * 0.095f) * speedScale;
        Orb o;
        o.p = { 0.08f + rnd.nextFloat() * 0.84f, 0.08f + rnd.nextFloat() * 0.84f };
        o.v = { std::cos (a) * speed, std::sin (a) * speed };
        o.radius = kind == OrbKind::monster ? 9.0f
                 : kind == OrbKind::mini ? 2.2f
                 : kind == OrbKind::glowingRainbow ? 5.5f : 4.2f;
        o.maxLife = (kind == OrbKind::glowingRainbow ? 34.0f : 8.0f) * lifeScale;
        o.life = o.maxLife;
        o.kind = kind;
        o.pulse = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        gameOrbs.push_back (o);
    }

    void CultureChamber::spawnSpecialOrbs (int count)
    {
        count = juce::jlimit (0, 600, count);
        for (int i = 0; i < count; ++i)
        {
            const auto kind = (i % 5 == 0) ? OrbKind::red
                            : (i % 7 == 0) ? OrbKind::pink
                                           : OrbKind::special;
            addOrb (kind, 1.0f, 1.0f);
        }
    }

    void CultureChamber::spawnRainbowOrbs (int count, bool glowing, int multiplier)
    {
        count = juce::jlimit (0, 1200, count);
        multiplier = juce::jlimit (1, 2, multiplier);
        for (int i = 0; i < count; ++i)
        {
            addOrb (glowing ? OrbKind::glowingRainbow : OrbKind::rainbow, 1.2f, 2.0f);
            if (multiplier == 2 && ! gameOrbs.empty())
                gameOrbs.back().multiplied = false;
            else if (! gameOrbs.empty())
                gameOrbs.back().multiplied = true;
        }
    }

    void CultureChamber::spawnMonsterOrbs (int count)
    {
        for (int i = 0; i < juce::jlimit (0, 200, count); ++i)
            addOrb (OrbKind::monster, 3.0f, 1.0f);
    }

    void CultureChamber::spawnMiniOrbs (int count)
    {
        for (int i = 0; i < juce::jlimit (0, 100, count); ++i)
            addOrb (OrbKind::mini, 1.0f / 3.0f, 10.0f);
    }

    void CultureChamber::setOrbInversion (float seconds)
    {
        orbInversionSeconds = juce::jmax (orbInversionSeconds, juce::jlimit (1.0f, 60.0f, seconds));
        for (auto& o : gameOrbs)
            o.v = -o.v;
    }

    juce::Colour CultureChamber::orbColour (OrbKind kind, float p) const
    {
        if (orbInversionSeconds > 0.0f)
            p = 1.0f - p;

        switch (kind)
        {
            case OrbKind::special: return juce::Colour::fromHSV (std::fmod (0.72f + p * 0.22f, 1.0f), 0.72f, 1.0f, 1.0f);
            case OrbKind::rainbow:
            case OrbKind::glowingRainbow: return juce::Colour::fromHSV (std::fmod (p + (float) phase * 0.13f, 1.0f), 0.92f, 1.0f, 1.0f);
            case OrbKind::monster: return juce::Colour::fromHSV (std::fmod (0.83f + p * 0.12f, 1.0f), 0.95f, 1.0f, 1.0f);
            case OrbKind::mini: return juce::Colour (0xffb8fff8);
            case OrbKind::green: return juce::Colour (0xff52ff79);
            case OrbKind::red: return juce::Colour (0xffff465f);
            case OrbKind::pink: return juce::Colour (0xffff70c8);
        }
        return juce::Colours::white;
    }

    void CultureChamber::updateGameOrbs (double dtSeconds)
    {
        const float dt = (float) juce::jlimit (0.0, 0.1, dtSeconds);
        orbInversionSeconds = juce::jmax (0.0f, orbInversionSeconds - dt);
        orbCollisionClock += dt;

        int greenBorn = 0;
        int cornerEvents = 0;
        std::vector<Orb> multiplied;
        multiplied.reserve (64);

        for (auto& o : gameOrbs)
        {
            o.life -= dt;
            o.pulse += dt * (o.kind == OrbKind::monster ? 11.0f : o.kind == OrbKind::mini ? 0.7f : 3.0f);

            const auto before = o.p;
            o.p += o.v * dt;

            const bool hitX = o.p.x < 0.0f || o.p.x > 1.0f;
            const bool hitY = o.p.y < 0.0f || o.p.y > 1.0f;
            if (hitX) { o.p.x = juce::jlimit (0.0f, 1.0f, o.p.x); o.v.x = -o.v.x; }
            if (hitY) { o.p.y = juce::jlimit (0.0f, 1.0f, o.p.y); o.v.y = -o.v.y; }

            if ((o.kind == OrbKind::rainbow || o.kind == OrbKind::glowingRainbow) && hitX && hitY)
            {
                ++cornerEvents;
                const auto px = toPixels (o.p.x, o.p.y);
                for (int i = 0; i < 90; ++i)
                {
                    Sprinkle s;
                    s.p = px;
                    s.v = { (rnd.nextFloat() - 0.5f) * 150.0f,
                            30.0f + rnd.nextFloat() * 150.0f };
                    s.life = 1.4f + rnd.nextFloat() * 1.3f;
                    s.c = juce::Colour::fromHSV (rnd.nextFloat(), 0.95f, 1.0f, 1.0f);
                    sprinkles.push_back (s);
                }
            }

            // Jackpot rainbows are born 2-for-1 once, after entering motion.
            if (o.kind == OrbKind::glowingRainbow && ! o.multiplied
                && o.life < o.maxLife * 0.82f && gameOrbs.size() + multiplied.size() < 2600)
            {
                Orb twin = o;
                twin.v = { -o.v.y * 0.92f, o.v.x * 0.92f };
                twin.multiplied = true;
                o.multiplied = true;
                multiplied.push_back (twin);
            }
        }

        for (auto& twin : multiplied)
            gameOrbs.push_back (twin);

        // Bounded collision sampling. Rainbow + red/pink breeds a persistent
        // green orb; we never do an O(N^2) pass over a 1000-orb jackpot.
        if (orbCollisionClock >= 0.10f && gameOrbs.size() > 1)
        {
            orbCollisionClock = 0.0f;
            const int checks = juce::jmin (420, (int) gameOrbs.size() * 2);
            for (int n = 0; n < checks && gameOrbs.size() < 2600; ++n)
            {
                const int a = rnd.nextInt ((int) gameOrbs.size());
                const int b = rnd.nextInt ((int) gameOrbs.size());
                if (a == b) continue;
                auto& x = gameOrbs[(size_t) a];
                auto& y = gameOrbs[(size_t) b];
                const bool xr = x.kind == OrbKind::rainbow || x.kind == OrbKind::glowingRainbow;
                const bool yr = y.kind == OrbKind::rainbow || y.kind == OrbKind::glowingRainbow;
                const bool xMate = x.kind == OrbKind::red || x.kind == OrbKind::pink;
                const bool yMate = y.kind == OrbKind::red || y.kind == OrbKind::pink;
                if (! ((xr && yMate) || (yr && xMate))) continue;
                if (x.p.getDistanceFrom (y.p) > 0.032f) continue;

                Orb green;
                green.kind = OrbKind::green;
                green.p = (x.p + y.p) * 0.5f;
                green.v = (x.v + y.v) * 0.45f;
                green.radius = 3.8f;
                green.maxLife = green.life = 18.0f;
                green.pulse = rnd.nextFloat() * 6.0f;
                green.multiplied = true;
                gameOrbs.push_back (green);
                ++greenBorn;
            }
        }

        gameOrbs.erase (std::remove_if (gameOrbs.begin(), gameOrbs.end(),
                        [] (const Orb& o) { return o.life <= 0.0f; }), gameOrbs.end());

        for (auto& s : sprinkles)
        {
            s.life -= dt;
            s.p += s.v * dt;
            s.v.y += 100.0f * dt;
        }
        sprinkles.erase (std::remove_if (sprinkles.begin(), sprinkles.end(),
                         [] (const Sprinkle& s) { return s.life <= 0.0f; }), sprinkles.end());

        if (greenBorn > 0 && onGreenOrbsBorn)
            onGreenOrbsBorn (greenBorn);
        if (cornerEvents > 0 && onRainbowCorner)
            onRainbowCorner();
    }

    void CultureChamber::paintGameOrbs (juce::Graphics& g)
    {
        for (const auto& o : gameOrbs)
        {
            const auto p = toPixels (o.p.x, o.p.y);
            const float pulse = 0.78f + 0.22f * std::sin (o.pulse);
            float radius = o.radius * pulse;
            const float huePhase = std::fmod (o.p.x * 0.37f + o.p.y * 0.63f + o.pulse * 0.03f, 1.0f);
            const auto colour = orbColour (o.kind, huePhase);

            if (o.kind == OrbKind::glowingRainbow || o.kind == OrbKind::monster)
            {
                g.setColour (colour.withAlpha (0.14f));
                g.fillEllipse (p.x - radius * 2.2f, p.y - radius * 2.2f,
                               radius * 4.4f, radius * 4.4f);
            }
            g.setColour (colour.withAlpha (0.88f));
            g.fillEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.fillEllipse (p.x - radius * 0.42f, p.y - radius * 0.48f,
                           radius * 0.54f, radius * 0.54f);
        }

        for (const auto& s : sprinkles)
        {
            g.setColour (s.c.withAlpha (juce::jlimit (0.0f, 1.0f, s.life)));
            g.fillRect (juce::Rectangle<float> (s.p.x, s.p.y, 3.0f, 7.0f));
        }
    }

    // =====================================================================
    //  Interaction
    // =====================================================================

    void CultureChamber::strike (juce::Point<float> pos, bool destructive,
                                 float strength, bool fromDrag)
    {
        const auto n = toNormalised (pos);

        // Radius grows with strength so a hard drag sweeps a wider front.
        const float radius = (fromDrag ? 0.10f : 0.15f) + strength * 0.14f;

        waves.ripple (n.x, n.y, strength, destructive);

        EngineCommand c;
        c.type = destructive ? CommandType::subtractAt : CommandType::mutateAt;
        c.fa = n.x;
        c.fb = n.y;
        c.fc = radius;
        c.fd = strength;
        processor.pushCommand (c);

        ripples.push_back ({ pos.x, pos.y, 4.0f, 0.55f, 0.55f,
                             destructive ? juce::Colour (0xffff6b6b)
                                         : juce::Colour::fromHSV (snap.worldHue, 0.6f, 1.0f, 1.0f),
                             destructive });

        // Every gesture is also an entropy observation: where the player
        // clicked and exactly when they did it.
        processor.noteUserGesture (n.x, n.y);

        if (onInteraction)
            onInteraction (strength * (fromDrag ? 0.35f : 1.0f),
                           fromDrag ? juce::String()
                                    : (destructive ? "STRIPPED" : "MUTATED"));
    }

    void CultureChamber::mouseDown (const juce::MouseEvent& e)
    {
        if (hudProbe && hudProbe (e.getPosition())) return;

        const bool right = e.mods.isPopupMenu() || e.mods.isRightButtonDown();

        // The old menu is still reachable, it just no longer owns right-click.
        if (right && (e.mods.isCtrlDown() || e.mods.isCommandDown()))
        {
            showContextMenu (hitTestCell (e.position));
            return;
        }

        dragging = true;
        dragDestructive = right;
        lastEmit = e.position;
        lastEmitTime = phase;
        dragSpeed = 0.0f;

        // Selection still happens on a left click over a cell, so inspecting
        // and mutating are the same gesture rather than competing ones.
        if (! right)
        {
            const int hit = hitTestCell (e.position);
            if (hit >= 0)
            {
                const auto& c = snap.cells[hit];
                selection.active = true;
                selection.cellSlot = hit;
                if (e.mods.isAltDown())        { selection.level = ScopeLevel::species; selection.id = c.species; }
                else if (e.mods.isShiftDown()) { selection.level = ScopeLevel::cell;    selection.id = hit; }
                else                           { selection.level = ScopeLevel::family;  selection.id = c.familyId; }
                emitSelection();
            }
        }

        strike (e.position, right, right ? 0.7f : 0.75f, false);
    }

    void CultureChamber::mouseDrag (const juce::MouseEvent& e)
    {
        mousePos = e.position;
        if (! dragging) return;

        const float dist = e.position.getDistanceFrom (lastEmit);
        const double dt = phase - lastEmitTime;

        // Speed drives strength: a slow, careful drag nudges; a fast slash
        // across the chamber causes the mass mutation the brief asks for.
        if (dt > 1.0e-4)
            dragSpeed = dragSpeed * 0.7f + (float) (dist / dt) * 0.3f;

        if (dist < dragEmitDistance && dt < dragEmitInterval) return;

        const float speedNorm = juce::jlimit (0.0f, 1.0f, dragSpeed / 1400.0f);
        const float strength = 0.25f + 0.7f * speedNorm;

        strike (e.position, dragDestructive, strength, true);

        lastEmit = e.position;
        lastEmitTime = phase;
    }

    void CultureChamber::mouseUp (const juce::MouseEvent&)
    {
        if (dragging && onInteraction)
            onInteraction (0.5f, dragDestructive ? "CARVED" : "WAVE");
        dragging = false;
        dragSpeed = 0.0f;
    }

    void CultureChamber::mouseMove (const juce::MouseEvent& e)
    {
        mousePos = e.position;
        const int h = hitTestCell (e.position);
        if (h != hoverCell) { hoverCell = h; repaint(); }
    }

    void CultureChamber::mouseExit (const juce::MouseEvent&)
    {
        hoverCell = -1;
        dragging = false;
        repaint();
    }

    void CultureChamber::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int hit = hitTestCell (e.position);
        if (hit >= 0)
        {
            const auto& c = snap.cells[hit];
            Selection s;
            s.active = true;
            s.level = ScopeLevel::family;
            s.id = c.familyId;
            s.cellSlot = hit;
            if (onSendToBreedingLab) onSendToBreedingLab (s);
        }
        else
        {
            EngineCommand cmd;
            cmd.type = CommandType::noteBurst;
            cmd.ia = 48 + rnd.nextInt (24);
            cmd.fa = 0.9f;
            processor.pushCommand (cmd);

            const auto n = toNormalised (e.position);
            waves.ripple (n.x, n.y, 1.0f, false);
            if (onInteraction) onInteraction (0.8f, "SEEDED");
        }
    }

    // =====================================================================
    //  File drops - the colony eats what you give it
    // =====================================================================

    bool CultureChamber::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& f : files)
        {
            const auto ext = juce::File (f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3"
                || ext == ".flac" || ext == ".ogg" || ext == ".m4a")
                return true;
        }
        return false;
    }

    void CultureChamber::fileDragEnter (const juce::StringArray&, int, int)
    {
        fileHover = true;
        repaint();
    }

    void CultureChamber::fileDragExit (const juce::StringArray&)
    {
        fileHover = false;
        repaint();
    }

    void CultureChamber::filesDropped (const juce::StringArray& files, int x, int y)
    {
        fileHover = false;

        const auto n = toNormalised (juce::Point<float> ((float) x, (float) y));
        waves.ripple (n.x, n.y, 1.0f, false);
        waves.swell (0.5f, false);

        // A drop is also a good entropy observation - a filename, a size and a
        // moment the user chose.
        for (const auto& f : files)
            processor.noteUserGesture (n.x, n.y, (uint64_t) juce::File (f).getSize());

        if (onFilesDropped) onFilesDropped (files);
        repaint();
    }

    // =====================================================================
    //  Painting
    // =====================================================================

    juce::Colour CultureChamber::cellColour (const CellView& c) const
    {
        /*  Hue comes from the cell's own island and its register, so related
            cells share a family resemblance and different populations are
            visibly different. Saturation is the colony's measured variety,
            which is what makes a colony collapsing into noise fade to grey. */
        const int ni = c.speciesGroupId >= 0 && snap.nicheCount > 0
                     ? (c.speciesGroupId % snap.nicheCount) : 0;
        float hue = snap.nicheHue[juce::jlimit (0, EngineSnapshot::maxNiches - 1, ni)];
        hue = std::fmod (hue + (float) c.species * 0.045f + 1.0f, 1.0f);

        const float sat = juce::jlimit (0.0f, 1.0f,
                                        (0.25f + 0.75f * colourSat) * (1.0f - greyLevel * 0.92f));
        const float bright = 0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, c.energy);
        return juce::Colour::fromHSV (hue, sat, bright, 1.0f);
    }

    void CultureChamber::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        field = b.reduced (18.0f);

        g.fillAll (bg0);

        // ---- the wave surface ------------------------------------------
        WaveField::Palette pal;
        pal.baseHue  = snap.worldHue;
        pal.variety  = colourSat;
        pal.greyness = greyLevel;
        pal.energy   = juce::jlimit (0.0f, 1.0f, snap.outputRms * 4.0f);
        pal.heat     = snap.thermalField;
        pal.nicheCount = juce::jlimit (0, 8, snap.nicheCount);
        for (int i = 0; i < pal.nicheCount; ++i)
        {
            pal.nicheHue[i] = snap.nicheHue[i];
            pal.nicheX[i]   = snap.nicheX[i];
            pal.nicheY[i]   = snap.nicheY[i];
        }
        waves.render (g, b, pal);

        // Darken the surface so the cells read on top of it.
        g.setColour (bg0.withAlpha (0.55f));
        g.fillRect (b);

        // ---- environmental fields ---------------------------------------
        const float breathe = 0.5f + 0.5f * std::sin ((float) phase * 0.7f);

        if (snap.infectionField > 0.03f)
        {
            g.setColour (infection.withAlpha (0.05f * snap.infectionField * (0.6f + 0.4f * breathe)));
            g.fillRect (field);
        }

        if (snap.pressureFront > 0.02f)
        {
            const float sweep = std::fmod ((float) phase * 0.6f, 1.0f);
            const float sx = field.getX() + sweep * field.getWidth();
            juce::ColourGradient pg (juce::Colours::transparentBlack, sx - 40.0f, 0,
                                     accent.withAlpha (0.18f * snap.pressureFront), sx, 0, false);
            pg.addColour (1.0, juce::Colours::transparentBlack);
            g.setGradientFill (pg);
            g.fillRect (juce::Rectangle<float> (sx - 40.0f, field.getY(), 80.0f, field.getHeight()));
        }

        // ---- ENZYME sparkles ---------------------------------------------
        if (snap.sparkleField > 0.02f)
        {
            juce::Random sr ((int) (phase * 24.0));
            const int count = (int) (snap.sparkleField * 90.0f);
            for (int i = 0; i < count; ++i)
            {
                const float sx = field.getX() + sr.nextFloat() * field.getWidth();
                const float sy = field.getY() + sr.nextFloat() * field.getHeight();
                const float s = 1.0f + sr.nextFloat() * 2.4f;
                g.setColour (juce::Colours::white.withAlpha (snap.sparkleField * sr.nextFloat() * 0.8f));
                g.fillEllipse (sx, sy, s, s);
            }
        }

        paintNiches (g);
        paintCells (g);

        // ---- interaction ripples ------------------------------------------
        for (const auto& r : ripples)
        {
            const float a = juce::jlimit (0.0f, 0.6f, r.life / r.maxLife * 0.6f);
            g.setColour (r.c.withAlpha (a));
            g.drawEllipse (juce::Rectangle<float> (r.r * 2.0f, r.r * 2.0f).withCentre ({ r.x, r.y }),
                           r.destructive ? 2.2f : 1.6f);
            if (r.destructive)
            {
                g.setColour (r.c.withAlpha (a * 0.4f));
                g.drawEllipse (juce::Rectangle<float> (r.r * 1.2f, r.r * 1.2f).withCentre ({ r.x, r.y }), 1.0f);
            }
        }

        // ---- the fractal reward -------------------------------------------
        ghost.render (g, field);

        // Lab-game orbs live above the colony but below alert flashes.
        paintGameOrbs (g);

        // ---- RADIATE red flash ---------------------------------------------
        if (redFlash > 0.01f)
        {
            g.setColour (juce::Colour (0xffff2020).withAlpha (redFlash * 0.28f));
            g.fillRect (b);
            g.setColour (juce::Colour (0xffff5050).withAlpha (redFlash * 0.7f));
            g.drawRect (b, 3.0f);
        }

        // ---- live spectrum along the bottom ---------------------------------
        paintSpectrum (g, juce::Rectangle<float> (field.getX(), field.getBottom() - 46.0f,
                                                  field.getWidth(), 44.0f));

        // ---- file drop affordance -------------------------------------------
        if (fileHover)
        {
            g.setColour (accent.withAlpha (0.14f));
            g.fillRect (b);
            g.setColour (accent);
            g.drawRect (b.reduced (6.0f), 2.0f);
            g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
            g.drawText ("DROP TO FEED THE COLONY", b, juce::Justification::centred);
        }

        // ---- hover label -------------------------------------------------------
        if (hoverCell >= 0 && hoverCell < snap.count && ! dragging)
        {
            const auto& c = snap.cells[hoverCell];
            const char* sp = c.species == 0 ? "grain" : c.species == 1 ? "spectral" : "resonator";
            juce::String txt;
            txt << sp << "  fam#" << c.familyId << "  gen " << c.generation
                << "  e" << juce::String (c.energy, 2);
            auto tb = juce::Rectangle<float> (mousePos.x + 12, mousePos.y - 8, 200, 20);
            g.setColour (bg0.withAlpha (0.85f));
            g.fillRoundedRectangle (tb, 4.0f);
            g.setColour (accent);
            g.setFont (11.5f);
            g.drawText (txt, tb.reduced (6, 2), juce::Justification::centredLeft);
        }

        // ---- bottom-left status -------------------------------------------------
        auto hud = juce::Rectangle<float> (b.getX() + 12, b.getBottom() - 26, 340, 18);
        g.setColour (text.withAlpha (0.55f));
        g.setFont (10.5f);
        g.drawText ("POP " + juce::String (snap.population)
                    + "   GEN " + juce::String (snap.generation)
                    + "   NICHES " + juce::String (snap.nicheCount)
                    + "   MAP " + juce::String ((int) (snap.coverage * 100.0f)) + "%"
                    + (snap.preservedMode ? "   [PRESERVED]" : ""),
                    hud, juce::Justification::centredLeft);
    }

    void CultureChamber::paintNiches (juce::Graphics& g)
    {
        // Faint territory markers so the player can see that the colony really
        // does keep several separate populations rather than one blob.
        for (int i = 0; i < juce::jmin (snap.nicheCount, EngineSnapshot::maxNiches); ++i)
        {
            const auto p = toPixels (snap.nicheX[i], snap.nicheY[i]);
            const float r = juce::jmin (field.getWidth(), field.getHeight()) * 0.17f;
            const auto c = juce::Colour::fromHSV (snap.nicheHue[i],
                                                  0.5f * (1.0f - greyLevel), 0.8f, 1.0f);
            g.setColour (c.withAlpha (0.05f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
            g.setColour (c.withAlpha (0.13f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 1.0f);
        }
    }

    void CultureChamber::paintCells (juce::Graphics& g)
    {
        // ---- symbiotic links ---------------------------------------------
        g.setColour (resonator.withAlpha (0.16f * (1.0f - greyLevel)));
        for (int i = 0; i < snap.count; ++i)
        {
            const auto& c = snap.cells[i];
            if (c.linkTo < 0) continue;
            for (int j = 0; j < snap.count; ++j)
            {
                if (snap.cells[j].familyId == c.linkTo || j == c.linkTo)
                {
                    g.drawLine ({ toPixels (c.x, c.y), toPixels (snap.cells[j].x, snap.cells[j].y) }, 1.0f);
                    break;
                }
            }
        }

        // ---- arcs -----------------------------------------------------------
        for (int i = 0; i < snap.arcCount; ++i)
        {
            const auto& a = snap.arcs[i];
            if (a.life <= 0.0f) continue;
            const auto p1 = toPixels (a.x1, a.y1);
            const auto p2 = toPixels (a.x2, a.y2);
            const auto mid = (p1 + p2) * 0.5f + juce::Point<float> (0, -30.0f);
            juce::Path path;
            path.startNewSubPath (p1);
            path.quadraticTo (mid, p2);

            juce::Colour ac = a.kind == 1 ? accent : a.kind == 2 ? infection : spectral;
            g.setColour (ac.withAlpha (juce::jlimit (0.0f, 0.7f, a.life)));
            g.strokePath (path, juce::PathStrokeType (1.5f));

            const float t = 1.0f - juce::jlimit (0.0f, 1.0f, a.life);
            juce::Point<float> dot = p1 + (p2 - p1) * t;
            g.setColour (ac.withAlpha (a.life));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
        }

        // ---- cells -------------------------------------------------------------
        const float baseR = juce::jmin (field.getWidth(), field.getHeight());

        for (int i = 0; i < snap.count; ++i)
        {
            const auto& c = snap.cells[i];
            const auto pos = toPixels (c.x, c.y);

            // The wave field physically swells the cells it passes through, so
            // a ripple visibly *does* something to them.
            const float wave = waves.amplitudeAt (c.x, c.y);
            const float rad = juce::jlimit (3.0f, 60.0f,
                                            c.radius * baseR * (1.0f + wave * 1.2f) + 4.0f);

            const bool dying = c.stage >= 5;
            const bool germ  = c.stage == 0;

            juce::Colour col = cellColour (c);
            float alpha = juce::jlimit (0.15f, 1.0f, 0.3f + 0.7f * c.energy) * (dying ? 0.35f : 1.0f);
            if (c.muted) alpha *= 0.3f;

            g.setColour (col.withAlpha (0.12f * alpha * (1.0f + wave)));
            g.fillEllipse (juce::Rectangle<float> (rad * 3.2f, rad * 3.2f).withCentre (pos));

            if (c.species == 0)         // grain: a cluster of fragments
            {
                juce::Random fr (c.familyId * 977 + (int) (phase * 8.0));
                for (int k = 0; k < 4; ++k)
                {
                    const float ang = fr.nextFloat() * juce::MathConstants<float>::twoPi;
                    const float dr  = rad * (0.4f + 0.9f * fr.nextFloat());
                    juce::Point<float> fp (pos.x + std::cos (ang) * dr, pos.y + std::sin (ang) * dr);
                    g.setColour (col.withAlpha (alpha * (0.5f + 0.5f * fr.nextFloat())));
                    g.fillEllipse (juce::Rectangle<float> (rad * 0.5f, rad * 0.5f).withCentre (fp));
                }
                g.setColour (col.withAlpha (alpha));
                juce::Path diamond;
                diamond.addPolygon (pos, 4, rad * 0.8f, (float) phase * 0.5f);
                g.fillPath (diamond);
            }
            else if (c.species == 1)    // spectral: a glowing orb
            {
                juce::ColourGradient og (col.withAlpha (alpha), pos.x, pos.y,
                                         col.withAlpha (0.0f), pos.x + rad, pos.y + rad, true);
                g.setGradientFill (og);
                g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos));
                g.setColour (juce::Colours::white.withAlpha (alpha * 0.5f));
                g.fillEllipse (juce::Rectangle<float> (rad * 0.4f, rad * 0.4f).withCentre (pos));
            }
            else                        // resonator: concentric membranes
            {
                for (int k = 3; k >= 1; --k)
                {
                    g.setColour (col.withAlpha (alpha * (0.15f + 0.15f * (float) k)));
                    g.drawEllipse (juce::Rectangle<float> (rad * 0.8f * (float) k,
                                                           rad * 0.8f * (float) k).withCentre (pos), 1.4f);
                }
                g.setColour (col.withAlpha (alpha));
                g.fillEllipse (juce::Rectangle<float> (rad * 0.7f, rad * 0.7f).withCentre (pos));
            }

            if (germ)
            {
                g.setColour (accent.withAlpha (0.55f));
                g.drawLine (pos.x, pos.y, pos.x, pos.y - rad * 1.6f, 1.4f);
            }

            if (c.infectionType > 0 && c.infection > 0.05f)
            {
                const float flick = 0.5f + 0.5f * std::sin ((float) phase * 20.0f + (float) i);
                g.setColour (infectionColour (c.infectionType).withAlpha (alpha * c.infection * flick));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.4f, rad * 2.4f).withCentre (pos), 1.8f);
            }

            if (c.pulse > 0.02f)
            {
                g.setColour (juce::Colours::white.withAlpha (c.pulse * 0.45f));
                g.drawEllipse (juce::Rectangle<float> (rad * (2.0f + 3.0f * (1.0f - c.pulse)),
                                                       rad * (2.0f + 3.0f * (1.0f - c.pulse))).withCentre (pos), 1.5f);
            }

            if (c.preserved)
            {
                g.setColour (selectRing.withAlpha (0.85f));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos), 1.0f);
            }

            const bool sel = selection.active &&
                ((selection.level == ScopeLevel::species && selection.id == c.species)
              || (selection.level == ScopeLevel::family  && selection.id == c.familyId)
              || (selection.level == ScopeLevel::cell    && selection.id == i));
            if (sel)
            {
                g.setColour (selectRing);
                g.drawEllipse (juce::Rectangle<float> (rad * 2.6f, rad * 2.6f).withCentre (pos), 2.0f);
            }

            if (i == hoverCell)
            {
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.9f, rad * 2.9f).withCentre (pos), 1.0f);
            }
        }
    }

    void CultureChamber::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::Path p;
        p.startNewSubPath (area.getX(), area.getBottom());

        const int n = EngineSnapshot::spectrumBins;
        for (int i = 0; i < n; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) (n - 1);
            const float v = juce::jlimit (0.0f, 1.0f, snap.spectrum[i]);
            p.lineTo (x, area.getBottom() - v * area.getHeight());
        }
        p.lineTo (area.getRight(), area.getBottom());
        p.closeSubPath();

        // The spectrum greys out with everything else - when it turns into a
        // flat grey wall, that *is* the noise lock, visible directly.
        const auto c = juce::Colour::fromHSV (snap.worldHue,
                                              0.6f * (1.0f - greyLevel), 0.9f, 1.0f);
        g.setColour (c.withAlpha (0.14f));
        g.fillPath (p);
        g.setColour (c.withAlpha (0.4f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    // =====================================================================

    int CultureChamber::hitTestCell (juce::Point<float> p) const
    {
        int best = -1;
        float bestD = 1.0e9f;
        const float baseR = juce::jmin (field.getWidth(), field.getHeight());
        for (int i = 0; i < snap.count; ++i)
        {
            const auto pos = toPixels (snap.cells[i].x, snap.cells[i].y);
            const float rad = juce::jlimit (5.0f, 60.0f, snap.cells[i].radius * baseR + 6.0f);
            const float d = pos.getDistanceFrom (p);
            if (d < rad * 1.4f && d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void CultureChamber::emitSelection()
    {
        if (onSelectionChanged) onSelectionChanged (selection);
        repaint();
    }

    // =====================================================================

    void CultureChamber::showContextMenu (int cellIndex)
    {
        juce::PopupMenu m;

        Selection target = selection;
        if (cellIndex >= 0)
        {
            const auto& c = snap.cells[cellIndex];
            target.active = true;
            target.cellSlot = cellIndex;
            if (! selection.active || selection.level == ScopeLevel::colony)
            { target.level = ScopeLevel::family; target.id = c.familyId; }
        }

        m.addSectionHeader (target.describe());
        m.addItem (1, "Inspect genome");
        m.addSeparator();
        m.addItem (2, "Isolate (solo)");
        m.addItem (3, "Clear isolation");
        m.addItem (4, "Mute");
        m.addItem (5, "Unmute");
        m.addSeparator();
        m.addItem (6, "Preserve");
        m.addItem (7, "Release (un-preserve)");
        m.addItem (8, "Eliminate now");
        m.addSeparator();

        juce::PopupMenu infect;
        infect.addItem (20, "Metallize");
        infect.addItem (21, "Reverse");
        infect.addItem (22, "Vocalise");
        infect.addItem (23, "Destabilise");
        infect.addItem (24, "Cure infection");
        m.addSubMenu ("Infect", infect);

        m.addItem (9, "Send to Breeding Lab");
        m.addSeparator();
        m.addItem (30, "Select whole colony");
        if (cellIndex >= 0)
        {
            m.addItem (31, "Select this species");
            m.addItem (32, "Select this family");
            m.addItem (33, "Select just this cell");
        }

        auto self = juce::Component::SafePointer<CultureChamber> (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [self, target, cellIndex] (int r) mutable
            {
                if (self == nullptr || r == 0) return;
                auto& proc = self->processor;

                auto send = [&] (CommandType t, int ib = 0, float fa = 0.0f, int ia = 0)
                {
                    EngineCommand c;
                    c.type = t;
                    c.scope = target.active ? target.level : ScopeLevel::colony;
                    c.scopeId = target.id;
                    c.ib = ib; c.fa = fa; c.ia = ia;
                    proc.pushCommand (c);
                };

                switch (r)
                {
                    case 1: if (self->onInspect) self->onInspect (target); break;
                    case 2: send (CommandType::isolate); break;
                    case 3: send (CommandType::unisolate); break;
                    case 4: send (CommandType::muteScope, 1); break;
                    case 5: send (CommandType::muteScope, 0); break;
                    case 6: send (CommandType::preserveScope, 1); break;
                    case 7: send (CommandType::preserveScope, 0); break;
                    case 8: send (CommandType::eliminateScope); break;
                    case 9: if (self->onSendToBreedingLab) self->onSendToBreedingLab (target); break;
                    case 20: send (CommandType::infect, 0, 0.0f, 1); break;
                    case 21: send (CommandType::infect, 0, 0.0f, 2); break;
                    case 22: send (CommandType::infect, 0, 0.0f, 3); break;
                    case 23: send (CommandType::infect, 0, 0.0f, 4); break;
                    case 24: send (CommandType::cure); break;
                    case 30: self->selection.active = false; self->selection.level = ScopeLevel::colony; self->emitSelection(); break;
                    case 31: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::species; self->selection.id = self->snap.cells[cellIndex].species; self->emitSelection(); } break;
                    case 32: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::family; self->selection.id = self->snap.cells[cellIndex].familyId; self->emitSelection(); } break;
                    case 33: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::cell; self->selection.id = cellIndex; self->emitSelection(); } break;
                    default: break;
                }
            });
    }
}
